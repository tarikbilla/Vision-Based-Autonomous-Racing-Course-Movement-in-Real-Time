#include "car_detector.hpp"
#include <iostream>
#include <cmath>
#include <algorithm>
#include <optional>

namespace {
struct MotionCandidate {
    cv::Rect box;
    cv::Point center;
    double area = 0.0;
    double solidity = 0.0;
};

double contourSolidity(const std::vector<cv::Point>& cnt) {
    double a = std::fabs(cv::contourArea(cnt));
    if (a <= 1e-9) return 0.0;
    std::vector<cv::Point> hull;
    cv::convexHull(cnt, hull);
    double ha = std::fabs(cv::contourArea(hull));
    if (ha <= 1e-9) return 0.0;
    return a / ha;
}

std::optional<MotionCandidate> pickMotionCandidate(
    const std::vector<std::vector<cv::Point>>& contours,
    bool hasPrev,
    const cv::Point& prev,
    double minArea,
    double maxArea,
    int minWh,
    int maxWh,
    double minSolidity,
    double maxJumpPx
) {
    std::vector<MotionCandidate> cands;
    cands.reserve(contours.size());

    for (const auto& c : contours) {
        double area = std::fabs(cv::contourArea(c));
        if (area < minArea || area > maxArea) continue;

        cv::Rect r = cv::boundingRect(c);
        if (r.width < minWh || r.height < minWh) continue;
        if (r.width > maxWh || r.height > maxWh) continue;

        double ar = (r.height > 0) ? (double)r.width / (double)r.height : 99.0;
        if (ar < 0.35 || ar > 2.85) continue;

        cv::Moments m = cv::moments(c);
        if (std::fabs(m.m00) <= 1e-9) continue;

        int cx = (int)std::lround(m.m10 / m.m00);
        int cy = (int)std::lround(m.m01 / m.m00);

        double sol = contourSolidity(c);
        if (sol < minSolidity) continue;

        cands.push_back(MotionCandidate{r, cv::Point(cx, cy), area, sol});
    }

    if (cands.empty()) return std::nullopt;

    if (!hasPrev) {
        auto it = std::max_element(cands.begin(), cands.end(),
            [](const MotionCandidate& a, const MotionCandidate& b) { return a.area < b.area; });
        return *it;
    }

    std::optional<MotionCandidate> best;
    double bestScore = 1e300;

    for (const auto& t : cands) {
        double d = std::hypot((double)t.center.x - prev.x, (double)t.center.y - prev.y);
        if (d > maxJumpPx) continue;
        double score = d - 0.0008 * t.area;
        if (score < bestScore) {
            bestScore = score;
            best = t;
        }
    }

    if (!best) {
        auto it = std::max_element(cands.begin(), cands.end(),
            [](const MotionCandidate& a, const MotionCandidate& b) { return a.area < b.area; });
        best = *it;
    }

    return best;
}
}

CarDetector::CarDetector() {
    // Initialize MOG2 background subtractor for motion detection
    bgSubtractor_ = cv::createBackgroundSubtractorMOG2(500, 16, true);
    motionFrameCount_ = 0;
    lastDetectedCenter_ = cv::Point(-1, -1);
    frameCounter_ = 0;
}

CarDetector::~CarDetector() {
    bgSubtractor_.release();
}

SimpleCarDetection CarDetector::detectCar(const cv::Mat& frame, const cv::Point& lastKnownPosition) {
    frameCounter_++;
    
    if (frame.empty()) {
        return {false, cv::Point(-1, -1), cv::Rect(), cv::Point(0, 0), 0, "NONE"};
    }

    SimpleCarDetection result = {false, cv::Point(-1, -1), cv::Rect(), cv::Point(0, 0), 0, "NONE"};

    // Try centroid tracker first (most reliable for continuous tracking)
    if (centeroidTrackerInitialized_) {
        result = detectByCentroid(frame);
        if (result.detected && result.confidence > 60) {
            result.velocity = calculateVelocity(result.center, lastDetectedCenter_);
            lastDetectedCenter_ = result.center;
            return result;
        }
    }

    // Try HSV detection to initialize or re-init centroid tracker
    result = detectByHSV(frame);
    if (result.detected && result.confidence > 50) {
        // Initialize centroid tracker with this detection
        lastTrackedBox_ = result.bbox;
        centeroidTrackerInitialized_ = true;
        trackerFailureCount_ = 0;
        result.velocity = calculateVelocity(result.center, lastDetectedCenter_);
        lastDetectedCenter_ = result.center;
        return result;
    }

    // Fallback to motion detection
    result = detectByMotion(frame);
    if (result.detected && result.confidence > 40) {
        // Initialize centroid tracker with motion detection
        lastTrackedBox_ = result.bbox;
        centeroidTrackerInitialized_ = true;
        trackerFailureCount_ = 0;
        result.velocity = calculateVelocity(result.center, lastDetectedCenter_);
        lastDetectedCenter_ = result.center;
        return result;
    }

    // If still not detected, return last known position with low confidence
    result.detected = false;
    return result;
}

SimpleCarDetection CarDetector::detectByCentroid(const cv::Mat& frame) {
    // Simple centroid tracker - searches around last known position
    SimpleCarDetection result = {false, cv::Point(-1, -1), cv::Rect(), cv::Point(0, 0), 0, "CENTROID"};
    
    if (!centeroidTrackerInitialized_) {
        return result;
    }

    // Convert to HSV for color detection around last known area
    cv::Mat hsv;
    cv::cvtColor(frame, hsv, cv::COLOR_BGR2HSV);
    
    // Create search region (expand from last box)
    int expandX = lastTrackedBox_.width / 2;
    int expandY = lastTrackedBox_.height / 2;
    cv::Rect searchRegion = lastTrackedBox_;
    searchRegion.x = std::max(0, searchRegion.x - expandX);
    searchRegion.y = std::max(0, searchRegion.y - expandY);
    searchRegion.width = std::min(frame.cols - searchRegion.x, searchRegion.width + 2*expandX);
    searchRegion.height = std::min(frame.rows - searchRegion.y, searchRegion.height + 2*expandY);
    
    // Create masks for orange/red in search region
    cv::Mat hsv_roi = hsv(searchRegion);
    cv::Mat mask1, mask2, mask3, mask4;
    
    cv::inRange(hsv_roi, cv::Scalar(0, 50, 80), cv::Scalar(25, 255, 255), mask1);
    cv::inRange(hsv_roi, cv::Scalar(0, 30, 100), cv::Scalar(35, 255, 255), mask2);
    cv::inRange(hsv_roi, cv::Scalar(170, 40, 80), cv::Scalar(180, 255, 255), mask3);
    cv::inRange(hsv_roi, cv::Scalar(0, 20, 150), cv::Scalar(40, 200, 255), mask4);
    
    cv::Mat mask = mask1 | mask2 | mask3 | mask4;
    
    // Morphological cleanup
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(3, 3));
    cv::morphologyEx(mask, mask, cv::MORPH_CLOSE, kernel, cv::Point(-1, -1), 2);
    
    // Find contours
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(mask.clone(), contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    
    if (!contours.empty()) {
        // Find largest contour
        int largestIdx = 0;
        double largestArea = 0;
        for (int i = 0; i < contours.size(); i++) {
            double area = cv::contourArea(contours[i]);
            if (area > largestArea && area > 10) {  // Minimum area
                largestArea = area;
                largestIdx = i;
            }
        }
        
        cv::Rect trackBox = cv::boundingRect(contours[largestIdx]);
        // Convert back to full frame coordinates
        trackBox.x += searchRegion.x;
        trackBox.y += searchRegion.y;
        
        lastTrackedBox_ = trackBox;
        result.bbox = trackBox;
        result.center = cv::Point(trackBox.x + trackBox.width/2, trackBox.y + trackBox.height/2);
        result.detected = true;
        result.confidence = 75;
        trackerFailureCount_ = 0;
    } else {
        trackerFailureCount_++;
        if (trackerFailureCount_ > TRACKER_MAX_FAILURES) {
            centeroidTrackerInitialized_ = false;
        }
    }
    
    return result;
}

SimpleCarDetection CarDetector::detectByHSV(const cv::Mat& frame) {
    if (frame.empty()) {
        return {false, cv::Point(-1, -1), cv::Rect(), cv::Point(0, 0), 0, "HSV"};
    }

    // Convert to HSV
    cv::Mat hsv;
    cv::cvtColor(frame, hsv, cv::COLOR_BGR2HSV);

    // ULTRA AGGRESSIVE masks to catch ANY orange/red shade
    cv::Mat mask1, mask2, mask3, mask4;
    
    // Mask 1: Pure orange (0-25 hue range, any saturation)
    cv::inRange(hsv, cv::Scalar(0, 50, 80), cv::Scalar(25, 255, 255), mask1);
    
    // Mask 2: Orange-red (0-35 hue, low sat OK)
    cv::inRange(hsv, cv::Scalar(0, 30, 100), cv::Scalar(35, 255, 255), mask2);
    
    // Mask 3: Red wraparound (170-180 hue)
    cv::inRange(hsv, cv::Scalar(170, 40, 80), cv::Scalar(180, 255, 255), mask3);
    
    // Mask 4: Very bright orange (low sat, high value)
    cv::inRange(hsv, cv::Scalar(0, 20, 150), cv::Scalar(40, 200, 255), mask4);
    
    // Combine all masks
    cv::Mat mask = mask1 | mask2 | mask3 | mask4;

    // Aggressive morphological cleanup
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(3, 3));
    cv::morphologyEx(mask, mask, cv::MORPH_CLOSE, kernel, cv::Point(-1, -1), 2);
    cv::morphologyEx(mask, mask, cv::MORPH_OPEN, kernel, cv::Point(-1, -1), 1);
    cv::dilate(mask, mask, kernel, cv::Point(-1, -1), 2);

    lastHSVMask_ = mask.clone();

    // Save debug image if requested
    if (debugMode_ && saveDebugImages_ && frameCounter_ % 10 == 0) {
        cv::imwrite("debug_hsv_mask.png", mask);
        cv::imwrite("debug_hsv_frame.png", frame);
    }

    // Extract candidates
    auto candidates = extractCandidates(mask, "HSV");
    
    if (candidates.empty()) {
        return {false, cv::Point(-1, -1), cv::Rect(), cv::Point(0, 0), 0, "HSV"};
    }

    // Select best candidate
    cv::Rect best = selectBestCandidate(candidates, lastDetectedCenter_);
    cv::Point center(best.x + best.width / 2, best.y + best.height / 2);

    // Calculate confidence based on size and location
    int confidence = std::min(100, 30 + (int)(best.area() / 100.0));

    return {true, center, best, cv::Point(0, 0), confidence, "HSV"};
}

SimpleCarDetection CarDetector::detectByMotion(const cv::Mat& frame) {
    if (frame.empty()) {
        return {false, cv::Point(-1, -1), cv::Rect(), cv::Point(0, 0), 0, "MOTION"};
    }

    // Convert to grayscale
    cv::Mat gray;
    if (frame.channels() == 3) {
        cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
    } else {
        gray = frame.clone();
    }

    // Apply background subtraction
    cv::Mat motionMask;
    double learningRate = (motionFrameCount_ < MOTION_WARMUP_FRAMES) ? -1.0 : 0.01;
    bgSubtractor_->apply(gray, motionMask, learningRate);
    motionFrameCount_++;

    // Quick warmup - ready after 15 frames
    if (motionFrameCount_ <= 15) {
        return {false, cv::Point(-1, -1), cv::Rect(), cv::Point(0, 0), 0, "MOTION"};
    }

    lastMotionMask_ = motionMask.clone();

    // AGGRESSIVE threshold for motion
    cv::threshold(motionMask, motionMask, 80, 255, cv::THRESH_BINARY);
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(5, 5));
    cv::morphologyEx(motionMask, motionMask, cv::MORPH_CLOSE, kernel);
    cv::morphologyEx(motionMask, motionMask, cv::MORPH_OPEN, kernel);
    cv::dilate(motionMask, motionMask, kernel, cv::Point(-1, -1), 1);

    // Find contours and pick the best motion candidate
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(motionMask.clone(), contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    bool hasPrev = lastDetectedCenter_.x >= 0 && lastDetectedCenter_.y >= 0;
    auto cand = pickMotionCandidate(
        contours,
        hasPrev,
        lastDetectedCenter_,
        250.0,
        12000.0,
        10,
        220,
        0.25,
        220.0
    );

    if (!cand) {
        return {false, cv::Point(-1, -1), cv::Rect(), cv::Point(0, 0), 0, "MOTION"};
    }

    cv::Rect best = cand->box;
    cv::Point center = cand->center;
    int confidence = std::min(100, 40 + (int)(best.area() / 150.0));

    return {true, center, best, cv::Point(0, 0), confidence, "MOTION"};
}

SimpleCarDetection CarDetector::detectByTemplate(const cv::Mat& frame) {
    // Template matching - not implemented for now
    return {false, cv::Point(-1, -1), cv::Rect(), cv::Point(0, 0), 0, "TEMPLATE"};
}

std::vector<cv::Rect> CarDetector::extractCandidates(const cv::Mat& mask, const std::string& source) {
    std::vector<cv::Rect> candidates;
    
    // Find contours
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(mask.clone(), contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    if (contours.empty()) {
        return candidates;
    }

    // VERY RELAXED filtering - accept almost anything
    const double MIN_AREA = 15.0;       // VERY LOW
    const double MAX_AREA = 50000.0;    // VERY HIGH
    const int MIN_WIDTH = 4;
    const int MAX_WIDTH = 500;

    for (const auto& contour : contours) {
        double area = cv::contourArea(contour);
        
        if (area < MIN_AREA || area > MAX_AREA) continue;

        cv::Rect rect = cv::boundingRect(contour);
        
        if (rect.width < MIN_WIDTH || rect.width > MAX_WIDTH) continue;
        if (rect.height < MIN_WIDTH || rect.height > MAX_WIDTH) continue;

        // RELAXED aspect ratio (accept almost anything)
        double aspectRatio = (double)rect.width / (rect.height + 1e-6);
        if (aspectRatio < 0.15 || aspectRatio > 6.0) continue;

        candidates.push_back(rect);
    }

    return candidates;
}

cv::Rect CarDetector::selectBestCandidate(const std::vector<cv::Rect>& candidates, const cv::Point& lastPos) {
    if (candidates.empty()) {
        return cv::Rect();
    }

    // If we have a last position, prefer candidates close to it
    if (lastPos.x > 0 && lastPos.y > 0) {
        cv::Rect best = candidates[0];
        double minDist = std::hypot(
            best.x + best.width/2 - lastPos.x,
            best.y + best.height/2 - lastPos.y
        );

        for (const auto& candidate : candidates) {
            cv::Point candCenter(candidate.x + candidate.width/2, candidate.y + candidate.height/2);
            double dist = std::hypot(candCenter.x - lastPos.x, candCenter.y - lastPos.y);
            
            // Prefer closer candidates, but also consider size
            double score = dist - candidate.area() / 1000.0;
            
            double bestScore = minDist - best.area() / 1000.0;
            
            if (score < bestScore) {
                best = candidate;
                minDist = dist;
            }
        }
        return best;
    }

    // No last position, pick largest candidate
    return *std::max_element(candidates.begin(), candidates.end(),
        [](const cv::Rect& a, const cv::Rect& b) {
            return a.area() < b.area();
        });
}

cv::Point CarDetector::calculateVelocity(const cv::Point& current, const cv::Point& previous) {
    if (previous.x < 0 || previous.y < 0) {
        return cv::Point(0, 0);
    }
    return cv::Point(current.x - previous.x, current.y - previous.y);
}
