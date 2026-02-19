#include "control_orchestrator.hpp"
#include <iostream>
#include <thread>
#include <chrono>

ControlOrchestrator::ControlOrchestrator(
    std::unique_ptr<CameraCapture> camera,
    std::unique_ptr<Tracker> tracker,
    std::unique_ptr<BoundaryDetector> boundary,
    std::unique_ptr<BLEClient> ble,
    const OrchestratorOptions& options
)
    : camera_(std::move(camera)),
      tracker_(std::move(tracker)),
      boundary_(std::move(boundary)),
      ble_(std::move(ble)),
      options_(options),
      stopEvent_(false),
      latestControl_(true, 0, 0, 0),
      latestHeading_(0.0),
      roiSelected_(false),
      trackerReady_(false) {
}

ControlOrchestrator::~ControlOrchestrator() {
    stop();
}

void ControlOrchestrator::start() {
    stopEvent_ = false;
    roiSelected_ = false;
    trackerReady_ = false;
    
    std::cout << "[*] Starting Control Orchestrator\n";
    
    if (options_.showWindow) {
        selectROI();
    }
    
    cameraThread_ = std::thread(&ControlOrchestrator::cameraLoop, this);
    trackingThread_ = std::thread(&ControlOrchestrator::trackingLoop, this);
    bleThread_ = std::thread(&ControlOrchestrator::bleLoop, this);
    // UI loop must be run on main thread, not as a thread
}

void ControlOrchestrator::stop() {
    stopEvent_ = true;
    
    if (cameraThread_.joinable()) cameraThread_.join();
    if (trackingThread_.joinable()) trackingThread_.join();
    if (bleThread_.joinable()) bleThread_.join();
    // if (uiThread_.joinable()) uiThread_.join(); // UI loop is not threaded anymore
    
    camera_->close();
    ble_->disconnect();
    
    std::cout << "[*] Control Orchestrator stopped\n";
}

void ControlOrchestrator::selectROI() {
    std::cout << "[*] Opening camera for ROI selection...\n";
    camera_->open();
    
    Frame frame;
    if (!camera_->getFrame(frame)) {
        throw std::runtime_error("Failed to get frame for ROI selection");
    }
    
    try {
        tracker_->initialize(frame.image, nullptr);
        roiSelected_ = true;
        trackerReady_ = true;
        std::cout << "[✓] ROI selected and tracker initialized\n";
    } catch (const std::exception& e) {
        std::cerr << "[!] ROI selection failed: " << e.what() << "\n";
        throw;
    }
}

void ControlOrchestrator::cameraLoop() {
    try {
        camera_->open();
        std::cout << "[✓] Camera opened\n";
        
        while (!stopEvent_) {
            Frame frame;
            if (!camera_->getFrame(frame)) {
                std::cerr << "[!] Failed to get frame\n";
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }
            
            {
                std::lock_guard<std::mutex> lock(frameLock_);
                latestFrame_ = frame.image.clone();
            }
            
            {
                std::lock_guard<std::mutex> lock(frameQueueMutex_);
                if (frameQueue_.size() < 5) {
                    frameQueue_.push(frame);
                    frameQueueCV_.notify_one();
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[!] Camera loop error: " << e.what() << "\n";
        stopEvent_ = true;
    }
}

void ControlOrchestrator::trackingLoop() {
    int frameCount = 0;
    auto lastProcessed = std::chrono::system_clock::now();
    const auto analysisInterval = std::chrono::milliseconds(150); // ~6-7 FPS
    
    while (!stopEvent_) {
        if (!trackerReady_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }
        
        Frame frame;
        {
            std::unique_lock<std::mutex> lock(frameQueueMutex_);
            if (frameQueue_.empty()) {
                frameQueueCV_.wait_for(lock, std::chrono::milliseconds(200));
                if (frameQueue_.empty()) continue;
            }
            frame = frameQueue_.front();
            frameQueue_.pop();
        }
        
        auto now = std::chrono::system_clock::now();
        if (now - lastProcessed < analysisInterval) {
            continue;
        }
        lastProcessed = now;
        
        // Detect road
        auto [left, center, right, roadMask] = boundary_->detectRoadEdges(frame.image);
        roadMask_ = roadMask;
        
        try {
            TrackedObject tracked;
            tracker_->update(frame.image, tracked);
            
            // Analyze boundary and generate control
            auto [rays, control] = boundary_->analyze(frame.image, tracked.center, tracked.movement);
            
            {
                std::lock_guard<std::mutex> lock(controlMutex_);
                latestControl_ = control;
            }
            latestHeading_ = boundary_->headingFromMovement(tracked.movement);
            
            frameCount++;
            if (frameCount % 30 == 0) {
                std::cout << "[" << frameCount << "] Center: (" << tracked.center.x << ","
                         << tracked.center.y << ") | Speed: " << control.speed << " | "
                         << "Left: " << control.left_turn_value << " | "
                         << "Right: " << control.right_turn_value << "\n";
            }
            
        } catch (const std::exception& e) {
            std::cerr << "[!] Tracking error: " << e.what() << "\n";
            latestControl_ = ControlVector(true, 0, 0, 0);
        }
    }
}

void ControlOrchestrator::bleLoop() {
    int interval_ms = 1000 / std::max(1, options_.commandRateHz);
    
    while (!stopEvent_) {
        {
            std::lock_guard<std::mutex> lock(controlMutex_);
            ble_->sendControl(latestControl_);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
    }
}

void ControlOrchestrator::uiLoop() {
    const int DISPLAY_DELAY = 30; // milliseconds
    
    while (!stopEvent_) {
        cv::Mat frameToDisplay;
        {
            std::lock_guard<std::mutex> lock(frameLock_);
            if (!latestFrame_.empty()) {
                frameToDisplay = latestFrame_.clone();
            }
        }
        
        if (!frameToDisplay.empty()) {
            // Draw road boundaries
            auto [left, center, right, mask] = boundary_->detectRoadEdges(frameToDisplay);
            if (left != -1 && right != -1) {
                cv::line(frameToDisplay, cv::Point(left, 0), cv::Point(left, frameToDisplay.rows),
                        cv::Scalar(0, 255, 0), 2);
                cv::line(frameToDisplay, cv::Point(right, 0), cv::Point(right, frameToDisplay.rows),
                        cv::Scalar(0, 255, 0), 2);
                if (center != -1) {
                    cv::line(frameToDisplay, cv::Point(center, 0), cv::Point(center, frameToDisplay.rows),
                            cv::Scalar(255, 0, 0), 2);
                }
            }
            
            cv::imshow("RC Car Autonomous Control", frameToDisplay);
            
            if (cv::waitKey(DISPLAY_DELAY) == 'q') {
                break;
            }
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(DISPLAY_DELAY));
        }
    }
    
    cv::destroyAllWindows();
}

void ControlOrchestrator::render(const cv::Mat& image, const TrackedObject& tracked,
                                  const std::vector<RayResult>& rays) {
    // Rendering logic
}

void ControlOrchestrator::detectRedCar(const cv::Mat& frame, TrackedObject& result) {
    // Red car detection fallback
}
