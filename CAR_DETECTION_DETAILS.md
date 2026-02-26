# Car Detection Details

## Overview
The system uses multiple detection methods to track the RC car in the video frame. Detection runs at ~15Hz for optimal control frequency.

## Detection Methods (Priority Order)

### 1. Motion Detection (PRIMARY - Default)
**Best for:** Any car color, reliable tracking, works in varied lighting

#### Step-by-Step Process:

**Step 1: Initialize Background Subtractor**
```cpp
bgSubtractor_ = cv::createBackgroundSubtractorMOG2(500, 16, true);
// Parameters: history=500 frames, varThreshold=16, detectShadows=true
```

**Step 2: Convert Frame to Grayscale**
```cpp
cv::Mat gray;
cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
```

**Step 3: Apply Background Subtraction**
```cpp
bgSubtractor_->apply(gray, motionMask, learningRate);
// learningRate = 0.003 for slow background adaptation
```

**Step 4: Threshold Motion Mask**
```cpp
cv::threshold(motionMask, motionMask, 80, 255, cv::THRESH_BINARY);
// Aggressive threshold (80) to isolate strong motion
```

**Step 5: Morphological Operations**
```cpp
// Remove noise
cv::morphologyEx(motionMask, motionMask, cv::MORPH_OPEN, kernel3x3);

// Fill small holes
cv::morphologyEx(motionMask, motionMask, cv::MORPH_CLOSE, kernel5x5);

// Slightly expand blobs
cv::dilate(motionMask, motionMask, kernel3x3);
```

**Step 6: Find Contours**
```cpp
std::vector<std::vector<cv::Point>> contours;
cv::findContours(motionMask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
```

**Step 7: Filter Candidates**
For each contour, check:
- **Area**: Between 200-20000 pixels
- **Bounding box size**: Width/height between 15-300 pixels
- **Aspect ratio**: Between 0.35-2.85 (roughly car-shaped)
- **Solidity**: > 0.45 (compact shape, not scattered)
- **Distance from previous**: < 100 pixels (if car was previously detected)

**Step 8: Select Best Candidate**
```cpp
// If no previous position: Choose largest area
// If previous position exists: Choose closest candidate (with area bonus)
score = distance - 0.0008 * area
```

**Step 9: Calculate Car Center and Movement**
```cpp
cv::Moments m = cv::moments(contour);
int cx = (int)(m.m10 / m.m00);
int cy = (int)(m.m01 / m.m00);

movement.x = cx - previousCenter.x;
movement.y = cy - previousCenter.y;
```

### 2. Color-Based Detection (FALLBACK)
**Best for:** Red cars, when motion detection fails

#### Step-by-Step Process:

**Step 1: Convert to HSV Color Space**
```cpp
cv::Mat hsv;
cv::cvtColor(frame, hsv, cv::COLOR_BGR2HSV);
```

**Step 2: Define Red Color Ranges**
```cpp
// Red wraps around in HSV, so we need two ranges:
// Range 1: H=0-12, S=70-255, V=50-255
// Range 2: H=168-179, S=70-255, V=50-255
```

**Step 3: Create Red Mask**
```cpp
cv::Mat red1, red2, redMask;
cv::inRange(hsv, cv::Scalar(0, 70, 50), cv::Scalar(12, 255, 255), red1);
cv::inRange(hsv, cv::Scalar(168, 70, 50), cv::Scalar(179, 255, 255), red2);
cv::bitwise_or(red1, red2, redMask);
```

**Step 4: Morphological Cleanup**
```cpp
cv::morphologyEx(redMask, redMask, cv::MORPH_OPEN, kernel5x5);
cv::morphologyEx(redMask, redMask, cv::MORPH_CLOSE, kernel7x7);
```

**Step 5: Find Red Contours**
```cpp
std::vector<std::vector<cv::Point>> contours;
cv::findContours(redMask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
```

**Step 6: Filter by Area and Shape**
- Minimum area: 300 pixels
- Maximum area: 15000 pixels
- Aspect ratio: 0.4 to 2.5

**Step 7: Select Largest Red Blob**
```cpp
auto largest = std::max_element(contours, 
    [](const auto& a, const auto& b) {
        return cv::contourArea(a) < cv::contourArea(b);
    });
```

### 3. Manual Tracker (BACKUP)
**Best for:** When automatic detection fails, manual ROI selection

#### Step-by-Step Process:

**Step 1: User Selects ROI**
- Press 's' to enter ROI selection mode
- Draw rectangle around car
- Press ENTER to confirm

**Step 2: Initialize Tracker**
```cpp
tracker_ = cv::TrackerCSRT::create();
tracker_->init(frame, roi);
```

**Step 3: Update Tracker Each Frame**
```cpp
bool success = tracker_->update(frame, roi);
if (success) {
    center.x = roi.x + roi.width / 2;
    center.y = roi.y + roi.height / 2;
}
```

## Detection Performance

### Frame Processing Rate
- **Target rate**: 15 FPS (66ms per frame)
- **Camera capture**: 15 FPS
- **Analysis interval**: 66ms minimum between analyses

### Warmup Period
- **First 30 frames**: Background model initialization
- **Motion detection enabled**: After frame 30
- **Centerline building**: Every 30 frames

## Detection Output

**TrackedObject Structure:**
```cpp
struct TrackedObject {
    cv::Rect bbox;         // Bounding box
    cv::Point center;      // Center point (cx, cy)
    cv::Point movement;    // Movement vector (dx, dy)
    bool valid;            // Detection success flag
    std::string method;    // "MOTION", "COLOR", or "TRACK"
};
```

## Visualization

**On-screen display shows:**
- **Green circle**: Car center point
- **Bounding box**: Detection area
- **Movement vector**: Direction of motion (arrow)
- **Detection method**: "MOTION", "COLOR", or "TRACK" label
- **Frame counter**: Current frame number

## Troubleshooting

### Motion Detection Issues
1. **No detection**: Increase learning rate (0.003 → 0.01)
2. **Too sensitive**: Increase threshold (80 → 100)
3. **Fragmented detection**: Increase CLOSE kernel size

### Color Detection Issues
1. **No red found**: Adjust HSV ranges (lighting dependent)
2. **Too many false positives**: Increase minimum area threshold
3. **Shadows detected**: Lower S (saturation) threshold

### Tracker Issues
1. **Drift**: Reinitialize tracker with new ROI
2. **Lost track**: Switch to auto-detection (press 'a')
3. **Slow performance**: Use CSRT for accuracy, KCF for speed

## Configuration Parameters

**From code (tunable):**
```cpp
// Motion detection
const double learningRate = 0.003;
const int motionThreshold = 80;

// Area constraints
const double minArea = 200.0;
const double maxArea = 20000.0;

// Size constraints
const int minWh = 15;
const int maxWh = 300;

// Shape constraints
const double minSolidity = 0.45;
const double maxJumpPx = 100.0;

// Aspect ratio
const double minAR = 0.35;
const double maxAR = 2.85;
```
