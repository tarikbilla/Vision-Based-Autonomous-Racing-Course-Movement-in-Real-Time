# Fix Compilation Errors for OpenCV 4.10.0

## Quick Fix Instructions

If you're getting compilation errors on your Raspberry Pi, apply these fixes to your source files.

## File 1: `src/object_tracker.cpp`

### Fix 1: Replace MOSSE tracker (around line 34-35)

**OLD:**
```cpp
case TrackerType::MOSSE:
    return cv::TrackerMOSSE::create();
```

**NEW:**
```cpp
case TrackerType::MOSSE:
    // MOSSE tracker was removed in OpenCV 4.5.1+, fallback to KCF
    std::cerr << "Warning: MOSSE tracker not available in OpenCV 4.10.0, using KCF instead" << std::endl;
    return cv::TrackerKCF::create();
```

### Fix 2: Replace init() call (around line 63)

**OLD:**
```cpp
if (!tracker_->init(frame, bbox)) {
    std::cerr << "Error: Failed to initialize tracker" << std::endl;
    initialized_ = false;
    return false;
}
```

**NEW:**
```cpp
// In OpenCV 4.x, init() returns void, not bool
try {
    tracker_->init(frame, bbox);
} catch (const cv::Exception& e) {
    std::cerr << "Error: Failed to initialize tracker: " << e.what() << std::endl;
    initialized_ = false;
    return false;
}
```

### Fix 3: Replace update() call (around line 96)

**OLD:**
```cpp
bool ok = tracker_->update(frame, bbox_);
```

**NEW:**
```cpp
// Update tracker - need cv::Rect (int) not cv::Rect2d (double)
cv::Rect bbox_int;
bool ok = tracker_->update(frame, bbox_int);

// Convert back to Rect2d for storage
bbox_ = cv::Rect2d(bbox_int);
```

## File 2: `src/boundary_detection.cpp`

### Fix 4: Remove unused variable (around line 123)

**OLD:**
```cpp
int min_distance = ray_max_length_;
int max_distance = 0;
int min_index = 0;  // <-- Remove this line
int max_index = 0;
```

**NEW:**
```cpp
int min_distance = ray_max_length_;
int max_distance = 0;
int max_index = 0;
```

And remove the line that sets `min_index`:
```cpp
// Remove this line:
if (rays_[i].distance < min_distance) {
    min_distance = rays_[i].distance;
    min_index = static_cast<int>(i);  // <-- Remove this line
}
```

**Keep only:**
```cpp
if (rays_[i].distance < min_distance) {
    min_distance = rays_[i].distance;
}
```

## File 3: `src/camera_capture.cpp`

### Fix 5: Fix initialization order (around line 13-15)

**OLD:**
```cpp
CameraCapture::CameraCapture() 
    : camera_index_(0), target_width_(1920), target_height_(1080), target_fps_(30),
      running_(false), paused_(false) {
}
```

**NEW:**
```cpp
CameraCapture::CameraCapture() 
    : running_(false), paused_(false),
      camera_index_(0), target_width_(1920), target_height_(1080), target_fps_(30) {
}
```

**Also fix the other constructor (around line 18):**

**OLD:**
```cpp
CameraCapture::CameraCapture(int camera_index)
    : camera_index_(camera_index), target_width_(1920), target_height_(1080), target_fps_(30),
      running_(false), paused_(false) {
}
```

**NEW:**
```cpp
CameraCapture::CameraCapture(int camera_index)
    : running_(false), paused_(false),
      camera_index_(camera_index), target_width_(1920), target_height_(1080), target_fps_(30) {
}
```

## Quick Apply Script

You can also use `sed` to apply these fixes automatically:

```bash
cd ~/Desktop/IoT-Project-Vision-based-autonomous-RC-car-control-system

# Fix 1: MOSSE tracker
sed -i 's/return cv::TrackerMOSSE::create();/\/\/ MOSSE tracker was removed in OpenCV 4.5.1+, fallback to KCF\n            std::cerr << "Warning: MOSSE tracker not available in OpenCV 4.10.0, using KCF instead" << std::endl;\n            return cv::TrackerKCF::create();/' src/object_tracker.cpp

# Fix 2: init() call - manual edit required (too complex for sed)

# Fix 3: update() call - manual edit required (too complex for sed)

# Fix 4: Remove unused min_index
sed -i '/int min_index = 0;/d' src/boundary_detection.cpp
sed -i 's/min_index = static_cast<int>(i);//' src/boundary_detection.cpp

# Fix 5: Initialization order - manual edit required
```

## After Applying Fixes

```bash
cd ~/Desktop/IoT-Project-Vision-based-autonomous-RC-car-control-system/build
rm -rf *
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j4
```

## Alternative: Copy Fixed Files

If you have access to the updated files, you can simply copy them:

```bash
# From your updated source location, copy:
# - src/object_tracker.cpp
# - src/boundary_detection.cpp  
# - src/camera_capture.cpp

# To your Raspberry Pi project directory
```
