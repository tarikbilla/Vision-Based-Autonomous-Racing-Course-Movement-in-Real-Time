// controller.cpp  (ONE PROGRAM: MAP BUILD (M) + RUN (R))
// C++17 + OpenCV + SimpleBLE
//
// Keys:
//   M = capture frame + build binary + clean + centerline + save CSV
//   R = toggle autopilot run/stop
//   B = reset background model (re-warmup)
//   Q/ESC = quit (sends STOP)
//
// Notes:
// - Works in RAW angled camera view (no bird-eye needed).
// - CSV points are in SAME pixel coordinates as the live frames.

#include <opencv2/opencv.hpp>
#include <SimpleBLE/SimpleBLE.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <exception>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

using namespace std;

// -------------------------
// Constants
// -------------------------
static const string DEFAULT_CHAR_UUID = "6e400002-b5a3-f393-e0a9-e50e24dcca9e";
static constexpr double PI = 3.14159265358979323846;

// -------------------------
// Helpers
// -------------------------
static inline double now_s() {
    using clock = chrono::steady_clock;
    static const auto t0 = clock::now();
    return chrono::duration<double>(clock::now() - t0).count();
}

static inline string lower_copy(string s) {
    for (char& c : s) c = char(std::tolower((unsigned char)c));
    return s;
}

static inline string trim_copy(const string& s) {
    size_t a = 0, b = s.size();
    while (a < b && isspace((unsigned char)s[a])) a++;
    while (b > a && isspace((unsigned char)s[b - 1])) b--;
    return s.substr(a, b - a);
}

// uuid_to_string works whether uuid() returns std::string OR a UUID object with .to_string()
static inline std::string uuid_to_string(const std::string& s) { return s; }
template <typename T>
static inline auto uuid_to_string(const T& u) -> decltype(u.to_string()) { return u.to_string(); }

// -------------------------
// DR!FT protocol helpers
// -------------------------
static string int_to_hex(int value, int digits = 4) {
    value = int(value);
    if (value < 0) value = 0;
    int maxv = 1;
    for (int i = 0; i < digits; ++i) maxv *= 16;
    maxv -= 1;
    if (value > maxv) value = maxv;

    stringstream ss;
    ss << hex << nouppercase << setw(digits) << setfill('0') << (unsigned int)value;
    return ss.str();
}

static string generate_command_direct(bool light_on, int speed, int steering_value) {
    const string device_identifier = "bf0a00082800";
    const int drift_value = 0;
    const string light_value = light_on ? "0200" : "0000";
    const string checksum = "00";

    speed = std::clamp(speed, 0, 255);
    steering_value = std::clamp(steering_value, 0, 255);

    return device_identifier
        + int_to_hex(speed, 4)
        + int_to_hex(drift_value, 4)
        + int_to_hex(steering_value, 4)
        + light_value
        + checksum;
}

static vector<uint8_t> bytes_from_hex(const string& hexstr) {
    vector<uint8_t> out;
    out.reserve(hexstr.size() / 2);

    auto hexval = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
        if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
        return -1;
    };

    if (hexstr.size() % 2 != 0) return {};
    for (size_t i = 0; i < hexstr.size(); i += 2) {
        int hi = hexval(hexstr[i]);
        int lo = hexval(hexstr[i + 1]);
        if (hi < 0 || lo < 0) return {};
        out.push_back((uint8_t)((hi << 4) | lo));
    }
    return out;
}

static void send_command(SimpleBLE::Peripheral& peripheral,
                         const string& service_uuid,
                         const string& char_uuid,
                         const string& command_hex) {
    auto b = bytes_from_hex(command_hex);
    if (b.empty()) return;

    try {
        peripheral.write_request(service_uuid, char_uuid, b);
    } catch (...) {
        try {
            peripheral.write_command(service_uuid, char_uuid, b);
        } catch (...) {
            // ignore
        }
    }
}

// -------------------------
// Data types
// -------------------------
struct Pt2 {
    double x = 0;
    double y = 0;
};

static double wrap_pi(double a) {
    while (a > PI) a -= 2.0 * PI;
    while (a < -PI) a += 2.0 * PI;
    return a;
}

// -------------------------
// Args
// -------------------------
struct Args {
    // BLE
    string mac = "";
    string char_uuid = DEFAULT_CHAR_UUID;
    bool light_on = false;

    // Camera
    int camera = 0;
    int width = 1280;   // request (camera may ignore)
    int height = 720;

    bool has_roi = false;
    cv::Rect roi;

    // Pure pursuit (pixels)
    double wheelbase_px = 60.0;
    double lookahead_px = 120.0;
    double delta_max_deg = 30.0;

    int base_speed = 8;
    int min_speed = 0;
    int speed_cap = 8;
    double turn_slow_k = 0.95;
    double send_hz = 25.0;

    // Tracking / detection
    int warmup_frames = 120;
    double var_threshold = 18.0;
    double min_area = 250.0;
    double max_area = 12000.0;
    int min_wh = 10;
    int max_wh = 220;
    double min_solidity = 0.25;
    double max_jump_px = 220.0;
    double theta_smooth = 0.25; // 0..1

    // Map building
    int inflate_px = 18;          // for obstacle inflation in map step
    int clean_close_k = 31;
    int clean_close_iter = 1;
    int clean_open_k = 7;
    int clean_open_iter = 1;
    int clean_max_hole_area = 5000;

    int center_N_samples = 1200;
    int center_smooth_win = 31;
    int center_close_k = 7;
    int center_close_iters = 2;

    // Files
    string out_csv = "centerline.csv";

    // UI
    bool nogui = false;
    bool debug = false;
};

static Args parse_args(int argc, char** argv) {
    Args a;
    for (int i = 1; i < argc; ++i) {
        string k = argv[i];
        auto need = [&](const string& key) -> string {
            if (i + 1 >= argc) throw runtime_error("Missing value after " + key);
            return string(argv[++i]);
        };

        if (k == "--mac") a.mac = need(k);
        else if (k == "--char") a.char_uuid = need(k);
        else if (k == "--light_on") a.light_on = true;

        else if (k == "--camera") a.camera = stoi(need(k));
        else if (k == "--width") a.width = stoi(need(k));
        else if (k == "--height") a.height = stoi(need(k));

        else if (k == "--roi") {
            string v = need(k);
            stringstream ss(v);
            string s1, s2, s3, s4;
            if (!getline(ss, s1, ',') || !getline(ss, s2, ',') || !getline(ss, s3, ',') || !getline(ss, s4, ',')) {
                throw runtime_error("Bad --roi. Use x,y,w,h (example: --roi 80,40,500,380)");
            }
            int x = stoi(trim_copy(s1));
            int y = stoi(trim_copy(s2));
            int w = stoi(trim_copy(s3));
            int h = stoi(trim_copy(s4));
            a.has_roi = true;
            a.roi = cv::Rect(x, y, w, h);
        }

        else if (k == "--wheelbase_px") a.wheelbase_px = stod(need(k));
        else if (k == "--lookahead_px") a.lookahead_px = stod(need(k));
        else if (k == "--delta_max_deg") a.delta_max_deg = stod(need(k));

        else if (k == "--base_speed") a.base_speed = stoi(need(k));
        else if (k == "--min_speed") a.min_speed = stoi(need(k));
        else if (k == "--speed_cap") a.speed_cap = stoi(need(k));
        else if (k == "--turn_slow_k") a.turn_slow_k = stod(need(k));
        else if (k == "--send_hz") a.send_hz = stod(need(k));

        else if (k == "--warmup_frames") a.warmup_frames = stoi(need(k));
        else if (k == "--var_threshold") a.var_threshold = stod(need(k));

        else if (k == "--min_area") a.min_area = stod(need(k));
        else if (k == "--max_area") a.max_area = stod(need(k));
        else if (k == "--min_wh") a.min_wh = stoi(need(k));
        else if (k == "--max_wh") a.max_wh = stoi(need(k));
        else if (k == "--min_solidity") a.min_solidity = stod(need(k));
        else if (k == "--max_jump_px") a.max_jump_px = stod(need(k));

        else if (k == "--theta_smooth") a.theta_smooth = stod(need(k));

        else if (k == "--inflate_px") a.inflate_px = stoi(need(k));
        else if (k == "--csv") a.out_csv = need(k);

        else if (k == "--nogui") a.nogui = true;
        else if (k == "--debug") a.debug = true;
        else {
            cerr << "Unknown arg: " << k << "\n";
        }
    }
    return a;
}

// -------------------------
// BLE: find + connect + find characteristic
// -------------------------
struct BleTarget {
    SimpleBLE::Peripheral peripheral;
    string service_uuid;
    string char_uuid;
};

static optional<BleTarget> find_connect_and_find_char(const string& device_name_or_mac,
                                                      const string& characteristic_uuid,
                                                      int scan_ms = 9000) {
    cout << string(60, '=') << "\nScanning for BLE devices...\n" << string(60, '=') << "\n";

    auto adapters = SimpleBLE::Adapter::get_adapters();
    if (adapters.empty()) {
        cerr << "No BLE adapters found. Enable Bluetooth.\n";
        return nullopt;
    }

    auto adapter = adapters[0];
    cout << "Using adapter: " << adapter.identifier() << "\n";
    cout << "Scanning for " << (scan_ms / 1000.0) << "s (car ON)...\n\n";

    adapter.scan_for(scan_ms);
    auto peripherals = adapter.scan_get_results();

    vector<SimpleBLE::Peripheral> candidates;
    candidates.reserve(peripherals.size());

    string q = lower_copy(trim_copy(device_name_or_mac));
    for (auto& p : peripherals) {
        string name = p.identifier();
        string addr = lower_copy(p.address());

        if (!q.empty()) {
            if (lower_copy(name).find(q) != string::npos || addr == q) {
                candidates.push_back(p);
            }
        } else {
            string nl = lower_copy(name);
            if (nl.find("drift") != string::npos || nl.find("dr!ft") != string::npos) {
                candidates.push_back(p);
            }
        }
    }

    if (candidates.empty()) {
        cerr << "No DR!FT candidate found.\n";
        cerr << "Tip: pass MAC explicitly: --mac f9:af:3c:e2:d2:f5\n";
        return nullopt;
    }

    auto car = candidates[0];
    cout << "Found candidate: Name='" << car.identifier() << "'  MAC=" << car.address() << "\n";

    cout << "\nConnecting...\n";
    car.connect();
    this_thread::sleep_for(chrono::milliseconds(600));
    cout << "Connected.\n";

    string target_service, target_char;
    bool found = false;

    for (auto s : car.services()) {
        for (auto c : s.characteristics()) {
            string cu = uuid_to_string(c.uuid());
            if (lower_copy(cu) == lower_copy(characteristic_uuid)) {
                target_service = uuid_to_string(s.uuid());
                target_char = cu;
                found = true;
                break;
            }
        }
        if (found) break;
    }

    if (!found) {
        cerr << "\nTarget characteristic not found. Listing everything:\n";
        for (auto s : car.services()) {
            cerr << "Service: " << uuid_to_string(s.uuid()) << "\n";
            for (auto c : s.characteristics()) {
                cerr << "  - " << uuid_to_string(c.uuid()) << "\n";
            }
        }
        car.disconnect();
        return nullopt;
    }

    cout << "Using characteristic: " << target_char << "\n";
    return BleTarget{car, target_service, target_char};
}

// -------------------------
// Map-building: raw frame -> free_white (255=drivable)
// (Your code, adapted into a function)
// -------------------------
static cv::Mat build_free_white_from_bgr(const cv::Mat& bgr, int inflate_px,
                                        cv::Mat* out_roi = nullptr,
                                        cv::Mat* out_barriers = nullptr,
                                        cv::Mat* out_obstacles_inflated = nullptr) {
    if (bgr.empty()) throw runtime_error("build_free_white_from_bgr: empty frame");

    cv::Mat hsv;
    cv::cvtColor(bgr, hsv, cv::COLOR_BGR2HSV);
    int h = bgr.rows, w = bgr.cols;

    // Red barriers
    cv::Mat red1, red2, red;
    cv::inRange(hsv, cv::Scalar(0, 70, 50),   cv::Scalar(12, 255, 255), red1);
    cv::inRange(hsv, cv::Scalar(168, 70, 50), cv::Scalar(179, 255, 255), red2);
    cv::bitwise_or(red1, red2, red);

    // Auto ROI from red pixels -> convex hull
    std::vector<cv::Point> pts;
    pts.reserve(10000);
    for (int y = 0; y < red.rows; ++y) {
        const uchar* p = red.ptr<uchar>(y);
        for (int x = 0; x < red.cols; ++x) {
            if (p[x] > 0) pts.emplace_back(x, y);
        }
    }
    if ((int)pts.size() < 50) {
        throw runtime_error("Not enough red pixels for ROI hull. Adjust red thresholds / lighting.");
    }

    std::vector<cv::Point> hull;
    cv::convexHull(pts, hull);

    cv::Mat roi(h, w, CV_8UC1, cv::Scalar(0));
    cv::fillConvexPoly(roi, hull, cv::Scalar(255));

    // White barrier pieces inside ROI
    cv::Mat white;
    cv::inRange(hsv, cv::Scalar(0, 0, 180), cv::Scalar(179, 70, 255), white);
    cv::bitwise_and(white, roi, white);

    // Barrier mask + cleanup
    cv::Mat barriers;
    cv::bitwise_or(red, white, barriers);
    cv::bitwise_and(barriers, roi, barriers);

    cv::morphologyEx(barriers, barriers, cv::MORPH_OPEN,
        cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(5, 5)), cv::Point(-1, -1), 1);

    cv::morphologyEx(barriers, barriers, cv::MORPH_CLOSE,
        cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(7, 7)), cv::Point(-1, -1), 3);

    cv::dilate(barriers, barriers,
        cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(9, 9)), cv::Point(-1, -1), 1);

    // Outside ROI is obstacle too
    cv::Mat not_roi, walls;
    cv::bitwise_not(roi, not_roi);
    cv::bitwise_or(barriers, not_roi, walls);

    // Flood-fill from corner to label outside
    cv::Mat flood = walls.clone();
    cv::Mat ffmask(h + 2, w + 2, CV_8UC1, cv::Scalar(0));
    cv::floodFill(flood, ffmask, cv::Point(0, 0), cv::Scalar(128));

    cv::Mat outsideMask;
    cv::compare(flood, 128, outsideMask, cv::CMP_EQ); // 255 where outside

    cv::Mat walls0, notOutside, inside_free;
    cv::compare(walls, 0, walls0, cv::CMP_EQ);        // 255 where walls==0
    cv::bitwise_not(outsideMask, notOutside);         // 255 where NOT outside
    cv::bitwise_and(notOutside, walls0, inside_free); // 255 where inside free

    cv::Mat free_white = inside_free.clone();         // 255 = FREE (white)

    // Inflation outputs (optional)
    if (out_obstacles_inflated) {
        cv::Mat obstacles_from_free_white;
        cv::bitwise_not(free_white, obstacles_from_free_white);

        int k = inflate_px * 2 + 1;
        cv::Mat obstacles_inflated;
        cv::dilate(obstacles_from_free_white, obstacles_inflated,
            cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(k, k)), cv::Point(-1, -1), 1);

        *out_obstacles_inflated = obstacles_inflated;
    }

    if (out_roi) *out_roi = roi;
    if (out_barriers) *out_barriers = barriers;

    return free_white;
}

// -------------------------
// Clean track mask (your code)
// -------------------------
static cv::Mat keep_largest_component(const cv::Mat& bin255) {
    cv::Mat bin01;
    cv::threshold(bin255, bin01, 0, 1, cv::THRESH_BINARY);

    cv::Mat labels, stats, centroids;
    int num = cv::connectedComponentsWithStats(bin01, labels, stats, centroids, 8, CV_32S);
    if (num <= 1) return bin255.clone();

    int largestLabel = 1;
    int largestArea = stats.at<int>(1, cv::CC_STAT_AREA);
    for (int i = 2; i < num; i++) {
        int area = stats.at<int>(i, cv::CC_STAT_AREA);
        if (area > largestArea) { largestArea = area; largestLabel = i; }
    }

    cv::Mat out = cv::Mat::zeros(bin255.size(), CV_8UC1);
    out.setTo(255, labels == largestLabel);
    return out;
}

static cv::Mat fill_small_holes(const cv::Mat& bin255, int max_hole_area = 5000) {
    cv::Mat inv;
    cv::bitwise_not(bin255, inv);

    cv::Mat bin01;
    cv::threshold(inv, bin01, 0, 1, cv::THRESH_BINARY);

    cv::Mat labels, stats, centroids;
    int num = cv::connectedComponentsWithStats(bin01, labels, stats, centroids, 8, CV_32S);

    cv::Mat out = bin255.clone();
    int h = bin255.rows, w = bin255.cols;

    for (int lab = 1; lab < num; lab++) {
        int area = stats.at<int>(lab, cv::CC_STAT_AREA);
        int x = stats.at<int>(lab, cv::CC_STAT_LEFT);
        int y = stats.at<int>(lab, cv::CC_STAT_TOP);
        int ww = stats.at<int>(lab, cv::CC_STAT_WIDTH);
        int hh = stats.at<int>(lab, cv::CC_STAT_HEIGHT);

        bool touches_border = (x == 0) || (y == 0) || (x + ww >= w) || (y + hh >= h);
        if (touches_border) continue;

        if (area <= max_hole_area) out.setTo(255, labels == lab);
    }
    return out;
}

static cv::Mat clean_track_mask(const cv::Mat& free_white,
                               int close_k, int close_iter,
                               int open_k, int open_iter,
                               int max_hole_area) {
    cv::Mat m;
    cv::threshold(free_white, m, 0, 255, cv::THRESH_BINARY);

    m = keep_largest_component(m);

    cv::Mat kC = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(close_k, close_k));
    cv::morphologyEx(m, m, cv::MORPH_CLOSE, kC, cv::Point(-1, -1), close_iter);

    m = fill_small_holes(m, max_hole_area);

    cv::Mat kO = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(open_k, open_k));
    cv::morphologyEx(m, m, cv::MORPH_OPEN, kO, cv::Point(-1, -1), open_iter);

    m = keep_largest_component(m);

    return m;
}

// -------------------------
// Centerline extraction (your code)
// -------------------------
static double dist2p(const cv::Point2f& a, const cv::Point2f& b) {
    double dx = a.x - b.x, dy = a.y - b.y;
    return dx * dx + dy * dy;
}

static vector<cv::Point2f> contourToPoint2f(const vector<cv::Point>& c) {
    vector<cv::Point2f> out;
    out.reserve(c.size());
    for (auto& p : c) out.emplace_back((float)p.x, (float)p.y);
    return out;
}

static vector<cv::Point2f> resampleClosedByArclength(const vector<cv::Point2f>& pts, int N) {
    if ((int)pts.size() < 2 || N < 2) return pts;

    vector<cv::Point2f> closed = pts;
    closed.push_back(pts.front());

    vector<double> s(closed.size(), 0.0);
    for (size_t i = 1; i < closed.size(); ++i) {
        double dx = closed[i].x - closed[i - 1].x;
        double dy = closed[i].y - closed[i - 1].y;
        s[i] = s[i - 1] + std::sqrt(dx * dx + dy * dy);
    }
    double L = s.back();
    if (L < 1e-6) return pts;

    vector<cv::Point2f> out;
    out.reserve(N);

    double step = L / (double)N;
    size_t seg = 1;
    for (int k = 0; k < N; ++k) {
        double target = k * step;
        while (seg < s.size() && s[seg] < target) seg++;
        if (seg >= s.size()) seg = s.size() - 1;

        size_t i1 = seg - 1;
        size_t i2 = seg;

        double s1 = s[i1], s2 = s[i2];
        double t = (s2 - s1) > 1e-9 ? (target - s1) / (s2 - s1) : 0.0;

        cv::Point2f p = closed[i1] + (closed[i2] - closed[i1]) * (float)t;
        out.push_back(p);
    }
    return out;
}

static vector<cv::Point2f> smoothClosedMovingAverage(const vector<cv::Point2f>& pts, int win) {
    if ((int)pts.size() < 3) return pts;
    if (win < 3) return pts;
    if (win % 2 == 0) win += 1;

    int n = (int)pts.size();
    int k = win / 2;
    vector<cv::Point2f> out(n);

    for (int i = 0; i < n; ++i) {
        double sx = 0.0, sy = 0.0;
        for (int j = -k; j <= k; ++j) {
            int idx = (i + j) % n;
            if (idx < 0) idx += n;
            sx += pts[idx].x;
            sy += pts[idx].y;
        }
        out[i] = cv::Point2f((float)(sx / win), (float)(sy / win));
    }
    return out;
}

static vector<Pt2> build_centerline_csv_points(const cv::Mat& free_white_clean,
                                               int N_SAMPLES, int SMOOTH_WIN,
                                               int CLOSE_K, int CLOSE_ITERS) {
    cv::Mat road255;
    cv::threshold(free_white_clean, road255, 127, 255, cv::THRESH_BINARY);

    // Morph close to clean small gaps
    cv::Mat road_s = road255.clone();
    cv::Mat k = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(CLOSE_K, CLOSE_K));
    cv::morphologyEx(road_s, road_s, cv::MORPH_CLOSE, k, cv::Point(-1, -1), CLOSE_ITERS);

    vector<vector<cv::Point>> contours;
    vector<cv::Vec4i> hierarchy;
    cv::findContours(road_s, contours, hierarchy, cv::RETR_CCOMP, cv::CHAIN_APPROX_NONE);

    if (contours.empty() || hierarchy.empty()) {
        throw runtime_error("Centerline: no contours/hierarchy found (binary wrong?)");
    }

    // Outer = largest parent=-1
    int outer = -1;
    double bestArea = -1.0;
    for (int i = 0; i < (int)contours.size(); ++i) {
        int parent = hierarchy[i][3];
        if (parent != -1) continue;
        double a = fabs(cv::contourArea(contours[i]));
        if (a > bestArea) { bestArea = a; outer = i; }
    }
    if (outer < 0) throw runtime_error("Centerline: no outer contour found.");

    // Inner = largest child of outer
    int inner = -1;
    double bestInnerArea = -1.0;
    int child = hierarchy[outer][2];
    while (child != -1) {
        double a = fabs(cv::contourArea(contours[child]));
        if (a > bestInnerArea) { bestInnerArea = a; inner = child; }
        child = hierarchy[child][0];
    }

    if (inner < 0) {
        // fallback: 2nd largest contour overall
        vector<int> idx(contours.size());
        iota(idx.begin(), idx.end(), 0);
        sort(idx.begin(), idx.end(), [&](int a, int b) {
            return fabs(cv::contourArea(contours[a])) > fabs(cv::contourArea(contours[b]));
        });
        if (idx.size() < 2) throw runtime_error("Centerline: could not find inner contour.");
        inner = idx[1];
    }

    vector<cv::Point2f> outerPts = contourToPoint2f(contours[outer]);
    vector<cv::Point2f> innerPts = contourToPoint2f(contours[inner]);

    // 1) Resample outer
    vector<cv::Point2f> outerR = resampleClosedByArclength(outerPts, N_SAMPLES);

    // 2) For each outer point, nearest inner point
    vector<cv::Point2f> innerNN;
    innerNN.reserve(outerR.size());
    for (auto& p : outerR) {
        int bestJ = 0;
        double bestD2 = dist2p(p, innerPts[0]);
        for (int j = 1; j < (int)innerPts.size(); ++j) {
            double d2 = dist2p(p, innerPts[j]);
            if (d2 < bestD2) { bestD2 = d2; bestJ = j; }
        }
        innerNN.push_back(innerPts[bestJ]);
    }

    // 3) Midpoints
    vector<cv::Point2f> center;
    center.reserve(outerR.size());
    for (size_t i = 0; i < outerR.size(); ++i) {
        center.push_back((outerR[i] + innerNN[i]) * 0.5f);
    }

    // 4) Smooth
    vector<cv::Point2f> centerS = smoothClosedMovingAverage(center, SMOOTH_WIN);

    // close explicitly
    vector<cv::Point2f> centerClosed = centerS;
    centerClosed.push_back(centerS.front());

    vector<Pt2> out;
    out.reserve(centerClosed.size());
    for (auto& p : centerClosed) out.push_back(Pt2{p.x, p.y});
    return out;
}

static void save_centerline_csv_with_header(const string& path, const vector<Pt2>& pts,
                                            int W, int H) {
    ofstream f(path);
    if (!f.is_open()) throw runtime_error("Could not write CSV: " + path);
    f << "# W=" << W << " H=" << H << "\n";
    f << "x,y\n";
    f.setf(std::ios::fixed); f << setprecision(3);
    for (auto& p : pts) f << p.x << "," << p.y << "\n";
}

// Load CSV + parse header W/H if present
static vector<Pt2> load_path_csv(const string& filename, int* outW = nullptr, int* outH = nullptr) {
    ifstream f(filename);
    if (!f.is_open()) throw runtime_error("CSV cannot be opened: " + filename);

    vector<Pt2> pts;
    string raw;

    int csvW = -1, csvH = -1;

    while (getline(f, raw)) {
        string line = trim_copy(raw);
        if (line.empty()) continue;

        // header like: # W=1280 H=720
        if (line.size() >= 1 && line[0] == '#') {
            // very simple parse
            auto find_int_after = [&](const string& key) -> int {
                size_t pos = line.find(key);
                if (pos == string::npos) return -1;
                pos += key.size();
                while (pos < line.size() && (line[pos] == ' ' || line[pos] == '=')) pos++;
                int v = 0;
                bool ok = false;
                while (pos < line.size() && isdigit((unsigned char)line[pos])) {
                    ok = true;
                    v = v * 10 + (line[pos] - '0');
                    pos++;
                }
                return ok ? v : -1;
            };
            int w = find_int_after("W");
            int h = find_int_after("H");
            if (w > 0) csvW = w;
            if (h > 0) csvH = h;
            continue;
        }

        // skip column header line
        if (lower_copy(line) == "x,y") continue;

        for (char& c : line) if (c == ';') c = ',';

        double x = 0, y = 0;
        try {
            if (line.find(',') != string::npos) {
                size_t pos = line.find(',');
                x = stod(trim_copy(line.substr(0, pos)));
                y = stod(trim_copy(line.substr(pos + 1)));
            } else {
                stringstream ss(line);
                ss >> x >> y;
                if (!ss) continue;
            }
        } catch (...) {
            continue;
        }
        pts.push_back({x, y});
    }

    if (pts.empty()) throw runtime_error("CSV empty/unreadable: " + filename);
    if (outW) *outW = csvW;
    if (outH) *outH = csvH;
    return pts;
}

// -------------------------
// Pure Pursuit helpers
// -------------------------
static int closest_index(const vector<Pt2>& path, double x, double y) {
    int best = 0;
    double bestd = 1e300;
    for (int i = 0; i < (int)path.size(); ++i) {
        double dx = path[i].x - x;
        double dy = path[i].y - y;
        double d2 = dx * dx + dy * dy;
        if (d2 < bestd) { bestd = d2; best = i; }
    }
    return best;
}
static int closest_index_window_forward(const vector<Pt2>& path,
                                        int center_idx,
                                        int behind, int ahead,
                                        double x, double y) {
    int n = (int)path.size();
    if (n == 0) return 0;

    int best = center_idx;
    double bestd = 1e300;

    for (int k = -behind; k <= ahead; ++k) {
        int i = (center_idx + k) % n;
        if (i < 0) i += n;

        double dx = path[i].x - x;
        double dy = path[i].y - y;
        double d2 = dx * dx + dy * dy;

        if (d2 < bestd) { bestd = d2; best = i; }
    }
    return best;
}
static int lookahead_index_loop(const vector<Pt2>& path, int start_idx, double x, double y, double Ld) {
    int n = (int)path.size();
    if (n == 0) return 0;

    double Ld2 = Ld * Ld;
    int i = (start_idx % n + n) % n;

    for (int k = 0; k < n; ++k) {
        const auto& p = path[i];
        double dx = p.x - x;
        double dy = p.y - y;
        if (dx * dx + dy * dy >= Ld2) return i;
        i = (i + 1) % n;
    }
    return start_idx;
}

static double pure_pursuit_delta(double x, double y, double theta, const Pt2& target,
                                 double wheelbaseL, double Ld) {
    double angle_to_target = atan2(target.y - y, target.x - x);
    double alpha = wrap_pi(angle_to_target - theta);
    return atan2(2.0 * wheelbaseL * sin(alpha), Ld);
}

static int delta_to_steering_byte(double delta, double delta_max_rad) {
    double u = (delta_max_rad > 1e-9) ? (delta / delta_max_rad) : 0.0;
    u = std::clamp(u, -1.0, 1.0);

    // Map to 0..100% like your Python manual control
    int pct = (int)llround(std::abs(u) * 100.0);
    pct = std::clamp(pct, 0, 100);

    // small deadband
    if (pct < 3) return 0;

    if (u > 0) {
        // RIGHT uses 0..100
        return pct;
    } else {
        // LEFT uses 255 - pct
        return 255 - pct;
    }
}

static double steer_strength(int steer_byte) {
    if (steer_byte == 0) return 0.0;

    // RIGHT: 1..100
    if (steer_byte > 0 && steer_byte <= 100) {
        return std::clamp(steer_byte / 100.0, 0.0, 1.0);
    }

    // LEFT: 255-100 .. 254  (i.e., 155..254)
    int pct = 255 - steer_byte;   // back to 1..100
    return std::clamp(pct / 100.0, 0.0, 1.0);
}

static void draw_path_polyline(cv::Mat& frame, const vector<Pt2>& path) {
    if (path.size() < 2) return;
    vector<cv::Point> pts;
    pts.reserve(path.size());
    for (const auto& p : path) pts.emplace_back((int)llround(p.x), (int)llround(p.y));
    cv::polylines(frame, pts, true, cv::Scalar(255, 255, 0), 2, cv::LINE_AA);
}

// -------------------------
// Robust motion detection candidate picker (your code)
// -------------------------
struct DetCandidate {
    int idx = -1;
    double area = 0;
    int cx = 0;
    int cy = 0;
    cv::Rect box;
    double solidity = 0;
};

static double contour_solidity(const vector<cv::Point>& cnt) {
    double a = fabs(cv::contourArea(cnt));
    if (a <= 1e-9) return 0.0;
    vector<cv::Point> hull;
    cv::convexHull(cnt, hull);
    double ha = fabs(cv::contourArea(hull));
    if (ha <= 1e-9) return 0.0;
    return a / ha;
}

static optional<DetCandidate> pick_car_candidate(
    const vector<vector<cv::Point>>& contours,
    bool has_prev,
    cv::Point prev_xy,
    double min_area,
    double max_area,
    int min_wh,
    int max_wh,
    double min_solidity,
    double max_jump_px
) {
    vector<DetCandidate> cands;
    cands.reserve(contours.size());

    for (int i = 0; i < (int)contours.size(); ++i) {
        const auto& c = contours[i];

        double area = fabs(cv::contourArea(c));
        if (area < min_area || area > max_area) continue;

        cv::Rect r = cv::boundingRect(c);
        if (r.width < min_wh || r.height < min_wh) continue;
        if (r.width > max_wh || r.height > max_wh) continue;

        double ar = (r.height > 0) ? (double)r.width / (double)r.height : 99.0;
        if (ar < 0.35 || ar > 2.85) continue;

        cv::Moments m = cv::moments(c);
        if (fabs(m.m00) <= 1e-9) continue;

        int cx = (int)llround(m.m10 / m.m00);
        int cy = (int)llround(m.m01 / m.m00);

        double sol = contour_solidity(c);
        if (sol < min_solidity) continue;

        cands.push_back(DetCandidate{i, area, cx, cy, r, sol});
    }

    if (cands.empty()) return nullopt;

    if (!has_prev) {
        auto it = max_element(cands.begin(), cands.end(),
            [](const DetCandidate& a, const DetCandidate& b) { return a.area < b.area; });
        return *it;
    }

    optional<DetCandidate> best;
    double best_score = 1e300;

    for (const auto& t : cands) {
        double d = hypot((double)t.cx - prev_xy.x, (double)t.cy - prev_xy.y);
        if (d > max_jump_px) continue;
        double score = d - 0.0008 * t.area;
        if (score < best_score) {
            best_score = score;
            best = t;
        }
    }

    if (!best) {
        auto it = max_element(cands.begin(), cands.end(),
            [](const DetCandidate& a, const DetCandidate& b) { return a.area < b.area; });
        best = *it;
    }

    return best;
}

// -------------------------
// MAIN (state machine: MAP / RUN)
// -------------------------
int main(int argc, char** argv) {
    try {
        Args args = parse_args(argc, argv);

        // Camera open
        cv::VideoCapture cap(args.camera, cv::CAP_DSHOW);
        if (!cap.isOpened()) throw runtime_error("Cannot open camera index " + to_string(args.camera));

        cap.set(cv::CAP_PROP_FRAME_WIDTH, args.width);
        cap.set(cv::CAP_PROP_FRAME_HEIGHT, args.height);

        if (!args.nogui) {
            cv::namedWindow("Original", cv::WINDOW_NORMAL);
            cv::namedWindow("MotionMask", cv::WINDOW_NORMAL);
            cv::namedWindow("MapDebug", cv::WINDOW_NORMAL);
        }

        // Background subtractor (created now, can be reset with B)
        auto make_bg = [&]() {
            return cv::createBackgroundSubtractorMOG2(
                max(300, args.warmup_frames + 10),
                args.var_threshold,
                false
            );
        };
        auto bg = make_bg();
        int frame_count = 0;

        // BLE connection is optional until RUN starts
        optional<BleTarget> ble;
        bool running = false;         // autopilot enabled/disabled

        // Path in memory (built with M or loaded from disk)
        vector<Pt2> path;
        int pathW = -1, pathH = -1;
        bool has_path = false;
        int path_idx = 0;
        bool has_path_idx = false;

        // Tracking state
        bool has_prev = false;
        cv::Point prev_pos(0, 0);
        //double theta = 0.0;
        double theta_ctrl = 0.0;   // used for control (real car heading)
        double theta_mot  = 0.0;   // measured from motion (debug)
        double last_seen_t = now_s();
        const double lost_timeout_s = 0.45;

        const double delta_max_rad = args.delta_max_deg * PI / 180.0;
        const double dt_send = 1.0 / max(1e-6, args.send_hz);
        double next_send_t = now_s();

        cout << "Controls:\n";
        cout << "  M = capture & build centerline CSV\n";
        cout << "  R = toggle autopilot run/stop\n";
        cout << "  B = reset background model\n";
        cout << "  Q/ESC = quit\n\n";

        cout << "Requested camera: " << args.width << "x" << args.height << "\n";
        cout << "CAP says: " << cap.get(cv::CAP_PROP_FRAME_WIDTH) << " x " << cap.get(cv::CAP_PROP_FRAME_HEIGHT) << "\n";

        while (true) {
            cv::Mat frame;
            if (!cap.read(frame) || frame.empty()) break;
            frame_count++;

            // On first frame, print actual
            if (frame_count == 1) {
                cout << "Actual frame: " << frame.cols << " x " << frame.rows << "\n";
            }

            // ROI for tracking (optional)
            cv::Rect roiRect(0, 0, frame.cols, frame.rows);
            if (args.has_roi) roiRect = args.roi & roiRect;
            cv::Mat view = frame(roiRect);

            // Always compute fg mask (for preview / for RUN)
            double learningRate = (!running) ? -1.0 : ((frame_count <= args.warmup_frames) ? -1.0 : 0.0);
            cv::Mat fg;
            bg->apply(view, fg, learningRate);

            // If running & something appears during warmup => freeze early
            if (running && frame_count <= args.warmup_frames) {
                int nz = cv::countNonZero(fg);
                if (nz > 500) args.warmup_frames = frame_count;
            }

            // Clean fg
            cv::threshold(fg, fg, 128, 255, cv::THRESH_BINARY);
            cv::medianBlur(fg, fg, 5);
            cv::Mat kOpen = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(5, 5));
            cv::Mat kClose = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(9, 9));
            cv::morphologyEx(fg, fg, cv::MORPH_OPEN, kOpen, cv::Point(-1, -1), 1);
            cv::morphologyEx(fg, fg, cv::MORPH_CLOSE, kClose, cv::Point(-1, -1), 1);

            // If RUN mode: detect car + follow
            int steer_byte = 0;
            int speed_cmd = 0;

            if (running && has_path) {
                vector<vector<cv::Point>> contours;
                cv::findContours(fg, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

                cv::Point prev_in_roi(0, 0);
                if (has_prev) prev_in_roi = cv::Point(prev_pos.x - roiRect.x, prev_pos.y - roiRect.y);

                auto candOpt = pick_car_candidate(
                    contours,
                    has_prev,
                    prev_in_roi,
                    args.min_area, args.max_area,
                    args.min_wh, args.max_wh,
                    args.min_solidity,
                    args.max_jump_px
                );

                int target_idx = 0;
                int i_closest = 0;

                if (candOpt) {
                    auto cand = *candOpt;

                    int cx = cand.cx + roiRect.x;
                    int cy = cand.cy + roiRect.y;

                    // update theta
                    if (has_prev) {
                        int dx = cx - prev_pos.x;
                        int dy = cy - prev_pos.y;

                        // update headings only when motion is large enough (avoid jitter)
                        if (dx * dx + dy * dy > 25) {
                            theta_mot = atan2((double)dy, (double)dx);  // debug

                            // theta_ctrl is the heading used for control
                            double a = 0.35; // 0.25..0.5 (higher = more responsive)
                            theta_ctrl = wrap_pi((1.0 - a) * theta_ctrl + a * theta_mot);
                        }
                    }


                    prev_pos = cv::Point(cx, cy);
                    has_prev = true;
                    last_seen_t = now_s();

                    if (!has_path_idx) {
    path_idx = closest_index(path, (double)cx, (double)cy);
    has_path_idx = true;
} else {
    path_idx = closest_index_window_forward(path, path_idx, 10, 120, (double)cx, (double)cy);
}

i_closest = path_idx;
target_idx = lookahead_index_loop(path, i_closest, (double)cx, (double)cy, args.lookahead_px);
                    Pt2 target = path[target_idx];

                    double delta = pure_pursuit_delta((double)cx, (double)cy, theta_ctrl, target,
                                 args.wheelbase_px, args.lookahead_px);
                    delta = std::clamp(delta, -delta_max_rad, delta_max_rad);
                    steer_byte = delta_to_steering_byte(delta, delta_max_rad);

                    double strength = steer_strength(steer_byte);
                    double slow = std::clamp(args.turn_slow_k * strength, 0.0, 0.7);
                    speed_cmd = (int)llround(args.base_speed * (1.0 - slow));
                    speed_cmd = std::clamp(speed_cmd, args.min_speed, args.base_speed);
                    speed_cmd = std::min(speed_cmd, args.speed_cap);

                    if (!args.nogui) {
                        draw_path_polyline(frame, path);

                        if (args.has_roi) cv::rectangle(frame, args.roi, cv::Scalar(255, 255, 0), 2);

                        cv::Rect r = cand.box;
                        r.x += roiRect.x; r.y += roiRect.y;
                        cv::rectangle(frame, r, cv::Scalar(0, 0, 255), 2);
                        cv::circle(frame, cv::Point(cx, cy), 6, cv::Scalar(0, 0, 255), -1);

                        // closest point (green)
                        cv::Point cp((int)llround(path[i_closest].x), (int)llround(path[i_closest].y));
                        cv::circle(frame, cp, 5, cv::Scalar(0, 255, 0), -1);

                        // target (yellow)
                        cv::Point tp((int)llround(target.x), (int)llround(target.y));
                        cv::circle(frame, tp, 7, cv::Scalar(0, 255, 255), -1);

                        int L = 60;
                        cv::Point p0(cx, cy);
                        cv::Point p1(cx + (int)llround(L * cos(theta_ctrl)),
             cy + (int)llround(L * sin(theta_ctrl)));
                        double theta_des = atan2(target.y - cy, target.x - cx);
                        cv::Point p2(cx + (int)llround(L * cos(theta_des)), cy + (int)llround(L * sin(theta_des)));
                        cv::arrowedLine(frame, p0, p1, cv::Scalar(255, 0, 0), 2);
                        cv::arrowedLine(frame, p0, p2, cv::Scalar(0, 255, 0), 2);

                        stringstream ss;
                        ss << "RUN steer=" << steer_byte << " speed=" << speed_cmd
                           << " lookahead=" << (int)args.lookahead_px
                           << " pts=" << path.size();
                        cv::putText(frame, ss.str(), cv::Point(10, 30),
                                    cv::FONT_HERSHEY_SIMPLEX, 0.75, cv::Scalar(255, 255, 255), 2);
                    }
                }

                // failsafe
                if ((now_s() - last_seen_t) > lost_timeout_s) {
                    steer_byte = 0;
                    speed_cmd = 0;
                }

                // send BLE
                double t = now_s();
                if (ble && t >= next_send_t) {
                    string cmd_hex = generate_command_direct(args.light_on, speed_cmd, steer_byte);
                    send_command(ble->peripheral, ble->service_uuid, ble->char_uuid, cmd_hex);
                    next_send_t = t + dt_send;
                }
            } else {
                // not running: just show info / overlay path if available
                if (!args.nogui) {
                    if (has_path) draw_path_polyline(frame, path);

                    stringstream ss;
                    ss << (running ? "RUN" : "IDLE")
                       << "  (M=map, R=run, B=reset BG) "
                       << "  path=" << (has_path ? "YES" : "NO");
                    cv::putText(frame, ss.str(), cv::Point(10, 30),
                                cv::FONT_HERSHEY_SIMPLEX, 0.75, cv::Scalar(255, 255, 255), 2);
                }
            }

            // GUI
            if (!args.nogui) {
                cv::imshow("MotionMask", fg);
                cv::imshow("Original", frame);

                int k = cv::waitKey(1) & 0xFF;

                // Quit
                if (k == 27 || k == 'q' || k == 'Q') {
                    break;
                }

                // Reset background
                if (k == 'b' || k == 'B') {
                    has_path_idx = false;
                    bg = make_bg();
                    frame_count = 0;
                    has_prev = false;
                    cout << "Background reset. (Warmup again)\n";
                }

                // Toggle RUN
                if (k == 'r' || k == 'R') {
                    running = !running;

                    if (running) {
                        // connect BLE if needed
                        if (!ble) {
                            ble = find_connect_and_find_char(args.mac, args.char_uuid, 9000);
                            if (!ble) {
                                cout << "BLE connect failed. RUN canceled.\n";
                                running = false;
                            } else {
                                // stop at start
                                send_command(ble->peripheral, ble->service_uuid, ble->char_uuid,
                                             generate_command_direct(args.light_on, 0, 0));
                                this_thread::sleep_for(chrono::milliseconds(150));
                            }
                        }

                        // If no path loaded in memory, try load from disk
                        if (!has_path) {
                            try {
                                int cw=-1, ch=-1;
                                auto p = load_path_csv(args.out_csv, &cw, &ch);
                                // check size
                                if (cw > 0 && ch > 0 && (cw != frame.cols || ch != frame.rows)) {
                                    cerr << "CSV size mismatch! CSV says " << cw << "x" << ch
                                         << " but live is " << frame.cols << "x" << frame.rows << "\n";
                                    cerr << "Press M to rebuild CSV from this live camera.\n";
                                    running = false;
                                } else {
                                    path = std::move(p);
                                    pathW = cw; pathH = ch;
                                    has_path = true;
                                    cout << "Loaded path from " << args.out_csv << " points=" << path.size() << "\n";
                                }
                            } catch (const exception& e) {
                                cerr << "No path yet: " << e.what() << "\n";
                                cerr << "Press M to build the map.\n";
                                running = false;
                            }
                        }

                        if (running) {
                            cout << "RUN ON\n";
                            last_seen_t = now_s();
                            next_send_t = now_s();
                        }

                    } else {
                        // stop car
                        if (ble) {
                            string stop_hex = generate_command_direct(false, 0, 0);
                            for (int i = 0; i < 6; ++i) {
                                send_command(ble->peripheral, ble->service_uuid, ble->char_uuid, stop_hex);
                                this_thread::sleep_for(chrono::milliseconds(30));
                            }
                        }
                        cout << "RUN OFF\n";
                    }
                }

                // Build MAP
                if (k == 'm' || k == 'M') {
                    // Never build while running (stop first)
                    if (running) {
                        running = false;
                        if (ble) {
                            string stop_hex = generate_command_direct(false, 0, 0);
                            send_command(ble->peripheral, ble->service_uuid, ble->char_uuid, stop_hex);
                        }
                        cout << "Stopped RUN to build MAP.\n";
                    }

                    // Capture and build
                    cv::Mat map_frame = frame.clone();
                    cv::imwrite("map_frame.png", map_frame);

                    cout << "Building map from live frame (" << map_frame.cols << "x" << map_frame.rows << ")...\n";

                    try {
                        cv::Mat roi, barriers, obstacles_inflated;
                        cv::Mat free_white = build_free_white_from_bgr(map_frame, args.inflate_px,
                                                                      &roi, &barriers, &obstacles_inflated);

                        cv::Mat free_clean = clean_track_mask(
                            free_white,
                            args.clean_close_k, args.clean_close_iter,
                            args.clean_open_k, args.clean_open_iter,
                            args.clean_max_hole_area
                        );

                        // centerline points
                        vector<Pt2> new_path = build_centerline_csv_points(
                            free_clean,
                            args.center_N_samples,
                            args.center_smooth_win,
                            args.center_close_k,
                            args.center_close_iters
                        );

                        // save outputs
                        cv::imwrite("track_free_white.png", free_white);
                        cv::imwrite("track_free_white_clean.png", free_clean);
                        cv::imwrite("roi.png", roi);
                        cv::imwrite("barriers_mask.png", barriers);
                        cv::imwrite("obstacles_inflated.png", obstacles_inflated);

                        // preview overlay
                        cv::Mat overlay;
                        cv::cvtColor(free_clean, overlay, cv::COLOR_GRAY2BGR);
                        vector<cv::Point> poly;
                        poly.reserve(new_path.size());
                        for (auto& p : new_path) poly.emplace_back((int)lround(p.x), (int)lround(p.y));
                        cv::polylines(overlay, poly, true, cv::Scalar(255, 0, 0), 3, cv::LINE_AA);
                        cv::imwrite("centerline_preview.png", overlay);

                        save_centerline_csv_with_header(args.out_csv, new_path, map_frame.cols, map_frame.rows);

                        // install into memory
                        path = std::move(new_path);
                        pathW = map_frame.cols;
                        pathH = map_frame.rows;
                        has_path = true;
                        has_path_idx = false;
                        cout << "MAP DONE. Saved:\n";
                        cout << "  map_frame.png\n";
                        cout << "  track_free_white.png\n";
                        cout << "  track_free_white_clean.png\n";
                        cout << "  centerline_preview.png\n";
                        cout << "  " << args.out_csv << "\n";
                        cout << "Points: " << path.size() << "\n";

                        if (!args.nogui) {
                            cv::imshow("MapDebug", overlay);
                        }
                    } catch (const exception& e) {
                        cerr << "MAP FAILED: " << e.what() << "\n";
                    }
                }
            } else {
                // nogui mode: no keyboard controls
                // (you can still run by providing a prebuilt CSV)
            }
        }

        // Stop car on exit
        if (ble) {
            cout << "Stopping car...\n";
            string stop_hex = generate_command_direct(false, 0, 0);
            for (int i = 0; i < 10; ++i) {
                send_command(ble->peripheral, ble->service_uuid, ble->char_uuid, stop_hex);
                this_thread::sleep_for(chrono::milliseconds(50));
            }
            ble->peripheral.disconnect();
        }

        cap.release();
        if (!args.nogui) cv::destroyAllWindows();
        cout << "Done.\n";
        return 0;

    } catch (const exception& e) {
        cerr << "FATAL: " << e.what() << "\n";
        return 1;
    }
}