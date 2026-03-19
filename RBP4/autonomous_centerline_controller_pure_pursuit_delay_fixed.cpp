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
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <mutex>
#include <optional>
#include <numeric>
#include <queue>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <cstdio>
#include <cstdlib>

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
    double max_deg = 12.0;
    double steer_smooth = 55.0;
    double ratio = 0.80;
    double c_sign = -1.0;
    double max_speed = 6000.0;
    double speed_step = 90.0;
    double speed_k = 0.03;
    bool braking = false;
    bool logging = false;
    std::string log_path;
    bool auto_enabled = true;
    double auto_steer_sign = 1.0;
    std::optional<double> last_good_theta;
    double v_ref_raw = 0.0;
    double delta_cmd = 0.0;
    std::string status = "INIT";
    double last_e_lat = 0.0;
    double last_e_head_deg = 0.0;
    double last_track_t = 0.0;
    bool flip_centerline = false;
    double drive_bias_deg = 0.0;
    double last_delta_for_speed = 0.0;
    double pp_target_x = std::numeric_limits<double>::quiet_NaN();
    double pp_target_y = std::numeric_limits<double>::quiet_NaN();
    double pp_alpha_deg = 0.0;
    double pp_lookahead_px = 0.0;
    double e_lat_filt = 0.0;
    double pp_alpha_filt = 0.0;
    double e_lat_rate_filt = 0.0;
    double pp_alpha_rate_filt = 0.0;
};

State state;
std::mutex state_mutex;

struct VisionData {
    double t_ms = 0.0;
    double dt_ms = 0.0;
    std::string mode = "GLOBAL";
    bool used_meas = false;
    bool holding = false;
    int meas_cx = -1;
    int meas_cy = -1;
    double kf_x = 0.0;
    double kf_y = 0.0;
    double kf_vx = 0.0;
    double kf_vy = 0.0;
    double blend_x = 0.0;
    double blend_y = 0.0;
    double ctrl_x = 0.0;
    double ctrl_y = 0.0;
    std::string blend_src = "NONE";
    double blend_a = 0.0;
    double speed_px_s = 0.0;
    double theta_deg = std::numeric_limits<double>::quiet_NaN();
    std::string theta_src = "NONE";
    double step_px = 0.0;
    double mu_cv = 0.5;
    double mu_ct = 0.5;
    double omega_deg_s = 0.0;
    double e_lat = std::numeric_limits<double>::quiet_NaN();
    double e_head_deg = std::numeric_limits<double>::quiet_NaN();
    double pp_target_x = std::numeric_limits<double>::quiet_NaN();
    double pp_target_y = std::numeric_limits<double>::quiet_NaN();
    double pp_alpha_deg = std::numeric_limits<double>::quiet_NaN();
    double pp_lookahead_px = 0.0;
};

VisionData vision;
std::mutex vision_mutex;

std::atomic<bool> cv_stop{false};
std::mutex centerline_mutex;
std::optional<std::vector<cv::Point2d>> new_centerline;

class BleClient {
  public:
    virtual ~BleClient() = default;
    virtual bool is_connected() const = 0;
    virtual bool write_gatt_char(const std::string& uuid, const std::vector<uint8_t>& data) = 0;
};

class DummyBleClient final : public BleClient {
  public:
    bool is_connected() const override { return true; }
    bool write_gatt_char(const std::string&, const std::vector<uint8_t>&) override { return true; }
};

class UnsupportedBleClient final : public BleClient {
  public:
    bool is_connected() const override { return false; }
    bool write_gatt_char(const std::string&, const std::vector<uint8_t>&) override { return false; }
};

std::unique_ptr<BleClient> ble_client;

std::ofstream log_file;

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

void write_csv_header() {
    log_file
        << "timestamp,a_sent,speed_raw,speed_drop,turn_deg,target_deg,b_val,c_val,cb_ratio_live,ratio_setting,c_sign,max_deg,steer_smooth,max_speed,speed_step,speed_k,braking,auto_enabled,auto_speed_cmd,auto_delta_cmd,auto_status,"
        << "t_ms,dt_ms,mode,used_meas,holding,meas_cx,meas_cy,kf_x,kf_y,kf_vx,kf_vy,blend_x,blend_y,ctrl_x,ctrl_y,blend_src,blend_a,speed_px_s,theta_deg,theta_src,step_px,mu_cv,mu_ct,omega_deg_s,e_lat,e_head_deg\n";
}

void start_logging() {
    std::scoped_lock lk(state_mutex);
    if (state.logging) {
        return;
    }

    const auto t = std::time(nullptr);
    std::tm tm{};
    localtime_r(&t, &tm);
    std::ostringstream ts;
    ts << std::put_time(&tm, "%Y%m%d_%H%M%S");

    std::filesystem::path p = std::filesystem::current_path() / ("auto_collect_" + ts.str() + ".csv");
    log_file.open(p.string(), std::ios::out | std::ios::trunc);
    if (!log_file) {
        std::cerr << "[LOG] Failed to open file: " << p << "\n";
        return;
    }
    write_csv_header();
    state.logging = true;
    state.log_path = p.string();
}

void stop_logging() {
    std::scoped_lock lk(state_mutex);
    state.logging = false;
    if (log_file.is_open()) {
        log_file.flush();
        log_file.close();
    }
}

std::string fmt_num(double v, int p) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(p) << v;
    return oss.str();
}

void log_row(const AbcResult& r) {
    if (!log_file.is_open()) {
        return;
    }

    State s;
    {
        std::scoped_lock lk(state_mutex);
        if (!state.logging) {
            return;
        }
        s = state;
    }

    VisionData v;
    {
        std::scoped_lock lk(vision_mutex);
        v = vision;
    }

    const double live_ratio = std::abs(r.b) > 1e-9 ? (r.c / r.b) : 0.0;

    log_file << fmt_num(now_sec(), 6) << ',' << std::llround(r.a_sent) << ',' << std::llround(s.speed) << ','
             << fmt_num(r.drop, 3) << ',' << fmt_num(s.turn_deg, 3) << ',' << fmt_num(s.target_deg, 3) << ','
             << std::llround(r.b) << ',' << std::llround(r.c) << ',' << fmt_num(live_ratio, 5) << ','
             << fmt_num(s.ratio, 5) << ',' << static_cast<int>(s.c_sign) << ',' << fmt_num(s.max_deg, 3) << ','
             << fmt_num(s.steer_smooth, 3) << ',' << std::llround(s.max_speed) << ',' << std::llround(s.speed_step)
             << ',' << fmt_num(s.speed_k, 5) << ',' << static_cast<int>(s.braking) << ','
             << static_cast<int>(s.auto_enabled) << ',' << std::llround(s.v_ref_raw) << ','
             << fmt_num(s.delta_cmd, 3) << ',' << s.status << ',' << std::llround(v.t_ms) << ','
             << fmt_num(v.dt_ms, 3) << ',' << v.mode << ',' << static_cast<int>(v.used_meas) << ','
             << static_cast<int>(v.holding) << ',' << v.meas_cx << ',' << v.meas_cy << ','
             << fmt_num(v.kf_x, 3) << ',' << fmt_num(v.kf_y, 3) << ',' << fmt_num(v.kf_vx, 3) << ','
             << fmt_num(v.kf_vy, 3) << ',' << fmt_num(v.blend_x, 3) << ',' << fmt_num(v.blend_y, 3) << ','
             << fmt_num(v.ctrl_x, 3) << ',' << fmt_num(v.ctrl_y, 3) << ',' << v.blend_src << ','
             << fmt_num(v.blend_a, 3) << ',' << fmt_num(v.speed_px_s, 3) << ',';

    if (std::isfinite(v.theta_deg)) {
        log_file << fmt_num(v.theta_deg, 3);
    } else {
        log_file << "nan";
    }

    log_file << ',' << v.theta_src << ',' << fmt_num(v.step_px, 3) << ',' << fmt_num(v.mu_cv, 4) << ','
             << fmt_num(v.mu_ct, 4) << ',' << fmt_num(v.omega_deg_s, 2) << ',';

    if (std::isfinite(v.e_lat)) {
        log_file << fmt_num(v.e_lat, 2);
    } else {
        log_file << "nan";
    }
    log_file << ',';
    if (std::isfinite(v.e_head_deg)) {
        log_file << fmt_num(v.e_head_deg, 2);
    } else {
        log_file << "nan";
    }
    log_file << '\n';
    log_file.flush();
}

constexpr int CAM_INDEX = 0;
constexpr int FRAME_W = 1280;
constexpr int FRAME_H = 720;
const std::string CENTERLINE_CSV = "centerline.csv";

constexpr int MOG2_HISTORY = 500;
constexpr double MOG2_VAR_THRESHOLD = 16.0;
constexpr bool MOG2_SHADOWS = false;
constexpr int THRESH_FG = 200;
constexpr int OPEN_K = 5;
constexpr int CLOSE_K = 7;
constexpr int OPEN_IT = 1;
constexpr int CLOSE_IT = 2;
constexpr double MIN_AREA = 250.0;
constexpr double MAX_AREA = 250000.0;
constexpr int MIN_WH = 10;
constexpr int MAX_WH = 900;
constexpr int MIN_MOTION_PIXELS = 120;
constexpr int ROI_HALF_SIZE = 180;
constexpr int ROI_MARGIN = 20;
constexpr int ROI_FAIL_TO_GLOBAL = 10;
constexpr int GLOBAL_SEARCH_EVERY = 8;
constexpr int HOLD_AFTER_NO_MEAS = 3;
constexpr bool RESET_VEL_AFTER_HOLD = true;

constexpr double CV_ACC_SIGMA = 200.0;
constexpr double CT_ACC_SIGMA = 30.0;
constexpr double CT_OMEGA_SIGMA = 0.3;
constexpr double CT_OMEGA_MAX = 6.0;
constexpr double CT_OMEGA_INIT_VAR = 0.01;
constexpr double MEAS_SIGMA_PX = 8.0;
constexpr double IMM_P_STAY_CV = 0.97;
constexpr double IMM_P_STAY_CT = 0.92;
std::array<double, 2> IMM_MU0 = {0.8, 0.2};

constexpr double MEAS_GATE_PX = 70.0;
constexpr double MEAS_ALPHA_SLOW = 0.65;
constexpr double MEAS_ALPHA_FAST = 0.88;
constexpr double FAST_SPEED_PX_S = 450.0;
constexpr int MOTION_WINDOW = 1;
constexpr double MIN_STEP_DIST = 1.2;
constexpr double EMA_ALPHA_SLOW = 0.28;
constexpr double EMA_ALPHA_FAST = 0.70;
constexpr double PREDICT_DT_MIN = 0.060;
constexpr double PREDICT_DT_MAX = 0.180;
constexpr double MIN_SPEED_FOR_VEL_HEADING = 8.0;
constexpr double MIN_SPEED_FOR_PREDICT = 5.0;
constexpr int NEAREST_WIN = 40;
constexpr int HEADING_LEN = 90;

constexpr int MAP_N_SAMPLES = 1200;
constexpr int MAP_SMOOTH_WIN = 31;
constexpr int MAP_CLEAN_CLOSE_K = 31;
constexpr int MAP_CLEAN_CLOSE_ITER = 1;
constexpr int MAP_CLEAN_OPEN_K = 7;
constexpr int MAP_CLEAN_OPEN_ITER = 1;
constexpr int MAP_CLEAN_MAX_HOLE = 5000;
constexpr int MAP_CENTER_CLOSE_K = 7;
constexpr int MAP_CENTER_CLOSE_ITER = 2;

class LightingMode {
  public:
    static std::string current() {
        return modes()[index()];
    }

    static void next() {
        index() = (index() + 1) % static_cast<int>(modes().size());
        std::cout << "[CV] Lighting mode -> " << current() << "\n";
    }

  private:
    static std::vector<std::string>& modes() {
        static std::vector<std::string> v = {"AUTO_CLAHE", "NONE", "EQUALIZE", "GAMMA_BRIGHT", "GAMMA_DARK"};
        return v;
    }

    static int& index() {
        static int i = 0;
        return i;
    }
};

cv::Mat preprocess_lighting(const cv::Mat& bgr) {
    const std::string mode = LightingMode::current();
    if (mode == "NONE") {
        return bgr.clone();
    }
    if (mode == "EQUALIZE") {
        cv::Mat lab;
        cv::cvtColor(bgr, lab, cv::COLOR_BGR2Lab);
        std::vector<cv::Mat> ch;
        cv::split(lab, ch);
        cv::equalizeHist(ch[0], ch[0]);
        cv::merge(ch, lab);
        cv::Mat out;
        cv::cvtColor(lab, out, cv::COLOR_Lab2BGR);
        return out;
    }
    if (mode == "GAMMA_BRIGHT" || mode == "GAMMA_DARK") {
        const double g = mode == "GAMMA_BRIGHT" ? 0.5 : 1.8;
        cv::Mat lut(1, 256, CV_8U);
        for (int i = 0; i < 256; ++i) {
            lut.at<uint8_t>(0, i) = static_cast<uint8_t>(std::pow(i / 255.0, g) * 255.0);
        }
        cv::Mat out;
        cv::LUT(bgr, lut, out);
        return out;
    }

    cv::Mat lab;
    cv::cvtColor(bgr, lab, cv::COLOR_BGR2Lab);
    std::vector<cv::Mat> ch;
    cv::split(lab, ch);
    auto clahe = cv::createCLAHE(3.0, cv::Size(8, 8));
    clahe->apply(ch[0], ch[0]);
    cv::merge(ch, lab);
    cv::Mat out;
    cv::cvtColor(lab, out, cv::COLOR_Lab2BGR);
    return out;
}

void draw_arrow(cv::Mat& img, double x, double y, double theta, int length, const cv::Scalar& color, int thickness = 3) {
    int x2 = static_cast<int>(std::lround(x + std::cos(theta) * length));
    int y2 = static_cast<int>(std::lround(y + std::sin(theta) * length));
    cv::arrowedLine(img, cv::Point(static_cast<int>(std::lround(x)), static_cast<int>(std::lround(y))),
                    cv::Point(x2, y2), color, thickness, cv::LINE_AA, 0, 0.25);
}

double ema_angle(std::optional<double> prev, double now_a, double alpha) {
    if (!prev.has_value()) {
        return now_a;
    }
    const double mx = (1.0 - alpha) * std::cos(*prev) + alpha * std::cos(now_a);
    const double my = (1.0 - alpha) * std::sin(*prev) + alpha * std::sin(now_a);
    return std::atan2(my, mx);
}

double adaptive_alpha(double speed) {
    const double t = clampv(speed / FAST_SPEED_PX_S, 0.0, 1.0);
    return (1.0 - t) * EMA_ALPHA_SLOW + t * EMA_ALPHA_FAST;
}

double adaptive_meas_alpha(double speed) {
    const double t = clampv(speed / FAST_SPEED_PX_S, 0.0, 1.0);
    return (1.0 - t) * MEAS_ALPHA_SLOW + t * MEAS_ALPHA_FAST;
}

class CamThread {
  public:
    CamThread(int src, int w, int h) : src_(src), w_(w), h_(h) {}

    void start() {
    std::vector<int> backends;
#if defined(__APPLE__)
    backends = {cv::CAP_AVFOUNDATION, cv::CAP_ANY};
#elif defined(_WIN32)
    backends = {cv::CAP_DSHOW, cv::CAP_MSMF, cv::CAP_ANY};
#else
    backends = {cv::CAP_V4L2, cv::CAP_ANY};
#endif

    bool opened = false;
    for (int backend : backends) {
        if (cap_.open(src_, backend) && cap_.isOpened()) {
        opened = true;
        break;
        }
        cap_.release();
    }

    if (!opened) {
            throw std::runtime_error("Cannot open camera");
        }
        cap_.set(cv::CAP_PROP_FRAME_WIDTH, w_);
        cap_.set(cv::CAP_PROP_FRAME_HEIGHT, h_);
        cap_.set(cv::CAP_PROP_BUFFERSIZE, 1);
        cap_.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('M', 'J', 'P', 'G'));
        cap_.set(cv::CAP_PROP_AUTO_EXPOSURE, 0.25);
        cap_.set(cv::CAP_PROP_AUTO_WB, 0);
        th_ = std::thread([this] { reader(); });
    }

    std::optional<cv::Mat> read_latest(int timeout_ms) {
        std::unique_lock<std::mutex> lk(m_);
        if (!cv_.wait_for(lk, std::chrono::milliseconds(timeout_ms), [&] { return has_frame_ || stopped_; })) {
            return std::nullopt;
        }
        if (!has_frame_) {
            return std::nullopt;
        }
        has_frame_ = false;
        return latest_.clone();
    }

    void stop() {
        {
            std::scoped_lock lk(m_);
            stopped_ = true;
        }
        cv_.notify_all();
        if (th_.joinable()) {
            th_.join();
        }
        cap_.release();
    }

  private:
    void reader() {
        while (true) {
            {
                std::scoped_lock lk(m_);
                if (stopped_) {
                    break;
                }
            }
            cv::Mat frame;
            if (!cap_.read(frame)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
                continue;
            }
            {
                std::scoped_lock lk(m_);
                latest_ = frame;
                has_frame_ = true;
            }
            cv_.notify_one();
        }
    }

    int src_;
    int w_;
    int h_;
    cv::VideoCapture cap_;
    std::thread th_;
    std::mutex m_;
    std::condition_variable cv_;
    cv::Mat latest_;
    bool has_frame_ = false;
    bool stopped_ = false;
};

cv::Ptr<cv::BackgroundSubtractorMOG2> build_mog2() {
    return cv::createBackgroundSubtractorMOG2(MOG2_HISTORY, MOG2_VAR_THRESHOLD, MOG2_SHADOWS);
}

cv::Mat clean_mask(const cv::Mat& mask) {
    cv::Mat m;
    cv::threshold(mask, m, THRESH_FG, 255, cv::THRESH_BINARY);
    auto ko = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(OPEN_K, OPEN_K));
    auto kc = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(CLOSE_K, CLOSE_K));
    cv::morphologyEx(m, m, cv::MORPH_OPEN, ko, cv::Point(-1, -1), OPEN_IT);
    cv::morphologyEx(m, m, cv::MORPH_CLOSE, kc, cv::Point(-1, -1), CLOSE_IT);
    return m;
}

std::optional<cv::Rect> clamp_roi(int x0, int y0, int x1, int y1, int W, int H) {
    x0 = clampv(x0, 0, W - 1);
    y0 = clampv(y0, 0, H - 1);
    x1 = clampv(x1, 0, W);
    y1 = clampv(y1, 0, H);
    if (x1 <= x0 || y1 <= y0) {
        return std::nullopt;
    }
    return cv::Rect(x0, y0, x1 - x0, y1 - y0);
}

std::optional<cv::Rect> make_roi(double px, double py, int W, int H) {
    return clamp_roi(static_cast<int>(px - ROI_HALF_SIZE - ROI_MARGIN), static_cast<int>(py - ROI_HALF_SIZE - ROI_MARGIN),
                     static_cast<int>(px + ROI_HALF_SIZE + ROI_MARGIN), static_cast<int>(py + ROI_HALF_SIZE + ROI_MARGIN),
                     W, H);
}

struct Det {
    cv::Rect bbox;
    int cx = -1;
    int cy = -1;
    double area = 0;
    int motion_px = 0;
};

std::optional<Det> pick_contour(const cv::Mat& mask, const std::optional<cv::Rect>& roi) {
    cv::Mat sub;
    int x0 = 0, y0 = 0;
    if (roi.has_value()) {
        x0 = roi->x;
        y0 = roi->y;
        sub = mask(*roi);
    } else {
        sub = mask;
    }

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(sub, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    std::optional<Det> best;
    double best_area = 0.0;
    for (const auto& cnt : contours) {
        const double area = cv::contourArea(cnt);
        if (area < MIN_AREA || area > MAX_AREA) {
            continue;
        }
        cv::Rect r = cv::boundingRect(cnt);
        r.x += x0;
        r.y += y0;
        if (r.width < MIN_WH || r.height < MIN_WH || r.width > MAX_WH || r.height > MAX_WH) {
            continue;
        }
        if (area > best_area) {
            best_area = area;
            Det d;
            d.bbox = r;
            d.cx = r.x + r.width / 2;
            d.cy = r.y + r.height / 2;
            d.area = area;
            cv::Rect br(std::max(0, r.x), std::max(0, r.y),
                        std::min(mask.cols, r.x + r.width) - std::max(0, r.x),
                        std::min(mask.rows, r.y + r.height) - std::max(0, r.y));
            d.motion_px = cv::countNonZero(mask(br));
            best = d;
        }
    }

    return best;
}

std::vector<cv::Point2d> ensure_closed(std::vector<cv::Point2d> arr) {
    if (arr.size() < 2) {
        return arr;
    }
    const auto& a = arr.front();
    const auto& b = arr.back();
    if (std::hypot(a.x - b.x, a.y - b.y) > 1e-6) {
        arr.push_back(a);
    }
    return arr;
}

std::optional<std::vector<cv::Point2d>> load_centerline(const std::string& path, bool flip) {
    if (!std::filesystem::exists(path)) {
        return std::nullopt;
    }

    std::ifstream f(path);
    if (!f) {
        return std::nullopt;
    }

    std::vector<cv::Point2d> pts;
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#' || (!line.empty() && (line[0] == 'x' || line[0] == 'X'))) {
            continue;
        }
        std::stringstream ss(line);
        std::string sx, sy;
        if (!std::getline(ss, sx, ',')) {
            continue;
        }
        if (!std::getline(ss, sy, ',')) {
            continue;
        }
        try {
            pts.emplace_back(std::stod(sx), std::stod(sy));
        } catch (...) {
        }
    }

    if (pts.size() < 2) {
        return std::nullopt;
    }

    auto arr = ensure_closed(pts);
    if (flip) {
        std::reverse(arr.begin(), arr.end());
        arr = ensure_closed(arr);
    }
    return arr;
}

void save_centerline(const std::string& path, const std::vector<cv::Point2d>& pts, int W, int H) {
    std::ofstream f(path, std::ios::out | std::ios::trunc);
    f << "# W=" << W << " H=" << H << "\n";
    f << "x,y\n";
    for (const auto& p : pts) {
        f << std::fixed << std::setprecision(3) << p.x << ',' << p.y << '\n';
    }
}

struct NearestResult {
    int seg_i = 0;
    double heading = 0.0;
    double e_lat = 0.0;
    cv::Point2d proj;
};

std::pair<NearestResult, int> nearest_on_polyline(const cv::Point2d& p, const std::vector<cv::Point2d>& path, int last_i,
                                                  int win) {
    const int nseg = static_cast<int>(path.size()) - 1;
    double best_d2 = 1e18;
    std::optional<NearestResult> best;
    int best_i = ((last_i % std::max(1, nseg)) + nseg) % nseg;

    for (int off = -win; off <= win; ++off) {
        int i = (last_i + off) % nseg;
        if (i < 0) {
            i += nseg;
        }
        const cv::Point2d a = path[i];
        const cv::Point2d b = path[i + 1];
        const cv::Point2d ab = b - a;
        const double ab2 = ab.dot(ab);
        if (ab2 < 1e-9) {
            continue;
        }
        const double t = clampv((p - a).dot(ab) / ab2, 0.0, 1.0);
        const cv::Point2d proj = a + t * ab;
        const cv::Point2d d = p - proj;
        const double d2 = d.dot(d);
        if (d2 < best_d2) {
            best_d2 = d2;
            const double heading = std::atan2(ab.y, ab.x);
            const double cross = ab.x * (p.y - a.y) - ab.y * (p.x - a.x);
            best = NearestResult{i, heading, (cross >= 0.0 ? 1.0 : -1.0) * std::sqrt(d2), proj};
            best_i = i;
        }
    }

    if (!best.has_value()) {
        throw std::runtime_error("nearest search failed");
    }
    return {*best, best_i};
}

std::pair<cv::Point2d, std::pair<int, double>> advance_along_path(const std::vector<cv::Point2d>& path, int seg_i,
                                                                  const cv::Point2d& proj_pt, double dist_px) {
    const int nseg = static_cast<int>(path.size()) - 1;
    if (nseg <= 0) {
        return {proj_pt, {seg_i, std::numeric_limits<double>::quiet_NaN()}};
    }

    double dist_left = std::max(0.0, dist_px);
    cv::Point2d cur = proj_pt;
    int i = ((seg_i % nseg) + nseg) % nseg;

    for (int k = 0; k < nseg + 1; ++k) {
        const cv::Point2d nxt = path[i + 1];
        const cv::Point2d seg = nxt - cur;
        const double seg_len = std::hypot(seg.x, seg.y);
        if (seg_len < 1e-9) {
            i = (i + 1) % nseg;
            cur = path[i];
            continue;
        }

        if (dist_left <= seg_len) {
            const double t = dist_left / seg_len;
            const cv::Point2d pt = cur + t * seg;
            const double hdg = std::atan2(seg.y, seg.x);
            return {pt, {i, hdg}};
        }

        dist_left -= seg_len;
        i = (i + 1) % nseg;
        cur = path[i];
    }

    const auto& a = path[i];
    const auto& b = path[i + 1];
    const double hdg = std::atan2(b.y - a.y, b.x - a.x);
    return {cur, {i, hdg}};
}

double dynamic_lookahead_px(double speed_px_s) {
    return clampv(88.0 + 0.78 * speed_px_s, 88.0, 170.0);
}

void draw_centerline(cv::Mat& img, const std::optional<std::vector<cv::Point2d>>& path_cl) {
    if (!path_cl.has_value() || path_cl->size() < 2) {
        return;
    }
    std::vector<cv::Point> pts;
    pts.reserve(path_cl->size());
    for (const auto& p : *path_cl) {
        pts.emplace_back(static_cast<int>(std::lround(p.x)), static_cast<int>(std::lround(p.y)));
    }
    const std::vector<std::vector<cv::Point>> lines = {pts};
    cv::polylines(img, lines, true, cv::Scalar(0, 200, 60), 2, cv::LINE_AA);
}

cv::Mat keep_largest(const cv::Mat& mask255) {
    cv::Mat labels, stats, cents;
    const int n = cv::connectedComponentsWithStats(mask255 > 0, labels, stats, cents, 8);
    if (n <= 1) {
        return mask255.clone();
    }

    int best = 1;
    int best_area = stats.at<int>(1, cv::CC_STAT_AREA);
    for (int i = 2; i < n; ++i) {
        int area = stats.at<int>(i, cv::CC_STAT_AREA);
        if (area > best_area) {
            best_area = area;
            best = i;
        }
    }
    cv::Mat out = cv::Mat::zeros(mask255.size(), mask255.type());
    out.setTo(255, labels == best);
    return out;
}

cv::Mat fill_holes(const cv::Mat& mask255, int max_hole) {
    cv::Mat inv;
    cv::bitwise_not(mask255, inv);
    cv::Mat labels, stats, cents;
    const int n = cv::connectedComponentsWithStats(inv > 0, labels, stats, cents, 8);
    cv::Mat out = mask255.clone();

    const int h = mask255.rows;
    const int w = mask255.cols;
    for (int lab = 1; lab < n; ++lab) {
        const int area = stats.at<int>(lab, cv::CC_STAT_AREA);
        const int x = stats.at<int>(lab, cv::CC_STAT_LEFT);
        const int y = stats.at<int>(lab, cv::CC_STAT_TOP);
        const int ww = stats.at<int>(lab, cv::CC_STAT_WIDTH);
        const int hh = stats.at<int>(lab, cv::CC_STAT_HEIGHT);
        if (x == 0 || y == 0 || x + ww >= w || y + hh >= h) {
            continue;
        }
        if (area <= max_hole) {
            out.setTo(255, labels == lab);
        }
    }
    return out;
}

cv::Mat build_free_white(const cv::Mat& bgr) {
    const int h = bgr.rows;
    const int w = bgr.cols;

    cv::Mat hsv;
    cv::cvtColor(bgr, hsv, cv::COLOR_BGR2HSV);

    cv::Mat red1, red2, red;
    cv::inRange(hsv, cv::Scalar(0, 70, 50), cv::Scalar(12, 255, 255), red1);
    cv::inRange(hsv, cv::Scalar(168, 70, 50), cv::Scalar(179, 255, 255), red2);
    cv::bitwise_or(red1, red2, red);

    std::vector<cv::Point> pts;
    cv::findNonZero(red, pts);
    if (pts.size() < 50) {
        throw std::runtime_error("Not enough red pixels — check lighting / track visible");
    }

    std::vector<cv::Point> hull;
    cv::convexHull(pts, hull);
    cv::Mat roi_mask = cv::Mat::zeros(h, w, CV_8U);
    cv::fillConvexPoly(roi_mask, hull, 255);

    cv::Mat white;
    cv::inRange(hsv, cv::Scalar(0, 0, 180), cv::Scalar(179, 70, 255), white);
    cv::bitwise_and(white, roi_mask, white);

    cv::Mat barriers;
    cv::bitwise_or(red, white, barriers);
    cv::bitwise_and(barriers, roi_mask, barriers);

    auto e5 = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(5, 5));
    auto e7 = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(7, 7));
    auto e9 = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(9, 9));

    cv::morphologyEx(barriers, barriers, cv::MORPH_OPEN, e5, cv::Point(-1, -1), 1);
    cv::morphologyEx(barriers, barriers, cv::MORPH_CLOSE, e7, cv::Point(-1, -1), 3);
    cv::dilate(barriers, barriers, e9, cv::Point(-1, -1), 1);

    cv::Mat inv_roi;
    cv::bitwise_not(roi_mask, inv_roi);

    cv::Mat walls;
    cv::bitwise_or(barriers, inv_roi, walls);

    cv::Mat flood = walls.clone();
    cv::Mat ffmask = cv::Mat::zeros(h + 2, w + 2, CV_8U);
    cv::floodFill(flood, ffmask, cv::Point(0, 0), cv::Scalar(128));

    cv::Mat outside = (flood == 128);
    outside.convertTo(outside, CV_8U, 255.0);

    cv::Mat not_outside;
    cv::bitwise_not(outside, not_outside);
    cv::Mat walls_zero = (walls == 0);
    walls_zero.convertTo(walls_zero, CV_8U, 255.0);

    cv::Mat out;
    cv::bitwise_and(not_outside, walls_zero, out);
    return out;
}

cv::Mat clean_track_mask(const cv::Mat& free_white) {
    cv::Mat m;
    cv::threshold(free_white, m, 0, 255, cv::THRESH_BINARY);
    m = keep_largest(m);

    auto kC = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(MAP_CLEAN_CLOSE_K, MAP_CLEAN_CLOSE_K));
    cv::morphologyEx(m, m, cv::MORPH_CLOSE, kC, cv::Point(-1, -1), MAP_CLEAN_CLOSE_ITER);

    m = fill_holes(m, MAP_CLEAN_MAX_HOLE);

    auto kO = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(MAP_CLEAN_OPEN_K, MAP_CLEAN_OPEN_K));
    cv::morphologyEx(m, m, cv::MORPH_OPEN, kO, cv::Point(-1, -1), MAP_CLEAN_OPEN_ITER);
    return keep_largest(m);
}

std::vector<cv::Point2d> resample(const std::vector<cv::Point2d>& pts, int N) {
    std::vector<cv::Point2d> closed = pts;
    closed.push_back(pts.front());

    std::vector<double> s;
    s.reserve(closed.size());
    s.push_back(0.0);
    for (size_t i = 1; i < closed.size(); ++i) {
        const auto d = std::hypot(closed[i].x - closed[i - 1].x, closed[i].y - closed[i - 1].y);
        s.push_back(s.back() + d);
    }

    const double L = s.back();
    if (L < 1e-6) {
        return pts;
    }

    std::vector<cv::Point2d> out;
    out.reserve(N);
    for (int i = 0; i < N; ++i) {
        const double t = (L * i) / N;
        auto it = std::upper_bound(s.begin(), s.end(), t);
        size_t idx = static_cast<size_t>(std::distance(s.begin(), it));
        idx = std::min(idx, s.size() - 1);
        size_t i0 = idx == 0 ? 0 : idx - 1;
        const double den = std::max(1e-9, s[idx] - s[i0]);
        const double a = (t - s[i0]) / den;
        const cv::Point2d p = closed[i0] * (1.0 - a) + closed[idx] * a;
        out.push_back(p);
    }
    return out;
}

std::vector<cv::Point2d> smooth_cyclic(const std::vector<cv::Point2d>& pts, int win) {
    const int n = static_cast<int>(pts.size());
    if (n < 3 || win < 3) {
        return pts;
    }

    win |= 1;
    const int k = win / 2;

    std::vector<cv::Point2d> padded;
    padded.reserve(n + 2 * k);
    for (int i = n - k; i < n; ++i) {
        padded.push_back(pts[i]);
    }
    for (const auto& p : pts) {
        padded.push_back(p);
    }
    for (int i = 0; i < k; ++i) {
        padded.push_back(pts[i]);
    }

    std::vector<cv::Point2d> out;
    out.reserve(n);
    for (int i = 0; i < n; ++i) {
        cv::Point2d acc(0.0, 0.0);
        for (int j = 0; j < win; ++j) {
            acc += padded[i + j];
        }
        out.push_back(acc * (1.0 / win));
    }
    return out;
}

std::vector<cv::Point2d> build_centerline_points(const cv::Mat& free_clean) {
    cv::Mat road;
    cv::threshold(free_clean, road, 127, 255, cv::THRESH_BINARY);

    auto k = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(MAP_CENTER_CLOSE_K, MAP_CENTER_CLOSE_K));
    cv::morphologyEx(road, road, cv::MORPH_CLOSE, k, cv::Point(-1, -1), MAP_CENTER_CLOSE_ITER);

    std::vector<std::vector<cv::Point>> contours;
    std::vector<cv::Vec4i> hierarchy;
    cv::findContours(road, contours, hierarchy, cv::RETR_CCOMP, cv::CHAIN_APPROX_NONE);
    if (contours.empty() || hierarchy.empty()) {
        throw std::runtime_error("No contours in track mask");
    }

    int outer = -1;
    double max_outer = -1.0;
    for (size_t i = 0; i < hierarchy.size(); ++i) {
        if (hierarchy[i][3] == -1) {
            const double a = std::abs(cv::contourArea(contours[i]));
            if (a > max_outer) {
                max_outer = a;
                outer = static_cast<int>(i);
            }
        }
    }
    if (outer < 0) {
        throw std::runtime_error("Cannot find outer contour");
    }

    int inner = -1;
    double best_a = -1.0;
    int child = hierarchy[outer][2];
    while (child != -1) {
        const double a = std::abs(cv::contourArea(contours[child]));
        if (a > best_a) {
            best_a = a;
            inner = child;
        }
        child = hierarchy[child][0];
    }

    if (inner < 0) {
        std::vector<int> idx(contours.size());
        std::iota(idx.begin(), idx.end(), 0);
        std::sort(idx.begin(), idx.end(), [&](int a, int b) {
            return std::abs(cv::contourArea(contours[a])) > std::abs(cv::contourArea(contours[b]));
        });
        if (idx.size() < 2) {
            throw std::runtime_error("Cannot find inner contour");
        }
        inner = idx[1];
    }

    std::vector<cv::Point2d> outer_pts;
    outer_pts.reserve(contours[outer].size());
    for (const auto& p : contours[outer]) {
        outer_pts.emplace_back(p.x, p.y);
    }

    std::vector<cv::Point2d> inner_pts;
    inner_pts.reserve(contours[inner].size());
    for (const auto& p : contours[inner]) {
        inner_pts.emplace_back(p.x, p.y);
    }

    const auto outer_r = resample(outer_pts, MAP_N_SAMPLES);

    std::vector<cv::Point2d> center;
    center.reserve(outer_r.size());
    for (const auto& op : outer_r) {
        double best_d2 = std::numeric_limits<double>::infinity();
        cv::Point2d bestp;
        for (const auto& ip : inner_pts) {
            const double d2 = (op - ip).dot(op - ip);
            if (d2 < best_d2) {
                best_d2 = d2;
                bestp = ip;
            }
        }
        center.push_back((op + bestp) * 0.5);
    }

    auto smooth = smooth_cyclic(center, MAP_SMOOTH_WIN);
    smooth.push_back(smooth.front());
    return smooth;
}

std::optional<std::vector<cv::Point2d>> build_centerline_from_frame(const cv::Mat& frame) {
    std::cout << "[MAP] Building...\n";
    try {
        const cv::Mat work = preprocess_lighting(frame);
        const cv::Mat free_white = build_free_white(work);
        const cv::Mat free_clean = clean_track_mask(free_white);
        auto pts = build_centerline_points(free_clean);

        {
            std::scoped_lock lk(state_mutex);
            if (state.flip_centerline) {
                std::reverse(pts.begin(), pts.end());
                pts = ensure_closed(pts);
            }
        }

        save_centerline(CENTERLINE_CSV, pts, frame.cols, frame.rows);
        std::cout << "[MAP] Done — " << pts.size() << " pts -> " << CENTERLINE_CSV << "\n";
        return pts;
    } catch (const std::exception& e) {
        std::cerr << "[MAP ERROR] " << e.what() << "\n";
        return std::nullopt;
    }
}

class CVModel {
  public:
    static constexpr int DIM = 4;
    cv::Mat x = cv::Mat::zeros(4, 1, CV_64F);
    cv::Mat P = cv::Mat::eye(4, 4, CV_64F) * 500.0;

    static cv::Mat F(double dt) {
        return (cv::Mat_<double>(4, 4) << 1, 0, dt, 0, 0, 1, 0, dt, 0, 0, 1, 0, 0, 0, 0, 1);
    }

    static cv::Mat Q(double dt) {
        const double dt2 = dt * dt;
        const double dt3 = dt2 * dt;
        const double dt4 = dt3 * dt;
        const double q = CV_ACC_SIGMA * CV_ACC_SIGMA;
        cv::Mat Q = (cv::Mat_<double>(4, 4) << dt4 / 4, 0, dt3 / 2, 0, 0, dt4 / 4, 0, dt3 / 2, dt3 / 2, 0, dt2, 0,
                     0, dt3 / 2, 0, dt2);
        return Q * q;
    }
};

class CTModel {
  public:
    static constexpr int DIM = 5;
    cv::Mat x = cv::Mat::zeros(5, 1, CV_64F);
    cv::Mat P = cv::Mat::eye(5, 5, CV_64F) * 500.0;

    CTModel() {
        P.at<double>(4, 4) = CT_OMEGA_INIT_VAR;
    }

    static cv::Mat F(double dt, double omega) {
        if (std::abs(omega) < 1e-4) {
            return (cv::Mat_<double>(5, 5) << 1, 0, dt, 0, 0, 0, 1, 0, dt, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0,
                    0, 1);
        }
        const double s = std::sin(omega * dt);
        const double c = std::cos(omega * dt);
        const double o = omega;
        return (cv::Mat_<double>(5, 5) << 1, 0, s / o, -(1 - c) / o, 0, 0, 1, (1 - c) / o, s / o, 0, 0, 0, c, -s, 0,
                0, 0, s, c, 0, 0, 0, 0, 0, 1);
    }

    static cv::Mat Q(double dt) {
        const double dt2 = dt * dt;
        const double dt3 = dt2 * dt;
        const double dt4 = dt3 * dt;
        const double qa = CT_ACC_SIGMA * CT_ACC_SIGMA;

        cv::Mat Q5 = cv::Mat::zeros(5, 5, CV_64F);
        cv::Mat t = (cv::Mat_<double>(4, 4) << dt4 / 4, 0, dt3 / 2, 0, 0, dt4 / 4, 0, dt3 / 2, dt3 / 2, 0, dt2, 0, 0,
                    dt3 / 2, 0, dt2) *
                    qa;
        t.copyTo(Q5(cv::Rect(0, 0, 4, 4)));
        Q5.at<double>(4, 4) = (CT_OMEGA_SIGMA * dt) * (CT_OMEGA_SIGMA * dt);
        return Q5;
    }
};

cv::Mat Hfor(int dim) {
    if (dim == 4) {
        return (cv::Mat_<double>(2, 4) << 1, 0, 0, 0, 0, 1, 0, 0);
    }
    return (cv::Mat_<double>(2, 5) << 1, 0, 0, 0, 0, 0, 1, 0, 0, 0);
}

class IMMTracker {
  public:
    static constexpr int N = 2;

    IMMTracker() {
        mu_ = cv::Mat(2, 1, CV_64F);
        mu_.at<double>(0, 0) = IMM_MU0[0];
        mu_.at<double>(1, 0) = IMM_MU0[1];

        Pi_ = (cv::Mat_<double>(2, 2) << IMM_P_STAY_CV, 1.0 - IMM_P_STAY_CV, 1.0 - IMM_P_STAY_CT, IMM_P_STAY_CT);
    }

    bool inited() const { return inited_; }

    void init_at(double x, double y) {
        cv_.x = cv::Mat::zeros(4, 1, CV_64F);
        cv_.x.at<double>(0, 0) = x;
        cv_.x.at<double>(1, 0) = y;
        cv_.P = cv::Mat::eye(4, 4, CV_64F) * 200.0;

        ct_.x = cv::Mat::zeros(5, 1, CV_64F);
        ct_.x.at<double>(0, 0) = x;
        ct_.x.at<double>(1, 0) = y;
        ct_.P = cv::Mat::eye(5, 5, CV_64F) * 200.0;
        ct_.P.at<double>(4, 4) = CT_OMEGA_INIT_VAR;

        inited_ = true;
    }

    std::array<double, 4> correct_always(double zx, double zy, double dt) {
        dt = clampv(dt, 1e-3, 0.2);
        if (!inited_) {
            init_at(zx, zy);
        }

        cv::Mat z = (cv::Mat_<double>(2, 1) << zx, zy);
        cv::Mat R = cv::Mat::eye(2, 2, CV_64F) * (MEAS_SIGMA_PX * MEAS_SIGMA_PX);

        cv::Mat c = Pi_.t() * mu_;

        cv::Mat mu_ij = cv::Mat::zeros(2, 2, CV_64F);
        for (int i = 0; i < N; ++i) {
            for (int j = 0; j < N; ++j) {
                mu_ij.at<double>(i, j) = Pi_.at<double>(i, j) * mu_.at<double>(i, 0) / (c.at<double>(j, 0) + 1e-300);
            }
        }

        std::array<cv::Mat, 2> mx;
        std::array<cv::Mat, 2> mP;

        for (int j = 0; j < N; ++j) {
            cv::Mat x5j = cv::Mat::zeros(5, 1, CV_64F);
            for (int i = 0; i < N; ++i) {
                auto [xi5, Pi5] = to5(i);
                x5j += mu_ij.at<double>(i, j) * xi5;
            }

            cv::Mat P5j = cv::Mat::zeros(5, 5, CV_64F);
            for (int i = 0; i < N; ++i) {
                auto [xi5, Pi5] = to5(i);
                cv::Mat d = xi5 - x5j;
                P5j += mu_ij.at<double>(i, j) * (Pi5 + d * d.t());
            }

            int dj = (j == 0) ? 4 : 5;
            mx[j] = x5j.rowRange(0, dj).clone();
            mP[j] = P5j(cv::Rect(0, 0, dj, dj)).clone();
        }

        cv::Mat liks = cv::Mat::zeros(2, 1, CV_64F);

        for (int j = 0; j < N; ++j) {
            if (j == 0) {
                cv_.x = mx[j].clone();
                cv_.P = mP[j].clone();

                cv::Mat F = CVModel::F(dt);
                cv::Mat Q = CVModel::Q(dt);

                cv::Mat xp = F * cv_.x;
                cv::Mat Pp = F * cv_.P * F.t() + Q;
                cv::Mat H = Hfor(4);
                cv::Mat S = H * Pp * H.t() + R;
                cv::Mat K = Pp * H.t() * S.inv();
                cv::Mat innov = z - H * xp;

                cv_.x = xp + K * innov;
                cv::Mat IKH = cv::Mat::eye(4, 4, CV_64F) - K * H;
                cv_.P = IKH * Pp * IKH.t() + K * R * K.t();

                double detS = std::max(cv::determinant(S), 1e-300);
                cv::Mat tmp = innov.t() * S.inv() * innov;
                double mahal2 = tmp.at<double>(0, 0);
                liks.at<double>(j, 0) = std::exp(std::max(-0.5 * mahal2, -500.0)) / std::sqrt(std::pow(2 * kPi, 2) * detS);
            } else {
                ct_.x = mx[j].clone();
                ct_.P = mP[j].clone();

                double omega = clampv(ct_.x.at<double>(4, 0), -CT_OMEGA_MAX, CT_OMEGA_MAX);
                ct_.x.at<double>(4, 0) = omega;
                last_omega_ = omega;

                cv::Mat F = CTModel::F(dt, omega);
                cv::Mat Q = CTModel::Q(dt);

                cv::Mat xp = F * ct_.x;
                cv::Mat Pp = F * ct_.P * F.t() + Q;
                cv::Mat H = Hfor(5);
                cv::Mat S = H * Pp * H.t() + R;
                cv::Mat K = Pp * H.t() * S.inv();
                cv::Mat innov = z - H * xp;

                ct_.x = xp + K * innov;
                cv::Mat IKH = cv::Mat::eye(5, 5, CV_64F) - K * H;
                ct_.P = IKH * Pp * IKH.t() + K * R * K.t();

                last_omega_ = ct_.x.at<double>(4, 0);

                double detS = std::max(cv::determinant(S), 1e-300);
                cv::Mat tmp = innov.t() * S.inv() * innov;
                double mahal2 = tmp.at<double>(0, 0);
                liks.at<double>(j, 0) = std::exp(std::max(-0.5 * mahal2, -500.0)) / std::sqrt(std::pow(2 * kPi, 2) * detS);
            }
        }

        mu_ = c.mul(liks);
        double d = cv::sum(mu_)[0];
        if (d < 1e-300) {
            mu_.at<double>(0, 0) = IMM_MU0[0];
            mu_.at<double>(1, 0) = IMM_MU0[1];
        } else {
            mu_ /= d;
        }

        return fuse();
    }

    std::array<double, 4> hold() { return fuse(); }

    double mu_cv() const { return mu_.at<double>(0, 0); }
    double mu_ct() const { return mu_.at<double>(1, 0); }
    double omega_deg_s() const { return ct_.x.rows >= 5 ? ct_.x.at<double>(4, 0) * 180.0 / kPi : 0.0; }

  private:
    std::pair<cv::Mat, cv::Mat> to5(int idx) const {
        if (idx == 0) {
            cv::Mat x5 = cv::Mat::zeros(5, 1, CV_64F);
            cv_.x.copyTo(x5.rowRange(0, 4));
            x5.at<double>(4, 0) = last_omega_;

            cv::Mat P5 = cv::Mat::zeros(5, 5, CV_64F);
            cv_.P.copyTo(P5(cv::Rect(0, 0, 4, 4)));
            P5.at<double>(4, 4) = CT_OMEGA_INIT_VAR;
            return {x5, P5};
        }
        return {ct_.x.clone(), ct_.P.clone()};
    }

    std::array<double, 4> fuse() {
        cv::Mat x5 = cv::Mat::zeros(5, 1, CV_64F);
        auto [x05, p05] = to5(0);
        auto [x15, p15] = to5(1);
        x5 += mu_.at<double>(0, 0) * x05;
        x5 += mu_.at<double>(1, 0) * x15;

        return {x5.at<double>(0, 0), x5.at<double>(1, 0), x5.at<double>(2, 0), x5.at<double>(3, 0)};
    }

    CVModel cv_;
    CTModel ct_;
    cv::Mat mu_;
    cv::Mat Pi_;
    bool inited_ = false;
    double last_omega_ = 0.0;
};

constexpr double AUTO_MIN_SPEED_RAW = 3000.0;
constexpr double AUTO_MID_SPEED_RAW = 3600.0;
constexpr double AUTO_MAX_SPEED_RAW = 4200.0;
constexpr double AUTO_BRAKE_IF_NO_TRACK_MS = 600.0;
constexpr double AUTO_HARD_EHEAD_DEG = 140.0;
constexpr double AUTO_HARD_ELAT_PX = 220.0;

constexpr double AUTO_PP_K_ALPHA = 0.15;
constexpr double AUTO_PP_K_LAT = 0.014;
constexpr double AUTO_PP_ALPHA_DEADBAND = 4.5;
constexpr double AUTO_PP_ELAT_DEADBAND = 10.0;
constexpr double AUTO_PP_RATE_DEG = 1.10;
constexpr double AUTO_PP_FILTER = 0.42;
constexpr double AUTO_PP_NEAR_STRAIGHT_GAIN = 0.38;
constexpr double AUTO_PP_NEAR_ALPHA = 7.0;
constexpr double AUTO_PP_NEAR_ELAT = 9.0;
constexpr double AUTO_PP_DELAY_COMP_S = 0.20;
constexpr double AUTO_PP_D_ALPHA = 0.055;
constexpr double AUTO_PP_D_ELAT = 0.018;
constexpr double AUTO_PP_RATE_ALPHA = 0.30;
constexpr double AUTO_PP_RATE_ELAT = 0.22;
constexpr double AUTO_PP_D_MAX = 3.0;
constexpr double AUTO_PP_SOFTSAT_DEG = 12.5;

double speed_schedule(double abs_delta_deg, double abs_e_lat, double abs_alpha_deg) {
    if (abs_alpha_deg > 22.0 || abs_e_lat > 44.0 || abs_delta_deg > 7.0) {
        return AUTO_MIN_SPEED_RAW;
    }
    if (abs_alpha_deg > 9.0 || abs_e_lat > 18.0 || abs_delta_deg > 3.0) {
        return AUTO_MID_SPEED_RAW;
    }
    return AUTO_MAX_SPEED_RAW;
}

void update_speed_toward_target(double target_speed_raw) {
    std::scoped_lock lk(state_mutex);
    const double step = state.speed_step;
    const double cur = state.speed;
    if (cur < target_speed_raw) {
        state.speed = std::min(target_speed_raw, cur + step);
    } else if (cur > target_speed_raw) {
        state.speed = std::max(target_speed_raw, cur - step);
    }
}

double first_order_update(double prev, double target, double alpha) {
    return (1.0 - alpha) * prev + alpha * target;
}

void autonomy_step() {
    VisionData v;
    {
        std::scoped_lock lk(vision_mutex);
        v = vision;
    }

    const double now = now_sec();
    {
        std::scoped_lock lk(state_mutex);
        state.status = "AUTO";
    }

    bool have_track = std::isfinite(v.e_lat) && std::isfinite(v.e_head_deg);
    {
        std::scoped_lock lk(state_mutex);
        if (have_track) {
            state.last_track_t = now;
        } else if ((now - state.last_track_t) * 1000.0 > AUTO_BRAKE_IF_NO_TRACK_MS) {
            state.status = "NO_TRACK";
            state.braking = true;
            state.speed = 0.0;
            state.target_deg = 0.0;
            state.v_ref_raw = 0.0;
            state.delta_cmd = 0.0;
            state.pp_alpha_deg = 0.0;
            return;
        } else {
            state.status = "TRACK_HOLD";
            state.target_deg = 0.0;
            state.v_ref_raw = AUTO_MIN_SPEED_RAW;
            state.pp_alpha_deg = 0.0;
            state.braking = false;
        }
    }

    if (!have_track) {
        update_speed_toward_target(AUTO_MIN_SPEED_RAW);
        return;
    }

    const double speed_px = std::max(10.0, v.speed_px_s);
    const double e_lat_raw = v.e_lat;

    double pp_alpha_deg = v.pp_alpha_deg;

    {
        std::scoped_lock lk(state_mutex);
        state.pp_target_x = v.pp_target_x;
        state.pp_target_y = v.pp_target_y;
        state.pp_lookahead_px = v.pp_lookahead_px;

        if (!std::isfinite(pp_alpha_deg)) {
            pp_alpha_deg = clampv(v.e_head_deg, -45.0, 45.0);
            state.status = "PP_FALLBACK";
        }

        double e_lat = e_lat_raw;
        if (std::abs(pp_alpha_deg) < AUTO_PP_ALPHA_DEADBAND) {
            pp_alpha_deg = 0.0;
        }
        if (std::abs(e_lat) < AUTO_PP_ELAT_DEADBAND) {
            e_lat = 0.0;
        }

        state.e_lat_filt = first_order_update(state.e_lat_filt, e_lat, 0.28);
        state.pp_alpha_filt = first_order_update(state.pp_alpha_filt, pp_alpha_deg, 0.24);

        const double dt_ctrl = 1.0 / CONTROL_HZ;
        const double e_lat_rate = (state.e_lat_filt - state.last_e_lat) / std::max(1e-3, dt_ctrl);
        const double pp_alpha_rate = (state.pp_alpha_filt - state.last_e_head_deg) / std::max(1e-3, dt_ctrl);

        state.e_lat_rate_filt = first_order_update(state.e_lat_rate_filt, e_lat_rate, AUTO_PP_RATE_ELAT);
        state.pp_alpha_rate_filt = first_order_update(state.pp_alpha_rate_filt, pp_alpha_rate, AUTO_PP_RATE_ALPHA);

        double e_lat_pred = state.e_lat_filt + AUTO_PP_DELAY_COMP_S * state.e_lat_rate_filt;
        double pp_alpha_pred = state.pp_alpha_filt + AUTO_PP_DELAY_COMP_S * state.pp_alpha_rate_filt;
        e_lat_pred = clampv(e_lat_pred, -95.0, 95.0);
        pp_alpha_pred = clampv(pp_alpha_pred, -35.0, 35.0);

        const double alpha_term = AUTO_PP_K_ALPHA * pp_alpha_pred;
        const double lat_term = std::atan2(AUTO_PP_K_LAT * e_lat_pred, speed_px + 110.0) * 180.0 / kPi;

        const double d_term = clampv(AUTO_PP_D_ALPHA * state.pp_alpha_rate_filt + AUTO_PP_D_ELAT * state.e_lat_rate_filt,
                                     -AUTO_PP_D_MAX, AUTO_PP_D_MAX);

        double raw_delta = -(alpha_term + lat_term + d_term);
        raw_delta = AUTO_PP_SOFTSAT_DEG * std::tanh(raw_delta / AUTO_PP_SOFTSAT_DEG);
        raw_delta *= state.auto_steer_sign;

        if (std::abs(pp_alpha_pred) < AUTO_PP_NEAR_ALPHA && std::abs(e_lat_pred) < AUTO_PP_NEAR_ELAT) {
            raw_delta *= AUTO_PP_NEAR_STRAIGHT_GAIN;
            state.status = "STRAIGHT";
        } else {
            state.status = "PP_TRACK";
        }

        const double prev_cmd = state.delta_cmd;
        const double filt_cmd = (1.0 - AUTO_PP_FILTER) * prev_cmd + AUTO_PP_FILTER * raw_delta;
        double delta = prev_cmd + clampv(filt_cmd - prev_cmd, -AUTO_PP_RATE_DEG, AUTO_PP_RATE_DEG);
        delta = clampv(delta, -state.max_deg, state.max_deg);

        const double target_speed = speed_schedule(std::abs(delta), std::abs(e_lat_pred), std::abs(pp_alpha_pred));

        state.last_e_lat = state.e_lat_filt;
        state.last_e_head_deg = state.pp_alpha_filt;
        state.pp_alpha_deg = pp_alpha_pred;
        state.delta_cmd = delta;
        state.target_deg = delta;
        state.v_ref_raw = target_speed;
        state.last_delta_for_speed = delta;
        state.braking = false;
    }

    double target;
    {
        std::scoped_lock lk(state_mutex);
        target = state.v_ref_raw;
    }
    update_speed_toward_target(target);
}

void control_loop() {
    const double period = 1.0 / CONTROL_HZ;
    while (!cv_stop.load()) {
        const auto t0 = Clock::now();

        bool connected = ble_client && ble_client->is_connected();
        if (connected) {
            bool auto_enabled = false;
            {
                std::scoped_lock lk(state_mutex);
                auto_enabled = state.auto_enabled;
            }

            if (auto_enabled) {
                autonomy_step();
            } else {
                std::scoped_lock lk(state_mutex);
                state.status = "AUTO_OFF";
                state.braking = true;
                state.speed = 0.0;
                state.target_deg = 0.0;
                state.v_ref_raw = 0.0;
                state.delta_cmd = 0.0;
            }

            bool braking = false;
            {
                std::scoped_lock lk(state_mutex);
                braking = state.braking;
            }

            if (braking) {
                ble_client->write_gatt_char(WRITE_UUID, BRAKE_PACKET);
                const auto abc = compute_abc();
                log_row(abc);
            } else {
                {
                    std::scoped_lock lk(state_mutex);
                    const double diff = state.target_deg - state.turn_deg;
                    const double step = state.steer_smooth * period;
                    if (std::abs(diff) <= step) {
                        state.turn_deg = state.target_deg;
                    } else {
                        state.turn_deg += std::copysign(step, diff);
                    }
                }

                const auto abc = compute_abc();
                ble_client->write_gatt_char(WRITE_UUID, build_packet(abc.a_sent, abc.b, abc.c));
                log_row(abc);
            }
        }

        const auto elapsed = std::chrono::duration<double>(Clock::now() - t0).count();
        const auto sleep_s = std::max(0.0, period - elapsed);
        std::this_thread::sleep_for(std::chrono::duration<double>(sleep_s));
    }
}

void cv_thread() {
    const char* display_env = std::getenv("DISPLAY");
    const char* wayland_env = std::getenv("WAYLAND_DISPLAY");
    const bool gui_enabled = (display_env && *display_env) || (wayland_env && *wayland_env);
    if (!gui_enabled) {
        std::cout << "[CV] Headless mode: DISPLAY/WAYLAND not set, GUI disabled.\n";
    }

    CamThread cam(CAM_INDEX, FRAME_W, FRAME_H);
    cam.start();

    auto backSub = build_mog2();
    IMMTracker imm;

    std::optional<std::vector<cv::Point2d>> path_cl;
    {
        std::scoped_lock lk(state_mutex);
        path_cl = load_centerline(CENTERLINE_CSV, state.flip_centerline);
        if (path_cl.has_value()) {
            std::cout << "[CV] Loaded centerline: " << path_cl->size() << " pts\n";
            if (state.flip_centerline) {
                std::cout << "[CV] Centerline direction flipped\n";
            }
        }
    }

    int last_seg_i = 0;
    bool show_mask = false;
    bool show_debug = false;
    std::optional<cv::Point2d> last_est;
    int roi_miss = 0;
    int tick = 0;
    int no_meas = 0;
    bool was_holding = false;
    std::vector<cv::Point2d> blend_buf;
    std::optional<double> prev_theta;
    std::optional<double> last_good_theta;
    double prev_t = now_sec();

    std::cout << "[CV] Running -- M=centerline  R=log  A=auto  V=mask  L=lighting  D=debug  Q=quit\n";

    try {
        while (!cv_stop.load()) {
            auto frame_opt = cam.read_latest(500);
            if (!frame_opt.has_value()) {
                continue;
            }
            cv::Mat frame = *frame_opt;

            {
                std::scoped_lock lk(centerline_mutex);
                if (new_centerline.has_value()) {
                    path_cl = *new_centerline;
                    last_seg_i = 0;
                    new_centerline.reset();
                }
            }

            tick += 1;
            const double now = now_sec();
            const double dt = clampv(now - prev_t, 1e-3, 0.2);
            prev_t = now;
            const double dt_ms = dt * 1000.0;
            const double t_ms = now_sec() * 1000.0;

            cv::Mat work = preprocess_lighting(frame);
            cv::Mat fg_raw;
            backSub->apply(work, fg_raw);
            cv::Mat fg = clean_mask(fg_raw);
            const int H = fg.rows;
            const int W = fg.cols;

            bool use_global = (!imm.inited()) || (roi_miss >= ROI_FAIL_TO_GLOBAL) || (tick % GLOBAL_SEARCH_EVERY == 0);
            std::optional<cv::Rect> roi;
            std::string mode = use_global ? "GLOBAL" : "ROI";
            if (!use_global && last_est.has_value()) {
                roi = make_roi(last_est->x, last_est->y, W, H);
                if (!roi.has_value()) {
                    mode = "GLOBAL";
                }
            }

            auto det = pick_contour(fg, roi);
            bool used_meas = false;
            int meas_cx = -1, meas_cy = -1;
            double kf_x = 0.0, kf_y = 0.0, kf_vx = 0.0, kf_vy = 0.0;

            if (det.has_value() && det->motion_px >= MIN_MOTION_PIXELS) {
                meas_cx = det->cx;
                meas_cy = det->cy;
                if (was_holding && RESET_VEL_AFTER_HOLD) {
                    imm.init_at(meas_cx, meas_cy);
                }
                auto x = imm.correct_always(meas_cx, meas_cy, dt);
                kf_x = x[0];
                kf_y = x[1];
                kf_vx = x[2];
                kf_vy = x[3];
                used_meas = true;
                no_meas = 0;
                roi_miss = 0;
                was_holding = false;
            } else {
                roi_miss += 1;
                no_meas += 1;
                if (imm.inited()) {
                    auto x = imm.hold();
                    kf_x = x[0];
                    kf_y = x[1];
                    kf_vx = x[2];
                    kf_vy = x[3];
                    if (no_meas >= HOLD_AFTER_NO_MEAS) {
                        kf_vx = 0.0;
                        kf_vy = 0.0;
                    }
                    was_holding = true;
                }
            }

            if (imm.inited()) {
                last_est = cv::Point2d(kf_x, kf_y);
            }

            double blend_x = kf_x;
            double blend_y = kf_y;
            std::string blend_src = "KF_ONLY";
            double a_used = 0.0;
            const double speed_px_s = std::hypot(kf_vx, kf_vy);
            const double meas_alpha = adaptive_meas_alpha(speed_px_s);

            if (used_meas && imm.inited()) {
                const double dx = meas_cx - kf_x;
                const double dy = meas_cy - kf_y;
                const double d = std::hypot(dx, dy);
                if (d <= std::max(1.0, MEAS_GATE_PX)) {
                    const double trust = std::max(0.0, 1.0 - d / std::max(1.0, MEAS_GATE_PX));
                    const double a = clampv(meas_alpha * trust + (1.0 - trust) * (0.4 * meas_alpha), 0.0, 1.0);
                    blend_x = a * meas_cx + (1.0 - a) * kf_x;
                    blend_y = a * meas_cy + (1.0 - a) * kf_y;
                    blend_src = "BLEND";
                    a_used = a;
                } else {
                    blend_src = "KF_GATED";
                }
            }

            double theta_rad = std::numeric_limits<double>::quiet_NaN();
            std::string theta_src = "NONE";
            double step_px = 0.0;
            std::optional<double> inst_theta;

            if (speed_px_s >= MIN_SPEED_FOR_VEL_HEADING) {
                inst_theta = std::atan2(kf_vy, kf_vx);
                theta_src = "KF_VEL";
            } else {
                blend_buf.emplace_back(blend_x, blend_y);
                const int max_len = std::max(2, MOTION_WINDOW + 1);
                if (static_cast<int>(blend_buf.size()) > max_len) {
                    blend_buf.erase(blend_buf.begin(), blend_buf.end() - max_len);
                }
                if (blend_buf.size() >= 2) {
                    size_t k0 = blend_buf.size() - 1 - MOTION_WINDOW;
                    auto p0 = blend_buf[k0];
                    auto p1 = blend_buf.back();
                    step_px = std::hypot(p1.x - p0.x, p1.y - p0.y);
                    if (step_px >= std::max(0.1, MIN_STEP_DIST)) {
                        inst_theta = std::atan2(p1.y - p0.y, p1.x - p0.x);
                        theta_src = "MOTION";
                    }
                }
            }

            if (inst_theta.has_value()) {
                theta_rad = ema_angle(prev_theta, *inst_theta, adaptive_alpha(speed_px_s));
                prev_theta = theta_rad;
                last_good_theta = theta_rad;
            } else if (last_good_theta.has_value()) {
                theta_rad = *last_good_theta;
                theta_src = "HOLD";
            }

            const double theta_deg = std::isfinite(theta_rad) ? theta_rad * 180.0 / kPi : std::numeric_limits<double>::quiet_NaN();
            const double dt_p = speed_px_s >= MIN_SPEED_FOR_PREDICT ? clampv(dt, PREDICT_DT_MIN, PREDICT_DT_MAX) : 0.0;
            const double ctrl_x = blend_x + kf_vx * dt_p;
            const double ctrl_y = blend_y + kf_vy * dt_p;

            double e_lateral = std::numeric_limits<double>::quiet_NaN();
            double e_heading_deg = std::numeric_limits<double>::quiet_NaN();
            std::optional<double> path_heading;
            std::optional<cv::Point2d> proj_pt;
            std::optional<cv::Point2d> pp_target;
            double pp_alpha_deg = std::numeric_limits<double>::quiet_NaN();
            double pp_lookahead_px = 0.0;
            std::optional<double> pp_path_heading;

            if (path_cl.has_value() && imm.inited()) {
                try {
                    auto [nr, best_i] = nearest_on_polyline(cv::Point2d(ctrl_x, ctrl_y), *path_cl, last_seg_i, NEAREST_WIN);
                    last_seg_i = best_i;
                    path_heading = nr.heading;
                    e_lateral = nr.e_lat;
                    proj_pt = nr.proj;

                    if (std::isfinite(theta_rad)) {
                        e_heading_deg = wrap_pi(theta_rad - *path_heading) * 180.0 / kPi;
                    }

                    pp_lookahead_px = dynamic_lookahead_px(speed_px_s);
                    auto adv = advance_along_path(*path_cl, nr.seg_i, nr.proj, pp_lookahead_px);
                    pp_target = adv.first;
                    pp_path_heading = adv.second.second;

                    if (pp_target.has_value()) {
                        const double desired_heading = std::atan2(pp_target->y - ctrl_y, pp_target->x - ctrl_x);
                        const double ref_heading = std::isfinite(theta_rad) ? theta_rad : *path_heading;
                        if (std::isfinite(ref_heading)) {
                            pp_alpha_deg = wrap_pi(desired_heading - ref_heading) * 180.0 / kPi;
                        }
                    }
                } catch (...) {
                }
            }

            {
                std::scoped_lock lk(vision_mutex);
                vision.t_ms = t_ms;
                vision.dt_ms = dt_ms;
                vision.mode = mode;
                vision.used_meas = used_meas;
                vision.holding = was_holding;
                vision.meas_cx = meas_cx;
                vision.meas_cy = meas_cy;
                vision.kf_x = kf_x;
                vision.kf_y = kf_y;
                vision.kf_vx = kf_vx;
                vision.kf_vy = kf_vy;
                vision.blend_x = blend_x;
                vision.blend_y = blend_y;
                vision.ctrl_x = ctrl_x;
                vision.ctrl_y = ctrl_y;
                vision.blend_src = blend_src;
                vision.blend_a = a_used;
                vision.speed_px_s = speed_px_s;
                vision.theta_deg = theta_deg;
                vision.theta_src = theta_src;
                vision.step_px = step_px;
                vision.mu_cv = imm.mu_cv();
                vision.mu_ct = imm.mu_ct();
                vision.omega_deg_s = imm.omega_deg_s();
                vision.e_lat = e_lateral;
                vision.e_head_deg = e_heading_deg;
                vision.pp_target_x = pp_target.has_value() ? pp_target->x : std::numeric_limits<double>::quiet_NaN();
                vision.pp_target_y = pp_target.has_value() ? pp_target->y : std::numeric_limits<double>::quiet_NaN();
                vision.pp_alpha_deg = pp_alpha_deg;
                vision.pp_lookahead_px = pp_lookahead_px;
            }

            cv::Mat out = frame.clone();
            draw_centerline(out, path_cl);

            if (!path_cl.has_value()) {
                cv::putText(out, "No centerline -- press M", cv::Point(10, H - 20), cv::FONT_HERSHEY_SIMPLEX, 0.7,
                            cv::Scalar(0, 80, 255), 2);
            }
            if (roi.has_value()) {
                cv::rectangle(out, *roi, cv::Scalar(255, 255, 0), 2);
            }
            if (det.has_value()) {
                cv::rectangle(out, det->bbox, cv::Scalar(0, 0, 255), 2);
                cv::circle(out, cv::Point(det->cx, det->cy), 5, cv::Scalar(0, 0, 255), -1);
            }
            if (imm.inited()) {
                cv::circle(out, cv::Point(std::lround(kf_x), std::lround(kf_y)), 7, cv::Scalar(0, 255, 0), 2);
                cv::circle(out, cv::Point(std::lround(blend_x), std::lround(blend_y)), 5, cv::Scalar(255, 255, 0), -1);
                if (std::isfinite(theta_rad)) {
                    draw_arrow(out, blend_x, blend_y, theta_rad, HEADING_LEN, cv::Scalar(255, 0, 0), 4);
                }
            }
            if (proj_pt.has_value() && path_heading.has_value()) {
                cv::circle(out, cv::Point(std::lround(proj_pt->x), std::lround(proj_pt->y)), 6, cv::Scalar(0, 255, 255), 2);
                draw_arrow(out, proj_pt->x, proj_pt->y, *path_heading, 60, cv::Scalar(0, 255, 255), 3);
            }
            if (pp_target.has_value()) {
                cv::circle(out, cv::Point(std::lround(pp_target->x), std::lround(pp_target->y)), 6, cv::Scalar(255, 0, 255), -1);
                cv::line(out, cv::Point(std::lround(ctrl_x), std::lround(ctrl_y)),
                         cv::Point(std::lround(pp_target->x), std::lround(pp_target->y)), cv::Scalar(255, 0, 255), 2,
                         cv::LINE_AA);
                if (pp_path_heading.has_value() && std::isfinite(*pp_path_heading)) {
                    draw_arrow(out, pp_target->x, pp_target->y, *pp_path_heading, 45, cv::Scalar(255, 0, 255), 2);
                }
            }

            State s;
            {
                std::scoped_lock lk(state_mutex);
                s = state;
            }

            std::string log_lbl = s.logging ? "LOG:ON" : "LOG:OFF";
            std::string auto_lbl = s.auto_enabled ? "AUTO:ON" : "AUTO:OFF";
            std::ostringstream line1;
            line1 << auto_lbl << " | " << s.status << " | dt=" << std::fixed << std::setprecision(1) << dt_ms
                  << "ms | " << log_lbl << " | LIT:" << LightingMode::current() << " | flip:" << (s.flip_centerline ? 1 : 0);
            cv::putText(out, line1.str(), cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX, 0.58, cv::Scalar(255, 255, 255), 2);

            if (std::isfinite(e_lateral)) {
                std::ostringstream line2;
                line2 << "e_lat=" << std::fixed << std::setprecision(1) << e_lateral << "px  e_head=" << e_heading_deg
                      << "deg";
                cv::putText(out, line2.str(), cv::Point(10, 60), cv::FONT_HERSHEY_SIMPLEX, 0.63, cv::Scalar(0, 255, 255),
                            2);
            }
            if (std::isfinite(theta_deg)) {
                std::ostringstream line3;
                line3 << "theta=" << std::fixed << std::setprecision(1) << theta_deg << "  speed=" << std::setprecision(0)
                      << speed_px_s << "px/s  pp_alpha=" << std::setprecision(1) << pp_alpha_deg
                      << "  LA=" << std::setprecision(0) << pp_lookahead_px << "  cmd_speed=" << s.v_ref_raw
                      << "  cmd_delta=" << std::setprecision(1) << s.delta_cmd;
                cv::putText(out, line3.str(), cv::Point(10, 90), cv::FONT_HERSHEY_SIMPLEX, 0.57, cv::Scalar(255, 255, 0),
                            2);
            }

            int key = -1;
            if (gui_enabled) {
                cv::imshow("Autonomous Controller [M=centerline | R=log | A=auto | V=mask | L=lighting | D=debug | Q=quit]", out);
                if (show_mask) {
                    cv::imshow("fg_mask", fg);
                }
                key = cv::waitKey(1) & 0xff;
            }
            if (key == 'q' || key == 'Q') {
                cv_stop.store(true);
                break;
            }
            if (key == 'v' || key == 'V') {
                show_mask = !show_mask;
                if (!show_mask) {
                    cv::destroyWindow("fg_mask");
                }
            }
            if (key == 'm' || key == 'M') {
                auto np = build_centerline_from_frame(frame);
                if (np.has_value()) {
                    std::scoped_lock lk(centerline_mutex);
                    new_centerline = *np;
                }
            }
            if (key == 'r' || key == 'R') {
                bool is_logging;
                {
                    std::scoped_lock lk(state_mutex);
                    is_logging = state.logging;
                }
                if (is_logging) {
                    stop_logging();
                    std::cout << "[LOG] Stopped\n";
                } else {
                    start_logging();
                    std::scoped_lock lk(state_mutex);
                    std::cout << "[LOG] Started -> " << state.log_path << "\n";
                }
            }
            if (key == 'a' || key == 'A') {
                std::scoped_lock lk(state_mutex);
                state.auto_enabled = !state.auto_enabled;
                state.braking = !state.auto_enabled;
                if (!state.auto_enabled) {
                    state.speed = 0.0;
                    state.target_deg = 0.0;
                    state.v_ref_raw = 0.0;
                    state.delta_cmd = 0.0;
                }
                std::cout << "[AUTO] " << (state.auto_enabled ? "ON" : "OFF") << "\n";
            }
            if (key == 'l' || key == 'L') {
                LightingMode::next();
            }
            if (key == 'd' || key == 'D') {
                show_debug = !show_debug;
                std::cout << "[CV] Debug overlay: " << (show_debug ? "ON" : "OFF") << "\n";
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[CV ERROR] " << e.what() << "\n";
    }

    cam.stop();
    if (gui_enabled) {
        cv::destroyAllWindows();
    }
    std::cout << "[CV] Stopped\n";
    cv_stop.store(true);
}

struct Args {
    std::optional<std::string> address;
    std::optional<std::string> name;
    bool discover = false;
    double scan_timeout = 6.0;
    bool no_ble = false;
    double auto_steer_sign = 1.0;
    bool flip_centerline = false;
    double max_deg = 9.0;
    double steer_smooth = 42.0;
    double ratio = 0.80;
    double c_sign = -1.0;
    double speed_k = 0.03;
};

void print_usage(const char* argv0) {
    std::cout << "Usage:\n"
              << "  " << argv0
              << " --address <id> [--name <ble-name>] [--discover] [--scan-timeout s] [--auto-steer-sign v] [--flip-centerline] [--max-deg v] [--steer-smooth v] [--ratio v] [--c-sign v] [--speed-k v]\n"
              << "  " << argv0 << " --name <ble-name> [same tuning flags]\n"
              << "  " << argv0 << " --discover [--scan-timeout s]\n"
              << "  " << argv0 << " --no-ble [same tuning flags]\n";
}

std::optional<Args> parse_args(int argc, char** argv) {
    Args args;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto need = [&](const std::string& flag) -> std::optional<std::string> {
            if (i + 1 >= argc) {
                std::cerr << "Missing value for " << flag << "\n";
                return std::nullopt;
            }
            return std::string(argv[++i]);
        };

        if (a == "--address") {
            auto v = need(a);
            if (!v.has_value()) {
                return std::nullopt;
            }
            args.address = *v;
        } else if (a == "--name") {
            auto v = need(a);
            if (!v.has_value()) {
                return std::nullopt;
            }
            args.name = *v;
        } else if (a == "--discover") {
            args.discover = true;
        } else if (a == "--scan-timeout") {
            auto v = need(a);
            if (!v.has_value()) {
                return std::nullopt;
            }
            args.scan_timeout = std::stod(*v);
        } else if (a == "--no-ble") {
            args.no_ble = true;
        } else if (a == "--auto-steer-sign") {
            auto v = need(a);
            if (!v.has_value()) {
                return std::nullopt;
            }
            args.auto_steer_sign = std::stod(*v);
        } else if (a == "--flip-centerline") {
            args.flip_centerline = true;
        } else if (a == "--max-deg") {
            auto v = need(a);
            if (!v.has_value()) {
                return std::nullopt;
            }
            args.max_deg = std::stod(*v);
        } else if (a == "--steer-smooth") {
            auto v = need(a);
            if (!v.has_value()) {
                return std::nullopt;
            }
            args.steer_smooth = std::stod(*v);
        } else if (a == "--ratio") {
            auto v = need(a);
            if (!v.has_value()) {
                return std::nullopt;
            }
            args.ratio = std::stod(*v);
        } else if (a == "--c-sign") {
            auto v = need(a);
            if (!v.has_value()) {
                return std::nullopt;
            }
            args.c_sign = std::stod(*v);
        } else if (a == "--speed-k") {
            auto v = need(a);
            if (!v.has_value()) {
                return std::nullopt;
            }
            args.speed_k = std::stod(*v);
        } else if (a == "-h" || a == "--help") {
            print_usage(argv[0]);
            return std::nullopt;
        } else {
            std::cerr << "Unknown arg: " << a << "\n";
            print_usage(argv[0]);
            return std::nullopt;
        }
    }

    if (!args.no_ble && !args.discover && !args.address.has_value() && !args.name.has_value()) {
        std::cerr << "Provide --address, --name, or --discover to use BLE, or use --no-ble.\n";
        return std::nullopt;
    }

    return args;
}

#ifdef __linux__
// ── Native Linux BLE client via BlueZ D-Bus / bluetoothctl ──────────────────
// Uses bluetoothctl (BlueZ 5.50+) for scan, connect, and GATT writes.
// Requires: sudo apt install bluez
// No gatttool needed (deprecated in modern BlueZ).
//
// Strategy:
//  - Scan: pipe commands into bluetoothctl via popen in write mode
//  - Connect: bluetoothctl connect <MAC>
//  - GATT write: bluetoothctl select-attribute + write  (interactive pipe)
//
// For GATT writes we keep a persistent bluetoothctl process and pipe
// commands to it via popen("bluetoothctl", "w").

// Run a shell command, return captured stdout.
static std::string run_capture(const std::string& cmd) {
    FILE* fp = popen(cmd.c_str(), "r");
    if (!fp) return {};
    std::string out;
    char buf[256];
    while (fgets(buf, sizeof(buf), fp)) out += buf;
    pclose(fp);
    return out;
}

// Scan for BLE devices and resolve a device name to its MAC address.
// Returns MAC string like "AA:BB:CC:DD:EE:FF" or empty string on failure.
static std::string resolve_name_to_mac(const std::string& name, int timeout_sec) {
    std::cout << "[BLE] Scanning for '" << name << "' (" << timeout_sec << "s)...\n";
    std::flush(std::cout);

    // Use bluetoothctl in scan mode: power on, scan on, wait, devices, scan off
    std::string cmd =
        "bluetoothctl power on >/dev/null 2>&1 ; "
        "bluetoothctl scan on >/dev/null 2>&1 & "
        "sleep " + std::to_string(timeout_sec) + " ; "
        "bluetoothctl scan off >/dev/null 2>&1 ; "
        "bluetoothctl devices 2>/dev/null";
    std::string output = run_capture(cmd);

    // Strip ANSI escape codes (bluetoothctl sometimes emits colour codes)
    std::string clean;
    clean.reserve(output.size());
    for (size_t i = 0; i < output.size(); ) {
        if (output[i] == '\x1b' && i + 1 < output.size() && output[i+1] == '[') {
            i += 2;
            while (i < output.size() && output[i] != 'm') ++i;
            if (i < output.size()) ++i;
        } else {
            clean += output[i++];
        }
    }

    std::istringstream ss(clean);
    std::string line;
    while (std::getline(ss, line)) {
        if (line.find(name) != std::string::npos) {
            // Line format: "Device AA:BB:CC:DD:EE:FF Name"
            std::istringstream ls(line);
            std::string tok;
            ls >> tok; // "Device" or "[NEW]" "Device"
            if (tok == "[NEW]" || tok == "[CHG]") ls >> tok; // skip prefix
            if (tok == "Device") {
                ls >> tok; // MAC
                if (tok.size() == 17 && tok[2] == ':') {
                    std::cout << "[BLE] Resolved '" << name << "' -> " << tok << "\n";
                    return tok;
                }
            }
        }
    }
    std::cerr << "[BLE] Device '" << name << "' not found during scan.\n";
    std::cerr << "[BLE] Raw output:\n" << clean << "\n";
    return {};
}

// Discover all nearby BLE devices and print them.
static void ble_discover(int timeout_sec) {
    std::cout << "[BLE] Scanning " << timeout_sec << "s for BLE devices...\n";
    std::flush(std::cout);
    std::string cmd =
        "bluetoothctl power on >/dev/null 2>&1 ; "
        "bluetoothctl scan on >/dev/null 2>&1 & "
        "sleep " + std::to_string(timeout_sec) + " ; "
        "bluetoothctl scan off >/dev/null 2>&1 ; "
        "bluetoothctl devices 2>/dev/null";
    std::string out = run_capture(cmd);
    std::cout << "[BLE] Devices found:\n" << out << "\n";
}

// ── LinuxBleClient ────────────────────────────────────────────────────────────
// Connects to a BLE peripheral by MAC address using bluetoothctl,
// then writes GATT characteristics using bluetoothctl select-attribute + write.

class LinuxBleClient final : public BleClient {
  public:
    explicit LinuxBleClient(std::string mac, std::string write_uuid)
        : mac_(std::move(mac)), write_uuid_(std::move(write_uuid)) {}

    ~LinuxBleClient() override {
        if (pipe_) {
            fputs("disconnect\n", pipe_);
            fputs("exit\n", pipe_);
            fflush(pipe_);
            pclose(pipe_);
            pipe_ = nullptr;
        }
    }

    bool connect() {
        if (system("command -v bluetoothctl >/dev/null 2>&1") != 0) {
            std::cerr << "[BLE] bluetoothctl is not installed. Install with: sudo apt install bluez\n";
            return false;
        }

        // 1. Power on adapter
        system("bluetoothctl power on >/dev/null 2>&1");

        // 2. Pair/trust if not already (best-effort)
        std::string trust_cmd = "bluetoothctl trust " + mac_ + " >/dev/null 2>&1";
        system(trust_cmd.c_str());

        // 3. Connect
        std::cout << "[BLE] Connecting to " << mac_ << " ...\n";
        std::flush(std::cout);
        std::string cmd = "bluetoothctl connect " + mac_ + " 2>&1";
        std::string out = run_capture(cmd);
        std::cout << "[BLE] " << out;

        bool ok = out.find("Connection successful") != std::string::npos ||
                  out.find("Connected: yes") != std::string::npos ||
                  out.find("already connected") != std::string::npos;
        if (!ok) {
            // Try a second time — BLE devices sometimes need a retry
            std::this_thread::sleep_for(std::chrono::milliseconds(800));
            out = run_capture(cmd);
            std::cout << "[BLE] Retry: " << out;
            ok = out.find("Connection successful") != std::string::npos ||
                 out.find("Connected: yes") != std::string::npos ||
                 out.find("already connected") != std::string::npos;
        }

        if (!ok) {
            std::cerr << "[BLE] Connection failed. Check that the car is on and in range.\n";
            return false;
        }

        std::cout << "[BLE] Waiting for GATT services to resolve...\n";
        std::flush(std::cout);
        bool services_ready = false;
        for (int i = 0; i < 30; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            const std::string info = run_capture("bluetoothctl info " + mac_ + " 2>/dev/null");
            if (info.find("ServicesResolved: yes") != std::string::npos) {
                services_ready = true;
                break;
            }
        }
        if (!services_ready) {
            std::cerr << "[BLE] Timed out waiting for ServicesResolved. Trying anyway...\n";
        } else {
            std::cout << "[BLE] Services resolved.\n";
        }

        // 4. Open a persistent bluetoothctl pipe for GATT writes
        pipe_ = popen("bluetoothctl", "w");
        if (!pipe_) {
            std::cerr << "[BLE] Could not open bluetoothctl pipe for writes.\n";
            return false;
        }

        // 5. Enter GATT menu in persistent bluetoothctl session.
        // Do NOT reconnect here; link is already up from step 3.
        fputs("menu gatt\n", pipe_);
        std::this_thread::sleep_for(std::chrono::milliseconds(300));

        // 6. Select the write characteristic by UUID.
        std::string sel = "select-attribute " + write_uuid_ + "\n";
        fputs(sel.c_str(), pipe_);
        fflush(pipe_);
        std::this_thread::sleep_for(std::chrono::milliseconds(300));

        connected_ = true;
        std::cout << "[BLE] Connected! GATT write pipe ready.\n";
        return true;
    }

    bool is_connected() const override { return connected_; }

    bool write_gatt_char(const std::string& /*uuid*/, const std::vector<uint8_t>& data) override {
        if (!connected_ || !pipe_) return false;

        // Build for bluetoothctl gatt menu:
        //   write 0xbf 0x0a 0x00 ...
        // 0x-prefixed tokens avoid parser errors like "Invalid value at index 0".
        std::ostringstream cmd;
        cmd << "write";
        for (auto b : data) {
            cmd << " 0x";
            cmd << std::hex << std::setw(2) << std::setfill('0')
                << static_cast<int>(b);
        }
        cmd << "\n";

        std::string s = cmd.str();
        if (fputs(s.c_str(), pipe_) == EOF) {
            connected_ = false;
            return false;
        }
        fflush(pipe_);
        return true;
    }

  private:
    std::string mac_;
    std::string write_uuid_;
    FILE*       pipe_     = nullptr;
    bool        connected_= false;
};

#endif  // __linux__

}  // namespace

int main(int argc, char** argv) {
    auto args_opt = parse_args(argc, argv);
    if (!args_opt.has_value()) {
        return 1;
    }
    const Args args = *args_opt;

    {
        std::scoped_lock lk(state_mutex);
        state.auto_steer_sign = args.auto_steer_sign;
        state.flip_centerline = args.flip_centerline;
        state.max_deg = args.max_deg;
        state.steer_smooth = args.steer_smooth;
        state.ratio = args.ratio;
        state.c_sign = args.c_sign;
        state.speed_k = args.speed_k;
        state.speed_step = 90.0;
        state.last_track_t = now_sec();
    }

    if (args.no_ble) {
        std::cout << "[BLE] Disabled -- running in no-Bluetooth mode\n";
        ble_client = std::make_unique<DummyBleClient>();
#ifdef __linux__
    } else if (args.discover) {
        ble_discover(static_cast<int>(args.scan_timeout));
        return 0;
    } else {
        // Resolve MAC: prefer --address, then --name via scan
        std::string mac;
        if (args.address.has_value()) {
            mac = args.address.value();
        } else if (args.name.has_value()) {
            mac = resolve_name_to_mac(args.name.value(), static_cast<int>(args.scan_timeout));
            if (mac.empty()) {
                return 2;
            }
        } else {
            std::cerr << "[BLE] Provide --address, --name, or --discover, or use --no-ble.\n";
            return 1;
        }
        auto client = std::make_unique<LinuxBleClient>(mac, std::string(WRITE_UUID));
        if (!client->connect()) {
            return 2;
        }
        ble_client = std::move(client);
#else
    } else {
        std::cerr << "[BLE] Native BLE is only supported on Linux/Raspberry Pi 4.\n";
        std::cerr << "[BLE] This binary must be built and run on the Raspberry Pi 4.\n";
        return 2;
#endif
    }

    std::thread ctl(control_loop);

    cv_thread();
    cv_stop.store(true);
    if (ctl.joinable()) {
        ctl.join();
    }

    stop_logging();
    if (ble_client && ble_client->is_connected()) {
        ble_client->write_gatt_char(WRITE_UUID, BRAKE_PACKET);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        ble_client->write_gatt_char(WRITE_UUID, IDLE_PACKET);
    }

    std::cout << "\n[STOP]\n";
    return 0;
}
