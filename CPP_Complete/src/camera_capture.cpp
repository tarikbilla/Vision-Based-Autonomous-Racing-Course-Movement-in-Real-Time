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
    
    // Prefer V4L2 backend on Linux to avoid GStreamer pipeline issues
    // Try opening with CAP_V4L2 first, then fall back to default
    cap_.open(source_, cv::CAP_V4L2);
    if (!cap_.isOpened()) {
        cap_.open(source_);
    }
    
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
        // Try to find available camera across indices with V4L2 preference
        std::cout << "[!] Camera source " << source_ << " failed. Searching for available cameras...\n";
        for (int i = 0; i < 10; ++i) {
            cap_.open(i, cv::CAP_V4L2);
            if (!cap_.isOpened()) cap_.open(i);
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
    
    // Warmup and verify at least one frame can be grabbed. If not, try a
    // GStreamer pipeline fallback (useful for some USB capture devices
    // and capture cards on embedded Linux where default backends fail).
    warmupCamera();

    cv::Mat test;
    if (!cap_.read(test) || test.empty()) {
        std::cerr << "[!] Camera read verification failed after open (backend may be unstable)\n";

        // Attempt GStreamer fallback using the device path (/dev/videoN)
        std::string devicePath = "/dev/video" + std::to_string(source_);
        std::string pipeline = "v4l2src device=" + devicePath
            + " ! video/x-raw, width=(int)" + std::to_string(width_)
            + ", height=(int)" + std::to_string(height_)
            + ", framerate=" + std::to_string(fps_) + "/1"
            + " ! videoconvert ! appsink drop=true";

        std::cerr << "[*] Trying GStreamer fallback pipeline: " << pipeline << "\n";
        cap_.release();
        if (cap_.open(pipeline, cv::CAP_GSTREAMER)) {
            warmupCamera();
            cv::Mat gstTest;
            if (cap_.read(gstTest) && !gstTest.empty()) {
                std::cerr << "[✓] GStreamer fallback succeeded\n";
                isOpened_ = true;
                return;
            } else {
                std::cerr << "[!] GStreamer fallback opened but failed to read frames\n";
                cap_.release();
            }
        } else {
            std::cerr << "[!] GStreamer fallback could not open (OpenCV may not have GStreamer support)\n";
        }

        throw std::runtime_error("Camera opened but failed to deliver frames (backend issues)");
    }

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
    // Read a few frames to allow camera to warm up; verify each read
    for (int i = 0; i < 5; ++i) {
        if (!cap_.read(frame) || frame.empty()) {
            // brief pause and retry
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
        // got a valid frame, short delay to stabilize
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
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
