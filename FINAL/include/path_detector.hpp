/*──────────────────────────────────────────────────────────────────────
 *  path_detector.hpp  –  Centerline detection & path utilities
 *──────────────────────────────────────────────────────────────────────*/
#ifndef VISIONRC_PATH_DETECTOR_HPP
#define VISIONRC_PATH_DETECTOR_HPP

#include <opencv2/opencv.hpp>
#include <optional>
#include <string>
#include <vector>

#include "types.hpp"

namespace vrc {

/* ── centerline file I/O ───────────────────────────────────────── */
std::optional<std::vector<cv::Point2d>>
load_centerline(const std::string& path, bool flip);

void save_centerline(const std::string& path,
                     const std::vector<cv::Point2d>& pts, int W, int H);

/* ── build centerline from a single camera frame ───────────────── */
std::optional<std::vector<cv::Point2d>>
build_centerline_from_frame(const cv::Mat& frame, bool flip);

/* ── geometry helpers ──────────────────────────────────────────── */
std::vector<cv::Point2d> ensure_closed(std::vector<cv::Point2d> arr);

std::pair<NearestResult, int>
nearest_on_polyline(const cv::Point2d& p,
                    const std::vector<cv::Point2d>& path,
                    int last_i, int win);

std::pair<cv::Point2d, std::pair<int, double>>
advance_along_path(const std::vector<cv::Point2d>& path, int seg_i,
                   const cv::Point2d& proj_pt, double dist_px);

double dynamic_lookahead_px(double speed_px_s);

/* ── lighting pre-processing ──────────────────────────────────── */
class LightingMode {
public:
    static std::string current();
    static void        next();
private:
    static std::vector<std::string>& modes();
    static int& index();
};

cv::Mat preprocess_lighting(const cv::Mat& bgr);

/* ── drawing helpers ──────────────────────────────────────────── */
void draw_centerline(cv::Mat& img,
                     const std::optional<std::vector<cv::Point2d>>& path_cl);
void draw_arrow(cv::Mat& img, double x, double y, double theta,
                int length, const cv::Scalar& color, int thickness = 3);

}  // namespace vrc

#endif  // VISIONRC_PATH_DETECTOR_HPP
