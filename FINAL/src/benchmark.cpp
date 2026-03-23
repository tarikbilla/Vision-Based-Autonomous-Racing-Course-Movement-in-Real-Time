/*──────────────────────────────────────────────────────────────────────
 *  benchmark.cpp  –  Headless timing benchmark for VisionRC_FINAL
 *
 *  Runs the full per-frame pipeline (camera capture → MOG2 → contour
 *  detection → IMM update → pure-pursuit geometry) without displaying
 *  any OpenCV window or sending BLE packets.
 *
 *  Outputs two files:
 *    benchmark_frames_<timestamp>.csv   per-frame latency details
 *    benchmark_summary_<timestamp>.txt  human-readable summary
 *──────────────────────────────────────────────────────────────────────*/
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
#include <numeric>
#include <sstream>
#include <thread>
#include <vector>
#include <algorithm>
#include <opencv2/opencv.hpp>

using namespace vrc;
using clock_t2 = std::chrono::high_resolution_clock;
using us_t     = std::chrono::microseconds;

static double us_now() {
    return std::chrono::duration<double,std::micro>(
               clock_t2::now().time_since_epoch()).count();
}

/* ── stats helpers ─────────────────────────────────────────────── */
struct Stats {
    double min, max, mean, median, p95, p99, stddev;
};
static Stats compute_stats(std::vector<double> v) {
    if (v.empty()) return {};
    std::sort(v.begin(), v.end());
    double sum  = std::accumulate(v.begin(),v.end(),0.0);
    double mean = sum/v.size();
    double sq   = 0; for (auto x:v) sq+=(x-mean)*(x-mean);
    auto pct=[&](double p){ size_t i=std::min(v.size()-1,(size_t)(p/100.0*(v.size()-1)));return v[i]; };
    return {v.front(), v.back(), mean,
            pct(50), pct(95), pct(99),
            std::sqrt(sq/v.size())};
}
static std::string fmt_stat(const std::string& name, const Stats& s) {
    std::ostringstream o;
    o << std::fixed << std::setprecision(3);
    o << name << ":\n"
      << "  min=" << s.min << " max=" << s.max << " mean=" << s.mean
      << " median=" << s.median << "\n"
      << "  p95=" << s.p95 << " p99=" << s.p99 << " stddev=" << s.stddev << "\n";
    return o.str();
}

/* ── global state needed by path_detector helpers ──────────────── */
namespace vrc {
    State                                    g_state;
    std::mutex                               g_state_mtx;
    VisionData                               g_vision;
    std::mutex                               g_vision_mtx;
    std::atomic<bool>                        g_stop{false};
    std::mutex                               g_centerline_mtx;
    std::optional<std::vector<cv::Point2d>>  g_new_centerline;
}

int main(int argc, char** argv) {
    /* ── args ───────────────────────────────────────────────────── */
    int    run_frames  = 300;   // default: 300 frames
    int    cam_idx     = 0;
    double warmup_s    = 2.0;   // discard first N seconds for MOG2 warmup

    for (int i=1; i<argc; i++) {
        std::string a = argv[i];
        if (a=="--frames" && i+1<argc)  run_frames = std::stoi(argv[++i]);
        if (a=="--camera" && i+1<argc)  cam_idx    = std::stoi(argv[++i]);
        if (a=="--warmup" && i+1<argc)  warmup_s   = std::stod(argv[++i]);
    }

    /* ── timestamp for output files ─────────────────────────────── */
    auto now_wall = std::time(nullptr); std::tm tm{}; localtime_r(&now_wall,&tm);
    std::ostringstream ts_ss; ts_ss << std::put_time(&tm,"%Y%m%d_%H%M%S");
    std::string ts = ts_ss.str();
    std::string csv_path = "benchmark_frames_" + ts + ".csv";
    std::string txt_path = "benchmark_summary_" + ts + ".txt";

    std::cout << "══════════════════════════════════════════════════\n"
              << "  VisionRC_FINAL  —  Benchmark (" << run_frames << " frames)\n"
              << "══════════════════════════════════════════════════\n"
              << "  Camera index : " << cam_idx << "\n"
              << "  Warmup       : " << warmup_s << " s\n"
              << "  CSV output   : " << csv_path << "\n"
              << "  TXT output   : " << txt_path << "\n\n";

    /* ── open camera ────────────────────────────────────────────── */
    cv::VideoCapture cap;
#ifdef __APPLE__
    cap.open(cam_idx, cv::CAP_AVFOUNDATION);
#else
    cap.open(cam_idx, cv::CAP_V4L2);
#endif
    if (!cap.isOpened()) {
        cap.open(cam_idx);
        if (!cap.isOpened()) {
            std::cerr << "[BENCH] Cannot open camera " << cam_idx << "\n";
            return 1;
        }
    }
    cap.set(cv::CAP_PROP_FRAME_WIDTH,  1280);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, 720);
    cap.set(cv::CAP_PROP_FPS,          30);
    int W = (int)cap.get(cv::CAP_PROP_FRAME_WIDTH);
    int H = (int)cap.get(cv::CAP_PROP_FRAME_HEIGHT);
    std::cout << "  Resolution   : " << W << "×" << H << "\n\n";

    /* ── load centerline ────────────────────────────────────────── */
    std::optional<std::vector<cv::Point2d>> path_cl;
    {
        auto cl = load_centerline("centerline.csv", false);
        if (cl && !cl->empty()) {
            path_cl = cl;
            std::cout << "[BENCH] Centerline loaded: " << cl->size() << " pts\n";
        } else {
            std::cout << "[BENCH] No centerline.csv — path geometry skipped\n";
        }
    }

    /* ── build MOG2 ─────────────────────────────────────────────── */
    auto mog2 = build_mog2();

    /* ── IMM tracker ────────────────────────────────────────────── */
    IMMTracker imm;

    /* ── CSV header ─────────────────────────────────────────────── */
    std::ofstream csv(csv_path);
    csv << "frame_idx,t_wall_us,cap_us,mog2_us,contour_us,imm_us,"
           "path_us,total_us,det_found,kf_x,kf_y,e_lat,e_head_deg\n";

    /* ── warmup ─────────────────────────────────────────────────── */
    std::cout << "[BENCH] Warming up MOG2 (" << warmup_s << " s)...\n";
    double warmup_end = us_now() + warmup_s * 1e6;
    cv::Mat frame;
    while (us_now() < warmup_end) {
        if (!cap.read(frame)) break;
        cv::Mat gray; cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
        cv::equalizeHist(gray, gray);
        cv::Mat fg; mog2->apply(gray, fg);
        cv::Mat cleaned = clean_mask(fg);
    }
    std::cout << "[BENCH] Warmup done. Starting measurement...\n\n";

    /* ── storage for per-phase timings ──────────────────────────── */
    struct Row {
        int    idx;
        double t_wall;       // wall time (µs from start)
        double cap_us;       // camera grab
        double mog2_us;      // MOG2 apply
        double contour_us;   // contour filter
        double imm_us;       // IMM update
        double path_us;      // path geometry (nearest + lookahead)
        double total_us;     // sum of all
        bool   det;
        double kf_x, kf_y, e_lat, e_head;
    };
    std::vector<Row> rows;
    rows.reserve(run_frames);

    /* ── inter-frame timing ─────────────────────────────────────── */
    std::vector<double> inter_frame_us;
    double t_bench_start = us_now();
    double t_prev_frame  = t_bench_start;

    /* ── main loop ──────────────────────────────────────────────── */
    std::optional<cv::Rect> roi;
    int    last_seg_i = 0;
    int    roi_miss   = 0;

    for (int fi=0; fi<run_frames; fi++) {
        Row r{}; r.idx = fi;

        /* ── 1. capture ─────────────────────────────────────── */
        double t0 = us_now();
        if (!cap.read(frame) || frame.empty()) {
            std::cerr << "[BENCH] Frame grab failed at " << fi << "\n";
            break;
        }
        double t1 = us_now();
        r.cap_us   = t1 - t0;
        r.t_wall   = t1 - t_bench_start;
        inter_frame_us.push_back(t1 - t_prev_frame);
        t_prev_frame = t1;

        /* ── 2. MOG2 ─────────────────────────────────────────── */
        cv::Mat work; cv::resize(frame, work, cv::Size(W,H));
        cv::Mat gray; cv::cvtColor(work, gray, cv::COLOR_BGR2GRAY);
        cv::equalizeHist(gray, gray);
        double t2 = us_now();
        cv::Mat fg_full; mog2->apply(gray, fg_full);
        cv::Mat fg = clean_mask(fg_full);
        double t3 = us_now();
        r.mog2_us = t3 - t2;

        /* ── 3. contour detection ───────────────────────────── */
        auto det = pick_contour(fg, roi);
        double t4 = us_now();
        r.contour_us = t4 - t3;
        r.det = det.has_value();

        /* ── 4. IMM update ──────────────────────────────────────── */
        double dt_s = inter_frame_us.back() / 1e6;
        std::array<double,4> kf_state = {0,0,0,0};
        if (det) {
            roi      = make_roi(det->cx, det->cy, W, H);
            roi_miss = 0;
            kf_state = imm.correct_always(det->cx, det->cy, dt_s);
        } else {
            roi_miss++;
            if (roi_miss > 10) { roi = std::nullopt; roi_miss = 0; }
            if (imm.inited()) kf_state = imm.hold();
        }
        double t5 = us_now();
        r.imm_us = t5 - t4;
        // kf_state = [x, y, vx, vy]
        if (imm.inited()) { r.kf_x=kf_state[0]; r.kf_y=kf_state[1]; }

        /* ── 5. path geometry (nearest + lookahead) ─────────── */
        r.e_lat = r.e_head = std::numeric_limits<double>::quiet_NaN();
        if (path_cl && imm.inited()) {
            try {
                double px = kf_state[0], py = kf_state[1];
                double vx = kf_state[2], vy = kf_state[3];
                auto [nr, seg_i] = nearest_on_polyline(
                    cv::Point2d(px, py), *path_cl, last_seg_i, 60);
                last_seg_i = seg_i;
                r.e_lat = nr.e_lat;
                double spd = std::hypot(vx, vy);
                if (spd > 5.0 && std::isfinite(nr.heading)) {
                    double theta = std::atan2(vy, vx);
                    r.e_head = wrap_pi(theta - nr.heading)*180.0/kPi;
                }
            } catch (...) {}
        }
        double t6 = us_now();
        r.path_us  = t6 - t5;
        r.total_us = t6 - t1;  // exclude camera stall from "processing" total

        rows.push_back(r);

        /* ── progress ───────────────────────────────────────── */
        if (fi % 50 == 0)
            std::cout << "  frame " << std::setw(4) << fi << "/" << run_frames
                      << "  total=" << std::fixed << std::setprecision(2)
                      << r.total_us/1000.0 << " ms\n";
    }
    cap.release();

    double t_bench_total = us_now() - t_bench_start;
    int    n = (int)rows.size();
    std::cout << "\n[BENCH] Done. " << n << " frames in "
              << std::fixed << std::setprecision(2) << t_bench_total/1e6 << " s\n\n";

    /* ── write CSV ──────────────────────────────────────────────── */
    for (auto& r : rows) {
        csv << r.idx << ","
            << std::fixed << std::setprecision(1) << r.t_wall << ","
            << std::setprecision(2) << r.cap_us << ","
            << r.mog2_us << ","
            << r.contour_us << ","
            << r.imm_us << ","
            << r.path_us << ","
            << r.total_us << ","
            << (r.det?1:0) << ","
            << std::setprecision(3) << r.kf_x << ","
            << r.kf_y << ",";
        if (std::isfinite(r.e_lat)) csv << r.e_lat; else csv << "nan";
        csv << ",";
        if (std::isfinite(r.e_head)) csv << r.e_head; else csv << "nan";
        csv << "\n";
    }
    csv.close();
    std::cout << "[BENCH] CSV written: " << csv_path << "\n";

    /* ── compute statistics ─────────────────────────────────────── */
    auto col = [&](auto fn) {
        std::vector<double> v; v.reserve(n);
        for (auto& r:rows) v.push_back(fn(r));
        return v;
    };

    auto s_cap     = compute_stats(col([](const Row& r){ return r.cap_us/1000.0; }));
    auto s_mog2    = compute_stats(col([](const Row& r){ return r.mog2_us/1000.0; }));
    auto s_cont    = compute_stats(col([](const Row& r){ return r.contour_us/1000.0; }));
    auto s_imm     = compute_stats(col([](const Row& r){ return r.imm_us/1000.0; }));
    auto s_path    = compute_stats(col([](const Row& r){ return r.path_us/1000.0; }));
    auto s_total   = compute_stats(col([](const Row& r){ return r.total_us/1000.0; }));
    auto s_inter   = compute_stats([&]{ std::vector<double> v; for(auto x:inter_frame_us) v.push_back(x/1000.0); return v; }());

    double fps_actual = n / (t_bench_total/1e6);
    long   det_count  = std::count_if(rows.begin(),rows.end(),[](const Row& r){return r.det;});
    double det_rate   = 100.0*det_count/n;

    /* ── build summary text ─────────────────────────────────────── */
    std::ostringstream sum;
    sum << "══════════════════════════════════════════════════════════════\n"
        << "  VisionRC_FINAL  —  Benchmark Summary\n"
        << "  Date/Time : " << ts << "\n"
        << "  Frames    : " << n << "\n"
        << "  Duration  : " << std::fixed << std::setprecision(3)
                             << t_bench_total/1e6 << " s\n"
        << "  Actual FPS: " << std::setprecision(2) << fps_actual << "\n"
        << "  Detection : " << det_count << "/" << n
                             << "  (" << std::setprecision(1) << det_rate << "%)\n"
        << "══════════════════════════════════════════════════════════════\n\n"
        << "All values in milliseconds (ms)\n\n"
        << "─── Per-phase latency ────────────────────────────────────────\n"
        << fmt_stat("Camera grab (cap_ms)",        s_cap)
        << fmt_stat("MOG2 apply  (mog2_ms)",       s_mog2)
        << fmt_stat("Contour det (contour_ms)",    s_cont)
        << fmt_stat("IMM update  (imm_ms)",         s_imm)
        << fmt_stat("Path geom   (path_ms)",        s_path)
        << fmt_stat("Processing total (total_ms)", s_total)
        << "\n─── Frame interval ───────────────────────────────────────────\n"
        << fmt_stat("Inter-frame (inter_ms) — inverse = FPS", s_inter)
        << "\n─── Derived metrics ──────────────────────────────────────────\n";

    sum << std::fixed << std::setprecision(2);
    sum << "  Mean camera-to-output latency    : "
        << s_total.mean   << " ms  (mean total)\n"
        << "  p95 camera-to-output latency     : "
        << s_total.p95    << " ms\n"
        << "  p99 camera-to-output latency     : "
        << s_total.p99    << " ms\n"
        << "  BLE control loop period          : "
        << 1000.0/20.0    << " ms  (1/20 Hz fixed)\n"
        << "  Estimated cam→BLE cmd latency    : "
        << s_total.mean + 1000.0/20.0 << " ms\n"
        << "  Effective tracking FPS           : "
        << 1000.0/s_inter.mean << " fps  (from inter-frame)\n"
        << "  Actual benchmark FPS             : "
        << fps_actual << " fps\n"
        << "══════════════════════════════════════════════════════════════\n";

    std::cout << sum.str();

    std::ofstream txt(txt_path);
    txt << sum.str();
    txt.close();
    std::cout << "[BENCH] Summary written: " << txt_path << "\n";

    return 0;
}
