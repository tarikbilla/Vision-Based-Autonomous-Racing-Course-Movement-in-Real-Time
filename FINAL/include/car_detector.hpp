/*──────────────────────────────────────────────────────────────────────
 *  car_detector.hpp  –  MOG2 + contour car detection & camera thread
 *──────────────────────────────────────────────────────────────────────*/
#ifndef VISIONRC_CAR_DETECTOR_HPP
#define VISIONRC_CAR_DETECTOR_HPP

#include <opencv2/opencv.hpp>

#include <condition_variable>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

namespace vrc {

/* ── detection constants ───────────────────────────────────────── */
constexpr int    CAM_INDEX        = 0;
constexpr int    FRAME_W          = 1280;
constexpr int    FRAME_H          = 720;

constexpr int    MOG2_HISTORY     = 500;
constexpr double MOG2_VAR_THRESH  = 16.0;
constexpr bool   MOG2_SHADOWS     = false;
constexpr int    THRESH_FG        = 200;
constexpr int    OPEN_K           = 5;
constexpr int    CLOSE_K          = 7;
constexpr int    OPEN_IT          = 1;
constexpr int    CLOSE_IT         = 2;
constexpr double MIN_AREA         = 250.0;
constexpr double MAX_AREA         = 250000.0;
constexpr int    MIN_WH           = 10;
constexpr int    MAX_WH           = 900;
constexpr int    MIN_MOTION_PX    = 120;
constexpr int    ROI_HALF         = 180;
constexpr int    ROI_MARGIN       = 20;
constexpr int    ROI_FAIL_GLOBAL  = 10;
constexpr int    GLOBAL_EVERY     = 8;
constexpr int    HOLD_NO_MEAS     = 3;
constexpr bool   RESET_VEL_HOLD   = true;

/* ── single detection ──────────────────────────────────────────── */
struct Det {
    cv::Rect bbox;
    int  cx = -1, cy = -1;
    double area      = 0;
    int    motion_px = 0;
};

/* ── camera thread (grab-only, latest-frame model) ─────────────── */
class CamThread {
public:
    CamThread(int src, int w, int h);
    void start();
    std::optional<cv::Mat> read_latest(int timeout_ms);
    void stop();
private:
    void reader();
    int src_, w_, h_;
    cv::VideoCapture cap_;
    std::thread th_;
    std::mutex m_;
    std::condition_variable cv_;
    cv::Mat latest_;
    bool has_frame_ = false;
    bool stopped_   = false;
};

/* ── MOG2 helpers ──────────────────────────────────────────────── */
cv::Ptr<cv::BackgroundSubtractorMOG2> build_mog2();
cv::Mat clean_mask(const cv::Mat& mask);

/* ── ROI helpers ───────────────────────────────────────────────── */
std::optional<cv::Rect> clamp_roi(int x0,int y0,int x1,int y1,int W,int H);
std::optional<cv::Rect> make_roi(double px, double py, int W, int H);

/* ── contour picker ────────────────────────────────────────────── */
std::optional<Det> pick_contour(const cv::Mat& mask,
                                const std::optional<cv::Rect>& roi);

}  // namespace vrc

#endif  // VISIONRC_CAR_DETECTOR_HPP
