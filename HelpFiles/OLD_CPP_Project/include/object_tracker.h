#ifndef OBJECT_TRACKER_H
#define OBJECT_TRACKER_H

#include <opencv2/opencv.hpp>
#include <opencv2/tracking.hpp>
#include <vector>
#include <deque>
#include "types.h"

namespace rc_car {

enum class TrackerType {
    GOTURN,
    CSRT,
    KCF,
    MOSSE
};

class ObjectTracker {
private:
    cv::Ptr<cv::Tracker> tracker_;
    TrackerType tracker_type_;
    cv::Rect2d bbox_;
    bool initialized_;
    
    std::deque<Position> midpoints_;
    static constexpr size_t MAX_MIDPOINTS = 10;
    
    cv::Ptr<cv::Tracker> createTracker(TrackerType type);
    std::string trackerTypeToString(TrackerType type);
    
public:
    ObjectTracker();
    explicit ObjectTracker(TrackerType type);
    ~ObjectTracker();
    
    bool initialize(const cv::Mat& frame, const cv::Rect2d& bbox, TrackerType type = TrackerType::CSRT);
    bool update(const cv::Mat& frame, TrackingResult& result);
    
    bool isInitialized() const { return initialized_; }
    cv::Rect2d getBBox() const { return bbox_; }
    
    void reset();
    TrackerType getTrackerType() const { return tracker_type_; }
    
    // ROI selection helper
    static cv::Rect2d selectROI(const cv::Mat& frame, const std::string& window_name = "Select Object to Track");
};

} // namespace rc_car

#endif // OBJECT_TRACKER_H
