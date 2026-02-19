/**
 * @file boundary_detection.cpp
 * @brief Implementation of ray-based boundary detection and guidance logic
 */

#include "boundary_detection.h"
#include <cmath>
#include <algorithm>
#include <iostream>
#include <limits>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace rc_car {

BoundaryDetection::BoundaryDetection()
    : black_threshold_(50), ray_max_length_(200), evasive_threshold_(80) {
    ray_angles_ = {-60.0, 0.0, 60.0};  // Default: -60°, 0°, +60°
    rays_.resize(3);
}

BoundaryDetection::BoundaryDetection(int black_threshold, int ray_max_length, int evasive_threshold)
    : black_threshold_(black_threshold), ray_max_length_(ray_max_length), 
      evasive_threshold_(evasive_threshold) {
    ray_angles_ = {-60.0, 0.0, 60.0};
    rays_.resize(3);
}

bool BoundaryDetection::isBoundaryPixel(const cv::Vec3b& pixel) const {
    // Convert to grayscale
    int gray_value = (static_cast<int>(pixel[0]) + static_cast<int>(pixel[1]) + static_cast<int>(pixel[2])) / 3;
    return gray_value < black_threshold_;
}

int BoundaryDetection::castRay(const Position& start, double angle, const cv::Mat& track_image) {
    double angle_rad = angle * M_PI / 180.0;
    double dx = std::cos(angle_rad);
    double dy = std::sin(angle_rad);
    
    int distance = ray_max_length_;
    
    // Cast ray from start position
    for (int i = 20; i < ray_max_length_; ++i) {  // Start at 20 to avoid detecting car itself
        int x = start.x + static_cast<int>(dx * i);
        int y = start.y + static_cast<int>(dy * i);
        
        // Check bounds
        if (x < 0 || x >= track_image.cols || y < 0 || y >= track_image.rows) {
            distance = i;
            break;
        }
        
        // Check pixel color
        cv::Vec3b pixel = track_image.at<cv::Vec3b>(y, x);
        if (isBoundaryPixel(pixel)) {
            distance = i;
            break;
        }
    }
    
    return distance;
}

void BoundaryDetection::updateRays(const Position& car_pos, double car_heading) {
    rays_.clear();
    rays_.reserve(ray_angles_.size());
    
    for (double relative_angle : ray_angles_) {
        double absolute_angle = car_heading + relative_angle;
        int distance = castRay(car_pos, absolute_angle, gray_frame_);
        
        Ray ray;
        ray.start = car_pos;
        ray.angle = absolute_angle;
        ray.distance = distance;
        
        // Calculate end position
        double angle_rad = absolute_angle * M_PI / 180.0;
        ray.end.x = car_pos.x + static_cast<int>(std::cos(angle_rad) * distance);
        ray.end.y = car_pos.y + static_cast<int>(std::sin(angle_rad) * distance);
        
        rays_.push_back(ray);
    }
}

ControlVector BoundaryDetection::process(const cv::Mat& frame, const Position& car_position,
                                        const MovementVector& movement, int base_speed) {
    // Validate inputs
    if (frame.empty()) {
        std::cerr << "Warning: Empty frame in boundary detection" << std::endl;
        return ControlVector(0, 0, 0, 0);  // Return stop command
    }
    
    // Validate car position is within frame bounds
    if (car_position.x < 0 || car_position.x >= frame.cols ||
        car_position.y < 0 || car_position.y >= frame.rows) {
        std::cerr << "Warning: Car position out of bounds" << std::endl;
        return ControlVector(0, 0, 0, 0);
    }
    
    // Clamp base speed to valid range
    base_speed = std::max(0, std::min(255, base_speed));
    
    // Convert to grayscale
    if (frame.channels() == 3) {
        cv::cvtColor(frame, gray_frame_, cv::COLOR_BGR2GRAY);
        cv::cvtColor(gray_frame_, gray_frame_, cv::COLOR_GRAY2BGR);  // Keep 3 channels for pixel access
    } else {
        frame.copyTo(gray_frame_);
    }
    
    // Calculate car heading from movement vector
    double car_heading = movement.angle();
    
    // Update rays
    updateRays(car_position, car_heading);
    
    // Find minimum and maximum ray distances
    int min_distance = ray_max_length_;
    int max_distance = 0;
    int max_index = 0;
    
    for (size_t i = 0; i < rays_.size(); ++i) {
        if (rays_[i].distance < min_distance) {
            min_distance = rays_[i].distance;
        }
        if (rays_[i].distance > max_distance) {
            max_distance = rays_[i].distance;
            max_index = static_cast<int>(i);
        }
    }
    
    // Generate control vector
    ControlVector control;
    control.light_on = 1;  // Lights on during autonomous mode
    control.speed = base_speed;
    
    // Evasive action if too close to boundary
    if (min_distance < evasive_threshold_) {
        // Steer toward the direction with maximum clearance
        int max_ray_index = max_index;
        
        // Determine steering direction based on which ray has max distance
        if (max_ray_index == 0) {  // Left ray (-60°)
            control.left_turn = 255;
            control.right_turn = 0;
        } else if (max_ray_index == 2) {  // Right ray (+60°)
            control.left_turn = 0;
            control.right_turn = 255;
        } else {  // Center ray (0°)
            // Choose based on left vs right clearance
            if (rays_[0].distance > rays_[2].distance) {
                control.left_turn = 128;
                control.right_turn = 0;
            } else {
                control.left_turn = 0;
                control.right_turn = 128;
            }
        }
    } else {
        // Normal steering based on car heading
        // Map heading angle to steering values
        double heading_deg = car_heading;
        
        if (heading_deg > 0) {  // Right turn
            // Map 0-60 degrees to 0-255
            control.right_turn = static_cast<int>(std::min(255.0, (heading_deg / 60.0) * 255.0));
            control.left_turn = 0;
        } else if (heading_deg < 0) {  // Left turn
            // Map 0 to -60 degrees to 0-255
            control.left_turn = static_cast<int>(std::min(255.0, (std::abs(heading_deg) / 60.0) * 255.0));
            control.right_turn = 0;
        } else {
            control.left_turn = 0;
            control.right_turn = 0;
        }
    }
    
    // Limit steering values (as per prototype: max 30)
    control.left_turn = std::min(control.left_turn, 30);
    control.right_turn = std::min(control.right_turn, 30);
    
    return control;
}

void BoundaryDetection::drawRays(cv::Mat& frame, const Position& car_pos) const {
    for (const auto& ray : rays_) {
        cv::line(frame, 
                cv::Point(ray.start.x, ray.start.y),
                cv::Point(ray.end.x, ray.end.y),
                cv::Scalar(0, 0, 255), 2);  // Red rays
        
        // Draw distance text
        std::string dist_text = std::to_string(ray.distance);
        cv::putText(frame, dist_text,
                   cv::Point(ray.end.x + 5, ray.end.y),
                   cv::FONT_HERSHEY_SIMPLEX, 0.4,
                   cv::Scalar(255, 255, 0), 1);
    }
    
    // Draw car position
    cv::circle(frame, cv::Point(car_pos.x, car_pos.y), 5, cv::Scalar(0, 255, 0), -1);
}

} // namespace rc_car
