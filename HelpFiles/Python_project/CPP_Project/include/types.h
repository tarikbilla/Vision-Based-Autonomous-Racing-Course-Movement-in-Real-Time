/**
 * @file types.h
 * @brief Common data structures and thread-safe utilities for RC car control system
 * @author Vision-Based Autonomous RC Car Control System
 * @date 2024
 */

#ifndef TYPES_H
#define TYPES_H

#include <opencv2/opencv.hpp>
#include <string>
#include <array>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace rc_car {

// Position and movement vector
struct Position {
    int x;
    int y;
    
    Position() : x(0), y(0) {}
    Position(int x, int y) : x(x), y(y) {}
};

struct MovementVector {
    int dx;
    int dy;
    
    MovementVector() : dx(0), dy(0) {}
    MovementVector(int dx, int dy) : dx(dx), dy(dy) {}
    
    /**
     * @brief Calculate magnitude (length) of movement vector
     * @return Magnitude in pixels
     */
    double magnitude() const {
        return std::sqrt(static_cast<double>(dx * dx + dy * dy));
    }
    
    /**
     * @brief Calculate angle of movement vector in degrees
     * @return Angle in degrees (-180 to 180)
     */
    double angle() const {
        if (dx == 0 && dy == 0) return 0.0;
        return std::atan2(static_cast<double>(dy), static_cast<double>(dx)) * 180.0 / M_PI;
    }
};

// Tracking result
struct TrackingResult {
    cv::Rect bbox;
    Position midpoint;
    MovementVector movement;
    bool tracking_lost;
    
    TrackingResult() : tracking_lost(false) {}
};

// Control vector: [light_on, speed, right_turn_value, left_turn_value]
struct ControlVector {
    int light_on;      // 0 or 1
    int speed;         // 0-255
    int right_turn;    // 0-255
    int left_turn;     // 0-255
    
    ControlVector() : light_on(0), speed(0), right_turn(0), left_turn(0) {}
    ControlVector(int light, int spd, int right, int left) 
        : light_on(light), speed(spd), right_turn(right), left_turn(left) {}
};

// Ray for boundary detection
struct Ray {
    Position start;
    Position end;
    double angle;      // Relative angle in degrees
    int distance;      // Distance to boundary (pixels)
    
    Ray() : angle(0), distance(200) {}
};

// Thread-safe queue wrapper
template<typename T>
class ThreadSafeQueue {
private:
    std::queue<T> queue_;
    mutable std::mutex mutex_;
    std::condition_variable condition_;
    
public:
    void push(const T& item) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(item);
        condition_.notify_one();
    }
    
    bool try_pop(T& item) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) {
            return false;
        }
        item = queue_.front();
        queue_.pop();
        return true;
    }
    
    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }
    
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }
    
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        while (!queue_.empty()) {
            queue_.pop();
        }
    }
};

} // namespace rc_car

#endif // TYPES_H
