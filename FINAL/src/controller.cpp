/*──────────────────────────────────────────────────────────────────────
 *  controller.cpp  –  Pure-pursuit autonomy + control loop +
 *                      vision/detection thread + CSV logging
 *──────────────────────────────────────────────────────────────────────*/
#include "controller.hpp"
#include "car_detector.hpp"
#include "imm_tracker.hpp"
#include "path_detector.hpp"
#include "types.hpp"

#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>

namespace vrc {

/* ── time ──────────────────────────────────────────────────────── */
double now_sec() {
    using namespace std::chrono;
    return duration_cast<duration<double>>(
               system_clock::now().time_since_epoch()).count();
}

/* ================================================================
 *  CSV logging
 * ================================================================*/
static std::ofstream log_file;

static std::string fmt_num(double v, int p) {
    std::ostringstream o; o << std::fixed << std::setprecision(p) << v;
    return o.str();
}

static void write_csv_header() {
    log_file
        << "timestamp,a_sent,speed_raw,speed_drop,turn_deg,target_deg,"
           "b_val,c_val,cb_ratio_live,ratio_setting,c_sign,max_deg,"
           "steer_smooth,max_speed,speed_step,speed_k,braking,"
           "auto_enabled,auto_speed_cmd,auto_delta_cmd,auto_status,"
           "t_ms,dt_ms,mode,used_meas,holding,meas_cx,meas_cy,"
           "kf_x,kf_y,kf_vx,kf_vy,blend_x,blend_y,ctrl_x,ctrl_y,"
           "blend_src,blend_a,speed_px_s,theta_deg,theta_src,step_px,"
           "mu_cv,mu_ct,omega_deg_s,e_lat,e_head_deg\n";
}

void start_logging() {
    std::scoped_lock lk(g_state_mtx);
    if (g_state.logging) return;
    auto t = std::time(nullptr); std::tm tm{}; localtime_r(&t, &tm);
    std::ostringstream ts;
    ts << std::put_time(&tm, "%Y%m%d_%H%M%S");
    auto p = std::filesystem::current_path() /
             ("auto_collect_" + ts.str() + ".csv");
    log_file.open(p.string(), std::ios::out | std::ios::trunc);
    if (!log_file) { std::cerr << "[LOG] Failed: " << p << "\n"; return; }
    write_csv_header();
    g_state.logging  = true;
    g_state.log_path = p.string();
}

void stop_logging() {
    std::scoped_lock lk(g_state_mtx);
    g_state.logging = false;
    if (log_file.is_open()) { log_file.flush(); log_file.close(); }
}

void log_row(const AbcResult& r) {
    if (!log_file.is_open()) return;
    State s; { std::scoped_lock lk(g_state_mtx); if (!s.logging && !g_state.logging) return; s = g_state; }
    VisionData v; { std::scoped_lock lk(g_vision_mtx); v = g_vision; }
    double live_ratio = std::abs(r.b)>1e-9 ? r.c/r.b : 0.0;

    log_file
        << fmt_num(now_sec(),6) << ',' << std::llround(r.a_sent) << ','
        << std::llround(s.speed) << ',' << fmt_num(r.drop,3) << ','
        << fmt_num(s.turn_deg,3) << ',' << fmt_num(s.target_deg,3) << ','
        << std::llround(r.b) << ',' << std::llround(r.c) << ','
        << fmt_num(live_ratio,5) << ',' << fmt_num(s.ratio,5) << ','
        << static_cast<int>(s.c_sign) << ',' << fmt_num(s.max_deg,3) << ','
        << fmt_num(s.steer_smooth,3) << ',' << std::llround(s.max_speed) << ','
        << std::llround(s.speed_step) << ',' << fmt_num(s.speed_k,5) << ','
        << static_cast<int>(s.braking) << ',' << static_cast<int>(s.auto_enabled) << ','
        << std::llround(s.v_ref_raw) << ',' << fmt_num(s.delta_cmd,3) << ','
        << s.status << ',' << std::llround(v.t_ms) << ','
        << fmt_num(v.dt_ms,3) << ',' << v.mode << ','
        << static_cast<int>(v.used_meas) << ',' << static_cast<int>(v.holding) << ','
        << v.meas_cx << ',' << v.meas_cy << ','
        << fmt_num(v.kf_x,3) << ',' << fmt_num(v.kf_y,3) << ','
        << fmt_num(v.kf_vx,3) << ',' << fmt_num(v.kf_vy,3) << ','
        << fmt_num(v.blend_x,3) << ',' << fmt_num(v.blend_y,3) << ','
        << fmt_num(v.ctrl_x,3) << ',' << fmt_num(v.ctrl_y,3) << ','
        << v.blend_src << ',' << fmt_num(v.blend_a,3) << ','
        << fmt_num(v.speed_px_s,3) << ',';

    if (std::isfinite(v.theta_deg)) log_file << fmt_num(v.theta_deg,3);
    else log_file << "nan";
    log_file << ',' << v.theta_src << ',' << fmt_num(v.step_px,3) << ','
             << fmt_num(v.mu_cv,4) << ',' << fmt_num(v.mu_ct,4) << ','
             << fmt_num(v.omega_deg_s,2) << ',';
    if (std::isfinite(v.e_lat)) log_file << fmt_num(v.e_lat,2); else log_file << "nan";
    log_file << ',';
    if (std::isfinite(v.e_head_deg)) log_file << fmt_num(v.e_head_deg,2); else log_file << "nan";
    log_file << '\n'; log_file.flush();
}

/* ================================================================
 *  compute_abc  (speed / steer → raw A, B, C)
 * ================================================================*/
AbcResult compute_abc() {
    std::scoped_lock lk(g_state_mtx);
    double b = g_state.turn_deg * B_PER_DEG;
    double c = g_state.c_sign * g_state.ratio * b;
    double speed_raw = clampv(g_state.speed, -g_state.max_speed, g_state.max_speed);
    double drop = g_state.speed_k * std::abs(b);
    double a_sent = 0;
    if (std::abs(speed_raw) > drop)
        a_sent = sign_nonzero(speed_raw) * (std::abs(speed_raw) - drop);
    a_sent = clampv(a_sent, -g_state.max_speed, g_state.max_speed);
    return {a_sent, b, c, drop};
}

/* ================================================================
 *  Autonomy helpers
 * ================================================================*/
static double speed_schedule(double abs_delta, double abs_e_lat,
                             double abs_alpha) {
    if (abs_alpha>22||abs_e_lat>44||abs_delta>7) return AUTO_MIN_SPEED_RAW;
    if (abs_alpha>9 ||abs_e_lat>18||abs_delta>3) return AUTO_MID_SPEED_RAW;
    return AUTO_MAX_SPEED_RAW;
}

static void update_speed_toward(double target) {
    std::scoped_lock lk(g_state_mtx);
    double step = g_state.speed_step, cur = g_state.speed;
    if (cur < target) g_state.speed = std::min(target, cur+step);
    else if (cur > target) g_state.speed = std::max(target, cur-step);
}

static double fo_update(double prev, double target, double a) {
    return (1.0-a)*prev + a*target;
}

static void autonomy_step() {
    VisionData v;
    { std::scoped_lock lk(g_vision_mtx); v = g_vision; }

    double now = now_sec();
    { std::scoped_lock lk(g_state_mtx); g_state.status = "AUTO"; }

    bool have_track = std::isfinite(v.e_lat) && std::isfinite(v.e_head_deg);
    {
        std::scoped_lock lk(g_state_mtx);
        if (have_track) {
            g_state.last_track_t = now;
        } else if ((now - g_state.last_track_t)*1000 > AUTO_BRAKE_NO_TRACK_MS) {
            g_state.status="NO_TRACK"; g_state.braking=true;
            g_state.speed=0; g_state.target_deg=0;
            g_state.v_ref_raw=0; g_state.delta_cmd=0; g_state.pp_alpha_deg=0;
            return;
        } else {
            g_state.status="TRACK_HOLD"; g_state.target_deg=0;
            g_state.v_ref_raw=AUTO_MIN_SPEED_RAW;
            g_state.pp_alpha_deg=0; g_state.braking=false;
        }
    }
    if (!have_track) { update_speed_toward(AUTO_MIN_SPEED_RAW); return; }

    double speed_px = std::max(10.0, v.speed_px_s);
    double e_lat_raw = v.e_lat;
    double pp_alpha_deg = v.pp_alpha_deg;

    {
        std::scoped_lock lk(g_state_mtx);
        g_state.pp_target_x = v.pp_target_x;
        g_state.pp_target_y = v.pp_target_y;
        g_state.pp_lookahead_px = v.pp_lookahead_px;

        if (!std::isfinite(pp_alpha_deg)) {
            pp_alpha_deg = clampv(v.e_head_deg, -45.0, 45.0);
            g_state.status = "PP_FALLBACK";
        }

        double e_lat = e_lat_raw;
        if (std::abs(pp_alpha_deg) < AUTO_PP_ALPHA_DEADBAND) pp_alpha_deg=0;
        if (std::abs(e_lat) < AUTO_PP_ELAT_DEADBAND) e_lat=0;

        g_state.e_lat_filt = fo_update(g_state.e_lat_filt, e_lat, 0.28);
        g_state.pp_alpha_filt = fo_update(g_state.pp_alpha_filt, pp_alpha_deg, 0.24);

        double dt_ctrl = 1.0/CONTROL_HZ;
        double e_lat_rate = (g_state.e_lat_filt - g_state.last_e_lat) / std::max(1e-3, dt_ctrl);
        double pp_alpha_rate = (g_state.pp_alpha_filt - g_state.last_e_head_deg) / std::max(1e-3, dt_ctrl);

        g_state.e_lat_rate_filt = fo_update(g_state.e_lat_rate_filt, e_lat_rate, AUTO_PP_RATE_ELAT);
        g_state.pp_alpha_rate_filt = fo_update(g_state.pp_alpha_rate_filt, pp_alpha_rate, AUTO_PP_RATE_ALPHA);

        double e_lat_pred = clampv(
            g_state.e_lat_filt + AUTO_PP_DELAY_COMP_S*g_state.e_lat_rate_filt, -95.0, 95.0);
        double pp_alpha_pred = clampv(
            g_state.pp_alpha_filt + AUTO_PP_DELAY_COMP_S*g_state.pp_alpha_rate_filt, -35.0, 35.0);

        double alpha_term = AUTO_PP_K_ALPHA * pp_alpha_pred;
        double lat_term = std::atan2(AUTO_PP_K_LAT * e_lat_pred, speed_px+110.0)*180.0/kPi;
        double d_term = clampv(AUTO_PP_D_ALPHA*g_state.pp_alpha_rate_filt +
                               AUTO_PP_D_ELAT*g_state.e_lat_rate_filt,
                               -AUTO_PP_D_MAX, AUTO_PP_D_MAX);

        double raw_delta = -(alpha_term + lat_term + d_term);
        raw_delta = AUTO_PP_SOFTSAT_DEG * std::tanh(raw_delta / AUTO_PP_SOFTSAT_DEG);
        raw_delta *= g_state.auto_steer_sign;

        if (std::abs(pp_alpha_pred) < AUTO_PP_NEAR_ALPHA &&
            std::abs(e_lat_pred) < AUTO_PP_NEAR_ELAT) {
            raw_delta *= AUTO_PP_NEAR_STRAIGHT;
            g_state.status = "STRAIGHT";
        } else {
            g_state.status = "PP_TRACK";
        }

        double prev_cmd = g_state.delta_cmd;
        double filt_cmd = (1.0-AUTO_PP_FILTER)*prev_cmd + AUTO_PP_FILTER*raw_delta;
        double delta = prev_cmd + clampv(filt_cmd-prev_cmd, -AUTO_PP_RATE_DEG, AUTO_PP_RATE_DEG);
        delta = clampv(delta, -g_state.max_deg, g_state.max_deg);

        double target_speed = speed_schedule(std::abs(delta), std::abs(e_lat_pred), std::abs(pp_alpha_pred));

        g_state.last_e_lat = g_state.e_lat_filt;
        g_state.last_e_head_deg = g_state.pp_alpha_filt;
        g_state.pp_alpha_deg = pp_alpha_pred;
        g_state.delta_cmd = delta;
        g_state.target_deg = delta;
        g_state.v_ref_raw = target_speed;
        g_state.last_delta_for_speed = delta;
        g_state.braking = false;
    }
    double target; { std::scoped_lock lk(g_state_mtx); target=g_state.v_ref_raw; }
    update_speed_toward(target);
}

/* ================================================================
 *  Control loop (runs at CONTROL_HZ)
 * ================================================================*/
void control_loop(BleClient* ble) {
    double period = 1.0 / CONTROL_HZ;
    while (!g_stop.load()) {
        auto t0 = std::chrono::steady_clock::now();

        bool connected = ble && ble->is_connected();
        if (connected) {
            bool auto_on;
            { std::scoped_lock lk(g_state_mtx); auto_on=g_state.auto_enabled; }

            if (auto_on) {
                autonomy_step();
            } else {
                std::scoped_lock lk(g_state_mtx);
                g_state.status="AUTO_OFF"; g_state.braking=true;
                g_state.speed=0; g_state.target_deg=0;
                g_state.v_ref_raw=0; g_state.delta_cmd=0;
            }

            bool braking;
            { std::scoped_lock lk(g_state_mtx); braking=g_state.braking; }

            if (braking) {
                ble->write_gatt_char(WRITE_UUID, brake_packet());
                log_row(compute_abc());
            } else {
                {
                    std::scoped_lock lk(g_state_mtx);
                    double diff = g_state.target_deg - g_state.turn_deg;
                    double step = g_state.steer_smooth * period;
                    if (std::abs(diff) <= step) g_state.turn_deg = g_state.target_deg;
                    else g_state.turn_deg += std::copysign(step, diff);
                }
                auto abc = compute_abc();
                ble->write_gatt_char(WRITE_UUID,
                    build_packet(abc.a_sent, abc.b, abc.c));
                log_row(abc);
            }
        }

        double elapsed = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - t0).count();
        std::this_thread::sleep_for(
            std::chrono::duration<double>(std::max(0.0, period-elapsed)));
    }
}

/* ================================================================
 *  Vision + detection thread
 * ================================================================*/
void cv_thread() {
    CamThread cam(CAM_INDEX, FRAME_W, FRAME_H);
    cam.start();

    auto backSub = build_mog2();
    IMMTracker imm;

    std::optional<std::vector<cv::Point2d>> path_cl;
    {
        std::scoped_lock lk(g_state_mtx);
        path_cl = load_centerline("centerline.csv", g_state.flip_centerline);
        if (path_cl) {
            std::cout << "[CV] Loaded centerline: " << path_cl->size() << " pts\n";
            if (g_state.flip_centerline)
                std::cout << "[CV] Centerline direction flipped\n";
        }
    }

    int last_seg_i = 0;
    bool show_mask = false;
    std::optional<cv::Point2d> last_est;
    int roi_miss = 0, tick = 0, no_meas = 0;
    bool was_holding = false;
    std::vector<cv::Point2d> blend_buf;
    std::optional<double> prev_theta, last_good_theta;
    double prev_t = now_sec();

    std::cout << "[CV] Running -- M=centerline  R=log  A=auto  V=mask  "
                 "L=lighting  Q=quit\n";

    try { while (!g_stop.load()) {
        auto frame_opt = cam.read_latest(500);
        if (!frame_opt) continue;
        cv::Mat frame = *frame_opt;

        { std::scoped_lock lk(g_centerline_mtx);
          if (g_new_centerline) {
              path_cl = *g_new_centerline;
              last_seg_i = 0;
              g_new_centerline.reset();
          }
        }

        ++tick;
        double now = now_sec();
        double dt = clampv(now-prev_t, 1e-3, 0.2);
        prev_t = now;
        double dt_ms = dt*1000, t_ms = now*1000;

        cv::Mat work = preprocess_lighting(frame);
        cv::Mat fg_raw; backSub->apply(work, fg_raw);
        cv::Mat fg = clean_mask(fg_raw);
        int H = fg.rows, W = fg.cols;

        bool use_global = (!imm.inited()) || (roi_miss>=ROI_FAIL_GLOBAL) ||
                          (tick % GLOBAL_EVERY == 0);
        std::optional<cv::Rect> roi;
        std::string mode = use_global ? "GLOBAL" : "ROI";
        if (!use_global && last_est) {
            roi = make_roi(last_est->x, last_est->y, W, H);
            if (!roi) mode = "GLOBAL";
        }

        auto det = pick_contour(fg, roi);
        bool used_meas = false;
        int meas_cx=-1, meas_cy=-1;
        double kf_x=0,kf_y=0,kf_vx=0,kf_vy=0;

        if (det && det->motion_px >= MIN_MOTION_PX) {
            meas_cx=det->cx; meas_cy=det->cy;
            if (was_holding && RESET_VEL_HOLD) imm.init_at(meas_cx, meas_cy);
            auto x = imm.correct_always(meas_cx, meas_cy, dt);
            kf_x=x[0]; kf_y=x[1]; kf_vx=x[2]; kf_vy=x[3];
            used_meas=true; no_meas=0; roi_miss=0; was_holding=false;
        } else {
            ++roi_miss; ++no_meas;
            if (imm.inited()) {
                auto x = imm.hold();
                kf_x=x[0]; kf_y=x[1]; kf_vx=x[2]; kf_vy=x[3];
                if (no_meas >= HOLD_NO_MEAS) { kf_vx=0; kf_vy=0; }
                was_holding = true;
            }
        }
        if (imm.inited()) last_est = cv::Point2d(kf_x, kf_y);

        double blend_x=kf_x, blend_y=kf_y;
        std::string blend_src="KF_ONLY"; double a_used=0;
        double speed_px_s = std::hypot(kf_vx, kf_vy);
        double meas_alpha = adaptive_meas_alpha(speed_px_s);

        if (used_meas && imm.inited()) {
            double dx=meas_cx-kf_x, dy=meas_cy-kf_y;
            double d = std::hypot(dx,dy);
            if (d <= std::max(1.0, MEAS_GATE_PX)) {
                double trust = std::max(0.0, 1.0-d/std::max(1.0, MEAS_GATE_PX));
                double a = clampv(meas_alpha*trust+(1-trust)*(0.4*meas_alpha), 0.0, 1.0);
                blend_x=a*meas_cx+(1-a)*kf_x;
                blend_y=a*meas_cy+(1-a)*kf_y;
                blend_src="BLEND"; a_used=a;
            } else blend_src="KF_GATED";
        }

        double theta_rad = std::numeric_limits<double>::quiet_NaN();
        std::string theta_src="NONE"; double step_px=0;
        std::optional<double> inst_theta;

        if (speed_px_s >= MIN_SPEED_VEL_HEADING) {
            inst_theta = std::atan2(kf_vy, kf_vx);
            theta_src = "KF_VEL";
        } else {
            blend_buf.emplace_back(blend_x, blend_y);
            int max_len = std::max(2, MOTION_WINDOW+1);
            if (static_cast<int>(blend_buf.size()) > max_len)
                blend_buf.erase(blend_buf.begin(), blend_buf.end()-max_len);
            if (blend_buf.size() >= 2) {
                auto p0=blend_buf[blend_buf.size()-1-MOTION_WINDOW];
                auto p1=blend_buf.back();
                step_px = std::hypot(p1.x-p0.x, p1.y-p0.y);
                if (step_px >= std::max(0.1, MIN_STEP_DIST)) {
                    inst_theta = std::atan2(p1.y-p0.y, p1.x-p0.x);
                    theta_src = "MOTION";
                }
            }
        }

        if (inst_theta) {
            theta_rad = ema_angle(prev_theta, *inst_theta, adaptive_alpha(speed_px_s));
            prev_theta = theta_rad; last_good_theta = theta_rad;
        } else if (last_good_theta) {
            theta_rad = *last_good_theta; theta_src = "HOLD";
        }

        double theta_deg = std::isfinite(theta_rad) ? theta_rad*180/kPi
                           : std::numeric_limits<double>::quiet_NaN();
        double dt_p = speed_px_s >= MIN_SPEED_PREDICT
                          ? clampv(dt, PREDICT_DT_MIN, PREDICT_DT_MAX) : 0;
        double ctrl_x = blend_x + kf_vx*dt_p;
        double ctrl_y = blend_y + kf_vy*dt_p;

        double e_lateral = std::numeric_limits<double>::quiet_NaN();
        double e_heading_deg = std::numeric_limits<double>::quiet_NaN();
        std::optional<double> path_heading;
        std::optional<cv::Point2d> proj_pt, pp_target;
        double pp_alpha_deg_v = std::numeric_limits<double>::quiet_NaN();
        double pp_lookahead_px_v = 0;
        std::optional<double> pp_path_heading;

        if (path_cl && imm.inited()) { try {
            auto [nr, best_i] = nearest_on_polyline(
                cv::Point2d(ctrl_x, ctrl_y), *path_cl, last_seg_i, NEAREST_WIN);
            last_seg_i = best_i;
            path_heading = nr.heading; e_lateral = nr.e_lat; proj_pt = nr.proj;
            if (std::isfinite(theta_rad))
                e_heading_deg = wrap_pi(theta_rad - *path_heading)*180/kPi;
            pp_lookahead_px_v = dynamic_lookahead_px(speed_px_s);
            auto adv = advance_along_path(*path_cl, nr.seg_i, nr.proj, pp_lookahead_px_v);
            pp_target = adv.first; pp_path_heading = adv.second.second;
            if (pp_target) {
                double desired = std::atan2(pp_target->y-ctrl_y, pp_target->x-ctrl_x);
                double ref = std::isfinite(theta_rad) ? theta_rad : *path_heading;
                if (std::isfinite(ref))
                    pp_alpha_deg_v = wrap_pi(desired - ref)*180/kPi;
            }
        } catch (...) {} }

        { std::scoped_lock lk(g_vision_mtx);
          g_vision.t_ms=t_ms; g_vision.dt_ms=dt_ms; g_vision.mode=mode;
          g_vision.used_meas=used_meas; g_vision.holding=was_holding;
          g_vision.meas_cx=meas_cx; g_vision.meas_cy=meas_cy;
          g_vision.kf_x=kf_x; g_vision.kf_y=kf_y;
          g_vision.kf_vx=kf_vx; g_vision.kf_vy=kf_vy;
          g_vision.blend_x=blend_x; g_vision.blend_y=blend_y;
          g_vision.ctrl_x=ctrl_x; g_vision.ctrl_y=ctrl_y;
          g_vision.blend_src=blend_src; g_vision.blend_a=a_used;
          g_vision.speed_px_s=speed_px_s; g_vision.theta_deg=theta_deg;
          g_vision.theta_src=theta_src; g_vision.step_px=step_px;
          g_vision.mu_cv=imm.mu_cv(); g_vision.mu_ct=imm.mu_ct();
          g_vision.omega_deg_s=imm.omega_deg_s();
          g_vision.e_lat=e_lateral; g_vision.e_head_deg=e_heading_deg;
          g_vision.pp_target_x = pp_target ? pp_target->x : std::numeric_limits<double>::quiet_NaN();
          g_vision.pp_target_y = pp_target ? pp_target->y : std::numeric_limits<double>::quiet_NaN();
          g_vision.pp_alpha_deg = pp_alpha_deg_v;
          g_vision.pp_lookahead_px = pp_lookahead_px_v;
        }

        /* ── visualisation ──────────────────────────────────────── */
        cv::Mat out = frame.clone();
        draw_centerline(out, path_cl);
        if (!path_cl) cv::putText(out,"No centerline -- press M",
                                  cv::Point(10,H-20), cv::FONT_HERSHEY_SIMPLEX,
                                  0.7, cv::Scalar(0,80,255), 2);
        if (roi) cv::rectangle(out, *roi, cv::Scalar(255,255,0), 2);
        if (det) {
            cv::rectangle(out, det->bbox, cv::Scalar(0,0,255), 2);
            cv::circle(out, cv::Point(det->cx,det->cy), 5, cv::Scalar(0,0,255), -1);
        }
        if (imm.inited()) {
            cv::circle(out, cv::Point(std::lround(kf_x),std::lround(kf_y)), 7, cv::Scalar(0,255,0), 2);
            cv::circle(out, cv::Point(std::lround(blend_x),std::lround(blend_y)), 5, cv::Scalar(255,255,0), -1);
            if (std::isfinite(theta_rad))
                draw_arrow(out, blend_x, blend_y, theta_rad, HEADING_LEN, cv::Scalar(255,0,0), 4);
        }
        if (proj_pt && path_heading) {
            cv::circle(out, cv::Point(std::lround(proj_pt->x),std::lround(proj_pt->y)), 6, cv::Scalar(0,255,255), 2);
            draw_arrow(out, proj_pt->x, proj_pt->y, *path_heading, 60, cv::Scalar(0,255,255), 3);
        }
        if (pp_target) {
            cv::circle(out, cv::Point(std::lround(pp_target->x),std::lround(pp_target->y)), 6, cv::Scalar(255,0,255), -1);
            cv::line(out, cv::Point(std::lround(ctrl_x),std::lround(ctrl_y)),
                     cv::Point(std::lround(pp_target->x),std::lround(pp_target->y)),
                     cv::Scalar(255,0,255), 2, cv::LINE_AA);
            if (pp_path_heading && std::isfinite(*pp_path_heading))
                draw_arrow(out, pp_target->x, pp_target->y, *pp_path_heading, 45, cv::Scalar(255,0,255), 2);
        }

        State s; { std::scoped_lock lk(g_state_mtx); s=g_state; }
        std::string log_lbl = s.logging ? "LOG:ON" : "LOG:OFF";
        std::string auto_lbl = s.auto_enabled ? "AUTO:ON" : "AUTO:OFF";

        { std::ostringstream o;
          o << auto_lbl << " | " << s.status << " | dt=" << std::fixed
            << std::setprecision(1) << dt_ms << "ms | " << log_lbl
            << " | LIT:" << LightingMode::current()
            << " | flip:" << (s.flip_centerline ? 1 : 0);
          cv::putText(out, o.str(), cv::Point(10,30), cv::FONT_HERSHEY_SIMPLEX,
                      0.58, cv::Scalar(255,255,255), 2);
        }
        if (std::isfinite(e_lateral)) {
            std::ostringstream o;
            o << "e_lat=" << std::fixed << std::setprecision(1) << e_lateral
              << "px  e_head=" << e_heading_deg << "deg";
            cv::putText(out, o.str(), cv::Point(10,60), cv::FONT_HERSHEY_SIMPLEX,
                        0.63, cv::Scalar(0,255,255), 2);
        }
        if (std::isfinite(theta_deg)) {
            std::ostringstream o;
            o << "theta=" << std::fixed << std::setprecision(1) << theta_deg
              << "  speed=" << std::setprecision(0) << speed_px_s
              << "px/s  pp_alpha=" << std::setprecision(1) << pp_alpha_deg_v
              << "  LA=" << std::setprecision(0) << pp_lookahead_px_v
              << "  cmd_speed=" << s.v_ref_raw
              << "  cmd_delta=" << std::setprecision(1) << s.delta_cmd;
            cv::putText(out, o.str(), cv::Point(10,90), cv::FONT_HERSHEY_SIMPLEX,
                        0.57, cv::Scalar(255,255,0), 2);
        }

        cv::imshow("VisionRC_FINAL [M=centerline | R=log | A=auto | "
                   "V=mask | L=lighting | Q=quit]", out);
        if (show_mask) cv::imshow("fg_mask", fg);

        int key = cv::waitKey(1) & 0xff;
        if (key=='q'||key=='Q') { g_stop.store(true); break; }
        if (key=='v'||key=='V') {
            show_mask = !show_mask;
            if (!show_mask) cv::destroyWindow("fg_mask");
        }
        if (key=='m'||key=='M') {
            bool flip; { std::scoped_lock lk(g_state_mtx); flip=g_state.flip_centerline; }
            auto np = build_centerline_from_frame(frame, flip);
            if (np) { std::scoped_lock lk(g_centerline_mtx); g_new_centerline = *np; }
        }
        if (key=='r'||key=='R') {
            bool is_logging; { std::scoped_lock lk(g_state_mtx); is_logging=g_state.logging; }
            if (is_logging) { stop_logging(); std::cout << "[LOG] Stopped\n"; }
            else { start_logging();
                   std::scoped_lock lk(g_state_mtx);
                   std::cout << "[LOG] Started -> " << g_state.log_path << "\n"; }
        }
        if (key=='a'||key=='A') {
            std::scoped_lock lk(g_state_mtx);
            g_state.auto_enabled = !g_state.auto_enabled;
            g_state.braking = !g_state.auto_enabled;
            if (!g_state.auto_enabled) {
                g_state.speed=0; g_state.target_deg=0;
                g_state.v_ref_raw=0; g_state.delta_cmd=0;
            }
            std::cout << "[AUTO] " << (g_state.auto_enabled?"ON":"OFF") << "\n";
        }
        if (key=='l'||key=='L') LightingMode::next();
    } } catch (const std::exception& e) {
        std::cerr << "[CV ERROR] " << e.what() << "\n";
    }

    cam.stop();
    cv::destroyAllWindows();
    std::cout << "[CV] Stopped\n";
    g_stop.store(true);
}

}  // namespace vrc
