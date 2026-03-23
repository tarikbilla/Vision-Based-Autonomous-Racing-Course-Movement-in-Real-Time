/*──────────────────────────────────────────────────────────────────────
 *  path_detector.cpp  –  Centerline detection, path geometry,
 *                         lighting pre-processing, drawing helpers
 *──────────────────────────────────────────────────────────────────────*/
#include "path_detector.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <numeric>
#include <sstream>
#include <stdexcept>

namespace vrc {

/* ── constants for map building ───────────────────────────────── */
static constexpr int MAP_N_SAMPLES        = 1200;
static constexpr int MAP_SMOOTH_WIN       = 31;
static constexpr int MAP_CLEAN_CLOSE_K    = 31;
static constexpr int MAP_CLEAN_CLOSE_ITER = 1;
static constexpr int MAP_CLEAN_OPEN_K     = 7;
static constexpr int MAP_CLEAN_OPEN_ITER  = 1;
static constexpr int MAP_CLEAN_MAX_HOLE   = 5000;
static constexpr int MAP_CENTER_CLOSE_K   = 7;
static constexpr int MAP_CENTER_CLOSE_ITER= 2;

/* ================================================================
 *  Lighting pre-processing
 * ================================================================*/
std::vector<std::string>& LightingMode::modes() {
    static std::vector<std::string> v =
        {"AUTO_CLAHE","NONE","EQUALIZE","GAMMA_BRIGHT","GAMMA_DARK"};
    return v;
}
int& LightingMode::index() { static int i = 0; return i; }

std::string LightingMode::current() { return modes()[index()]; }
void LightingMode::next() {
    index() = (index() + 1) % static_cast<int>(modes().size());
    std::cout << "[CV] Lighting mode -> " << current() << "\n";
}

cv::Mat preprocess_lighting(const cv::Mat& bgr) {
    const auto mode = LightingMode::current();
    if (mode == "NONE") return bgr.clone();

    if (mode == "EQUALIZE") {
        cv::Mat lab; cv::cvtColor(bgr, lab, cv::COLOR_BGR2Lab);
        std::vector<cv::Mat> ch; cv::split(lab, ch);
        cv::equalizeHist(ch[0], ch[0]);
        cv::merge(ch, lab);
        cv::Mat out; cv::cvtColor(lab, out, cv::COLOR_Lab2BGR);
        return out;
    }
    if (mode == "GAMMA_BRIGHT" || mode == "GAMMA_DARK") {
        double g = mode == "GAMMA_BRIGHT" ? 0.5 : 1.8;
        cv::Mat lut(1, 256, CV_8U);
        for (int i = 0; i < 256; ++i)
            lut.at<uint8_t>(0, i) =
                static_cast<uint8_t>(std::pow(i / 255.0, g) * 255.0);
        cv::Mat out; cv::LUT(bgr, lut, out);
        return out;
    }
    // AUTO_CLAHE
    cv::Mat lab; cv::cvtColor(bgr, lab, cv::COLOR_BGR2Lab);
    std::vector<cv::Mat> ch; cv::split(lab, ch);
    auto clahe = cv::createCLAHE(3.0, cv::Size(8, 8));
    clahe->apply(ch[0], ch[0]);
    cv::merge(ch, lab);
    cv::Mat out; cv::cvtColor(lab, out, cv::COLOR_Lab2BGR);
    return out;
}

/* ================================================================
 *  Centerline I/O
 * ================================================================*/
std::vector<cv::Point2d> ensure_closed(std::vector<cv::Point2d> arr) {
    if (arr.size() < 2) return arr;
    auto& a = arr.front(); auto& b = arr.back();
    if (std::hypot(a.x - b.x, a.y - b.y) > 1e-6) arr.push_back(a);
    return arr;
}

std::optional<std::vector<cv::Point2d>>
load_centerline(const std::string& path, bool flip) {
    if (!std::filesystem::exists(path)) return std::nullopt;
    std::ifstream f(path); if (!f) return std::nullopt;

    std::vector<cv::Point2d> pts;
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#' || line[0] == 'x' || line[0]=='X')
            continue;
        std::stringstream ss(line);
        std::string sx, sy;
        if (!std::getline(ss, sx, ',')) continue;
        if (!std::getline(ss, sy, ',')) continue;
        try { pts.emplace_back(std::stod(sx), std::stod(sy)); }
        catch (...) {}
    }
    if (pts.size() < 2) return std::nullopt;

    auto arr = ensure_closed(pts);
    if (flip) { std::reverse(arr.begin(), arr.end()); arr = ensure_closed(arr); }
    return arr;
}

void save_centerline(const std::string& path,
                     const std::vector<cv::Point2d>& pts, int W, int H) {
    std::ofstream f(path, std::ios::out | std::ios::trunc);
    f << "# W=" << W << " H=" << H << "\n" << "x,y\n";
    for (auto& p : pts)
        f << std::fixed << std::setprecision(3) << p.x << ',' << p.y << '\n';
}

/* ================================================================
 *  Geometry helpers
 * ================================================================*/
std::pair<NearestResult, int>
nearest_on_polyline(const cv::Point2d& p,
                    const std::vector<cv::Point2d>& path,
                    int last_i, int win) {
    int nseg = static_cast<int>(path.size()) - 1;
    double best_d2 = 1e18;
    std::optional<NearestResult> best;
    int best_i = ((last_i % std::max(1, nseg)) + nseg) % nseg;

    for (int off = -win; off <= win; ++off) {
        int i = (last_i + off) % nseg;
        if (i < 0) i += nseg;
        auto a = path[i], b = path[i+1];
        auto ab = b - a;
        double ab2 = ab.dot(ab);
        if (ab2 < 1e-9) continue;
        double t = clampv((p - a).dot(ab) / ab2, 0.0, 1.0);
        auto proj = a + t * ab;
        auto d = p - proj;
        double d2 = d.dot(d);
        if (d2 < best_d2) {
            best_d2 = d2;
            double heading = std::atan2(ab.y, ab.x);
            double cross = ab.x*(p.y-a.y) - ab.y*(p.x-a.x);
            best = NearestResult{i, heading,
                                 (cross>=0?1.0:-1.0)*std::sqrt(d2), proj};
            best_i = i;
        }
    }
    if (!best.has_value()) throw std::runtime_error("nearest search failed");
    return {*best, best_i};
}

std::pair<cv::Point2d, std::pair<int, double>>
advance_along_path(const std::vector<cv::Point2d>& path, int seg_i,
                   const cv::Point2d& proj_pt, double dist_px) {
    int nseg = static_cast<int>(path.size()) - 1;
    if (nseg <= 0)
        return {proj_pt, {seg_i, std::numeric_limits<double>::quiet_NaN()}};

    double dist_left = std::max(0.0, dist_px);
    cv::Point2d cur = proj_pt;
    int i = ((seg_i % nseg) + nseg) % nseg;

    for (int k = 0; k < nseg + 1; ++k) {
        auto nxt = path[i+1];
        auto seg = nxt - cur;
        double seg_len = std::hypot(seg.x, seg.y);
        if (seg_len < 1e-9) { i = (i+1) % nseg; cur = path[i]; continue; }
        if (dist_left <= seg_len) {
            double t = dist_left / seg_len;
            auto pt = cur + t * seg;
            return {pt, {i, std::atan2(seg.y, seg.x)}};
        }
        dist_left -= seg_len;
        i = (i+1) % nseg;
        cur = path[i];
    }
    return {cur, {i, std::atan2(path[i+1].y-path[i].y,
                                path[i+1].x-path[i].x)}};
}

double dynamic_lookahead_px(double speed_px_s) {
    return clampv(88.0 + 0.78 * speed_px_s, 88.0, 170.0);
}

/* ================================================================
 *  Drawing helpers
 * ================================================================*/
void draw_arrow(cv::Mat& img, double x, double y, double theta,
                int length, const cv::Scalar& color, int thickness) {
    int x2 = static_cast<int>(std::lround(x + std::cos(theta) * length));
    int y2 = static_cast<int>(std::lround(y + std::sin(theta) * length));
    cv::arrowedLine(img,
        cv::Point(static_cast<int>(std::lround(x)),
                  static_cast<int>(std::lround(y))),
        cv::Point(x2, y2), color, thickness, cv::LINE_AA, 0, 0.25);
}

void draw_centerline(cv::Mat& img,
                     const std::optional<std::vector<cv::Point2d>>& path_cl) {
    if (!path_cl || path_cl->size() < 2) return;
    std::vector<cv::Point> pts;
    pts.reserve(path_cl->size());
    for (auto& p : *path_cl)
        pts.emplace_back(static_cast<int>(std::lround(p.x)),
                         static_cast<int>(std::lround(p.y)));
    cv::polylines(img, std::vector<std::vector<cv::Point>>{pts},
                  true, cv::Scalar(0, 200, 60), 2, cv::LINE_AA);
}

/* ================================================================
 *  Track mask helpers
 * ================================================================*/
static cv::Mat keep_largest(const cv::Mat& mask255) {
    cv::Mat labels, stats, cents;
    int n = cv::connectedComponentsWithStats(mask255 > 0, labels, stats, cents, 8);
    if (n <= 1) return mask255.clone();
    int best = 1, best_area = stats.at<int>(1, cv::CC_STAT_AREA);
    for (int i = 2; i < n; ++i) {
        int a = stats.at<int>(i, cv::CC_STAT_AREA);
        if (a > best_area) { best_area = a; best = i; }
    }
    cv::Mat out = cv::Mat::zeros(mask255.size(), mask255.type());
    out.setTo(255, labels == best);
    return out;
}

static cv::Mat fill_holes(const cv::Mat& mask255, int max_hole) {
    cv::Mat inv; cv::bitwise_not(mask255, inv);
    cv::Mat labels, stats, cents;
    int n = cv::connectedComponentsWithStats(inv > 0, labels, stats, cents, 8);
    cv::Mat out = mask255.clone();
    int h = mask255.rows, w = mask255.cols;
    for (int lab = 1; lab < n; ++lab) {
        int area = stats.at<int>(lab, cv::CC_STAT_AREA);
        int x = stats.at<int>(lab, cv::CC_STAT_LEFT);
        int y = stats.at<int>(lab, cv::CC_STAT_TOP);
        int ww = stats.at<int>(lab, cv::CC_STAT_WIDTH);
        int hh = stats.at<int>(lab, cv::CC_STAT_HEIGHT);
        if (x==0||y==0||x+ww>=w||y+hh>=h) continue;
        if (area <= max_hole) out.setTo(255, labels == lab);
    }
    return out;
}

static cv::Mat build_free_white(const cv::Mat& bgr) {
    int h = bgr.rows, w = bgr.cols;
    cv::Mat hsv; cv::cvtColor(bgr, hsv, cv::COLOR_BGR2HSV);

    cv::Mat red1, red2, red;
    cv::inRange(hsv, cv::Scalar(0,70,50), cv::Scalar(12,255,255), red1);
    cv::inRange(hsv, cv::Scalar(168,70,50), cv::Scalar(179,255,255), red2);
    cv::bitwise_or(red1, red2, red);

    std::vector<cv::Point> pts;
    cv::findNonZero(red, pts);
    if (pts.size() < 50)
        throw std::runtime_error("Not enough red pixels");

    std::vector<cv::Point> hull;
    cv::convexHull(pts, hull);
    cv::Mat roi_mask = cv::Mat::zeros(h, w, CV_8U);
    cv::fillConvexPoly(roi_mask, hull, 255);

    cv::Mat white;
    cv::inRange(hsv, cv::Scalar(0,0,180), cv::Scalar(179,70,255), white);
    cv::bitwise_and(white, roi_mask, white);

    cv::Mat barriers; cv::bitwise_or(red, white, barriers);
    cv::bitwise_and(barriers, roi_mask, barriers);

    auto e5 = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(5,5));
    auto e7 = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(7,7));
    auto e9 = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(9,9));
    cv::morphologyEx(barriers, barriers, cv::MORPH_OPEN,  e5, cv::Point(-1,-1), 1);
    cv::morphologyEx(barriers, barriers, cv::MORPH_CLOSE, e7, cv::Point(-1,-1), 3);
    cv::dilate(barriers, barriers, e9, cv::Point(-1,-1), 1);

    cv::Mat inv_roi; cv::bitwise_not(roi_mask, inv_roi);
    cv::Mat walls; cv::bitwise_or(barriers, inv_roi, walls);

    cv::Mat flood = walls.clone();
    cv::Mat ffmask = cv::Mat::zeros(h+2, w+2, CV_8U);
    cv::floodFill(flood, ffmask, cv::Point(0,0), cv::Scalar(128));

    cv::Mat outside = (flood == 128);
    outside.convertTo(outside, CV_8U, 255.0);
    cv::Mat not_outside; cv::bitwise_not(outside, not_outside);
    cv::Mat walls_zero = (walls == 0);
    walls_zero.convertTo(walls_zero, CV_8U, 255.0);

    cv::Mat out; cv::bitwise_and(not_outside, walls_zero, out);
    return out;
}

static cv::Mat clean_track_mask(const cv::Mat& free_white) {
    cv::Mat m; cv::threshold(free_white, m, 0, 255, cv::THRESH_BINARY);
    m = keep_largest(m);
    auto kC = cv::getStructuringElement(cv::MORPH_ELLIPSE,
                  cv::Size(MAP_CLEAN_CLOSE_K, MAP_CLEAN_CLOSE_K));
    cv::morphologyEx(m, m, cv::MORPH_CLOSE, kC, cv::Point(-1,-1),
                     MAP_CLEAN_CLOSE_ITER);
    m = fill_holes(m, MAP_CLEAN_MAX_HOLE);
    auto kO = cv::getStructuringElement(cv::MORPH_ELLIPSE,
                  cv::Size(MAP_CLEAN_OPEN_K, MAP_CLEAN_OPEN_K));
    cv::morphologyEx(m, m, cv::MORPH_OPEN, kO, cv::Point(-1,-1),
                     MAP_CLEAN_OPEN_ITER);
    return keep_largest(m);
}

static std::vector<cv::Point2d> resample(const std::vector<cv::Point2d>& pts,
                                         int N) {
    auto closed = pts;
    closed.push_back(pts.front());
    std::vector<double> s; s.reserve(closed.size()); s.push_back(0.0);
    for (size_t i = 1; i < closed.size(); ++i)
        s.push_back(s.back() + std::hypot(closed[i].x-closed[i-1].x,
                                          closed[i].y-closed[i-1].y));
    double L = s.back();
    if (L < 1e-6) return pts;

    std::vector<cv::Point2d> out; out.reserve(N);
    for (int i = 0; i < N; ++i) {
        double t = (L * i) / N;
        auto it = std::upper_bound(s.begin(), s.end(), t);
        size_t idx = static_cast<size_t>(std::distance(s.begin(), it));
        idx = std::min(idx, s.size()-1);
        size_t i0 = idx == 0 ? 0 : idx - 1;
        double den = std::max(1e-9, s[idx] - s[i0]);
        double a = (t - s[i0]) / den;
        out.push_back(closed[i0] * (1.0-a) + closed[idx] * a);
    }
    return out;
}

static std::vector<cv::Point2d> smooth_cyclic(const std::vector<cv::Point2d>& pts,
                                              int win) {
    int n = static_cast<int>(pts.size());
    if (n < 3 || win < 3) return pts;
    win |= 1; int k = win / 2;

    std::vector<cv::Point2d> padded;
    padded.reserve(n + 2*k);
    for (int i = n-k; i < n; ++i) padded.push_back(pts[i]);
    for (auto& p : pts) padded.push_back(p);
    for (int i = 0; i < k; ++i) padded.push_back(pts[i]);

    std::vector<cv::Point2d> out; out.reserve(n);
    for (int i = 0; i < n; ++i) {
        cv::Point2d acc(0,0);
        for (int j = 0; j < win; ++j) acc += padded[i+j];
        out.push_back(acc * (1.0 / win));
    }
    return out;
}

static std::vector<cv::Point2d> build_centerline_points(const cv::Mat& free_clean) {
    cv::Mat road; cv::threshold(free_clean, road, 127, 255, cv::THRESH_BINARY);
    auto k = cv::getStructuringElement(cv::MORPH_ELLIPSE,
                 cv::Size(MAP_CENTER_CLOSE_K, MAP_CENTER_CLOSE_K));
    cv::morphologyEx(road, road, cv::MORPH_CLOSE, k, cv::Point(-1,-1),
                     MAP_CENTER_CLOSE_ITER);

    std::vector<std::vector<cv::Point>> contours;
    std::vector<cv::Vec4i> hierarchy;
    cv::findContours(road, contours, hierarchy,
                     cv::RETR_CCOMP, cv::CHAIN_APPROX_NONE);
    if (contours.empty() || hierarchy.empty())
        throw std::runtime_error("No contours in track mask");

    int outer = -1; double max_outer = -1;
    for (size_t i = 0; i < hierarchy.size(); ++i) {
        if (hierarchy[i][3] == -1) {
            double a = std::abs(cv::contourArea(contours[i]));
            if (a > max_outer) { max_outer = a; outer = static_cast<int>(i); }
        }
    }
    if (outer < 0) throw std::runtime_error("Cannot find outer contour");

    int inner = -1; double best_a = -1;
    int child = hierarchy[outer][2];
    while (child != -1) {
        double a = std::abs(cv::contourArea(contours[child]));
        if (a > best_a) { best_a = a; inner = child; }
        child = hierarchy[child][0];
    }
    if (inner < 0) {
        std::vector<int> idx(contours.size());
        std::iota(idx.begin(), idx.end(), 0);
        std::sort(idx.begin(), idx.end(), [&](int a, int b) {
            return std::abs(cv::contourArea(contours[a])) >
                   std::abs(cv::contourArea(contours[b]));
        });
        if (idx.size() < 2) throw std::runtime_error("Cannot find inner contour");
        inner = idx[1];
    }

    std::vector<cv::Point2d> outer_pts, inner_pts;
    for (auto& p : contours[outer]) outer_pts.emplace_back(p.x, p.y);
    for (auto& p : contours[inner]) inner_pts.emplace_back(p.x, p.y);

    auto outer_r = resample(outer_pts, MAP_N_SAMPLES);
    std::vector<cv::Point2d> center; center.reserve(outer_r.size());
    for (auto& op : outer_r) {
        double best_d2 = std::numeric_limits<double>::infinity();
        cv::Point2d bestp;
        for (auto& ip : inner_pts) {
            double d2 = (op - ip).dot(op - ip);
            if (d2 < best_d2) { best_d2 = d2; bestp = ip; }
        }
        center.push_back((op + bestp) * 0.5);
    }

    auto smooth = smooth_cyclic(center, MAP_SMOOTH_WIN);
    smooth.push_back(smooth.front());
    return smooth;
}

std::optional<std::vector<cv::Point2d>>
build_centerline_from_frame(const cv::Mat& frame, bool flip) {
    std::cout << "[MAP] Building...\n";
    try {
        auto work = preprocess_lighting(frame);
        auto free_white = build_free_white(work);
        auto free_clean = clean_track_mask(free_white);
        auto pts = build_centerline_points(free_clean);
        if (flip) {
            std::reverse(pts.begin(), pts.end());
            pts = ensure_closed(pts);
        }
        save_centerline("centerline.csv", pts, frame.cols, frame.rows);
        std::cout << "[MAP] Done — " << pts.size() << " pts -> centerline.csv\n";
        return pts;
    } catch (const std::exception& e) {
        std::cerr << "[MAP ERROR] " << e.what() << "\n";
        return std::nullopt;
    }
}

}  // namespace vrc
