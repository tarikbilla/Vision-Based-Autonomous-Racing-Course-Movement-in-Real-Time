#include "control_orchestrator.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <cctype>

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
      trackerReady_(false),
      warmupFrames_(120),
            useMotionDetection_(false),
      programStartTime_(std::chrono::high_resolution_clock::now()) {
        // Initialize background subtractor for motion detection
        bgSubtractor_ = cv::createBackgroundSubtractorMOG2(400, 18.0, false);
}

ControlOrchestrator::~ControlOrchestrator() {
    stop();
}

void ControlOrchestrator::start() {
    stopEvent_ = false;
    roiSelected_ = false;
    trackerReady_ = false;
    
    std::cout << "[*] Starting Control Orchestrator\n";
    
    selectROI();
    
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
    
    camera_->close();
    
    // Calculate total execution time
    auto programEndTime = std::chrono::high_resolution_clock::now();
    auto totalDuration = std::chrono::duration_cast<std::chrono::seconds>(programEndTime - programStartTime_);
    
    // Print timing statistics
    std::cout << "\n========================================\n";
    std::cout << "TIMING STATISTICS\n";
    std::cout << "========================================\n";
    std::cout << "Total Program Execution Time: " << totalDuration.count() << " seconds\n";
    
    if (totalBLECommandsSent_ > 0) {
        double avgLatency = (double)totalBLELatencyMs_ / totalBLECommandsSent_;
        std::cout << "Total BLE Commands Sent: " << totalBLECommandsSent_ << "\n";
        std::cout << "Average BLE Send Latency: " << avgLatency << " ms\n";
        std::cout << "Total BLE Latency: " << totalBLELatencyMs_ << " ms\n";
    }
    std::cout << "========================================\n\n";
    
    // Send stop command multiple times to ensure car receives it
    if (ble_) {
        std::cout << "[*] Sending STOP command (speed=0, left=0, right=0)...\n";
        ControlVector stop_control(false, 0, 0, 0);
        for (int i = 0; i < 10; ++i) {
            ble_->sendControl(stop_control);
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        std::cout << "[*] Stop command sent. Waiting for car to stop...\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    
    ble_->disconnect();
    
    std::cout << "[*] Control Orchestrator stopped\n";
}

// New method to send STOP and disconnect BLE
void ControlOrchestrator::sendStopAndDisconnect() {
    if (ble_) {
        std::cout << "[*] Sending STOP command (speed=0, left=0, right=0)...\n";
        ControlVector stop_control(false, 0, 0, 0);
        
        // Send stop command 10 times with delays to ensure car receives and processes it
        for (int i = 0; i < 10; ++i) {
            ble_->sendControl(stop_control);
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        
        std::cout << "[*] Stop command sent. Waiting for car to stop...\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(500)); // Give car time to stop
        
        std::cout << "[*] Disconnecting BLE...\n";
        ble_->disconnect();
        std::cout << "[✓] BLE disconnected\n";
    }
}

void ControlOrchestrator::selectROI() {
    std::cout << "[*] Waiting for first camera frame...\n";
    
    // In headless mode, wait for terminal key to start auto detection
    if (!options_.showWindow) {
        std::cout << "[*] Headless mode (no camera feed).\n";
        std::cout << "[*] After BLE connection, press 'a' then Enter to start auto car detection and running.\n";
        std::cout << "[*] Press 'q' then Enter to quit.\n";

        while (!stopEvent_) {
            std::cout << "[headless] command> " << std::flush;
            std::string input;
            if (!std::getline(std::cin, input)) {
                std::cout << "\n[!] No interactive terminal input detected. Auto-starting headless detection.\n";
                break;
            }

            if (input.empty()) {
                continue;
            }

            char cmd = static_cast<char>(std::tolower(static_cast<unsigned char>(input[0])));
            if (cmd == 'q') {
                throw std::runtime_error("User requested quit before start");
            }
            if (cmd == 'a') {
                break;
            }

            std::cout << "[!] Unknown command. Use 'a' to start or 'q' to quit.\n";
        }

        std::cout << "[*] Headless mode: enabling auto red car detection and autonomous control\n";
        useMotionDetection_ = true;
        options_.colorTracking = true;
        trackerReady_ = true;
        std::cout << "[✓] Start command accepted. Autonomous run is active.\n";
        return;
    }
    
    std::cout << "[*] Press 's' to select ROI, 'a' for auto red-car tracking, or 'q' to quit." << std::endl;

    camera_->open();

    while (!stopEvent_ && !trackerReady_) {
        Frame frame;
        if (!camera_->getFrame(frame)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        cv::imshow("Camera Live", frame.image);
        int key = cv::waitKey(1);
        if (key == 'q') {
            throw std::runtime_error("User requested quit during ROI selection");
        }
        if (key == 'a') {
            // Auto detect red car immediately
            std::cout << "[*] Auto-detecting red car..." << std::endl;
            TrackedObject initialCar;
            bool detected = detectRedCarInitial(frame.image, initialCar);
            if (detected) {
                std::cout << "[✓] Red car detected at (" << initialCar.center.x << "," << initialCar.center.y << ")" << std::endl;
                lastCenter_ = initialCar.center;
            } else {
                std::cout << "[!] Red car not found. Make sure car is visible and on track." << std::endl;
            }
            // Always run robust candidate picking after 'a'
            // (matches controller.cpp logic)
            // Optionally: run motion detection fallback
            useMotionDetection_ = true;
            options_.colorTracking = true;
            trackerReady_ = true;
            std::cout << "[✓] Tracking enabled. Starting autonomous control..." << std::endl;
            break;
        }
        if (key == 's') {
            cv::Mat frozen = frame.image.clone();
            cv::imshow("ROI Select", frozen);
            cv::waitKey(1);

            int roi_retry_count = 0;
            while (!stopEvent_ && roi_retry_count < 5) {
                cv::Rect roi = cv::selectROI("ROI Select", frozen, false, true);
                if (roi.width == 0 || roi.height == 0) {
                    std::cout << "[*] ROI cancelled. Press 's' again to reselect or 'a' for auto tracking." << std::endl;
                    cv::destroyWindow("ROI Select");
                    break;
                }
                if (roi.width < 10 || roi.height < 10) {
                    std::cout << "[!] ROI too small (" << roi.width << "x" << roi.height << "). Please select a larger area." << std::endl;
                    roi_retry_count++;
                    continue;
                }

                std::cout << "[*] Selected ROI: (" << roi.x << "," << roi.y << "," << roi.width << "," << roi.height << ")" << std::endl;
                std::cout << "[*] Initializing tracker with selected ROI..." << std::endl;
                try {
                    tracker_->initialize(frozen, &roi);
                    // After tracker init, run robust candidate picking
                    TrackedObject roiCar;
                    bool detected = detectRedCarInitial(frozen(roi), roiCar);
                    if (detected) {
                        std::cout << "[✓] Car detected in ROI at (" << roiCar.center.x << "," << roiCar.center.y << ")" << std::endl;
                        lastCenter_ = cv::Point(roi.x + roiCar.center.x, roi.y + roiCar.center.y);
                    } else {
                        std::cout << "[!] Car not found in ROI. Fallback to motion detection." << std::endl;
                    }
                    useMotionDetection_ = true;
                    trackerReady_ = true;
                    std::cout << "[✓] Tracker initialized. Starting autonomous control..." << std::endl;
                    cv::destroyWindow("ROI Select");
                    break;
                } catch (const std::exception& e) {
                    roi_retry_count++;
                    if (roi_retry_count >= 5) {
                        std::cout << "[!] Tracker failed after " << roi_retry_count << " attempts. Falling back to auto color tracking." << std::endl;
                        options_.colorTracking = true;
                        useMotionDetection_ = true;
                        trackerReady_ = true;
                        cv::destroyWindow("ROI Select");
                        break;
                    }
                    std::cout << "[!] Tracker initialization failed: " << e.what() << std::endl;
                    std::cout << "[*] Please select ROI again (attempt " << roi_retry_count << "/5)." << std::endl;
                }
            }
            if (trackerReady_) break;
        }
    }
    if (options_.showWindow) {
        cv::destroyWindow("Camera Live");
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
    const auto analysisInterval = std::chrono::milliseconds(66); // ~15 FPS for better control frequency
    
    std::cout << "[*] Tracking started at ~15Hz analysis rate.\n";
    
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
        frameCount++;
        
        // Detect road edges for guidance
        auto [left, center, right, roadMask] = boundary_->detectRoadEdges(frame.image);
        roadMask_ = roadMask;

        // Build centerline map (from red/white barriers) when available
        if (!boundary_->hasCenterline() && frameCount % 30 == 0) {
            if (boundary_->buildCenterlineFromFrame(frame.image)) {
                std::cout << "[✓] Centerline map built from live frame\n";
            }
        }
        
        TrackedObject tracked;
        bool detectionSuccess = false;
        std::string detectionMethod = "NONE";
        
        // Priority 1: Motion detection (PRIMARY - works regardless of color)
        if (useMotionDetection_ && frameCount > warmupFrames_) {
            detectionSuccess = detectCarByMotion(frame.image, frameCount, tracked);
            if (detectionSuccess) {
                detectionMethod = "MOTION";
            }
        }
        
        // Priority 2: Color-based detection (fallback if motion fails)
        if (!detectionSuccess && options_.colorTracking) {
            detectionSuccess = detectRedCarRealtime(frame.image, tracked);
            if (detectionSuccess) {
                detectionMethod = "COLOR";
            }
        }
        
        // Priority 3: Tracker (if manual ROI was selected)
        if (!detectionSuccess && !options_.colorTracking) {
            try {
                tracker_->update(frame.image, tracked);
                detectionSuccess = true;
                detectionMethod = "TRACK";
            } catch (const std::exception& e) {
                // Tracker failed
            }
        }
        
        if (detectionSuccess) {
            // Analyze boundary and generate control
            auto [rays, control] = boundary_->analyze(frame.image, tracked.center, tracked.movement);

            {
                std::lock_guard<std::mutex> lock(controlMutex_);
                latestControl_ = control;
                
                // Also store tracked object and rays for rendering
                std::lock_guard<std::mutex> trackedLock(trackedMutex_);
                latestTracked_ = tracked;
                latestRays_ = rays;
            }
            
            latestHeading_ = boundary_->headingFromMovement(tracked.movement);

            // VERBOSE: Log EVERY detection for debugging
            std::cout << "[" << detectionMethod << "] [" << frameCount << "] "
                     << "Car: (" << tracked.center.x << "," << tracked.center.y << ") | "
                     << "Speed: " << control.speed << " | "
                     << "L:" << control.left_turn_value << " R:" << control.right_turn_value;

            if (rays.size() >= 3) {
                std::cout << " | Rays: L=" << rays[0].distance
                          << " C=" << rays[1].distance
                          << " R=" << rays[2].distance;
            }

            // Show path center if detected
            auto [left, center, right, mask] = boundary_->detectRoadEdges(frame.image);
            if (center != -1) {
                int offset = tracked.center.x - center;
                std::cout << " | Path: C=" << center << " Offset=" << offset;
            }
            std::cout << std::endl;
        } else {
            // No detection - stop car for safety
            // VERBOSE: Log EVERY failed detection
            std::cout << "[FAIL] [" << frameCount << "] Car not detected - STOPPING (motion:" 
                     << (useMotionDetection_ ? "Y" : "N") 
                     << " color:" << (options_.colorTracking ? "Y" : "N") 
                     << " warmup:" << (frameCount > warmupFrames_ ? "done" : "wait") << ")\n";
            latestControl_ = ControlVector(true, 0, 0, 0);
        }
    }
}

void ControlOrchestrator::bleLoop() {
    // Use 50Hz command rate for smoother control (override config)
    int interval_ms = 20; // 50 commands per second
    
    while (!stopEvent_) {
        auto sendStartTime = std::chrono::high_resolution_clock::now();
        
        {
            std::lock_guard<std::mutex> lock(controlMutex_);
            ble_->sendControl(latestControl_);
        }
        
        auto sendEndTime = std::chrono::high_resolution_clock::now();
        auto sendDuration = std::chrono::duration_cast<std::chrono::microseconds>(sendEndTime - sendStartTime);
        
        // Update timing stats
        totalBLECommandsSent_++;
        totalBLELatencyMs_ += sendDuration.count() / 1000; // Convert to milliseconds
        lastBLESendTime_ = sendEndTime;
        
        std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
    }
}

void ControlOrchestrator::uiLoop() {
    const int DISPLAY_DELAY = 30; // milliseconds
    
    while (!stopEvent_) {
        cv::Mat frameToDisplay;
        TrackedObject trackedToRender;
        std::vector<RayResult> raysToRender;
        
        {
            std::lock_guard<std::mutex> lock(frameLock_);
            if (!latestFrame_.empty()) {
                frameToDisplay = latestFrame_.clone();
            }
        }
        
        // Get latest detected car and rays
        {
            std::lock_guard<std::mutex> lock(trackedMutex_);
            trackedToRender = latestTracked_;
            raysToRender = latestRays_;
        }
        
        if (!frameToDisplay.empty()) {
            render(frameToDisplay, trackedToRender, raysToRender);
            if (options_.showWindow) {
                if (cv::waitKey(DISPLAY_DELAY) == 'q') {
                    break;
                }
            } else {
                // In headless mode, just sleep instead of waitKey
                std::this_thread::sleep_for(std::chrono::milliseconds(DISPLAY_DELAY));
            }
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(DISPLAY_DELAY));
        }
    }
    cv::destroyAllWindows();
}

void ControlOrchestrator::render(const cv::Mat& image, const TrackedObject& tracked,
                                  const std::vector<RayResult>& rays) {
    if (image.empty()) return;
    cv::Mat display = image.clone();

    // 1. ALWAYS DRAW CAR FIRST (HIGHEST PRIORITY - ALWAYS VISIBLE)
    if (tracked.bbox.area() > 0) {
        // Draw bounding box in red (thick, very visible)
        cv::rectangle(display, tracked.bbox, cv::Scalar(0, 0, 255), 3);
        // Draw center point (large red circle for high visibility)
        cv::circle(display, tracked.center, 8, cv::Scalar(0, 0, 255), -1);
        // Draw direction arrow if car is moving
        if (tracked.movement.x != 0 || tracked.movement.y != 0) {
            cv::Point arrowEnd = tracked.center + tracked.movement * 8;
            cv::arrowedLine(display, tracked.center, arrowEnd, cv::Scalar(0, 255, 0), 3);
        }
        // Status text - CAR DETECTED
        cv::putText(display, "CAR DETECTED", cv::Point(10, 35), cv::FONT_HERSHEY_SIMPLEX, 0.9, cv::Scalar(0, 0, 255), 2);
        cv::putText(display, cv::format("Pos: (%d,%d)", tracked.center.x, tracked.center.y), cv::Point(10, 65), cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(0, 255, 0), 1);
    } else {
        // Draw warning - NO DETECTION
        cv::putText(display, "NO CAR DETECTED", cv::Point(10, 35), cv::FONT_HERSHEY_SIMPLEX, 0.9, cv::Scalar(0, 0, 255), 2);
    }

    // 2. Draw centerline from map (if built)
    if (boundary_ && boundary_->hasCenterline()) {
        const auto& centerline = boundary_->getCenterline();
        if (!centerline.empty()) {
            std::vector<cv::Point> pts;
            pts.reserve(centerline.size());
            for (const auto& p : centerline) {
                pts.emplace_back((int)std::lround(p.x), (int)std::lround(p.y));
            }
            cv::polylines(display, pts, true, cv::Scalar(0, 255, 255), 3, cv::LINE_AA);
            for (size_t i = 0; i < pts.size(); i += 10) {
                cv::circle(display, pts[i], 2, cv::Scalar(0, 255, 255), -1);
            }
        }
    }

    // 3. Draw car position and direction
    if (tracked.bbox.area() > 0) {
        cv::rectangle(display, tracked.bbox, cv::Scalar(0, 0, 255), 2);
        cv::circle(display, tracked.center, 6, cv::Scalar(0, 0, 255), -1);
        // Draw direction arrow
        cv::Point arrowEnd = tracked.center + tracked.movement * 5;
        cv::arrowedLine(display, tracked.center, arrowEnd, cv::Scalar(0, 0, 255), 2);
    }

    // 4. Draw rays (optional, for debugging)
    for (const auto& ray : rays) {
        double heading = boundary_->headingFromMovement(tracked.movement);
        double angleRad = (heading + ray.angle_deg) * CV_PI / 180.0;
        cv::Point end(
            tracked.center.x + static_cast<int>(ray.distance * std::cos(angleRad)),
            tracked.center.y + static_cast<int>(ray.distance * std::sin(angleRad))
        );
        cv::line(display, tracked.center, end, cv::Scalar(255, 0, 255), 2);
    }

    // Only show window if GUI is enabled
    if (options_.showWindow) {
        cv::imshow("RC Car Autonomous Control", display);
        cv::waitKey(1);
    }
// End of render function
}

bool ControlOrchestrator::detectRedCar(const cv::Mat& frame, TrackedObject& result) {
    if (frame.empty()) return false;

    cv::Mat hsv;
    cv::cvtColor(frame, hsv, cv::COLOR_BGR2HSV);

    // RELAXED HSV ranges for bright red/orange car detection
    // Lower saturation threshold to catch lighter shades of red/orange
    cv::Mat mask1, mask2, mask3, mask4, mask5;
    cv::inRange(hsv, cv::Scalar(0, 30, 50), cv::Scalar(12, 255, 255), mask1);        // Red (0-12 hue)
    cv::inRange(hsv, cv::Scalar(168, 30, 50), cv::Scalar(180, 255, 255), mask2);    // Red (168-180 hue)
    cv::inRange(hsv, cv::Scalar(10, 40, 60), cv::Scalar(30, 255, 255), mask3);      // Orange (10-30 hue)
    cv::inRange(hsv, cv::Scalar(0, 20, 100), cv::Scalar(30, 200, 255), mask4);      // Very bright red/orange (low sat)
    cv::inRange(hsv, cv::Scalar(165, 20, 100), cv::Scalar(180, 200, 255), mask5);   // Very bright red edge (low sat)
    cv::Mat mask = mask1 | mask2 | mask3 | mask4 | mask5;

    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(3, 3));
    cv::morphologyEx(mask, mask, cv::MORPH_OPEN, kernel, cv::Point(-1, -1), 1);
    cv::morphologyEx(mask, mask, cv::MORPH_DILATE, kernel, cv::Point(-1, -1), 1);

    // Constrain to road region if available
    if (!roadMask_.empty()) {
        if (roadMask_.size() == mask.size()) {
            cv::bitwise_and(mask, roadMask_, mask);
        }
    }

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    if (contours.empty()) return false;

    std::vector<std::vector<cv::Point>> valid;
    for (const auto& c : contours) {
        double area = cv::contourArea(c);
        if (area > 40 && area < 5000) valid.push_back(c);
    }
    if (valid.empty()) return false;

    auto largest = *std::max_element(valid.begin(), valid.end(), [](const auto& a, const auto& b){
        return cv::contourArea(a) < cv::contourArea(b);
    });

    cv::Rect bbox = cv::boundingRect(largest);
    cv::Point center(bbox.x + bbox.width/2, bbox.y + bbox.height/2);

    cv::Point movement(0,0);
    if (lastCenter_.x != 0 || lastCenter_.y != 0) {
        movement = cv::Point(center.x - lastCenter_.x, center.y - lastCenter_.y);
    }
    lastCenter_ = center;

    result.bbox = bbox;
    result.center = center;
    result.movement = movement;
    return true;
}

bool ControlOrchestrator::detectCarByMotion(const cv::Mat& frame, int frameCount, TrackedObject& result) {
    if (frame.empty()) return false;
    
    // Apply background subtraction with CONSERVATIVE learning rate
    cv::Mat fgMask;
    double learningRate = (frameCount <= 30) ? -1.0 : 0.01;  // VERY QUICK warmup
    bgSubtractor_->apply(frame, fgMask, learningRate);
    
    // During very short warmup, don't try to detect
    if (frameCount <= 30) {
        if (frameCount == 30) {
            std::cout << "[✓] Background learning FAST complete. Car detection active via MOTION.\n";
        }
        return false;
    }
    
    // Aggressive threshold and cleanup
    cv::threshold(fgMask, fgMask, 100, 255, cv::THRESH_BINARY);
    cv::medianBlur(fgMask, fgMask, 3);
    
    // HEAVY morphological processing
    cv::Mat kernelOpen = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(3, 3));
    cv::Mat kernelClose = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(7, 7));
    cv::morphologyEx(fgMask, fgMask, cv::MORPH_OPEN, kernelOpen, cv::Point(-1, -1), 1);
    cv::morphologyEx(fgMask, fgMask, cv::MORPH_CLOSE, kernelClose, cv::Point(-1, -1), 2);
    cv::dilate(fgMask, fgMask, cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(5, 5)), cv::Point(-1, -1), 1);
    
    // Find contours
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(fgMask.clone(), contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    
    if (contours.empty()) return false;
    
    // VERY RELAXED filtering - accept almost anything that looks vaguely car-like
    std::vector<std::pair<double, size_t>> candidates;
    const double minArea = 20.0;      // VERY LOW
    const double maxArea = 30000.0;   // VERY HIGH
    const int minWH = 5;              // TINY
    const int maxWH = 500;            // HUGE
    
    for (size_t i = 0; i < contours.size(); ++i) {
        const auto& c = contours[i];
        double area = cv::contourArea(c);
        
        if (area < minArea || area > maxArea) continue;
        
        cv::Rect bbox = cv::boundingRect(c);
        if (bbox.width < minWH || bbox.height < minWH) continue;
        if (bbox.width > maxWH || bbox.height > maxWH) continue;
        
        double score = area; // Prefer larger blobs
        if (lastCenter_.x != 0 || lastCenter_.y != 0) {
            cv::Moments m = cv::moments(c);
            if (std::abs(m.m00) <= 1e-9) continue;
            
            int cx = static_cast<int>(m.m10 / m.m00);
            int cy = static_cast<int>(m.m01 / m.m00);
            
            double dist = std::hypot(cx - lastCenter_.x, cy - lastCenter_.y);
            if (dist > 400.0) continue; // Large tolerance for jumps
            score = -dist + 0.001 * area; // Prefer closer + larger
        }
        
        candidates.push_back({score, i});
    }
    
    if (candidates.empty()) return false;
    
    // Pick best candidate
    auto best = std::max_element(candidates.begin(), candidates.end(),
        [](const auto& a, const auto& b) { return a.first < b.first; });
    
    size_t bestIdx = best->second;
    cv::Rect bbox = cv::boundingRect(contours[bestIdx]);
    cv::Point center(bbox.x + bbox.width / 2, bbox.y + bbox.height / 2);
    cv::Point movement(0, 0);
    
    if (lastCenter_.x != 0 || lastCenter_.y != 0) {
        movement = cv::Point(center.x - lastCenter_.x, center.y - lastCenter_.y);
    }
    lastCenter_ = center;

    result.bbox = bbox;
    result.center = center;
    result.movement = movement;
    return true;
}

bool ControlOrchestrator::detectRedCarInitial(const cv::Mat& frame, TrackedObject& result) {
    if (frame.empty()) return false;

    // Convert to HSV for red color detection
    cv::Mat hsv;
    cv::cvtColor(frame, hsv, cv::COLOR_BGR2HSV);

    // Red color detection (two hue ranges - optimized for DRiFT car)
    cv::Mat mask1, mask2;
    // Lower red hue range (0-12)
    cv::inRange(hsv, cv::Scalar(0, 100, 40), cv::Scalar(12, 255, 255), mask1);
    // Upper red hue range (168-180)
    cv::inRange(hsv, cv::Scalar(168, 100, 40), cv::Scalar(180, 255, 255), mask2);
    cv::Mat redMask = mask1 | mask2;

    // Morphological operations to clean up noise
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(5, 5));
    cv::morphologyEx(redMask, redMask, cv::MORPH_OPEN, kernel, cv::Point(-1, -1), 2);
    cv::morphologyEx(redMask, redMask, cv::MORPH_DILATE, kernel, cv::Point(-1, -1), 1);

    // Find contours
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(redMask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    
    if (contours.empty()) {
        std::cout << "[!] No red car detected in frame\n";
        return false;
    }

    // Filter candidates based on area and properties
    std::vector<DetectionCandidate> candidates;
    
    for (size_t i = 0; i < contours.size(); ++i) {
        const auto& c = contours[i];
        double area = cv::contourArea(c);
        
        // Area filter: 80-8000 pixels for initial detection (car can be far or close)
        if (area < 80.0 || area > 8000.0) continue;
        
        cv::Rect bbox = cv::boundingRect(c);
        
        // Width/height filter
        if (bbox.width < 8 || bbox.height < 8) continue;
        if (bbox.width > 300 || bbox.height > 300) continue;
        
        // Aspect ratio filter (car is roughly square)
        double aspectRatio = (bbox.height > 0) ? (double)bbox.width / bbox.height : 99.0;
        if (aspectRatio < 0.35 || aspectRatio > 2.85) continue;
        
        // Solidity filter (convex blob check)
        std::vector<cv::Point> hull;
        cv::convexHull(c, hull);
        double hullArea = cv::contourArea(hull);
        double solidity = (hullArea > 1e-9) ? area / hullArea : 0.0;
        if (solidity < 0.25) continue;
        
        // Calculate center of mass
        cv::Moments m = cv::moments(c);
        if (std::abs(m.m00) <= 1e-9) continue;
        
        int cx = static_cast<int>(m.m10 / m.m00);
        int cy = static_cast<int>(m.m01 / m.m00);
        
        candidates.push_back({
            static_cast<int>(i),
            area,
            cx, cy,
            bbox,
            solidity
        });
    }
    
    if (candidates.empty()) {
        std::cout << "[!] No valid red objects found (failed area/shape filters)\n";
        return false;
    }

    // Pick the largest red blob (most likely the car)
    auto largest = *std::max_element(candidates.begin(), candidates.end(),
        [](const DetectionCandidate& a, const DetectionCandidate& b) {
            return a.area < b.area;
        });

    result.bbox = largest.bbox;
    result.center = cv::Point(largest.cx, largest.cy);
    result.movement = cv::Point(0, 0); // No movement for initial detection
    lastCenter_ = result.center;
    
    std::cout << "[✓] RED CAR DETECTED: pos=(" << result.center.x << "," << result.center.y 
              << ") size=" << result.bbox.width << "x" << result.bbox.height 
              << " area=" << static_cast<int>(largest.area) << "px\n";
    
    return true;
}

bool ControlOrchestrator::detectRedCarRealtime(const cv::Mat& frame, TrackedObject& result) {
    if (frame.empty()) return false;

    // Convert to HSV
    cv::Mat hsv;
    cv::cvtColor(frame, hsv, cv::COLOR_BGR2HSV);

    // ULTRA-RELAXED HSV ranges for BRIGHT RED/ORANGE car
    // Strategy: Accept ANY pixel that looks remotely red/orange
    cv::Mat mask1, mask2, mask3, mask4, mask5, mask6;
    
    // Pure red ranges (wraparound in HSV)
    cv::inRange(hsv, cv::Scalar(0, 20, 60), cv::Scalar(20, 255, 255), mask1);       // Red 0-20 (low sat OK)
    cv::inRange(hsv, cv::Scalar(150, 20, 60), cv::Scalar(180, 255, 255), mask2);   // Red 150-180 (low sat OK)
    
    // Orange ranges
    cv::inRange(hsv, cv::Scalar(8, 30, 70), cv::Scalar(35, 255, 255), mask3);      // Orange 8-35
    
    // Very bright red (low saturation but high value)
    cv::inRange(hsv, cv::Scalar(0, 10, 120), cv::Scalar(25, 180, 255), mask4);     // Bright red/orange
    cv::inRange(hsv, cv::Scalar(140, 10, 120), cv::Scalar(180, 180, 255), mask5);  // Bright red wrap
    
    // Fallback: anything in frame that's clearly not gray/black/white
    cv::Mat redMask = mask1 | mask2 | mask3 | mask4 | mask5;

    // Aggressive morphological cleanup
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(5, 5));
    cv::morphologyEx(redMask, redMask, cv::MORPH_CLOSE, kernel);
    cv::morphologyEx(redMask, redMask, cv::MORPH_OPEN, kernel);
    
    // Find contours
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(redMask.clone(), contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    
    if (contours.empty()) {
        return false;
    }

    // Pick best candidate with relaxed area constraints
    std::vector<DetectionCandidate> candidates;
    const double minArea = 20.0;      // VERY LOW - catch small cars
    const double maxArea = 20000.0;   // VERY HIGH
    const int minWH = 5;              // Very small
    const int maxWH = 500;            // Very large
    const double minSolidity = 0.15;  // RELAXED
    const double maxJumpPx = 500.0;   // Large jump distance
    
    for (size_t i = 0; i < contours.size(); ++i) {
        const auto& c = contours[i];
        double area = cv::contourArea(c);
        
        if (area < minArea || area > maxArea) continue;
        
        cv::Rect bbox = cv::boundingRect(c);
        if (bbox.width < minWH || bbox.height < minWH) continue;
        if (bbox.width > maxWH || bbox.height > maxWH) continue;
        
        double aspectRatio = (bbox.height > 0) ? (double)bbox.width / bbox.height : 99.0;
        // RELAXED aspect ratio
        if (aspectRatio < 0.2 || aspectRatio > 5.0) continue;
        
        std::vector<cv::Point> hull;
        cv::convexHull(c, hull);
        double hullArea = cv::contourArea(hull);
        double solidity = (hullArea > 1e-9) ? area / hullArea : 0.0;
        if (solidity < minSolidity) continue;
        
        cv::Moments m = cv::moments(c);
        if (std::abs(m.m00) <= 1e-9) continue;
        
        int cx = static_cast<int>(m.m10 / m.m00);
        int cy = static_cast<int>(m.m01 / m.m00);
        
        // Score: proximity to last position + prefer larger blobs
        double dist = (lastCenter_.x != 0 || lastCenter_.y != 0) ?
            std::hypot(cx - lastCenter_.x, cy - lastCenter_.y) : 0;
        
        if (dist > maxJumpPx && (lastCenter_.x != 0 || lastCenter_.y != 0)) continue;
        
        double score = (lastCenter_.x != 0 || lastCenter_.y != 0) ?
            (-dist + 0.001 * area) : area;
        
        candidates.push_back({
            static_cast<int>(i),
            area,
            cx, cy,
            bbox,
            solidity,
            score
        });
    }
    
    if (candidates.empty()) {
        return false;
    }

    // Pick best candidate
    auto best = *std::max_element(candidates.begin(), candidates.end(),
        [](const DetectionCandidate& a, const DetectionCandidate& b) {
            return a.score < b.score;
        });

    cv::Point center(best.cx, best.cy);
    cv::Point movement(0, 0);
    
    if (lastCenter_.x != 0 || lastCenter_.y != 0) {
        movement = cv::Point(center.x - lastCenter_.x, center.y - lastCenter_.y);
    }
    lastCenter_ = center;

    result.bbox = best.bbox;
    result.center = center;
    result.movement = movement;
    return true;
}

