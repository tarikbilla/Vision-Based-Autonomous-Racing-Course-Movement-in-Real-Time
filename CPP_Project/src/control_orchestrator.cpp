#include "control_orchestrator.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <sstream>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

namespace rc_car {

ControlOrchestrator::ControlOrchestrator()
    : running_(false), tracking_enabled_(false), guidance_enabled_(false),
      autonomous_mode_(false), tracker_type_(TrackerType::CSRT), base_speed_(10),
      show_ui_(true) {
}

ControlOrchestrator::~ControlOrchestrator() {
    stop();
}

bool ControlOrchestrator::initialize(const std::string& config_file) {
    // Load configuration
    config_ = std::make_unique<ConfigManager>(config_file);
    
    // Initialize camera
    int camera_index = config_->getInt("camera.index", 0);
    int width = config_->getInt("camera.width", 1920);
    int height = config_->getInt("camera.height", 1080);
    int fps = config_->getInt("camera.fps", 30);
    
    camera_ = std::make_unique<CameraCapture>();
    if (!camera_->initialize(camera_index, width, height, fps)) {
        std::cerr << "Error: Failed to initialize camera" << std::endl;
        return false;
    }
    
    // Initialize tracker
    std::string tracker_type_str = config_->getString("tracker.type", "CSRT");
    if (tracker_type_str == "GOTURN") {
        tracker_type_ = TrackerType::GOTURN;
    } else if (tracker_type_str == "CSRT") {
        tracker_type_ = TrackerType::CSRT;
    } else if (tracker_type_str == "KCF") {
        tracker_type_ = TrackerType::KCF;
    } else if (tracker_type_str == "MOSSE") {
        tracker_type_ = TrackerType::MOSSE;
    }
    
    tracker_ = std::make_unique<ObjectTracker>(tracker_type_);
    
    // Initialize boundary detection
    int black_threshold = config_->getInt("boundary.black_threshold", 50);
    int ray_max_length = config_->getInt("boundary.ray_max_length", 200);
    int evasive_threshold = config_->getInt("boundary.evasive_threshold", 80);
    base_speed_ = config_->getInt("boundary.base_speed", 10);
    
    guidance_ = std::make_unique<BoundaryDetection>(black_threshold, ray_max_length, evasive_threshold);
    
    // Parse ray angles
    std::string ray_angles_str = config_->getString("boundary.ray_angles", "-60,0,60");
    std::vector<double> ray_angles;
    std::stringstream ss(ray_angles_str);
    std::string angle_str;
    while (std::getline(ss, angle_str, ',')) {
        try {
            ray_angles.push_back(std::stod(angle_str));
        } catch (...) {
            // Skip invalid angles
        }
    }
    if (!ray_angles.empty()) {
        guidance_->setRayAngles(ray_angles);
    }
    
    // Initialize BLE handler
    std::string device_mac = config_->getString("ble.device_mac", "f9:af:3c:e2:d2:f5");
    std::string characteristic_uuid = config_->getString("ble.characteristic_uuid", 
                                                          "6e400002-b5a3-f393-e0a9-e50e24dcca9e");
    int command_rate = config_->getInt("ble.command_rate_hz", 200);
    
    ble_handler_ = std::make_unique<BLEHandler>(device_mac, characteristic_uuid);
    ble_handler_->setCommandRate(command_rate);
    
    // UI settings
    show_ui_ = config_->getBool("system.show_ui", true);
    autonomous_mode_ = config_->getBool("system.autonomous_mode", false);
    
    std::cout << "Control orchestrator initialized successfully" << std::endl;
    return true;
}

bool ControlOrchestrator::start() {
    if (running_) {
        return true;
    }
    
    // Start camera
    if (!camera_->start()) {
        std::cerr << "Error: Failed to start camera" << std::endl;
        return false;
    }
    
    // Wait for first frame
    cv::Mat first_frame;
    int attempts = 0;
    while (!camera_->getFrame(first_frame) && attempts < 50) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        attempts++;
    }
    
    if (first_frame.empty()) {
        std::cerr << "Error: Could not capture first frame" << std::endl;
        return false;
    }
    
    // Select ROI for tracking
    std::cout << "Select the object (car) to track in the window..." << std::endl;
    cv::Rect2d bbox = ObjectTracker::selectROI(first_frame, "Select Object to Track");
    
    if (bbox.width <= 0 || bbox.height <= 0) {
        std::cerr << "Error: Invalid ROI selected" << std::endl;
        return false;
    }
    
    // Initialize tracker
    if (!tracker_->initialize(first_frame, bbox, tracker_type_)) {
        std::cerr << "Error: Failed to initialize tracker" << std::endl;
        return false;
    }
    
    // Connect to BLE device
    if (!ble_handler_->connect()) {
        std::cerr << "Warning: Failed to connect to BLE device. Continuing without BLE..." << std::endl;
        // Continue anyway for testing
    }
    
    // Start threads
    running_ = true;
    tracking_enabled_ = true;
    guidance_enabled_ = true;
    
    tracking_thread_ = std::thread(&ControlOrchestrator::trackingLoop, this);
    guidance_thread_ = std::thread(&ControlOrchestrator::guidanceLoop, this);
    ble_thread_ = std::thread(&ControlOrchestrator::bleLoop, this);
    
    // Start BLE sending
    if (ble_handler_->isConnected()) {
        ble_handler_->startSending();
    }
    
    std::cout << "System started successfully" << std::endl;
    return true;
}

void ControlOrchestrator::stop() {
    if (!running_) {
        return;
    }
    
    std::cout << "Stopping system..." << std::endl;
    
    running_ = false;
    tracking_enabled_ = false;
    guidance_enabled_ = false;
    
    // Emergency stop
    emergencyStop();
    
    // Stop camera
    if (camera_) {
        camera_->stop();
    }
    
    // Stop BLE sending
    if (ble_handler_) {
        ble_handler_->stopSending();
        ble_handler_->disconnect();
    }
    
    // Wait for threads
    if (tracking_thread_.joinable()) {
        tracking_thread_.join();
    }
    if (guidance_thread_.joinable()) {
        guidance_thread_.join();
    }
    if (ble_thread_.joinable()) {
        ble_thread_.join();
    }
    
    std::cout << "System stopped" << std::endl;
}

void ControlOrchestrator::trackingLoop() {
    cv::Mat frame;
    TrackingResult result;
    
    while (running_) {
        if (!tracking_enabled_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        
        // Get frame from camera
        if (!camera_->getFrame(frame)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        
        // Push frame to queue
        frame_queue_.push(frame.clone());
        
        // Update tracker
        if (tracker_->isInitialized()) {
            tracker_->update(frame, result);
            
            // Push tracking result
            tracking_queue_.push(result);
            
            // Display if UI enabled
            if (show_ui_) {
                cv::Mat display_frame = frame.clone();
                
                // Draw bounding box
                if (!result.tracking_lost) {
                    cv::rectangle(display_frame, result.bbox, cv::Scalar(255, 0, 0), 2);
                    cv::circle(display_frame, cv::Point(result.midpoint.x, result.midpoint.y), 
                              5, cv::Scalar(0, 255, 0), -1);
                    
                    // Draw movement vector
                    if (result.movement.dx != 0 || result.movement.dy != 0) {
                        cv::Point end(result.midpoint.x + result.movement.dx,
                                     result.midpoint.y + result.movement.dy);
                        cv::arrowedLine(display_frame, 
                                       cv::Point(result.midpoint.x, result.midpoint.y),
                                       end, cv::Scalar(0, 255, 255), 2);
                    }
                }
                
                cv::putText(display_frame, result.tracking_lost ? "TRACKING LOST" : "TRACKING",
                           cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX, 1.0,
                           result.tracking_lost ? cv::Scalar(0, 0, 255) : cv::Scalar(0, 255, 0), 2);
                
                cv::imshow("Tracking", display_frame);
                cv::waitKey(1);
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void ControlOrchestrator::guidanceLoop() {
    cv::Mat frame;
    TrackingResult tracking_result;
    ControlVector control;
    
    while (running_) {
        if (!guidance_enabled_ || !autonomous_mode_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        
        // Get frame and tracking result
        if (frame_queue_.empty() || tracking_queue_.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        
        // Get latest frame (discard old ones)
        while (frame_queue_.try_pop(frame)) {
            // Keep getting latest frame
        }
        
        // Get latest tracking result
        if (!tracking_queue_.try_pop(tracking_result)) {
            continue;
        }
        
        if (tracking_result.tracking_lost) {
            // Send stop command if tracking lost
            control = ControlVector(0, 0, 0, 0);
        } else {
            // Process boundary detection
            control = guidance_->process(frame, tracking_result.midpoint, 
                                        tracking_result.movement, base_speed_);
        }
        
        // Push control command
        control_queue_.push(control);
        
        // Display rays if UI enabled
        if (show_ui_ && !frame.empty() && !tracking_result.tracking_lost) {
            cv::Mat display_frame = frame.clone();
            guidance_->drawRays(display_frame, tracking_result.midpoint);
            
            std::string info = "Speed: " + std::to_string(control.speed) +
                              " L:" + std::to_string(control.left_turn) +
                              " R:" + std::to_string(control.right_turn);
            cv::putText(display_frame, info, cv::Point(10, 60),
                       cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(255, 255, 255), 2);
            
            cv::imshow("Guidance", display_frame);
            cv::waitKey(1);
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void ControlOrchestrator::bleLoop() {
    ControlVector control;
    
    while (running_) {
        if (!autonomous_mode_ || !ble_handler_->isConnected()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        
        // Get latest control command
        if (control_queue_.try_pop(control)) {
            // Keep getting latest command (discard old ones)
            while (control_queue_.try_pop(control)) {
                // Keep updating to latest
            }
            
            // Update BLE handler
            ble_handler_->setControl(control);
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void ControlOrchestrator::setManualControl(const ControlVector& control) {
    if (ble_handler_ && ble_handler_->isConnected()) {
        ble_handler_->setControl(control);
    }
}

void ControlOrchestrator::emergencyStop() {
    if (ble_handler_) {
        ble_handler_->emergencyStop();
    }
}

} // namespace rc_car
