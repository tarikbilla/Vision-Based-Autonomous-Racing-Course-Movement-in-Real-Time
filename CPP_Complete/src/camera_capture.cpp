#include "camera_capture.hpp"
#include <iostream>
#include <chrono>
#include <thread>

// ========================
// CameraCapture Implementation
// ========================

CameraCapture::CameraCapture(int source, int width, int height, int fps)
    : width_(width), height_(height), fps_(fps), isOpened_(false), source_(source) {
}

CameraCapture::~CameraCapture() {
    close();
}

void CameraCapture::open() {
    if (isOpened_) {
        close();
    }
    
    cap_.open(source_);
    
    if (!cap_.isOpened()) {
        // Try to find available camera
        std::cout << "[!] Camera source " << source_ << " failed. Searching for available cameras...\n";
        for (int i = 0; i < 10; ++i) {
            cap_.open(i);
            if (cap_.isOpened()) {
                std::cout << "[✓] Found camera at index " << i << "\n";
                source_ = i;
                break;
            }
            cap_.release();
        }
    }
    
    if (!cap_.isOpened()) {
        throw std::runtime_error("Camera capture failed to open (tried indices 0-9)");
    }
    
    cap_.set(cv::CAP_PROP_FRAME_WIDTH, width_);
    cap_.set(cv::CAP_PROP_FRAME_HEIGHT, height_);
    cap_.set(cv::CAP_PROP_FPS, fps_);
    
    warmupCamera();
    isOpened_ = true;
}

void CameraCapture::close() {
    if (cap_.isOpened()) {
        cap_.release();
    }
    isOpened_ = false;
}

void CameraCapture::warmupCamera() {
    cv::Mat frame;
    for (int i = 0; i < 3; ++i) {
        cap_ >> frame;
    }
}

bool CameraCapture::getFrame(Frame& frame) {
    if (!isOpened_) {
        open();
    }
    
    cv::Mat mat;
    if (!cap_.read(mat)) {
        return false;
    }
    
    frame.image = mat;
    frame.timestamp = std::chrono::system_clock::now().time_since_epoch().count() / 1e9;
    return true;
}

bool CameraCapture::isOpened() const {
    return isOpened_;
}

// ========================
// SimulatedCamera Implementation
// ========================

SimulatedCamera::SimulatedCamera(int width, int height, int fps)
    : CameraCapture(0, width, height, fps), frameCount_(0) {
}

SimulatedCamera::~SimulatedCamera() {
    close();
}

void SimulatedCamera::open() {
    isOpened_ = true;
    frameCount_ = 0;
}

void SimulatedCamera::close() {
    isOpened_ = false;
}

bool SimulatedCamera::getFrame(Frame& frame) {
    if (!isOpened_) {
        open();
    }
    
    // Generate simulated frame with moving green dot
    cv::Mat image(height_, width_, CV_8UC3, cv::Scalar(0, 0, 0));
    
    int cx = (width_ / 2) + (width_ / 4) * std::sin(frameCount_ * 0.08);
    int cy = (height_ / 2) + (height_ / 6) * std::cos(frameCount_ * 0.08);
    int radius = 12;
    
    cv::circle(image, cv::Point(cx, cy), radius, cv::Scalar(0, 255, 0), -1);
    
    frame.image = image;
    frame.timestamp = std::chrono::system_clock::now().time_since_epoch().count() / 1e9;
    
    frameCount_++;
    std::this_thread::sleep_for(std::chrono::milliseconds(1000 / fps_));
    
    return true;
}

bool SimulatedCamera::isOpened() const {
    return isOpened_;
}
