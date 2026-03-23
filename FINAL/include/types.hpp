/*──────────────────────────────────────────────────────────────────────
 *  types.hpp  –  Shared data types for VisionRC_FINAL
 *──────────────────────────────────────────────────────────────────────*/
#ifndef VISIONRC_TYPES_HPP
#define VISIONRC_TYPES_HPP

#include <opencv2/opencv.hpp>

#include <array>
#include <atomic>
#include <cmath>
#include <limits>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace vrc {

/* ── helpers ────────────────────────────────────────────────────── */
constexpr double kPi = 3.14159265358979323846;

template <typename T>
T clampv(T v, T lo, T hi) { return std::max(lo, std::min(hi, v)); }

inline double sign_nonzero(double v) { return v < 0.0 ? -1.0 : 1.0; }

inline double wrap_pi(double a) {
    return std::fmod(a + kPi, 2.0 * kPi) - kPi;
}

inline int16_t to_s16(double v) {
    auto iv = static_cast<int>(std::llround(v));
    return static_cast<int16_t>(clampv(iv, -32768, 32767));
}

/* ── BLE packet protocol ────────────────────────────────────────── */
constexpr const char* WRITE_UUID = "6e400002-b5a3-f393-e0a9-e50e24dcca9e";
constexpr double      B_PER_DEG  = 16000.0 / 90.0;

inline const std::vector<uint8_t>& header_bytes() {
    static std::vector<uint8_t> h{0xbf,0x0a,0x00,0x08,0x28,0x00};
    return h;
}
inline const std::vector<uint8_t>& brake_packet() {
    static std::vector<uint8_t> p{0xbf,0x0a,0x00,0x08,0x28,0x00,
                                   0x00,0x00,0x00,0x00,0x00,0x00,
                                   0x00,0x01,0x00,0xba};
    return p;
}
inline const std::vector<uint8_t>& idle_packet() {
    static std::vector<uint8_t> p{0xbf,0x0a,0x00,0x08,0x28,0x00,
                                   0x00,0x00,0x00,0x00,0x00,0x00,
                                   0x00,0x00,0x00,0x4e};
    return p;
}
constexpr uint8_t TAIL = 0x4E;

inline std::vector<uint8_t> build_packet(double a, double b, double c) {
    auto out = header_bytes();
    auto push_s16le = [&](double x) {
        int16_t iv = to_s16(x);
        out.push_back(static_cast<uint8_t>(iv & 0xff));
        out.push_back(static_cast<uint8_t>((iv >> 8) & 0xff));
    };
    push_s16le(a); push_s16le(b); push_s16le(c); push_s16le(0.0);
    out.push_back(TAIL);
    return out;
}

/* ── global controller state ────────────────────────────────────── */
struct State {
    double speed          = 0.0;
    double turn_deg       = 0.0;
    double target_deg     = 0.0;
    double max_deg        = 12.0;
    double steer_smooth   = 55.0;
    double ratio          = 0.80;
    double c_sign         = -1.0;
    double max_speed      = 6000.0;
    double speed_step     = 90.0;
    double speed_k        = 0.03;
    bool   braking        = false;
    bool   logging        = false;
    std::string log_path;
    bool   auto_enabled   = true;
    double auto_steer_sign= 1.0;
    std::optional<double> last_good_theta;
    double v_ref_raw      = 0.0;
    double delta_cmd      = 0.0;
    std::string status    = "INIT";
    double last_e_lat         = 0.0;
    double last_e_head_deg    = 0.0;
    double last_track_t       = 0.0;
    bool   flip_centerline    = false;
    double drive_bias_deg     = 0.0;
    double last_delta_for_speed = 0.0;
    double pp_target_x    = std::numeric_limits<double>::quiet_NaN();
    double pp_target_y    = std::numeric_limits<double>::quiet_NaN();
    double pp_alpha_deg   = 0.0;
    double pp_lookahead_px= 0.0;
    double e_lat_filt         = 0.0;
    double pp_alpha_filt      = 0.0;
    double e_lat_rate_filt    = 0.0;
    double pp_alpha_rate_filt = 0.0;
};

/* ── vision telemetry ───────────────────────────────────────────── */
struct VisionData {
    double t_ms       = 0.0;
    double dt_ms      = 0.0;
    std::string mode  = "GLOBAL";
    bool   used_meas  = false;
    bool   holding    = false;
    int    meas_cx    = -1;
    int    meas_cy    = -1;
    double kf_x = 0, kf_y = 0, kf_vx = 0, kf_vy = 0;
    double blend_x = 0, blend_y = 0;
    double ctrl_x  = 0, ctrl_y  = 0;
    std::string blend_src = "NONE";
    double blend_a      = 0.0;
    double speed_px_s   = 0.0;
    double theta_deg    = std::numeric_limits<double>::quiet_NaN();
    std::string theta_src = "NONE";
    double step_px      = 0.0;
    double mu_cv = 0.5, mu_ct = 0.5;
    double omega_deg_s  = 0.0;
    double e_lat        = std::numeric_limits<double>::quiet_NaN();
    double e_head_deg   = std::numeric_limits<double>::quiet_NaN();
    double pp_target_x  = std::numeric_limits<double>::quiet_NaN();
    double pp_target_y  = std::numeric_limits<double>::quiet_NaN();
    double pp_alpha_deg = std::numeric_limits<double>::quiet_NaN();
    double pp_lookahead_px = 0.0;
};

/* ── ABC result (speed / steering) ─────────────────────────────── */
struct AbcResult { double a_sent, b, c, drop; };

/* ── nearest-point result on polyline ──────────────────────────── */
struct NearestResult {
    int         seg_i   = 0;
    double      heading = 0.0;
    double      e_lat   = 0.0;
    cv::Point2d proj;
};

/* ── command-line arguments ─────────────────────────────────────── */
struct Args {
    std::optional<std::string> address;
    std::optional<std::string> name;
    bool   discover          = false;
    double scan_timeout      = 6.0;
    bool   no_ble            = false;
    double auto_steer_sign   = 1.0;
    bool   flip_centerline   = false;
    double max_deg           = 9.0;
    double steer_smooth      = 42.0;
    double ratio             = 0.80;
    double c_sign            = -1.0;
    double speed_k           = 0.03;
    std::string ble_backend  = "auto";   // "auto", "simpleble"
};

/* ── global singletons (defined once in main.cpp) ──────────────── */
extern State      g_state;
extern std::mutex g_state_mtx;
extern VisionData g_vision;
extern std::mutex g_vision_mtx;
extern std::atomic<bool> g_stop;
extern std::mutex g_centerline_mtx;
extern std::optional<std::vector<cv::Point2d>> g_new_centerline;

}  // namespace vrc

#endif  // VISIONRC_TYPES_HPP
