# Path Center Detection Details

## Overview
The system detects the track path and computes a centerline that the car should follow. This is done by analyzing red/white barriers and creating a free-space map.

## Complete Detection Process

### Step 1: Convert Frame to HSV Color Space
```cpp
cv::Mat hsv;
cv::cvtColor(bgr_frame, hsv, cv::COLOR_BGR2HSV);
// HSV makes color detection more reliable than RGB
```

---

### Step 2: Detect Red Barriers (Track Boundaries)

**Define Red Color Ranges:**
- Red wraps around HSV hue, so two ranges are needed
- Range 1: Hue 0-12° (red-orange)
- Range 2: Hue 168-179° (purple-red)

**Create Red Mask:**
```cpp
cv::Mat red1, red2, red;
cv::inRange(hsv, cv::Scalar(0, 70, 50), cv::Scalar(12, 255, 255), red1);
cv::inRange(hsv, cv::Scalar(168, 70, 50), cv::Scalar(179, 255, 255), red2);
cv::bitwise_or(red1, red2, red);
// Result: Binary mask where 255 = red pixel, 0 = not red
```

---

### Step 3: Build ROI (Region of Interest) from Red Pixels

**Collect All Red Pixel Coordinates:**
```cpp
std::vector<cv::Point> redPoints;
for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
        if (red.at<uchar>(y, x) > 0) {
            redPoints.push_back(cv::Point(x, y));
        }
    }
}
```

**Create Convex Hull:**
```cpp
std::vector<cv::Point> hull;
cv::convexHull(redPoints, hull);
// Hull defines the outer boundary of the track
```

**Fill ROI Mask:**
```cpp
cv::Mat roi(height, width, CV_8UC1, cv::Scalar(0));
cv::fillConvexPoly(roi, hull, cv::Scalar(255));
// ROI = 255 inside track, 0 outside track
```

---

### Step 4: Detect White Markers/Lines

**Define White Color Range:**
- Hue: Any (0-179)
- Saturation: Low (0-70) - white has low saturation
- Value: High (180-255) - white is bright

**Create White Mask:**
```cpp
cv::Mat white;
cv::inRange(hsv, cv::Scalar(0, 0, 180), cv::Scalar(179, 70, 255), white);
cv::bitwise_and(white, roi, white);
// Only keep white pixels inside ROI
```

---

### Step 5: Combine Red and White as Barriers

```cpp
cv::Mat barriers;
cv::bitwise_or(red, white, barriers);
cv::bitwise_and(barriers, roi, barriers);
// barriers = all obstacles (red boundaries + white markers) inside ROI
```

---

### Step 6: Morphological Cleanup of Barriers

**Remove Small Noise:**
```cpp
cv::Mat kernel5x5 = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(5, 5));
cv::morphologyEx(barriers, barriers, cv::MORPH_OPEN, kernel5x5, cv::Point(-1,-1), 1);
// OPEN = erode then dilate, removes small noise blobs
```

**Fill Small Gaps:**
```cpp
cv::Mat kernel7x7 = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(7, 7));
cv::morphologyEx(barriers, barriers, cv::MORPH_CLOSE, kernel7x7, cv::Point(-1,-1), 3);
// CLOSE = dilate then erode, fills small holes
```

**Inflate Barriers (Safety Margin):**
```cpp
cv::Mat kernel9x9 = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(9, 9));
cv::dilate(barriers, barriers, kernel9x9, cv::Point(-1,-1), 1);
// Make barriers slightly larger for safety
```

---

### Step 7: Create Walls Map (Barriers + Outside ROI)

```cpp
cv::Mat notRoi;
cv::bitwise_not(roi, notRoi);
// notRoi = 255 outside track, 0 inside

cv::Mat walls;
cv::bitwise_or(barriers, notRoi, walls);
// walls = barriers + everything outside ROI
// 255 = obstacle or outside, 0 = potentially drivable
```

---

### Step 8: Flood Fill to Find Free Space

**Flood Fill from Corner:**
```cpp
cv::Mat flood = walls.clone();
cv::Mat ffmask(height + 2, width + 2, CV_8UC1, cv::Scalar(0));
cv::floodFill(flood, ffmask, cv::Point(0, 0), cv::Scalar(128));
// Start from (0,0) which is outside the track
// Fill all connected regions with 128
```

**Extract Outside Mask:**
```cpp
cv::Mat outsideMask;
cv::compare(flood, 128, outsideMask, cv::CMP_EQ);
// outsideMask = 255 where flood filled (outside track)
```

**Get Free Space Inside Track:**
```cpp
cv::Mat walls0, notOutside, insideFree;
cv::compare(walls, 0, walls0, cv::CMP_EQ);
// walls0 = 255 where walls are 0 (not an obstacle)

cv::bitwise_not(outsideMask, notOutside);
// notOutside = 255 inside track

cv::bitwise_and(notOutside, walls0, insideFree);
// insideFree = drivable area inside track
```

---

### Step 9: Clean the Free Space Map

**Parameters:**
```cpp
const int cleanCloseK = 31;      // Kernel size for closing
const int cleanCloseIter = 1;     // Close iterations
const int cleanOpenK = 7;         // Kernel size for opening
const int cleanOpenIter = 1;      // Open iterations
const int cleanMaxHoleArea = 5000; // Maximum hole size to fill
```

**Close Operation (Fill Medium Holes):**
```cpp
cv::Mat kernelClose = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(31, 31));
cv::morphologyEx(freeWhite, cleaned, cv::MORPH_CLOSE, kernelClose, cv::Point(-1,-1), 1);
```

**Open Operation (Remove Thin Protrusions):**
```cpp
cv::Mat kernelOpen = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(7, 7));
cv::morphologyEx(cleaned, cleaned, cv::MORPH_OPEN, kernelOpen, cv::Point(-1,-1), 1);
```

**Fill Remaining Small Holes:**
```cpp
// Find contours
std::vector<std::vector<cv::Point>> contours;
cv::findContours(~cleaned, contours, cv::RETR_CCOMP, cv::CHAIN_APPROX_SIMPLE);

// Fill holes smaller than cleanMaxHoleArea
for (const auto& cnt : contours) {
    double area = std::fabs(cv::contourArea(cnt));
    if (area > 0 && area <= cleanMaxHoleArea) {
        cv::drawContours(cleaned, {cnt}, -1, cv::Scalar(255), cv::FILLED);
    }
}
```

---

### Step 10: Sample Points Along Free Space Boundary

**Parameters:**
```cpp
const int centerSamples = 1200;  // Number of sample points
```

**Sample Perimeter:**
```cpp
std::vector<cv::Point2f> samples;
double perimeter = cv::arcLength(cleanedContour, true);
double step = perimeter / centerSamples;

for (int i = 0; i < centerSamples; ++i) {
    double dist = i * step;
    cv::Point2f pt = getPointOnContour(cleanedContour, dist);
    samples.push_back(pt);
}
```

---

### Step 11: Compute Distance Transform

```cpp
cv::Mat distanceTransform;
cv::distanceTransform(cleaned, distanceTransform, cv::DIST_L2, 5);
// Each pixel value = distance to nearest obstacle
// High values = far from barriers (center of track)
```

---

### Step 12: Build Centerline Points

**For Each Sample Point:**
```cpp
std::vector<cv::Point2f> centerlinePoints;

for (const auto& samplePt : samples) {
    // 1. Find perpendicular direction to contour at this point
    cv::Vec2f tangent = computeTangent(contour, samplePt);
    cv::Vec2f normal(-tangent[1], tangent[0]);
    
    // 2. Cast ray inward along normal direction
    // 3. Find point with maximum distance transform value
    float maxDist = 0;
    cv::Point2f centerPt = samplePt;
    
    for (int step = 0; step < maxRayLength; ++step) {
        cv::Point2f testPt = samplePt + normal * step;
        float dist = distanceTransform.at<float>(testPt);
        if (dist > maxDist) {
            maxDist = dist;
            centerPt = testPt;
        }
    }
    
    centerlinePoints.push_back(centerPt);
}
```

---

### Step 13: Smooth Centerline

**Parameters:**
```cpp
const int centerSmoothWin = 31;  // Smoothing window size
```

**Apply Moving Average:**
```cpp
std::vector<cv::Point2f> smoothed;
for (int i = 0; i < centerlinePoints.size(); ++i) {
    int start = std::max(0, i - centerSmoothWin/2);
    int end = std::min((int)centerlinePoints.size(), i + centerSmoothWin/2);
    
    float sumX = 0, sumY = 0;
    for (int j = start; j < end; ++j) {
        sumX += centerlinePoints[j].x;
        sumY += centerlinePoints[j].y;
    }
    
    smoothed.push_back(cv::Point2f(sumX / (end-start), sumY / (end-start)));
}
```

---

### Step 14: Final Morphological Operations on Centerline

```cpp
const int centerCloseK = 7;
const int centerCloseIters = 2;

cv::Mat centerMask = createMaskFromPoints(smoothed);
cv::Mat kernelCenter = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(7, 7));
cv::morphologyEx(centerMask, centerMask, cv::MORPH_CLOSE, kernelCenter, cv::Point(-1,-1), 2);
```

---

### Step 15: Store Centerline

```cpp
trackMask_ = cleaned;               // Store cleaned free space
centerline_ = smoothed;             // Store centerline points
hasCenterline_ = true;              // Mark as available
```

---

## Using the Centerline for Control

### Step 1: Find Closest Point on Centerline
```cpp
int closestIdx = 0;
float minDist = 1e9;
for (int i = 0; i < centerline.size(); ++i) {
    float dist = hypot(carCenter.x - centerline[i].x, 
                       carCenter.y - centerline[i].y);
    if (dist < minDist) {
        minDist = dist;
        closestIdx = i;
    }
}
```

### Step 2: Find Lookahead Target Point
```cpp
const double lookaheadPx = 80.0;  // Look ahead distance
int targetIdx = closestIdx;
float accumulatedDist = 0;

while (accumulatedDist < lookaheadPx && targetIdx < centerline.size() - 1) {
    float segmentDist = hypot(centerline[targetIdx+1].x - centerline[targetIdx].x,
                              centerline[targetIdx+1].y - centerline[targetIdx].y);
    accumulatedDist += segmentDist;
    targetIdx++;
}

cv::Point2f target = centerline[targetIdx];
```

### Step 3: Compute Steering Angle (Pure Pursuit)
```cpp
const double wheelbasePx = 60.0;  // Car wheelbase in pixels

// Vector from car to target
double dx = target.x - carCenter.x;
double dy = target.y - carCenter.y;

// Angle to target relative to car heading
double alpha = atan2(dy, dx) - carHeading;

// Pure pursuit steering formula
double delta = atan2(2.0 * wheelbasePx * sin(alpha), lookaheadPx);

// Clamp to maximum steering angle
double deltaMaxRad = 45.0 * M_PI / 180.0;
delta = std::clamp(delta, -deltaMaxRad, deltaMaxRad);
```

### Step 4: Convert to Steering Command
```cpp
// Map steering angle to 0-255 range
// 0-100: right turn (0=max right, 100=straight)
// 155-255: left turn (155=slight left, 255=max left)

int steerByte;
if (delta > 0) {
    // Right turn
    steerByte = (int)(100 * delta / deltaMaxRad);
} else {
    // Left turn
    steerByte = 255 - (int)(100 * (-delta) / deltaMaxRad);
}
```

---

## Visualization

**On-screen display:**
- **Blue overlay**: Free drivable space
- **Green line**: Centerline path
- **Yellow circle**: Lookahead target point
- **Red line**: Vector from car to target
- **Text**: Offset from centerline in pixels

---

## Tuning Parameters

### Detection Sensitivity
```cpp
// Red detection (increase for darker red)
cv::Scalar(0, 70, 50) → cv::Scalar(0, 50, 40)

// White detection (decrease for dimmer white)
cv::Scalar(0, 0, 180) → cv::Scalar(0, 0, 150)
```

### Smoothness
```cpp
// More smoothing (larger kernel)
cleanCloseK = 31 → cleanCloseK = 41

// More centerline smoothing
centerSmoothWin = 31 → centerSmoothWin = 51
```

### Control Response
```cpp
// Lookahead distance (larger = smoother, smaller = tighter)
lookaheadPx = 80.0 → lookaheadPx = 60.0

// Wheelbase (affects turn sharpness)
wheelbasePx = 60.0 → wheelbasePx = 40.0
```
