#pragma once

#include <opencv2/opencv.hpp>
#include <vector>
#include <string>

struct SimpleCarDetection {
    bool detected;
    cv::Point center;
    cv::Rect bbox;
    cv::Point velocity;
    int confidence;  // 0-100
    std::string method;  // "KCF", "MOTION", "HSV"
};

class CarDetector {
public:
    CarDetector();
    ~CarDetector();

    // Main detection function - uses KCF primarily
    SimpleCarDetection detectCar(const cv::Mat& frame, const cv::Point& lastKnownPosition = cv::Point(-1, -1));

    // Individual detection methods
    SimpleCarDetection detectByCentroid(const cv::Mat& frame);
    SimpleCarDetection detectByHSV(const cv::Mat& frame);
    SimpleCarDetection detectByMotion(const cv::Mat& frame);
    SimpleCarDetection detectByTemplate(const cv::Mat& frame);

    // Debug/visualization
    void setDebugMode(bool debug) { debugMode_ = debug; }
    void setSaveDebugImages(bool save) { saveDebugImages_ = save; }
    cv::Mat getLastHSVMask() const { return lastHSVMask_; }
    cv::Mat getLastMotionMask() const { return lastMotionMask_; }

    // Configuration
    void setCarColorHue(int hueMin, int hueMax) { carHueMin_ = hueMin; carHueMax_ = hueMax; }
    void setCarSaturation(int satMin, int satMax) { carSatMin_ = satMin; carSatMax_ = satMax; }
    void setCarValue(int valMin, int valMax) { carValMin_ = valMin; carValMax_ = valMax; }

private:
    // Simple centroid tracker (no external tracker needed)
    cv::Rect lastTrackedBox_;
    bool centeroidTrackerInitialized_ = false;
    int trackerFailureCount_ = 0;
    const int TRACKER_MAX_FAILURES = 10;
    
    // HSV detection parameters (for orange/red car)
    int carHueMin_ = 5;
    int carHueMax_ = 20;
    int carSatMin_ = 100;
    int carSatMax_ = 255;
    int carValMin_ = 100;
    int carValMax_ = 255;

    // Motion detection
    cv::Ptr<cv::BackgroundSubtractor> bgSubtractor_;
    int motionFrameCount_ = 0;
    const int MOTION_WARMUP_FRAMES = 15;

    // Debug
    bool debugMode_ = false;
    bool saveDebugImages_ = false;
    cv::Mat lastHSVMask_;
    cv::Mat lastMotionMask_;
    cv::Point lastDetectedCenter_;
    int frameCounter_ = 0;

    // Helper functions
    std::vector<cv::Rect> extractCandidates(const cv::Mat& mask, const std::string& source);
    cv::Rect selectBestCandidate(const std::vector<cv::Rect>& candidates, const cv::Point& lastPos);
    cv::Point calculateVelocity(const cv::Point& current, const cv::Point& previous);
};
