#ifndef CAMERA_CAPTURE_H
#define CAMERA_CAPTURE_H

#include <opencv2/opencv.hpp>
#include <thread>
#include <atomic>
#include <mutex>
#include "types.h"

namespace rc_car {

class CameraCapture {
private:
    cv::VideoCapture cap_;
    std::thread capture_thread_;
    std::atomic<bool> running_;
    std::atomic<bool> paused_;
    
    int camera_index_;
    int target_width_;
    int target_height_;
    int target_fps_;
    
    cv::Mat current_frame_;
    std::mutex frame_mutex_;
    
    void captureLoop();
    
public:
    CameraCapture();
    explicit CameraCapture(int camera_index);
    ~CameraCapture();
    
    bool initialize(int camera_index = 0, int width = 1920, int height = 1080, int fps = 30);
    bool initialize(const std::string& video_source);
    
    bool start();
    void stop();
    void pause();
    void resume();
    
    bool getFrame(cv::Mat& frame);
    bool isRunning() const { return running_; }
    bool isOpened() const { return cap_.isOpened(); }
    
    void setResolution(int width, int height);
    void setFPS(int fps);
    
    int getWidth() const { return target_width_; }
    int getHeight() const { return target_height_; }
    int getFPS() const { return target_fps_; }
};

} // namespace rc_car

#endif // CAMERA_CAPTURE_H
