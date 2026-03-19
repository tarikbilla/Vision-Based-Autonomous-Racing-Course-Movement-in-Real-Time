#include <opencv2/opencv.hpp>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <mutex>
#include <optional>
#include <numeric>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <cctype>
#include <cerrno>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <memory>

#ifdef HAVE_SIMPLEBLE
#include <simpleble/SimpleBLE.h>
#endif

namespace {

using Clock = std::chrono::steady_clock;
using SysClock = std::chrono::system_clock;

constexpr double kPi = 3.14159265358979323846;

constexpr const char* WRITE_UUID = "6e400002-b5a3-f393-e0a9-e50e24dcca9e";
constexpr double B_PER_DEG = 16000.0 / 90.0;
constexpr double CONTROL_HZ = 20.0;

std::vector<uint8_t> HEADER = {0xbf, 0x0a, 0x00, 0x08, 0x28, 0x00};
std::vector<uint8_t> BRAKE_PACKET = {0xbf, 0x0a, 0x00, 0x08, 0x28, 0x00, 0x00, 0x00, 0x00,
                                     0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0xba};
std::vector<uint8_t> IDLE_PACKET = {0xbf, 0x0a, 0x00, 0x08, 0x28, 0x00, 0x00, 0x00,
                                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x4e};
constexpr uint8_t TAIL = 0x4E;

template <typename T>
T clampv(T v, T lo, T hi) {
    return std::max(lo, std::min(hi, v));
}

double sign_nonzero(double v) {
    return v < 0.0 ? -1.0 : 1.0;
}

double wrap_pi(double a) {
    return std::fmod(a + kPi, 2.0 * kPi) - kPi;
}

int16_t to_s16(double v) {
    const auto iv = static_cast<int>(std::llround(v));
    return static_cast<int16_t>(clampv(iv, -32768, 32767));
}

std::vector<uint8_t> build_packet(double a, double b, double c) {
    std::vector<uint8_t> out = HEADER;
    auto push_s16le = [&](double x) {
        int16_t iv = to_s16(x);
        out.push_back(static_cast<uint8_t>(iv & 0xff));
        out.push_back(static_cast<uint8_t>((iv >> 8) & 0xff));
    };
    push_s16le(a);
    push_s16le(b);
    push_s16le(c);
    push_s16le(0.0);
    out.push_back(TAIL);
    return out;
}

struct State {
    double speed = 0.0;
    double turn_deg = 0.0;
    double target_deg = 0.0;
    double max_speed = 200.0;
    double max_deg = 24.0;
    double steer_smooth = 70.0;
    double ratio = 0.9;
    int c_sign = -1;
    double speed_k = 0.08;
    bool braking = false;
    bool logging = false;
    std::string log_path;
    bool auto_enabled = true;
    double v_ref_raw = 0.0;
    double delta_cmd = 0.0;
    std::string status;
    double speed_step = 1.0;
};

std::mutex state_mutex;
State state;
std::ofstream log_file;

// ===== SIMPLIFIED VISION STRUCT (LOW-REGULATION) =====
struct VisionData {
    int64_t t_ms = 0;
    double dt_ms = 0.0;
    double meas_cx = 0.0;
    double meas_cy = 0.0;
    bool used_meas = false;
};

std::mutex vision_mutex;
VisionData vision;

double now_sec() {
    auto now = SysClock::now();
    auto d = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::duration<double>>(d).count();
}

struct AbcResult {
    double a_sent;
    double b;
    double c;
    double drop;
};

AbcResult compute_abc() {
    std::scoped_lock lk(state_mutex);
    const double b = state.turn_deg * B_PER_DEG;
    const double c = state.c_sign * state.ratio * b;

    const double speed_raw = clampv(state.speed, -state.max_speed, state.max_speed);
    const double drop = state.speed_k * std::abs(b);
    double a_sent = 0.0;
    if (std::abs(speed_raw) > drop) {
        a_sent = sign_nonzero(speed_raw) * (std::abs(speed_raw) - drop);
    }
    a_sent = clampv(a_sent, -state.max_speed, state.max_speed);
    return {a_sent, b, c, drop};
}

// ===== LOW-REGULATION CONSTANTS =====
constexpr int CAM_INDEX = 0;
constexpr int DEFAULT_FRAME_W = 480;      // Ultra-low: 480p width
constexpr int DEFAULT_FRAME_H = 270;      // Ultra-low: 270p height
constexpr int DEFAULT_CAM_FPS = 10;       // Throttle to 10 FPS
constexpr int SKIP_FRAMES = 2;            // Skip every 2nd frame for processing
const std::string CENTERLINE_CSV = "centerline.csv";

int get_env_int(const char* key, int default_value) {
    const char* v = std::getenv(key);
    if (!v || !*v) return default_value;
    try { return std::stoi(std::string(v)); } catch (...) { return default_value; }
}

// ===== MINIMAL IMAGE PROCESSING PARAMETERS =====
constexpr int MOG2_HISTORY = 200;         // Reduced from 500
constexpr double MOG2_VAR_THRESHOLD = 25.0; // Increased threshold (less sensitive)
constexpr bool MOG2_SHADOWS = false;
constexpr int THRESH_FG = 200;
constexpr int OPEN_K = 3;                 // Minimal morphology
constexpr int CLOSE_K = 5;
constexpr int OPEN_IT = 1;
constexpr int CLOSE_IT = 1;
constexpr double MIN_AREA = 100.0;        // Reduced from 250
constexpr double MAX_AREA = 100000.0;     // Reduced from 250000
constexpr int MIN_WH = 5;
constexpr int MAX_WH = 400;
constexpr int MIN_MOTION_PIXELS = 50;     // Reduced from 120
constexpr int ROI_HALF_SIZE = 120;        // Smaller ROI
constexpr int ROI_MARGIN = 10;
constexpr int ROI_FAIL_TO_GLOBAL = 20;    // More aggressive global search
constexpr int GLOBAL_SEARCH_EVERY = 16;   // Less frequent global search
constexpr int HOLD_AFTER_NO_MEAS = 2;

// ===== MINIMAL KALMAN FILTER PARAMETERS =====
constexpr double CV_ACC_SIGMA = 300.0;    // More relaxed
constexpr double CT_ACC_SIGMA = 50.0;
constexpr double CT_OMEGA_SIGMA = 0.5;
constexpr double CT_OMEGA_MAX = 8.0;
constexpr double MEAS_SIGMA_PX = 12.0;    // Increased measurement noise
constexpr double IMM_P_STAY_CV = 0.95;    // Less commitment to CV model
constexpr double IMM_P_STAY_CT = 0.90;
std::array<double, 2> IMM_MU0 = {0.7, 0.3};

constexpr double MEAS_GATE_PX = 100.0;    // Larger gate
constexpr double MEAS_ALPHA_SLOW = 0.50;
constexpr double MEAS_ALPHA_FAST = 0.85;
constexpr double FAST_SPEED_PX_S = 300.0;
constexpr int MOTION_WINDOW = 1;
constexpr double MIN_STEP_DIST = 2.0;
constexpr double EMA_ALPHA_SLOW = 0.20;
constexpr double EMA_ALPHA_FAST = 0.60;
constexpr double PREDICT_DT_MIN = 0.080;
constexpr double PREDICT_DT_MAX = 0.250;
constexpr double MIN_SPEED_FOR_VEL_HEADING = 10.0;
constexpr int NEAREST_WIN = 30;
constexpr int HEADING_LEN = 60;

// ===== MINIMAL MAP PARAMETERS =====
constexpr int MAP_N_SAMPLES = 400;        // Reduced from 1200
constexpr int MAP_SMOOTH_WIN = 21;        // Reduced from 31
constexpr int MAP_CLEAN_CLOSE_K = 21;

std::mutex centerline_mutex;
std::vector<cv::Point2d> centerline_pts;
double centerline_scale_x = 1.0;
double centerline_scale_y = 1.0;
bool centerline_loaded = false;

// ===== SIMPLIFIED CENTERLINE LOADING =====
std::optional<std::vector<cv::Point2d>> load_centerline(
    const std::string& path, bool flip, int target_w, int target_h
) {
    std::ifstream f(path);
    if (!f) {
        std::cerr << "[CL] Cannot open " << path << "\n";
        return std::nullopt;
    }

    std::vector<cv::Point2d> pts;
    std::string line;
    int src_w = target_w, src_h = target_h;

    // Parse CSV header for source dimensions
    while (std::getline(f, line)) {
        if (line.empty() || line[0] != '#') break;
        if (line.find("W=") != std::string::npos) {
            try {
                size_t pos = line.find("W=") + 2;
                src_w = std::stoi(line.substr(pos));
            } catch (...) {}
        }
        if (line.find("H=") != std::string::npos) {
            try {
                size_t pos = line.find("H=") + 2;
                src_h = std::stoi(line.substr(pos));
            } catch (...) {}
        }
    }

    double sx = (src_w > 0) ? static_cast<double>(target_w) / src_w : 1.0;
    double sy = (src_h > 0) ? static_cast<double>(target_h) / src_h : 1.0;

    // Parse points (minimal error handling for speed)
    do {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream iss(line);
        double x, y;
        if (iss >> x >> y) {
            x *= sx;
            y *= sy;
            if (flip) x = target_w - x;
            pts.push_back({x, y});
        }
    } while (std::getline(f, line));

    f.close();
    std::cout << "[CL] Loaded " << pts.size() << " points (sx=" << sx << ", sy=" << sy << ")\n";
    return pts;
}

// ===== SIMPLIFIED CAMERA THREAD =====
class CamThread {
public:
    CamThread(int idx, int w, int h, int fps) 
        : idx_(idx), w_(w), h_(h), fps_(fps), stop_(false), frame_count_(0) {}

    void start() {
        if (thread_.joinable()) return;
        cv::VideoCapture cap(idx_, cv::CAP_V4L2);
        if (!cap.isOpened()) {
            std::cerr << "[CAM] Failed to open camera\n";
            return;
        }

        // Set minimal properties
        cap.set(cv::CAP_PROP_FRAME_WIDTH, w_);
        cap.set(cv::CAP_PROP_FRAME_HEIGHT, h_);
        if (fps_ > 0) cap.set(cv::CAP_PROP_FPS, fps_);
        cap.set(cv::CAP_PROP_BUFFERSIZE, 1);

        cap_ = std::move(cap);
        thread_ = std::thread([this]() { this->run(); });
    }

    void stop() {
        stop_ = true;
        if (thread_.joinable()) thread_.join();
    }

    cv::Mat get_frame() {
        std::scoped_lock lk(frame_mutex_);
        return frame_.clone();
    }

    ~CamThread() { stop(); }

private:
    int idx_, w_, h_, fps_;
    cv::VideoCapture cap_;
    std::thread thread_;
    std::atomic_bool stop_;
    int frame_count_;
    cv::Mat frame_;
    std::mutex frame_mutex_;

    void run() {
        auto mog2 = cv::createBackgroundSubtractorMOG2(MOG2_HISTORY, MOG2_VAR_THRESHOLD, MOG2_SHADOWS);
        
        while (!stop_) {
            cv::Mat img;
            if (!cap_.read(img)) break;

            frame_count_++;
            if (frame_count_ % (SKIP_FRAMES + 1) != 0) continue; // Skip frames

            // Ultra-minimal processing: just MOG2 + threshold
            cv::Mat fg;
            mog2->apply(img, fg);
            cv::threshold(fg, fg, THRESH_FG, 255, cv::THRESH_BINARY);

            {
                std::scoped_lock lk(frame_mutex_);
                frame_ = fg.clone();
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }
};

// ===== MINIMAL CONTROL THREAD =====
void control_thread_func(CamThread* cam, int frame_w, int frame_h) {
    int frame_skip_count = 0;
    auto last_cmd_time = Clock::now();

    while (true) {
        auto now = Clock::now();
        auto delta = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_cmd_time);

        if (delta.count() < 50) { // 20 Hz minimum
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        // Get current frame
        cv::Mat fg = cam->get_frame();
        if (fg.empty()) continue;

        // Ultra-simple centerline detection: find centroid of white pixels
        cv::Moments m = cv::moments(fg);
        double cx = m.m10 / (m.m00 + 1e-6);
        double cy = m.m01 / (m.m00 + 1e-6);

        // Simple steering: center-based
        double err = cx - frame_w / 2.0;
        double target_deg = clampv(-err / 20.0, -state.max_deg, state.max_deg);

        {
            std::scoped_lock lk(state_mutex);
            state.target_deg = target_deg;
            state.turn_deg = clampv(
                state.target_deg + (state.turn_deg - state.target_deg) * (state.steer_smooth / 100.0),
                -state.max_deg, state.max_deg
            );

            {
                std::scoped_lock lk(vision_mutex);
                vision.meas_cx = cx;
                vision.meas_cy = cy;
                vision.used_meas = true;
            }
        }

        last_cmd_time = now;
    }
}

// ===== BLE COMMUNICATION (SIMPLIFIED) =====
bool send_ble_command(double a, double b, double c) {
    // Placeholder: would send via BLE here
    // For now, just log
    auto pkt = build_packet(a, b, c);
    std::cout << "[BLE] Send: a=" << a << " b=" << b << " c=" << c << "\n";
    return true;
}

void ble_thread_func() {
    auto last_send = Clock::now();

    while (true) {
        auto now = Clock::now();
        auto delta = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_send);

        if (delta.count() >= 50) { // 20 Hz
            auto result = compute_abc();
            send_ble_command(result.a_sent, result.b, result.c);
            last_send = now;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

} // namespace

int main(int argc, char* argv[]) {
    std::cout << "=== LOW-REGULATION AUTONOMOUS CONTROLLER ===\n";
    std::cout << "Ultra-low CPU mode: 480p, 10 FPS, minimal processing\n\n";

    // Parse CLI arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--max-deg" && i + 1 < argc) {
            state.max_deg = std::stod(argv[++i]);
        } else if (arg == "--steer-smooth" && i + 1 < argc) {
            state.steer_smooth = std::stod(argv[++i]);
        } else if (arg == "--ratio" && i + 1 < argc) {
            state.ratio = std::stod(argv[++i]);
        } else if (arg == "--c-sign" && i + 1 < argc) {
            state.c_sign = static_cast<int>(std::stod(argv[++i]));
        } else if (arg == "--speed-k" && i + 1 < argc) {
            state.speed_k = std::stod(argv[++i]);
        } else if (arg == "--speed" && i + 1 < argc) {
            state.speed = std::stod(argv[++i]);
        }
    }

    // Read env vars for camera resolution
    int frame_w = get_env_int("CAM_FRAME_W", DEFAULT_FRAME_W);
    int frame_h = get_env_int("CAM_FRAME_H", DEFAULT_FRAME_H);
    int frame_fps = get_env_int("CAM_FPS", DEFAULT_CAM_FPS);

    frame_w = clampv(frame_w, 320, 1280);
    frame_h = clampv(frame_h, 240, 1080);
    frame_fps = clampv(frame_fps, 5, 30);

    std::cout << "Camera: " << frame_w << "x" << frame_h << " @ " << frame_fps << " FPS\n";
    std::cout << "Control: max_deg=" << state.max_deg << ", steer_smooth=" << state.steer_smooth
              << ", ratio=" << state.ratio << ", c_sign=" << state.c_sign << ", speed_k=" << state.speed_k << "\n\n";

    // Load centerline
    auto centerline = load_centerline(CENTERLINE_CSV, false, frame_w, frame_h);
    if (centerline) {
        std::scoped_lock lk(centerline_mutex);
        centerline_pts = *centerline;
        centerline_loaded = true;
    } else {
        std::cout << "[WARN] No centerline loaded, using centroid-based steering\n";
    }

    // Start camera thread
    CamThread cam(CAM_INDEX, frame_w, frame_h, frame_fps);
    cam.start();
    std::cout << "[OK] Camera started\n";

    // Start control thread
    std::thread ctrl(control_thread_func, &cam, frame_w, frame_h);
    ctrl.detach();
    std::cout << "[OK] Control thread started\n";

    // Start BLE thread
    std::thread ble(ble_thread_func);
    ble.detach();
    std::cout << "[OK] BLE thread started\n";

    // Main loop: simple keyboard control
    std::cout << "\nKeyboard controls:\n";
    std::cout << "  'w' / 's': speed up / down\n";
    std::cout << "  'a' / 'd': steer left / right\n";
    std::cout << "  'b': brake\n";
    std::cout << "  'q': quit\n\n";

    while (true) {
        char c;
        if (std::cin.get(c) && c == 'q') break;

        {
            std::scoped_lock lk(state_mutex);
            switch (c) {
                case 'w': state.speed = std::min(state.speed + state.speed_step, state.max_speed); break;
                case 's': state.speed = std::max(state.speed - state.speed_step, -state.max_speed); break;
                case 'a': state.target_deg = std::max(state.target_deg - 2.0, -state.max_deg); break;
                case 'd': state.target_deg = std::min(state.target_deg + 2.0, state.max_deg); break;
                case 'b': state.braking = true; break;
                case ' ': state.speed = 0; state.braking = false; break;
            }
        }
    }

    std::cout << "\n[OK] Shutting down...\n";
    cam.stop();

    return 0;
}
