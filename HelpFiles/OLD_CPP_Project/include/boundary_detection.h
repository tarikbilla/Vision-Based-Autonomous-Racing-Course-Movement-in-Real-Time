#ifndef BOUNDARY_DETECTION_H
#define BOUNDARY_DETECTION_H

#include <opencv2/opencv.hpp>
#include <vector>
#include "types.h"

namespace rc_car {

class BoundaryDetection {
private:
    int black_threshold_;
    int ray_max_length_;
    int evasive_threshold_;
    std::vector<double> ray_angles_;  // Relative angles in degrees
    
    cv::Mat gray_frame_;
    cv::Mat binary_frame_;
    
    std::vector<Ray> rays_;
    
    void updateRays(const Position& car_pos, double car_heading);
    int castRay(const Position& start, double angle, const cv::Mat& track_image);
    bool isBoundaryPixel(const cv::Vec3b& pixel) const;
    
public:
    BoundaryDetection();
    BoundaryDetection(int black_threshold, int ray_max_length, int evasive_threshold);
    
    void setBlackThreshold(int threshold) { black_threshold_ = threshold; }
    void setRayMaxLength(int length) { ray_max_length_ = length; }
    void setEvasiveThreshold(int threshold) { evasive_threshold_ = threshold; }
    void setRayAngles(const std::vector<double>& angles) { ray_angles_ = angles; }
    
    // Main processing function
    ControlVector process(const cv::Mat& frame, const Position& car_position, 
                         const MovementVector& movement, int base_speed = 10);
    
    // Get ray information for visualization
    const std::vector<Ray>& getRays() const { return rays_; }
    
    // Draw rays on frame for visualization
    void drawRays(cv::Mat& frame, const Position& car_pos) const;
};

} // namespace rc_car

#endif // BOUNDARY_DETECTION_H
