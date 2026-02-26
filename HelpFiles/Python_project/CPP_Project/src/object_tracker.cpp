/**
 * @file object_tracker.cpp
 * @brief Implementation of object tracking using OpenCV trackers (GOTURN, CSRT, KCF, MOSSE)
 */

#include "object_tracker.h"
#include <iostream>
#include <cmath>
#include <sstream>
#include <algorithm>

namespace rc_car {

ObjectTracker::ObjectTracker() 
    : tracker_type_(TrackerType::CSRT), initialized_(false) {
}

ObjectTracker::ObjectTracker(TrackerType type)
    : tracker_type_(type), initialized_(false) {
}

ObjectTracker::~ObjectTracker() {
    tracker_.release();
}

cv::Ptr<cv::Tracker> ObjectTracker::createTracker(TrackerType type) {
    switch (type) {
        case TrackerType::GOTURN:
            return cv::TrackerGOTURN::create();
        case TrackerType::CSRT:
            return cv::TrackerCSRT::create();
        case TrackerType::KCF:
            return cv::TrackerKCF::create();
        case TrackerType::MOSSE:
            // MOSSE tracker was removed in OpenCV 4.5.1+, fallback to KCF
            std::cerr << "Warning: MOSSE tracker not available in OpenCV 4.10.0, using KCF instead" << std::endl;
            return cv::TrackerKCF::create();
        default:
            std::cerr << "Warning: Unknown tracker type, using CSRT" << std::endl;
            return cv::TrackerCSRT::create();
    }
}

std::string ObjectTracker::trackerTypeToString(TrackerType type) {
    switch (type) {
        case TrackerType::GOTURN: return "GOTURN";
        case TrackerType::CSRT: return "CSRT";
        case TrackerType::KCF: return "KCF";
        case TrackerType::MOSSE: return "MOSSE";
        default: return "UNKNOWN";
    }
}

bool ObjectTracker::initialize(const cv::Mat& frame, const cv::Rect2d& bbox, TrackerType type) {
    tracker_type_ = type;
    bbox_ = bbox;
    
    tracker_ = createTracker(type);
    if (tracker_.empty()) {
        std::cerr << "Error: Failed to create tracker: " << trackerTypeToString(type) << std::endl;
        initialized_ = false;
        return false;
    }
    
    // In OpenCV 4.x, init() returns void, not bool
    try {
        tracker_->init(frame, bbox);
    } catch (const cv::Exception& e) {
        std::cerr << "Error: Failed to initialize tracker: " << e.what() << std::endl;
        initialized_ = false;
        return false;
    }
    
    // Initialize midpoint history
    midpoints_.clear();
    Position midpoint(static_cast<int>(bbox.x + bbox.width / 2),
                      static_cast<int>(bbox.y + bbox.height / 2));
    midpoints_.push_back(midpoint);
    
    initialized_ = true;
    std::cout << "Tracker initialized: " << trackerTypeToString(type) << std::endl;
    
    return true;
}

bool ObjectTracker::update(const cv::Mat& frame, TrackingResult& result) {
    // Validate state
    if (!initialized_ || tracker_.empty()) {
        result.tracking_lost = true;
        return false;
    }
    
    // Validate input frame
    if (frame.empty()) {
        std::cerr << "Warning: Empty frame provided to tracker" << std::endl;
        result.tracking_lost = true;
        return false;
    }
    
    // Update tracker - need cv::Rect (int) not cv::Rect2d (double)
    cv::Rect bbox_int;
    bool ok = tracker_->update(frame, bbox_int);
    
    // Convert back to Rect2d for storage
    bbox_ = cv::Rect2d(bbox_int);
    
    // Validate bounding box
    if (!ok || bbox_.width <= 0 || bbox_.height <= 0 || 
        bbox_.x < 0 || bbox_.y < 0 ||
        bbox_.x + bbox_.width > frame.cols || 
        bbox_.y + bbox_.height > frame.rows) {
        result.tracking_lost = true;
        return false;
    }
    
    // Update result
    result.bbox = cv::Rect(static_cast<int>(bbox_.x), static_cast<int>(bbox_.y),
                          static_cast<int>(bbox_.width), static_cast<int>(bbox_.height));
    
    // Calculate midpoint
    result.midpoint.x = static_cast<int>(bbox_.x + bbox_.width / 2);
    result.midpoint.y = static_cast<int>(bbox_.y + bbox_.height / 2);
    
    // Update midpoint history
    midpoints_.push_back(result.midpoint);
    if (midpoints_.size() > MAX_MIDPOINTS) {
        midpoints_.pop_front();
    }
    
    // Calculate movement vector
    if (midpoints_.size() >= 2) {
        const Position& current = midpoints_.back();
        const Position& previous = midpoints_[midpoints_.size() - 2];
        result.movement.dx = current.x - previous.x;
        result.movement.dy = current.y - previous.y;
    } else {
        result.movement.dx = 0;
        result.movement.dy = 0;
    }
    
    result.tracking_lost = false;
    return true;
}

void ObjectTracker::reset() {
    initialized_ = false;
    tracker_.release();
    midpoints_.clear();
}

cv::Rect2d ObjectTracker::selectROI(const cv::Mat& frame, const std::string& window_name) {
    cv::Rect2d bbox = cv::selectROI(window_name, frame, false);
    return bbox;
}

} // namespace rc_car
