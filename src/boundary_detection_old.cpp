#include "boundary_detection.hpp"
#include <cmath>
#include <algorithm>
#include <iostream>

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
    if (frame.empty() || frame.channels() != 3) {
        return std::make_tuple(-1, -1, -1, cv::Mat());
    }
    
    int height = frame.rows;
    int width = frame.cols;
    
    // Convert to grayscale
    cv::Mat gray;
    cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
    
    cv::Mat blurred;
    cv::GaussianBlur(gray, blurred, cv::Size(5, 5), 0);
    
    // Adaptive threshold
    cv::Mat binary;
    cv::adaptiveThreshold(blurred, binary, 255,
                     cv::ADAPTIVE_THRESH_GAUSSIAN_C,
                     cv::THRESH_BINARY_INV,
                     11, 2);
    
    // Morphology
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(2, 2));
    cv::morphologyEx(binary, binary, cv::MORPH_OPEN, kernel);
    cv::morphologyEx(binary, binary, cv::MORPH_CLOSE, kernel);
    
    // Find contours
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(binary.clone(), contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    
    if (contours.empty()) {
        return std::make_tuple(-1, -1, -1, cv::Mat());
    }
    
    // Sort by area
    std::sort(contours.begin(), contours.end(),
              [](const std::vector<cv::Point>& a, const std::vector<cv::Point>& b) {
                  return cv::contourArea(a) > cv::contourArea(b);
              });
    
    // Filter by area and get boundaries
    std::vector<std::vector<cv::Point>> largeContours;
    for (const auto& c : contours) {
        double area = cv::contourArea(c);
        if (area >= 700) {
            largeContours.push_back(c);
        }
    }
    
    if (largeContours.size() < 2) {
        return std::make_tuple(-1, -1, -1, cv::Mat());
    }
    
    // Get outer and inner boundaries
    auto& outerBoundary = largeContours[0];
    auto& innerBoundary = largeContours[1];
    
    // Get extremes
    int leftEdgeX = INT_MAX;
    int rightEdgeX = INT_MIN;
    
    for (const auto& p : outerBoundary) {
        leftEdgeX = std::min(leftEdgeX, p.x);
        rightEdgeX = std::max(rightEdgeX, p.x);
    }
    
    // Create masks
    cv::Mat outerMask(height, width, CV_8U, cv::Scalar(0));
    cv::Mat innerMask(height, width, CV_8U, cv::Scalar(0));
    cv::drawContours(outerMask, std::vector<std::vector<cv::Point>>{outerBoundary}, 0, 255, -1);
    cv::drawContours(innerMask, std::vector<std::vector<cv::Point>>{innerBoundary}, 0, 255, -1);
    
    // Road area
    cv::Mat roadMask;
    cv::bitwise_xor(outerMask, innerMask, roadMask);
    
    // Calculate center
    int centerX = (leftEdgeX + rightEdgeX) / 2;
    int midY = height / 2;
    
    // Find center by midpoint of road
    for (int x = leftEdgeX; x < rightEdgeX; ++x) {
        if (roadMask.at<uint8_t>(midY, x) > 0) {
            centerX = (leftEdgeX + rightEdgeX) / 2;
            break;
        }
    }
    
    // Validate
    int minSeparation = std::max(50, static_cast<int>(width * 0.15));
    if (rightEdgeX - leftEdgeX >= minSeparation) {
        lastRoadMask_ = roadMask;
        return std::make_tuple(leftEdgeX, centerX, rightEdgeX, roadMask);
    }
    
    return std::make_tuple(-1, -1, -1, cv::Mat());
}

cv::Rect BoundaryDetector::detectCarInFrame(const cv::Mat& frame, const cv::Mat& roadMask) {
    if (frame.empty() || frame.channels() != 3) {
        return cv::Rect(-1, -1, -1, -1);
    }
    
    int height = frame.rows;
    int width = frame.cols;
    
    // Convert to grayscale
    cv::Mat gray;
    cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
    
    cv::Mat blurred;
    cv::GaussianBlur(gray, blurred, cv::Size(5, 5), 0);
    
    // Adaptive threshold
    cv::Mat binary;
    cv::adaptiveThreshold(blurred, binary, 255,
                         cv::ADAPTIVE_THRESH_GAUSSIAN_C,
                     cv::THRESH_BINARY_INV,
                     11, 2);
    
    // Morphology
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(2, 2));
    cv::morphologyEx(binary, binary, cv::MORPH_OPEN, kernel);
    cv::morphologyEx(binary, binary, cv::MORPH_CLOSE, kernel);
    
    // Find contours
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(binary.clone(), contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    
    if (contours.empty()) {
        return cv::Rect(-1, -1, -1, -1);
    }
    
    // Find largest suitable contour
    for (const auto& c : contours) {
        double area = cv::contourArea(c);
        if (area >= 50 && area <= 10000) {
            return cv::boundingRect(c);
        }
    }
    
    return cv::Rect(-1, -1, -1, -1);
}

std::pair<std::vector<RayResult>, ControlVector> BoundaryDetector::analyze(
    const cv::Mat& frame,
    const cv::Point& carCenter,
    const cv::Point& carMovement) {
    
    std::vector<RayResult> rays;
    
    // Convert to grayscale for ray casting
    cv::Mat gray;
    cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
    
    // Cast rays
    for (int angleDeg : rayAnglesDeg_) {
        int distance = castRay(gray, carCenter, angleDeg);
        rays.push_back({angleDeg, distance});
    }
    
    // Determine heading
    double heading = headingFromMovement(carMovement);
    
    // Generate control vector based on ray results and heading
    ControlVector control;
    control.light_on = lightOn_;
    control.speed = defaultSpeed_;
    
    // Simple proportional control based on center ray
    if (rays.size() >= 3) {
        int centerDistance = rays[1].distance;
        int leftDistance = rays[0].distance;
        int rightDistance = rays[2].distance;
        
        if (centerDistance < evasiveDistance_) {
            control.speed = 0;
        }
        
        // Steering logic
        int steeringDiff = rightDistance - leftDistance;
        control.right_turn_value = std::min(steeringLimit_, std::max(-steeringLimit_, steeringDiff));
        control.left_turn_value = -control.right_turn_value;
    }
    
    return {rays, control};
}
