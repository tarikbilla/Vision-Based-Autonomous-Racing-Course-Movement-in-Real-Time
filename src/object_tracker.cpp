#include "object_tracker.hpp"
#include <iostream>
#include <cmath>

// ========================
// OpenCVTracker Implementation
// ========================

OpenCVTracker::OpenCVTracker(const std::string& trackerType)
    : trackerType_(trackerType), tracker_(nullptr), initialized_(false) {
}

OpenCVTracker::~OpenCVTracker() {
}

cv::Ptr<cv::Tracker> OpenCVTracker::createTracker() {
    // Note: cv::Tracker classes (CSRT, KCF, GOTURN) are in opencv_contrib
    // which may not be available in all OpenCV installations.
    // For this implementation, we'll use template matching (TM) as fallback
    std::cout << "[!] Tracker algorithms require opencv_contrib. Using fallback method.\n";
    return nullptr;  // Will use SimulatedTracker or template matching instead
}

bool OpenCVTracker::initialize(const cv::Mat& frame, const cv::Rect* roi) {
    if (frame.empty()) {
        throw std::runtime_error("Empty frame provided to tracker");
    }
    
    cv::Rect selectedROI;
    
    if (roi == nullptr) {
        // Interactive ROI selection
        selectedROI = cv::selectROI("Select ROI", frame, false, true);
        cv::destroyWindow("Select ROI");
    } else {
        selectedROI = *roi;
    }
    
    if (selectedROI.width <= 0 || selectedROI.height <= 0) {
        throw std::runtime_error("Invalid ROI selection");
    }
    
    // Prepare frame for tracker
    cv::Mat workFrame = frame.clone();
    if (workFrame.channels() == 1) {
        cv::cvtColor(workFrame, workFrame, cv::COLOR_GRAY2BGR);
    }
    if (workFrame.type() != CV_8UC3) {
        workFrame.convertTo(workFrame, CV_8UC3);
    }
    
    // Validate ROI bounds
    if (selectedROI.x + selectedROI.width > workFrame.cols ||
        selectedROI.y + selectedROI.height > workFrame.rows) {
        throw std::runtime_error("ROI out of bounds");
    }
    
    tracker_ = createTracker();
    // If tracker_ is nullptr, we'll use SimulatedTracker mode
    // This is acceptable as we're defaulting to simulation when contrib isn't available
    
    // Extract and store template for template matching
    roiTemplate_ = frame(selectedROI).clone();
    
    lastCenter_ = cv::Point(
        selectedROI.x + selectedROI.width / 2,
        selectedROI.y + selectedROI.height / 2
    );
    initialized_ = true;
    return true;
}

bool OpenCVTracker::update(const cv::Mat& frame, TrackedObject& result) {
    if (!initialized_) {
        throw std::runtime_error("Tracker not initialized");
    }
    
    // If tracker is available, use it; otherwise use template matching
    if (tracker_) {
        cv::Rect bbox;
        bool ok = tracker_->update(frame, bbox);
        
        if (!ok) {
            throw std::runtime_error("Tracking lost");
        }
        
        cv::Point center(
            bbox.x + bbox.width / 2,
            bbox.y + bbox.height / 2
        );
        
        cv::Point movement(
            center.x - lastCenter_.x,
            center.y - lastCenter_.y
        );
        
        lastCenter_ = center;
        result.bbox = bbox;
        result.center = center;
        result.movement = movement;
        return true;
    } else {
        // Fallback: use template matching
        cv::Mat result_map;
        cv::matchTemplate(frame, roiTemplate_, result_map, cv::TM_CCOEFF_NORMED);
        
        double minVal, maxVal;
        cv::Point minLoc, maxLoc;
        cv::minMaxLoc(result_map, &minVal, &maxVal, &minLoc, &maxLoc);
        
        if (maxVal < 0.5) {
            throw std::runtime_error("Template matching confidence too low");
        }
        
        cv::Rect bbox(maxLoc.x, maxLoc.y, roiTemplate_.cols, roiTemplate_.rows);
        cv::Point center(
            bbox.x + bbox.width / 2,
            bbox.y + bbox.height / 2
        );
        
        cv::Point movement(
            center.x - lastCenter_.x,
            center.y - lastCenter_.y
        );
        
        lastCenter_ = center;
        result.bbox = bbox;
        result.center = center;
        result.movement = movement;
        return true;
    }
    
    return true;
}

// ========================
// SimulatedTracker Implementation
// ========================

SimulatedTracker::SimulatedTracker()
    : initialized_(false) {
}

SimulatedTracker::~SimulatedTracker() {
}

bool SimulatedTracker::initialize(const cv::Mat& frame, const cv::Rect* roi) {
    initialized_ = true;
    return true;
}

bool SimulatedTracker::update(const cv::Mat& frame, TrackedObject& result) {
    if (!initialized_) {
        throw std::runtime_error("Tracker not initialized");
    }
    
    // Find green pixels (simulated target)
    cv::Mat greenMask;
    cv::inRange(frame, cv::Scalar(0, 200, 0), cv::Scalar(100, 255, 100), greenMask);
    
    std::vector<cv::Point> greenPixels;
    cv::findNonZero(greenMask, greenPixels);
    
    if (greenPixels.empty()) {
        throw std::runtime_error("Simulated target not found");
    }
    
    int cx = 0, cy = 0;
    for (const auto& p : greenPixels) {
        cx += p.x;
        cy += p.y;
    }
    cx /= greenPixels.size();
    cy /= greenPixels.size();
    
    cv::Point center(cx, cy);
    cv::Rect bbox(cx - 12, cy - 12, 24, 24);
    
    cv::Point movement(0, 0);
    if (lastCenter_.x != 0 && lastCenter_.y != 0) {
        movement = cv::Point(cx - lastCenter_.x, cy - lastCenter_.y);
    }
    
    lastCenter_ = center;
    result.bbox = bbox;
    result.center = center;
    result.movement = movement;
    
    return true;
}

// ========================
// Factory Function
// ========================

std::unique_ptr<Tracker> createTracker(const std::string& trackerType, bool simulate) {
    if (simulate) {
        return std::make_unique<SimulatedTracker>();
    } else {
        return std::make_unique<OpenCVTracker>(trackerType);
    }
}
