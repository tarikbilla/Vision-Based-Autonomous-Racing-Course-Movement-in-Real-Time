#ifndef CONTROL_ORCHESTRATOR_HPP
#define CONTROL_ORCHESTRATOR_HPP

#include "camera_capture.hpp"
#include "object_tracker.hpp"
#include "boundary_detection.hpp"
#include "ble_handler.hpp"
#include "config_manager.hpp"

#include <thread>
#include <queue>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <memory>

struct OrchestratorOptions {
    bool showWindow;
    int commandRateHz;
    bool colorTracking;
};

class ControlOrchestrator {
public:
    ControlOrchestrator(
        std::unique_ptr<CameraCapture> camera,
        std::unique_ptr<Tracker> tracker,
        std::unique_ptr<BoundaryDetector> boundary,
        std::unique_ptr<BLEClient> ble,
        const OrchestratorOptions& options
    );
    
    ~ControlOrchestrator();
    
    void start();
    void stop();
    
private:
    std::unique_ptr<CameraCapture> camera_;
    std::unique_ptr<Tracker> tracker_;
    std::unique_ptr<BoundaryDetector> boundary_;
    std::unique_ptr<BLEClient> ble_;
    OrchestratorOptions options_;
    
    // Threading and synchronization
    std::thread cameraThread_;
    std::thread trackingThread_;
    std::thread bleThread_;
    // std::thread uiThread_; // Removed: UI loop must run on main thread
    
    std::atomic<bool> stopEvent_;
    std::queue<Frame> frameQueue_;
    std::mutex frameQueueMutex_;
    std::condition_variable frameQueueCV_;
    
    // State
    ControlVector latestControl_;
    std::mutex controlMutex_;
    double latestHeading_;
    
    bool roiSelected_;
    cv::Rect selectedROI_;
    std::atomic<bool> trackerReady_;
    
    cv::Point lastCenter_;
    cv::Mat latestFrame_;
    std::mutex frameLock_;
    cv::Mat roadMask_;
    
    // Thread functions
    void cameraLoop();
    void trackingLoop();
    void bleLoop();
    void uiLoop();
public:
    // Expose UI loop for main thread execution
    void runUILoop() { uiLoop(); }
    void selectROI();
    
    // Helper functions
    void render(const cv::Mat& image, const TrackedObject& tracked, const std::vector<RayResult>& rays);
    void detectRedCar(const cv::Mat& frame, TrackedObject& result);
};

#endif // CONTROL_ORCHESTRATOR_HPP
