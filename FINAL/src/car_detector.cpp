/*──────────────────────────────────────────────────────────────────────
 *  car_detector.cpp  –  MOG2 + contour car detection & camera thread
 *──────────────────────────────────────────────────────────────────────*/
#include "car_detector.hpp"
#include "types.hpp"

#include <algorithm>
#include <chrono>
#include <iostream>

namespace vrc {

/* ================================================================
 *  CamThread – grab-only, latest-frame
 * ================================================================*/
CamThread::CamThread(int src, int w, int h) : src_(src), w_(w), h_(h) {}

void CamThread::start() {
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
    if (!opened) throw std::runtime_error("Cannot open camera");

    cap_.set(cv::CAP_PROP_FRAME_WIDTH, w_);
    cap_.set(cv::CAP_PROP_FRAME_HEIGHT, h_);
    cap_.set(cv::CAP_PROP_BUFFERSIZE, 1);
    cap_.set(cv::CAP_PROP_FOURCC,
             cv::VideoWriter::fourcc('M','J','P','G'));
    cap_.set(cv::CAP_PROP_AUTO_EXPOSURE, 0.25);
    cap_.set(cv::CAP_PROP_AUTO_WB, 0);

    th_ = std::thread([this]{ reader(); });
}

std::optional<cv::Mat> CamThread::read_latest(int timeout_ms) {
    std::unique_lock<std::mutex> lk(m_);
    if (!cv_.wait_for(lk, std::chrono::milliseconds(timeout_ms),
                      [&]{ return has_frame_ || stopped_; }))
        return std::nullopt;
    if (!has_frame_) return std::nullopt;
    has_frame_ = false;
    return latest_.clone();
}

void CamThread::stop() {
    { std::scoped_lock lk(m_); stopped_ = true; }
    cv_.notify_all();
    if (th_.joinable()) th_.join();
    cap_.release();
}

void CamThread::reader() {
    while (true) {
        { std::scoped_lock lk(m_); if (stopped_) break; }
        cv::Mat frame;
        if (!cap_.read(frame)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }
        { std::scoped_lock lk(m_); latest_ = frame; has_frame_ = true; }
        cv_.notify_one();
    }
}

/* ================================================================
 *  MOG2 helpers
 * ================================================================*/
cv::Ptr<cv::BackgroundSubtractorMOG2> build_mog2() {
    return cv::createBackgroundSubtractorMOG2(MOG2_HISTORY,
                                              MOG2_VAR_THRESH,
                                              MOG2_SHADOWS);
}

cv::Mat clean_mask(const cv::Mat& mask) {
    cv::Mat m;
    cv::threshold(mask, m, THRESH_FG, 255, cv::THRESH_BINARY);
    auto ko = cv::getStructuringElement(cv::MORPH_ELLIPSE,
                  cv::Size(OPEN_K, OPEN_K));
    auto kc = cv::getStructuringElement(cv::MORPH_ELLIPSE,
                  cv::Size(CLOSE_K, CLOSE_K));
    cv::morphologyEx(m, m, cv::MORPH_OPEN,  ko, cv::Point(-1,-1), OPEN_IT);
    cv::morphologyEx(m, m, cv::MORPH_CLOSE, kc, cv::Point(-1,-1), CLOSE_IT);
    return m;
}

/* ================================================================
 *  ROI helpers
 * ================================================================*/
std::optional<cv::Rect> clamp_roi(int x0,int y0,int x1,int y1,int W,int H) {
    x0 = clampv(x0,0,W-1);
    y0 = clampv(y0,0,H-1);
    x1 = clampv(x1,0,W);
    y1 = clampv(y1,0,H);
    if (x1<=x0 || y1<=y0) return std::nullopt;
    return cv::Rect(x0, y0, x1-x0, y1-y0);
}

std::optional<cv::Rect> make_roi(double px, double py, int W, int H) {
    return clamp_roi(static_cast<int>(px-ROI_HALF-ROI_MARGIN),
                     static_cast<int>(py-ROI_HALF-ROI_MARGIN),
                     static_cast<int>(px+ROI_HALF+ROI_MARGIN),
                     static_cast<int>(py+ROI_HALF+ROI_MARGIN), W, H);
}

/* ================================================================
 *  Contour picker
 * ================================================================*/
std::optional<Det> pick_contour(const cv::Mat& mask,
                                const std::optional<cv::Rect>& roi) {
    cv::Mat sub; int x0=0, y0=0;
    if (roi) { x0=roi->x; y0=roi->y; sub=mask(*roi); }
    else sub = mask;

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(sub, contours, cv::RETR_EXTERNAL,
                     cv::CHAIN_APPROX_SIMPLE);

    std::optional<Det> best;
    double best_area = 0;
    for (auto& cnt : contours) {
        double area = cv::contourArea(cnt);
        if (area < MIN_AREA || area > MAX_AREA) continue;
        cv::Rect r = cv::boundingRect(cnt);
        r.x += x0; r.y += y0;
        if (r.width<MIN_WH||r.height<MIN_WH||r.width>MAX_WH||r.height>MAX_WH)
            continue;
        if (area > best_area) {
            best_area = area;
            Det d;
            d.bbox = r;
            d.cx = r.x + r.width/2;
            d.cy = r.y + r.height/2;
            d.area = area;
            cv::Rect br(std::max(0,r.x), std::max(0,r.y),
                        std::min(mask.cols, r.x+r.width) - std::max(0,r.x),
                        std::min(mask.rows, r.y+r.height) - std::max(0,r.y));
            d.motion_px = cv::countNonZero(mask(br));
            best = d;
        }
    }
    return best;
}

}  // namespace vrc
