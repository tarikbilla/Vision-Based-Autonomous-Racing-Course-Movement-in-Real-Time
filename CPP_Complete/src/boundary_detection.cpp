#include "boundary_detection.hpp"
#include <cmath>
#include <algorithm>
#include <iostream>
#include <numeric>
#include <stdexcept>

namespace {
constexpr double kPi = 3.14159265358979323846;

double wrapPi(double a) {
    while (a > kPi) a -= 2.0 * kPi;
    while (a < -kPi) a += 2.0 * kPi;
    return a;
}

cv::Mat buildFreeWhiteFromBgr(const cv::Mat& bgr, int inflatePx,
                              cv::Mat* outRoi = nullptr,
                              cv::Mat* outBarriers = nullptr,
                              cv::Mat* outObstaclesInflated = nullptr) {
    if (bgr.empty()) throw std::runtime_error("buildFreeWhiteFromBgr: empty frame");

    cv::Mat hsv;
    cv::cvtColor(bgr, hsv, cv::COLOR_BGR2HSV);
    int h = bgr.rows, w = bgr.cols;

    cv::Mat red1, red2, red;
    cv::inRange(hsv, cv::Scalar(0, 70, 50), cv::Scalar(12, 255, 255), red1);
    cv::inRange(hsv, cv::Scalar(168, 70, 50), cv::Scalar(179, 255, 255), red2);
    cv::bitwise_or(red1, red2, red);

    std::vector<cv::Point> pts;
    pts.reserve(10000);
    for (int y = 0; y < red.rows; ++y) {
        const uchar* p = red.ptr<uchar>(y);
        for (int x = 0; x < red.cols; ++x) {
            if (p[x] > 0) pts.emplace_back(x, y);
        }
    }
    if ((int)pts.size() < 50) {
        throw std::runtime_error("Not enough red pixels for ROI hull. Adjust red thresholds / lighting.");
    }

    std::vector<cv::Point> hull;
    cv::convexHull(pts, hull);

    cv::Mat roi(h, w, CV_8UC1, cv::Scalar(0));
    cv::fillConvexPoly(roi, hull, cv::Scalar(255));

    cv::Mat white;
    cv::inRange(hsv, cv::Scalar(0, 0, 180), cv::Scalar(179, 70, 255), white);
    cv::bitwise_and(white, roi, white);

    cv::Mat barriers;
    cv::bitwise_or(red, white, barriers);
    cv::bitwise_and(barriers, roi, barriers);

    cv::morphologyEx(barriers, barriers, cv::MORPH_OPEN,
        cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(5, 5)), cv::Point(-1, -1), 1);

    cv::morphologyEx(barriers, barriers, cv::MORPH_CLOSE,
        cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(7, 7)), cv::Point(-1, -1), 3);

    cv::dilate(barriers, barriers,
        cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(9, 9)), cv::Point(-1, -1), 1);

    cv::Mat notRoi, walls;
    cv::bitwise_not(roi, notRoi);
    cv::bitwise_or(barriers, notRoi, walls);

    cv::Mat flood = walls.clone();
    cv::Mat ffmask(h + 2, w + 2, CV_8UC1, cv::Scalar(0));
    cv::floodFill(flood, ffmask, cv::Point(0, 0), cv::Scalar(128));

    cv::Mat outsideMask;
    cv::compare(flood, 128, outsideMask, cv::CMP_EQ);

    cv::Mat walls0, notOutside, insideFree;
    cv::compare(walls, 0, walls0, cv::CMP_EQ);
    cv::bitwise_not(outsideMask, notOutside);
    cv::bitwise_and(notOutside, walls0, insideFree);

    cv::Mat freeWhite = insideFree.clone();

    if (outObstaclesInflated) {
        cv::Mat obstaclesFromFreeWhite;
        cv::bitwise_not(freeWhite, obstaclesFromFreeWhite);

        int k = inflatePx * 2 + 1;
        cv::Mat obstaclesInflated;
        cv::dilate(obstaclesFromFreeWhite, obstaclesInflated,
            cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(k, k)), cv::Point(-1, -1), 1);
        *outObstaclesInflated = obstaclesInflated;
    }

    if (outRoi) *outRoi = roi;
    if (outBarriers) *outBarriers = barriers;

    return freeWhite;
}

cv::Mat keepLargestComponent(const cv::Mat& bin255) {
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

cv::Mat fillSmallHoles(const cv::Mat& bin255, int maxHoleArea) {
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

        bool touchesBorder = (x == 0) || (y == 0) || (x + ww >= w) || (y + hh >= h);
        if (touchesBorder) continue;

        if (area <= maxHoleArea) out.setTo(255, labels == lab);
    }
    return out;
}

cv::Mat cleanTrackMask(const cv::Mat& freeWhite,
                       int closeK, int closeIter,
                       int openK, int openIter,
                       int maxHoleArea) {
    cv::Mat m;
    cv::threshold(freeWhite, m, 0, 255, cv::THRESH_BINARY);

    m = keepLargestComponent(m);

    cv::Mat kC = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(closeK, closeK));
    cv::morphologyEx(m, m, cv::MORPH_CLOSE, kC, cv::Point(-1, -1), closeIter);

    m = fillSmallHoles(m, maxHoleArea);

    cv::Mat kO = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(openK, openK));
    cv::morphologyEx(m, m, cv::MORPH_OPEN, kO, cv::Point(-1, -1), openIter);

    m = keepLargestComponent(m);

    return m;
}

double dist2p(const cv::Point2f& a, const cv::Point2f& b) {
    double dx = a.x - b.x, dy = a.y - b.y;
    return dx * dx + dy * dy;
}

std::vector<cv::Point2f> contourToPoint2f(const std::vector<cv::Point>& c) {
    std::vector<cv::Point2f> out;
    out.reserve(c.size());
    for (const auto& p : c) out.emplace_back((float)p.x, (float)p.y);
    return out;
}

std::vector<cv::Point2f> resampleClosedByArclength(const std::vector<cv::Point2f>& pts, int N) {
    if ((int)pts.size() < 2 || N < 2) return pts;

    std::vector<cv::Point2f> closed = pts;
    closed.push_back(pts.front());

    std::vector<double> s(closed.size(), 0.0);
    for (size_t i = 1; i < closed.size(); ++i) {
        double dx = closed[i].x - closed[i - 1].x;
        double dy = closed[i].y - closed[i - 1].y;
        s[i] = s[i - 1] + std::sqrt(dx * dx + dy * dy);
    }
    double L = s.back();
    if (L < 1e-6) return pts;

    std::vector<cv::Point2f> out;
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

std::vector<cv::Point2f> smoothClosedMovingAverage(const std::vector<cv::Point2f>& pts, int win) {
    if ((int)pts.size() < 3) return pts;
    if (win < 3) return pts;
    if (win % 2 == 0) win += 1;

    int n = (int)pts.size();
    int k = win / 2;
    std::vector<cv::Point2f> out(n);

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

std::vector<cv::Point2f> buildCenterlinePoints(const cv::Mat& freeWhiteClean,
                                               int samples,
                                               int smoothWin,
                                               int closeK,
                                               int closeIters) {
    cv::Mat road255;
    cv::threshold(freeWhiteClean, road255, 127, 255, cv::THRESH_BINARY);

    cv::Mat roadS = road255.clone();
    cv::Mat k = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(closeK, closeK));
    cv::morphologyEx(roadS, roadS, cv::MORPH_CLOSE, k, cv::Point(-1, -1), closeIters);

    std::vector<std::vector<cv::Point>> contours;
    std::vector<cv::Vec4i> hierarchy;
    cv::findContours(roadS, contours, hierarchy, cv::RETR_CCOMP, cv::CHAIN_APPROX_NONE);

    if (contours.empty() || hierarchy.empty()) {
        throw std::runtime_error("Centerline: no contours/hierarchy found");
    }

    int outer = -1;
    double bestArea = -1.0;
    for (int i = 0; i < (int)contours.size(); ++i) {
        int parent = hierarchy[i][3];
        if (parent != -1) continue;
        double a = std::fabs(cv::contourArea(contours[i]));
        if (a > bestArea) { bestArea = a; outer = i; }
    }
    if (outer < 0) throw std::runtime_error("Centerline: no outer contour found");

    int inner = -1;
    double bestInnerArea = -1.0;
    int child = hierarchy[outer][2];
    while (child != -1) {
        double a = std::fabs(cv::contourArea(contours[child]));
        if (a > bestInnerArea) { bestInnerArea = a; inner = child; }
        child = hierarchy[child][0];
    }

    if (inner < 0) {
        std::vector<int> idx(contours.size());
        std::iota(idx.begin(), idx.end(), 0);
        std::sort(idx.begin(), idx.end(), [&](int a, int b) {
            return std::fabs(cv::contourArea(contours[a])) > std::fabs(cv::contourArea(contours[b]));
        });
        if (idx.size() < 2) throw std::runtime_error("Centerline: could not find inner contour");
        inner = idx[1];
    }

    std::vector<cv::Point2f> outerPts = contourToPoint2f(contours[outer]);
    std::vector<cv::Point2f> innerPts = contourToPoint2f(contours[inner]);

    std::vector<cv::Point2f> outerR = resampleClosedByArclength(outerPts, samples);

    std::vector<cv::Point2f> innerNN;
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

    std::vector<cv::Point2f> center;
    center.reserve(outerR.size());
    for (size_t i = 0; i < outerR.size(); ++i) {
        center.push_back((outerR[i] + innerNN[i]) * 0.5f);
    }

    std::vector<cv::Point2f> centerS = smoothClosedMovingAverage(center, smoothWin);
    std::vector<cv::Point2f> centerClosed = centerS;
    centerClosed.push_back(centerS.front());
    return centerClosed;
}

int closestIndex(const std::vector<cv::Point2f>& path, double x, double y) {
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

int closestIndexWindowForward(const std::vector<cv::Point2f>& path,
                              int centerIdx,
                              int behind, int ahead,
                              double x, double y) {
    int n = (int)path.size();
    if (n == 0) return 0;

    int best = centerIdx;
    double bestd = 1e300;

    for (int k = -behind; k <= ahead; ++k) {
        int i = (centerIdx + k) % n;
        if (i < 0) i += n;

        double dx = path[i].x - x;
        double dy = path[i].y - y;
        double d2 = dx * dx + dy * dy;

        if (d2 < bestd) { bestd = d2; best = i; }
    }
    return best;
}

int lookaheadIndexLoop(const std::vector<cv::Point2f>& path, int startIdx, double x, double y, double ld) {
    int n = (int)path.size();
    if (n == 0) return 0;

    double ld2 = ld * ld;
    int i = (startIdx % n + n) % n;

    for (int k = 0; k < n; ++k) {
        const auto& p = path[i];
        double dx = p.x - x;
        double dy = p.y - y;
        if (dx * dx + dy * dy >= ld2) return i;
        i = (i + 1) % n;
    }
    return startIdx;
}

double purePursuitDelta(double x, double y, double theta, const cv::Point2f& target,
                        double wheelbaseL, double ld) {
    double angleToTarget = std::atan2(target.y - y, target.x - x);
    double alpha = wrapPi(angleToTarget - theta);
    return std::atan2(2.0 * wheelbaseL * std::sin(alpha), ld);
}

int deltaToSteeringByte(double delta, double deltaMaxRad) {
    double u = (deltaMaxRad > 1e-9) ? (delta / deltaMaxRad) : 0.0;
    u = std::clamp(u, -1.0, 1.0);

    int pct = (int)llround(std::abs(u) * 100.0);
    pct = std::clamp(pct, 0, 100);

    if (pct < 3) return 0;

    if (u > 0) {
        return pct;
    }
    return 255 - pct;
}
}

BoundaryDetector::BoundaryDetector(
    int blackThreshold,
    const std::vector<int>& rayAnglesDeg,
    int rayMaxLength,
    int evasiveDistance,
    int defaultSpeed,
    int steeringLimit,
    bool lightOn
)
    : blackThreshold_(blackThreshold),
      rayAnglesDeg_(rayAnglesDeg),
      rayMaxLength_(rayMaxLength),
      evasiveDistance_(evasiveDistance),
      defaultSpeed_(defaultSpeed),
      steeringLimit_(steeringLimit),
      lightOn_(lightOn),
      lastHeading_(0.0) {
        speedCap_ = defaultSpeed_;
}

BoundaryDetector::~BoundaryDetector() {
}

double BoundaryDetector::headingFromMovement(const cv::Point& movement) {
    if (movement.x == 0 && movement.y == 0) {
        return lastHeading_;
    }
    lastHeading_ = std::atan2(movement.y, movement.x) * 180.0 / M_PI;
    return lastHeading_;
}

int BoundaryDetector::castRay(const cv::Mat& gray, const cv::Point& origin, double angleDeg) {
    int height = gray.rows;
    int width = gray.cols;
    double angleRad = angleDeg * M_PI / 180.0;
    
    for (int dist = 1; dist <= rayMaxLength_; ++dist) {
        int x = origin.x + static_cast<int>(dist * std::cos(angleRad));
        int y = origin.y + static_cast<int>(dist * std::sin(angleRad));
        
        if (x < 0 || x >= width || y < 0 || y >= height) {
            return dist;
        }
        
        if (gray.at<uint8_t>(y, x) < blackThreshold_) {
            return dist;
        }
    }
    
    return rayMaxLength_;
}

std::tuple<int, int, int, cv::Mat> BoundaryDetector::detectRoadEdges(const cv::Mat& frame) {
    if (frame.empty()) {
        return std::make_tuple(-1, -1, -1, cv::Mat());
    }
    
    // Convert to grayscale for simple boundary detection (per PRD)
    cv::Mat gray;
    cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
    
    // Create binary mask: BLACK_THRESHOLD < pixel < 255 = road (white)
    cv::Mat roadMask;
    cv::threshold(gray, roadMask, blackThreshold_, 255, cv::THRESH_BINARY);
    
    // Morphological cleanup
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(5, 5));
    cv::morphologyEx(roadMask, roadMask, cv::MORPH_CLOSE, kernel, cv::Point(-1, -1), 2);
    cv::morphologyEx(roadMask, roadMask, cv::MORPH_OPEN, kernel, cv::Point(-1, -1), 1);
    
    // Find the road center by scanning from left and right
    int height = frame.rows;
    int width = frame.cols;
    int center_y = height / 2;
    
    int left_x = -1, right_x = -1;
    
    // Scan from left to find road edge
    for (int x = 0; x < width; ++x) {
        if (roadMask.at<uint8_t>(center_y, x) > 127) {
            left_x = x;
            break;
        }
    }
    
    // Scan from right to find road edge
    for (int x = width - 1; x >= 0; --x) {
        if (roadMask.at<uint8_t>(center_y, x) > 127) {
            right_x = x;
            break;
        }
    }
    
    int center_x = -1;
    if (left_x != -1 && right_x != -1) {
        center_x = (left_x + right_x) / 2;
    }
    
    return std::make_tuple(left_x, center_x, right_x, roadMask);
}

bool BoundaryDetector::buildCenterlineFromFrame(const cv::Mat& frame) {
    try {
        const int inflatePx = 18;
        const int cleanCloseK = 31;
        const int cleanCloseIter = 1;
        const int cleanOpenK = 7;
        const int cleanOpenIter = 1;
        const int cleanMaxHoleArea = 5000;
        const int centerSamples = 1200;
        const int centerSmoothWin = 31;
        const int centerCloseK = 7;
        const int centerCloseIters = 2;

        cv::Mat freeWhite = buildFreeWhiteFromBgr(frame, inflatePx);
        cv::Mat cleaned = cleanTrackMask(freeWhite, cleanCloseK, cleanCloseIter,
                                         cleanOpenK, cleanOpenIter, cleanMaxHoleArea);
        std::vector<cv::Point2f> centerline = buildCenterlinePoints(cleaned,
                                                                    centerSamples,
                                                                    centerSmoothWin,
                                                                    centerCloseK,
                                                                    centerCloseIters);

        trackMask_ = cleaned;
        centerline_ = std::move(centerline);
        hasCenterline_ = !centerline_.empty();
        hasPathIndex_ = false;
        return hasCenterline_;
    } catch (const std::exception&) {
        return false;
    }
}

std::pair<std::vector<RayResult>, ControlVector> BoundaryDetector::analyze(
    const cv::Mat& frame,
    const cv::Point& carPos,
    const cv::Point& movement
) {
    if (hasCenterline_) {
        std::vector<RayResult> emptyRays;
        return std::make_pair(emptyRays, analyzeWithCenterline(carPos, movement));
    }
    // Convert to grayscale for ray casting (per PRD: detect boundaries as dark pixels)
    cv::Mat gray;
    if (frame.channels() == 3) {
        cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
    } else {
        gray = frame;
    }
    
    int height = gray.rows;
    int width = gray.cols;
    
    // Calculate car heading from movement vector
    double heading = headingFromMovement(movement);
    
    // Cast three rays: -60°, 0°, +60° (relative to car heading)
    std::vector<RayResult> rays;
    std::vector<int> rayDistances;
    
    for (int rayAngle : rayAnglesDeg_) {
        double absoluteAngle = heading + rayAngle;
        int distance = castRay(gray, carPos, absoluteAngle);
        rays.push_back(RayResult{rayAngle, distance});
        rayDistances.push_back(distance);
    }
    
    // Determine steering based on ray distances (PRD: evasive action if closest < evasive_distance)
    int closestDist = *std::min_element(rayDistances.begin(), rayDistances.end());
    int leftRayIdx = 0;   // -60°
    int centerRayIdx = 1; // 0°
    int rightRayIdx = 2;  // +60°
    
    int leftDist = rayDistances[leftRayIdx];
    int centerDist = rayDistances[centerRayIdx];
    int rightDist = rayDistances[rightRayIdx];
    
    int leftTurn = 0;
    int rightTurn = 0;
    int speed = defaultSpeed_;
    
    // Evasive action: if any ray is too close to boundary
    if (closestDist < evasiveDistance_) {
        speed = std::max(5, defaultSpeed_ / 2); // Reduce speed
        
        if (leftDist < rightDist) {
            // Obstacle on left, turn right
            rightTurn = std::min(steeringLimit_, (evasiveDistance_ - leftDist) * 1);
        } else if (rightDist < leftDist) {
            // Obstacle on right, turn left
            leftTurn = std::min(steeringLimit_, (evasiveDistance_ - rightDist) * 1);
        } else {
            // Obstacle ahead, try to go around based on which side has more space
            if (rightDist > leftDist) {
                rightTurn = steeringLimit_;
            } else {
                leftTurn = steeringLimit_;
            }
        }
    } else {
        // Normal navigation: steer based on center ray
        // If center ray is much shorter than sides, there's a turn ahead
        if (centerDist < (leftDist + rightDist) / 2) {
            // Turn based on which side has more space
            if (rightDist > leftDist) {
                rightTurn = std::min(steeringLimit_, (int)((leftDist - centerDist) * 0.7));
            } else {
                leftTurn = std::min(steeringLimit_, (int)((rightDist - centerDist) * 0.7));
            }
        }
        // Maintain normal speed
        speed = defaultSpeed_;
    }
    
    ControlVector control(
        lightOn_,
        std::clamp(speed, 0, 255),
        std::clamp(rightTurn, 0, 255),
        std::clamp(leftTurn, 0, 255)
    );
    
    return std::make_pair(rays, control);
}

ControlVector BoundaryDetector::analyzeWithCenterline(const cv::Point& carCenter,
                                                      const cv::Point& movement) {
    if (!hasCenterline_ || centerline_.empty()) {
        return ControlVector(lightOn_, 0, 0, 0);
    }

    double headingDeg = headingFromMovement(movement);
    double theta = headingDeg * kPi / 180.0;

    if (!hasPathIndex_) {
        lastPathIndex_ = closestIndex(centerline_, (double)carCenter.x, (double)carCenter.y);
        hasPathIndex_ = true;
    } else {
        lastPathIndex_ = closestIndexWindowForward(centerline_, lastPathIndex_, 10, 120,
                                                   (double)carCenter.x, (double)carCenter.y);
    }

    int targetIdx = lookaheadIndexLoop(centerline_, lastPathIndex_,
                                       (double)carCenter.x, (double)carCenter.y,
                                       lookaheadPx_);

    const auto& target = centerline_[targetIdx];
    double deltaMaxRad = deltaMaxDeg_ * kPi / 180.0;
    double delta = purePursuitDelta((double)carCenter.x, (double)carCenter.y, theta,
                                    target, wheelbasePx_, lookaheadPx_);
    delta = std::clamp(delta, -deltaMaxRad, deltaMaxRad);
    int steerByte = deltaToSteeringByte(delta, deltaMaxRad);

    auto steerStrength = [](int steerByteVal) -> double {
        if (steerByteVal == 0) return 0.0;
        if (steerByteVal > 0 && steerByteVal <= 100) {
            return std::clamp(steerByteVal / 100.0, 0.0, 1.0);
        }
        int pct = 255 - steerByteVal;
        return std::clamp(pct / 100.0, 0.0, 1.0);
    };

    double strength = steerStrength(steerByte);
    double slow = std::clamp(turnSlowK_ * strength, 0.0, 0.7);
    int baseSpeed = defaultSpeed_;
    int speed = (int)std::lround(baseSpeed * (1.0 - slow));
    speed = std::clamp(speed, minSpeed_, std::max(minSpeed_, baseSpeed));
    if (speedCap_ > 0) {
        speed = std::min(speed, speedCap_);
    }

    int rightTurn = 0;
    int leftTurn = 0;
    if (steerByte > 0 && steerByte <= 100) {
        rightTurn = steerByte;
    } else if (steerByte >= 155 && steerByte <= 255) {
        leftTurn = 255 - steerByte;
    }

    return ControlVector(lightOn_, speed, rightTurn, leftTurn);
}
