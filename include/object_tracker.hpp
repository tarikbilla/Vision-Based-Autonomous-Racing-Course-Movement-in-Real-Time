#ifndef OBJECT_TRACKER_HPP
#define OBJECT_TRACKER_HPP

#include <opencv2/opencv.hpp>
#include <memory>
#include <tuple>

struct TrackedObject {
    cv::Rect bbox;
    cv::Point center;
    cv::Point movement;
};

class Tracker {
public:
    virtual ~Tracker() = default;
    virtual bool initialize(const cv::Mat& frame, const cv::Rect* roi = nullptr) = 0;
    virtual bool update(const cv::Mat& frame, TrackedObject& result) = 0;
};

class OpenCVTracker : public Tracker {
public:
    explicit OpenCVTracker(const std::string& trackerType = "CSRT");
    ~OpenCVTracker() override;
    
    bool initialize(const cv::Mat& frame, const cv::Rect* roi = nullptr) override;
    bool update(const cv::Mat& frame, TrackedObject& result) override;
    
private:
    std::string trackerType_;
    cv::Ptr<cv::Tracker> tracker_;
    cv::Mat roiTemplate_;
    cv::Point lastCenter_;
    bool initialized_;
    
    cv::Ptr<cv::Tracker> createTracker();
};

class SimulatedTracker : public Tracker {
public:
    SimulatedTracker();
    ~SimulatedTracker() override;
    
    bool initialize(const cv::Mat& frame, const cv::Rect* roi = nullptr) override;
    bool update(const cv::Mat& frame, TrackedObject& result) override;
    
private:
    cv::Point lastCenter_;
    bool initialized_;
};

std::unique_ptr<Tracker> createTracker(const std::string& trackerType, bool simulate);

#endif // OBJECT_TRACKER_HPP
