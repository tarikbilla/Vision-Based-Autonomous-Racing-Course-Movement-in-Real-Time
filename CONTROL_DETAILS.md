# Control Details

## Overview
The control system analyzes the car's position relative to the track boundaries and centerline, then generates steering and speed commands sent via BLE.

## Control Architecture

```
Camera Frame → Car Detection → Boundary Analysis → Control Generation → BLE Command → RC Car
     (15fps)      (MOTION)         (Ray Casting)      (Speed/Steer)      (50Hz)
```

---

## Two Control Modes

### Mode 1: Ray Casting (No Centerline)
Used when centerline is not available or as fallback.

### Mode 2: Pure Pursuit (With Centerline)
Used when centerline is successfully built from the track.

---

## Mode 1: Ray Casting Control

### Step 1: Calculate Car Heading from Movement
```cpp
double heading = atan2(movement.y, movement.x) * 180.0 / M_PI;
// movement = (dx, dy) from previous frame
// heading in degrees (0° = right, 90° = down)
```

### Step 2: Cast Three Rays
Cast rays at three angles relative to car heading:
- **Left ray**: heading - 60°
- **Center ray**: heading + 0°
- **Right ray**: heading + 60°

```cpp
std::vector<int> rayAngles = {-60, 0, 60};  // degrees
std::vector<int> rayDistances;

for (int angle : rayAngles) {
    double absoluteAngle = heading + angle;
    int distance = castRay(frame, carCenter, absoluteAngle);
    rayDistances.push_back(distance);
}
```

### Step 3: Cast Individual Ray
```cpp
int castRay(const cv::Mat& gray, cv::Point origin, double angleDeg) {
    double angleRad = angleDeg * M_PI / 180.0;
    
    for (int dist = 1; dist <= rayMaxLength; ++dist) {
        int x = origin.x + (int)(dist * cos(angleRad));
        int y = origin.y + (int)(dist * sin(angleRad));
        
        // Check if out of bounds
        if (x < 0 || x >= width || y < 0 || y >= height) {
            return dist;
        }
        
        // Check if hit black boundary
        if (gray.at<uint8_t>(y, x) < blackThreshold) {
            return dist;  // Hit obstacle
        }
    }
    
    return rayMaxLength;  // No obstacle found
}
```

**Parameters:**
- `rayMaxLength`: 200 pixels
- `blackThreshold`: 50 (pixels darker than this = obstacle)

### Step 4: Analyze Ray Distances
```cpp
int leftDist = rayDistances[0];    // Left ray
int centerDist = rayDistances[1];  // Center ray
int rightDist = rayDistances[2];   // Right ray

int closestDist = min({leftDist, centerDist, rightDist});
```

### Step 5: Evasive Action (Emergency)
If any ray is too close to boundary:

```cpp
if (closestDist < evasiveDistance) {  // evasiveDistance = 100 pixels
    // Reduce speed for safety
    speed = max(5, defaultSpeed / 2);
    
    // Turn away from closest obstacle
    if (leftDist < rightDist) {
        // Obstacle on left → turn right
        rightTurn = min(steeringLimit, (int)((evasiveDistance - leftDist) * 1.5));
        leftTurn = 0;
    } 
    else if (rightDist < leftDist) {
        // Obstacle on right → turn left
        leftTurn = min(steeringLimit, (int)((evasiveDistance - rightDist) * 1.5));
        rightTurn = 0;
    }
    else {
        // Obstacle ahead → turn toward more space
        if (rightDist > leftDist) {
            rightTurn = steeringLimit;
        } else {
            leftTurn = steeringLimit;
        }
    }
}
```

### Step 6: Normal Navigation
If no immediate danger:

```cpp
else {
    // Normal speed
    speed = defaultSpeed;
    
    // Steer based on which side has more space
    if (centerDist < (leftDist + rightDist) / 2) {
        // Turn ahead detected
        if (rightDist > leftDist) {
            // More space on right
            int diff = leftDist - centerDist;
            rightTurn = min(steeringLimit, diff * 2);
            leftTurn = 0;
        } else {
            // More space on left
            int diff = rightDist - centerDist;
            leftTurn = min(steeringLimit, diff * 2);
            rightTurn = 0;
        }
    } else {
        // Go straight
        rightTurn = 0;
        leftTurn = 0;
    }
}
```

---

## Mode 2: Pure Pursuit Control (With Centerline)

### Step 1: Find Closest Point on Centerline
```cpp
int closestIndex(const std::vector<cv::Point2f>& path, double x, double y) {
    int best = 0;
    double minDist = 1e300;
    
    for (int i = 0; i < path.size(); ++i) {
        double d = hypot(path[i].x - x, path[i].y - y);
        if (d < minDist) {
            minDist = d;
            best = i;
        }
    }
    
    return best;
}

int pathIndex = closestIndex(centerline, carCenter.x, carCenter.y);
```

### Step 2: Find Lookahead Target Point
```cpp
int lookaheadIndex(const std::vector<cv::Point2f>& path, int startIdx, 
                   double carX, double carY, double lookahead) {
    int idx = startIdx;
    double accumulated = 0;
    
    while (accumulated < lookahead && idx < path.size() - 1) {
        double dx = path[idx+1].x - path[idx].x;
        double dy = path[idx+1].y - path[idx].y;
        accumulated += hypot(dx, dy);
        idx++;
    }
    
    // Handle loop: wrap around if needed
    if (idx >= path.size()) {
        idx = idx % path.size();
    }
    
    return idx;
}

int targetIdx = lookaheadIndex(centerline, pathIndex, 
                                carCenter.x, carCenter.y, lookaheadPx);
cv::Point2f target = centerline[targetIdx];
```

**Parameters:**
- `lookaheadPx`: 80 pixels (how far ahead to look)

### Step 3: Compute Steering Angle
```cpp
double purePursuitDelta(double carX, double carY, double carHeading,
                        cv::Point2f target, double wheelbase, double lookahead) {
    // Vector from car to target
    double dx = target.x - carX;
    double dy = target.y - carY;
    
    // Angle to target
    double angleToTarget = atan2(dy, dx);
    
    // Angle error (relative to car heading)
    double alpha = angleToTarget - carHeading;
    
    // Wrap to [-π, π]
    while (alpha > M_PI) alpha -= 2 * M_PI;
    while (alpha < -M_PI) alpha += 2 * M_PI;
    
    // Pure pursuit formula
    double delta = atan2(2.0 * wheelbase * sin(alpha), lookahead);
    
    return delta;
}

double delta = purePursuitDelta(carCenter.x, carCenter.y, heading,
                                target, wheelbasePx, lookaheadPx);
```

**Parameters:**
- `wheelbasePx`: 60 pixels (car wheelbase)
- `deltaMaxDeg`: 45° (maximum steering angle)

### Step 4: Clamp Steering Angle
```cpp
double deltaMaxRad = deltaMaxDeg * M_PI / 180.0;
delta = clamp(delta, -deltaMaxRad, deltaMaxRad);
```

### Step 5: Convert to Steering Byte
```cpp
int deltaToSteeringByte(double delta, double deltaMax) {
    if (fabs(delta) < 0.001) {
        return 0;  // Straight
    }
    
    double ratio = delta / deltaMax;  // -1.0 to +1.0
    
    if (ratio > 0) {
        // Right turn: 1 to 100
        return (int)round(ratio * 100);
    } else {
        // Left turn: 155 to 255
        return 255 - (int)round((-ratio) * 100);
    }
}

int steerByte = deltaToSteeringByte(delta, deltaMaxRad);
```

### Step 6: Extract Left/Right Turn Values
```cpp
int rightTurn = 0;
int leftTurn = 0;

if (steerByte > 0 && steerByte <= 100) {
    rightTurn = steerByte;
} else if (steerByte >= 155 && steerByte <= 255) {
    leftTurn = 255 - steerByte;
}
```

### Step 7: Adjust Speed Based on Steering
```cpp
double steerStrength(int steerByte) {
    if (steerByte == 0) return 0.0;
    if (steerByte > 0 && steerByte <= 100) {
        return clamp(steerByte / 100.0, 0.0, 1.0);
    }
    int pct = 255 - steerByte;
    return clamp(pct / 100.0, 0.0, 1.0);
}

double strength = steerStrength(steerByte);
double slowdown = clamp(turnSlowK * strength, 0.0, 0.7);  // turnSlowK = 0.3

int speed = (int)round(defaultSpeed * (1.0 - slowdown));
speed = clamp(speed, minSpeed, defaultSpeed);
```

**Parameters:**
- `turnSlowK`: 0.3 (how much to slow down in turns)
- `minSpeed`: 20 (minimum speed)
- `defaultSpeed`: 50 (from config)

---

## Control Output

### ControlVector Structure
```cpp
struct ControlVector {
    bool light_on;      // Headlights on/off
    int speed;          // 0-255 (forward speed)
    int right_turn;     // 0-255 (right steering amount)
    int left_turn;      // 0-255 (left steering amount)
};
```

### Constraints
```cpp
speed = clamp(speed, 0, 255);
right_turn = clamp(right_turn, 0, 255);
left_turn = clamp(left_turn, 0, 255);

// Never have both left and right turn at same time
if (right_turn > 0) left_turn = 0;
if (left_turn > 0) right_turn = 0;
```

---

## Command Rate Control

### BLE Thread Loop
```cpp
void bleLoop() {
    const int commandRateHz = 50;  // From config
    const auto interval = chrono::milliseconds(1000 / commandRateHz);
    
    while (!stopEvent) {
        auto start = chrono::steady_clock::now();
        
        // Get latest control from tracking thread
        ControlVector control = getLatestControl();
        
        // Send to car via BLE
        ble->sendControl(control);
        
        // Sleep to maintain rate
        auto elapsed = chrono::steady_clock::now() - start;
        if (elapsed < interval) {
            this_thread::sleep_for(interval - elapsed);
        }
    }
}
```

**Command rate**: 50 Hz (20ms per command)

---

## Visualization

### On-screen display shows:
- **Ray lines**: Three rays (left, center, right) in yellow
- **Ray endpoints**: Red circles where rays hit obstacles
- **Target point**: Yellow circle (lookahead point on centerline)
- **Steering indicator**: Arrow showing turn direction
- **Speed bar**: Green bar showing current speed (0-255)
- **Control values**: Text showing speed, left, right values

---

## Configuration Parameters

**From `config/config.json`:**
```json
{
  "boundary": {
    "black_threshold": 50,
    "ray_max_length": 200,
    "evasive_distance": 100,
    "default_speed": 50,
    "steering_limit": 1,
    "light_on": true
  },
  "ui": {
    "command_rate_hz": 50
  }
}
```

**Hardcoded in boundary_detection.cpp:**
```cpp
const double lookaheadPx_ = 80.0;
const double wheelbasePx_ = 60.0;
const double deltaMaxDeg_ = 45.0;
const double turnSlowK_ = 0.3;
const int minSpeed_ = 20;
```

---

## Tuning Guide

### For Smoother Turns
```cpp
lookaheadPx = 80 → 100  // Look further ahead
turnSlowK = 0.3 → 0.5   // Slow down more in turns
```

### For Tighter Turns
```cpp
lookaheadPx = 80 → 60   // Look closer ahead
wheelbasePx = 60 → 40   // Smaller wheelbase
```

### For Faster Response
```cpp
commandRateHz = 50 → 75  // Send commands faster
deltaMaxDeg = 45 → 60    // Allow sharper steering
```

### For More Stability
```cpp
lookaheadPx = 80 → 120   // Look much further ahead
turnSlowK = 0.3 → 0.2    // Don't slow down as much
minSpeed = 20 → 30       // Higher minimum speed
```
