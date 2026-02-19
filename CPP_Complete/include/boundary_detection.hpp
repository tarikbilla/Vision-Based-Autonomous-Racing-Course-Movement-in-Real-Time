#ifndef BOUNDARY_DETECTION_HPP
#define BOUNDARY_DETECTION_HPP

#include "commands.hpp"
#include <opencv2/opencv.hpp>
#include <vector>
#include <tuple>

struct RayResult {
    int angle_deg;
    int distance;
};

class BoundaryDetector {
public:
    BoundaryDetector(
        int blackThreshold,
        const std::vector<int>& rayAnglesDeg,
        int rayMaxLength,
        int evasiveDistance,
        int defaultSpeed,
        int steeringLimit,
        bool lightOn
    );
    
    ~BoundaryDetector();
    
    // Detect road edges - returns (left_x, center_x, right_x, road_mask)
    std::tuple<int, int, int, cv::Mat> detectRoadEdges(const cv::Mat& frame);
    
    // Detect car in frame
    cv::Rect detectCarInFrame(const cv::Mat& frame, const cv::Mat& roadMask = cv::Mat());
    
    // Analyze frame and return rays + control vector
    std::pair<std::vector<RayResult>, ControlVector> analyze(
        const cv::Mat& frame,
        const cv::Point& carCenter,
        const cv::Point& carMovement
    );
    
private:
    int blackThreshold_;
    std::vector<int> rayAnglesDeg_;
    int rayMaxLength_;
    int evasiveDistance_;
    int defaultSpeed_;
    int steeringLimit_;
    bool lightOn_;
    double lastHeading_;
    cv::Point lastCarPosition_;
    cv::Mat lastRoadMask_;
    
public:
    double headingFromMovement(const cv::Point& movement);
private:
    int castRay(const cv::Mat& gray, const cv::Point& origin, double angleDeg);
};

#endif // BOUNDARY_DETECTION_HPP
