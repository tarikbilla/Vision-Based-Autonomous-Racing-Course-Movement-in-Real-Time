#ifndef CAMERA_CAPTURE_HPP
#define CAMERA_CAPTURE_HPP

#include <string>
#include <opencv2/opencv.hpp>
#include <memory>

struct Frame {
    cv::Mat image;
    double timestamp;
};

class CameraCapture {
public:
    CameraCapture(int source, int width, int height, int fps);
    virtual ~CameraCapture();
    
    virtual void open();
    virtual void close();
    virtual bool getFrame(Frame& frame);
    virtual bool isOpened() const;
    
protected:
    int width_;
    int height_;
    int fps_;
    bool isOpened_;
    
private:
    int source_;
    cv::VideoCapture cap_;
    
    void warmupCamera();
};

class SimulatedCamera : public CameraCapture {
public:
    SimulatedCamera(int width, int height, int fps);
    ~SimulatedCamera();
    
    void open() override;
    void close() override;
    bool getFrame(Frame& frame) override;
    bool isOpened() const override;
    
private:
    int frameCount_;
};

#endif // CAMERA_CAPTURE_HPP
