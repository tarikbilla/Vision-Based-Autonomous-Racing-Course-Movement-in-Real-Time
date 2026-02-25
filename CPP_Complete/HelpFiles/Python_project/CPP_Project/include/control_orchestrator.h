#ifndef CONTROL_ORCHESTRATOR_H
#define CONTROL_ORCHESTRATOR_H

#include <thread>
#include <atomic>
#include <memory>
#include "camera_capture.h"
#include "object_tracker.h"
#include "boundary_detection.h"
#include "ble_handler.h"
#include "config_manager.h"
#include "types.h"

namespace rc_car {

class ControlOrchestrator {
private:
    std::unique_ptr<CameraCapture> camera_;
    std::unique_ptr<ObjectTracker> tracker_;
    std::unique_ptr<BoundaryDetection> guidance_;
    std::unique_ptr<BLEHandler> ble_handler_;
    std::unique_ptr<ConfigManager> config_;
    
    // Threads
    std::thread tracking_thread_;
    std::thread guidance_thread_;
    std::thread ble_thread_;
    
    // Queues for inter-thread communication
    ThreadSafeQueue<cv::Mat> frame_queue_;
    ThreadSafeQueue<TrackingResult> tracking_queue_;
    ThreadSafeQueue<ControlVector> control_queue_;
    
    // Control flags
    std::atomic<bool> running_;
    std::atomic<bool> tracking_enabled_;
    std::atomic<bool> guidance_enabled_;
    std::atomic<bool> autonomous_mode_;
    
    // Configuration
    TrackerType tracker_type_;
    int base_speed_;
    bool show_ui_;
    
    // Thread functions
    void trackingLoop();
    void guidanceLoop();
    void bleLoop();
    
    // UI (optional)
    void displayFrame(const cv::Mat& frame, const TrackingResult& tracking, 
                     const ControlVector& control);
    
public:
    ControlOrchestrator();
    ~ControlOrchestrator();
    
    bool initialize(const std::string& config_file = "config/config.json");
    bool start();
    void stop();
    
    void setAutonomousMode(bool enabled) { autonomous_mode_ = enabled; }
    bool isAutonomousMode() const { return autonomous_mode_; }
    
    void setTrackingEnabled(bool enabled) { tracking_enabled_ = enabled; }
    void setGuidanceEnabled(bool enabled) { guidance_enabled_ = enabled; }
    
    bool isRunning() const { return running_; }
    
    // Manual control (for testing)
    void setManualControl(const ControlVector& control);
    
    // Emergency stop
    void emergencyStop();
};

} // namespace rc_car

#endif // CONTROL_ORCHESTRATOR_H
