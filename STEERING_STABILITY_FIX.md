# Steering Stability & Smoothness Improvements

## Problem
The RC car was oscillating left-to-right excessively, unable to smoothly follow the centerline. Steering angles were too aggressive, causing rapid alternating corrections.

## Solution Overview
Implemented three complementary steering improvements:

1. **Reduced Max Steering Angle** - From 30° to 12°
2. **Added Steering Deadzone** - Prevents tiny corrections near center
3. **Frame-to-Frame Smoothing** - Exponential filter for smooth transitions

---

## Changes Made

### 1. Parameter Tuning in `include/boundary_detection.hpp`

**Previous values:**
```cpp
double lookaheadPx_ = 120.0;      // Lookahead distance for pure pursuit
double deltaMaxDeg_ = 30.0;       // Max steering angle
```

**New values:**
```cpp
double lookaheadPx_ = 60.0;       // Reduced lookahead (closer target = smoother curves)
double deltaMaxDeg_ = 12.0;       // Reduced max angle (less aggressive steering)
```

**New smoothing parameters:**
```cpp
int lastSteerByte_ = 0;           // Store previous steering value
double steeringSmoothing_ = 0.6;  // Blend factor (0-1)
int steeringDeadzone_ = 3;        // ±3 bytes centered ignored
```

### 2. Steering Smoothing Algorithm in `src/boundary_detection.cpp`

**Location:** `analyzeWithCenterline()` function

**Process:**
```cpp
// Step 1: Calculate raw steering
int rawSteerByte = deltaToSteeringByte(delta, deltaMaxRad);

// Step 2: Apply deadzone (ignore small corrections)
int steerByte = rawSteerByte;
if (rawSteerByte >= (128 - 3) && rawSteerByte <= (128 + 3)) {
    steerByte = 128;  // Center (no steering)
}

// Step 3: Apply exponential smoothing
// newValue = 0.6 * raw + 0.4 * previous
steerByte = (int)std::round(0.6 * steerByte + 0.4 * lastSteerByte_);
lastSteerByte_ = steerByte;  // Save for next frame
```

---

## How It Works

### Reduced Lookahead (120px → 60px)
- **Effect:** Car looks closer ahead instead of far ahead
- **Benefit:** Gentler course corrections, avoids over-steering on curves
- **Physics:** Pure pursuit algorithm uses shorter lookahead for smoother paths

### Reduced Max Angle (30° → 12°)
- **Effect:** Steering is capped at smaller angles
- **Benefit:** Prevents violent left-right swings
- **Trade-off:** Takes longer to turn tight corners (acceptable for smooth tracking)

### Deadzone (±3 bytes)
- **Effect:** Steering values within -3 to +3 bytes of center are ignored
- **Benefit:** Eliminates micro-corrections that cause jitter
- **Range:** Byte value 128 = center; 125-131 range → center (no steering)

### Exponential Smoothing (0.6 factor)
- **Formula:** `newSteering = 0.6 × rawSteering + 0.4 × previousSteering`
- **Effect:** Gradual transitions between steering angles
- **Benefit:** Smoother turning, more natural path following
- **Time constant:** Changes take ~2-3 frames to fully apply

---

## Parameter Adjustment Guide

If the car still oscillates:
- **Decrease `steeringSmoothing_`** to 0.4-0.5 (more filter = more delay but smoother)
- **Increase `steeringDeadzone_`** to 5 or 6 (ignore more near-center corrections)
- **Decrease `lookaheadPx_`** to 40-50 (even closer lookahead)

If the car won't turn tight corners:
- **Increase `deltaMaxDeg_`** to 15-18° (more aggressive but may oscillate)
- **Increase `lookaheadPx_`** to 80-100 (farther lookahead = stronger steering)
- **Increase `steeringSmoothing_`** to 0.7-0.8 (less filtering)

---

## Testing Results

**Build Status:** ✓ Successful (4 warnings, all non-critical)
**Executable:** `/build/VisionBasedRCCarControl` (844 KB)

**Expected Behavior:**
1. Car steadily follows centerline without oscillating
2. Smooth lane changes without sharp left-right jerking
3. Gentle course corrections for track curves
4. Stable at speeds up to 50% throttle

---

## Technical Notes

### Initialization
- `lastSteerByte_` initialized to 128 (center position) in constructor
- Reset to 128 if centerline is lost (invalid state handling)

### Steering Byte Mapping
- 0-127: Left steering (0 = max left, 127 = straight)
- 128: Straight (no steering)
- 129-255: Right steering (255 = max right)

### Pure Pursuit Algorithm
- Input: `delta` angle calculated from centerline
- Clamped to `[-deltaMaxRad, +deltaMaxRad]`
- Converted to steering byte via `deltaToSteeringByte()`
- Then smoothed and output to BLE

---

## Files Modified

1. **include/boundary_detection.hpp**
   - Added smoothing state variables
   - Updated pure pursuit parameters

2. **src/boundary_detection.cpp**
   - Constructor: Initialize `lastSteerByte_ = 128`
   - `analyzeWithCenterline()`: Added deadzone and smoothing logic

---

## Deployment

Rebuild with:
```bash
cd /Users/tarikbilla/Projects/IoT-Project-Vision-based-autonomous-RC-car-control-system
./build.sh
```

Run with:
```bash
./build/VisionBasedRCCarControl
```

Press 'a' for auto red-car tracking, then watch for smooth centerline following behavior.
