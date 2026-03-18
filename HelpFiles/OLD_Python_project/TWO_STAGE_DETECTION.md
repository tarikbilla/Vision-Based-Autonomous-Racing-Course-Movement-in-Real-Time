# RC Autonomy System - Two-Stage Detection Guide

## Problem Solved

**Issue**: Road edge markers (red/white blocks) were being detected as the car instead of the actual red car.

**Root Cause**: System was detecting ALL red/white colored objects without distinguishing between:
- Road boundary markers (large, on edges)
- The small red car (small, in center of road)

**Solution**: Implemented **two-stage detection**:
1. **Stage 1**: Detect road edges → Create a road region mask
2. **Stage 2**: Find red car ONLY within the detected road region

---

## How It Works Now

### Stage 1: Road Edge Detection

```
Camera Frame
     │
     ├─ Convert BGR → HSV
     │
     ├─ Find RED markers (two HSV ranges for red wraparound)
     ├─ Find WHITE markers (high V, low S)
     │
     ├─ Combine into edge mask
     ├─ Morphological cleanup (close, open)
     │
     ├─ Find contours of edge markers
     │
     └─ Identify:
         • Leftmost edge cluster  → left_edge_x
         • Rightmost edge cluster → right_edge_x
         • Calculate center_x = (left + right) / 2
         • Create road_mask = region between edges
```

**Output**: 
- Road mask (binary image showing which pixels are "inside the road")
- Left edge position, center line, right edge position

### Stage 2: Car Detection (Constrained)

```
Red Car Detection
     │
     ├─ Find ALL red pixels (car red)
     ├─ Filter by morphology
     │
     ├─ **Apply road_mask constraint**
     │  └─ Only look at red pixels inside road region
     │
     ├─ Find contours (now only car-sized, not edges!)
     │
     ├─ Filter by size:
     │  • Ignore tiny noise (< 40 pixels)
     │  • Ignore large objects (> 5000 pixels)
     │    (edges won't pass through because they're outside road_mask)
     │
     └─ Return: Car center and bounding box
```

**Result**: Edge markers are ignored because they're outside the road_mask!

### Visual Representation

```
BEFORE (Problems):
Road edges are detected as car

Camera View:
┌─────────────────────────────────┐
│ |Red marker|  <- Detected as car!
│ |         |
│ |       [car]  <- Real car ignored!
│ |         |
│ |Red marker|  <- Detected as car!
└─────────────────────────────────┘

AFTER (Fixed):
Only car inside road region detected

Camera View:
┌─────────────────────────────────┐
│ Green Green GREEN GREEN GREEN    │ ← Road edges (not detected as car)
│ Green   [car]        GREEN      │ ← Car ONLY found here
│ Green                GREEN      │
│ Green Green GREEN GREEN GREEN    │
│ (Road region mask shown in green)
└─────────────────────────────────┘

Live Display:
┌────────────────────────────────┐
│                                │
│  |      ║      ║      |        │ ← Green vertical lines = road edges
│  |      ║ [car]║      |        │ ← Blue vertical line = centerline
│  |~~~~~~~~~~~~~~~~~~~~|        │ ← Green overlay = detected road area
│  |      ║      ║      |        │
└────────────────────────────────┘
```

---

## Usage

### Start System
```bash
python run_autonomy.py
```

### Select Tracking Mode

**Option 1: Auto Tracking (Recommended)**
```
Press 'a'
  ↓
System detects road edges automatically
  ↓
Road mask created (shows detected road region)
  ↓
Car detection constrained to road region
  ↓
System follows car down the road
```

**Option 2: Manual ROI (Now More Robust)**
```
Press 's'
  ↓
Select car ROI (click and drag)
  ↓
System validates ROI size (min 10×10)
  ↓
Tracker initialized with better error checking
  ↓
System uses tracker-based tracking
```

---

## What You'll See

### Live Camera Window

When auto-tracking is enabled:

```
┌─────────────────────────────────────────┐
│                                         │
│  Green line (left edge)    Green line (right edge)
│  |                             |       │
│  |      Blue line (center) |   |       │
│  |      ║                   |   |       │
│  |      ║  [RED CAR]        ║   |       │
│  |      ║                   ║   |       │
│  |  Road L=50 C=320 R=590   ║   |       │
│  |______________________________|       │
│ Green overlay = detected road area     │
│                                         │
└─────────────────────────────────────────┘

Console Output (every ~1 second):
[30] Center: (320, 250) | Speed: 10 | Left: 0 | Right: 0 | Rays: L=150 C=180 R=160
[60] Center: (318, 251) | Speed: 10 | Left: 3 | Right: 0 | Rays: L=149 C=179 R=161
```

### Analysis Window

Shows:
- Red car bounding box
- Yellow center point
- Distance rays (left, center, right)

---

## Technical Improvements

### 1. Better Road Edge Detection
- Uses contour analysis instead of band scanning
- Handles curved roads better
- Creates binary road mask for constraint
- Morphological cleanup reduces noise

### 2. Constrained Car Detection
- Car detection only happens inside detected road
- Road mask filters out edge markers
- Size constraints (40-5000 pixels) prevent false positives
- Better red color range for actual car vs markers

### 3. Enhanced Tracker Initialization
- Better frame format validation (BGR, uint8)
- ROI bounds checking
- Better error messages
- Graceful fallback to auto color tracking

### 4. Visual Feedback
- Green overlay shows detected road area
- Clearer edge visualization
- Road coordinates displayed
- Status information on screen

---

## Key Parameters (in config/default.json)

```json
{
  "tracking": {
    "color_tracking": true     // Enable this for auto-tracking with constraints
  },
  "guidance": {
    "black_threshold": 50      // Adjust based on road surface darkness
  }
}
```

### Tuning Tips

If road edges aren't detected well:
```
Increase black_threshold (50 → 70)   if road is light gray
Decrease black_threshold (50 → 30)   if road is dark/black
```

If car is still misdetected:
```
Check that car is clearly red (RGB: high R, low G, low B)
Ensure road edges have high contrast (bright red/white)
Adjust lighting on track if needed
```

---

## System Architecture

```
TRACKING_LOOP:
    │
    ├─ Read frame from camera
    │
    ├─ Detect road edges
    │  └─ Create road_mask (showing road region)
    │
    ├─ Try to update tracker
    │
    ├─ If tracker fails:
    │  ├─ Detect red car
    │  ├─ APPLY ROAD_MASK CONSTRAINT  ← Key improvement!
    │  └─ Only look for car inside road region
    │
    ├─ Analyze tracked car position
    │  └─ Calculate steering/speed based on road centerline
    │
    └─ Send control commands to RC car


RENDER_THREAD:
    │
    ├─ Get latest frame
    ├─ Get road edges and mask
    │
    ├─ Draw road region overlay (green tint)
    ├─ Draw edge lines (green)
    ├─ Draw centerline (blue)
    │
    └─ Display on screen
```

---

## Troubleshooting

### "Road edges not detected"
- Check track has clear red/white boundary markers
- Ensure good lighting
- Verify markers are not too dark (increase brightness)
- Adjust `black_threshold` in config

### "Car still detected as edge marker"
- The road_mask constraint should prevent this
- If still happening: check car is clearly inside the road boundary in the video
- Verify red car color differs from edge markers

### "Tracker initialization fails with valid ROI"
- New error checking validates frame format
- Check frame is 3-channel BGR format
- Check ROI is within frame bounds
- ROI minimum size is 10×10 pixels

### "Car lost after a few frames"
- Tracker may be losing track
- Use 'a' for auto color tracking instead (more robust)
- Ensure good lighting and contrast

---

## Video Camera Setup Recommendations

Since camera is **fixed outside the car capturing entire track**:

1. **Camera Position**: 
   - Mount above track, angled down to see full width
   - Ensure road edges are visible in frame
   - Can capture curves and turns

2. **Lighting**:
   - Even illumination across track
   - Avoid shadows from track walls
   - Road edge markers should be clearly visible

3. **Track Design**:
   - Clear red or white boundary markers on edges
   - Markers should be consistent in color
   - Road surface should be darker than markers
   - Good contrast helps edge detection

4. **Car**:
   - Must be clearly red
   - Should be smaller than road edge markers
   - Good contrast with road surface

---

## Testing Validation

All system components tested and verified:
```
✓ Road edge detection (with mask generation)
✓ Car detection constrained to road region
✓ Tracker initialization with better validation
✓ Live visualization with road overlay
✓ Graceful fallback on tracker failure
```

Run validation:
```bash
python validate_system.py
```

---

## Next Steps

1. **Test with auto-tracking**:
   ```bash
   python run_autonomy.py
   # Press 'a'
   ```

2. **Observe the visualization**:
   - Green overlay should show detected road area
   - Green edges should be at road boundaries
   - Blue centerline should be down the middle
   - Red car should be tracked inside road

3. **Monitor console output**:
   - Check car position tracking
   - Verify steering/speed calculations
   - Look for any error messages

4. **Adjust if needed**:
   - If edges wrong: adjust `black_threshold`
   - If car misdetected: improve lighting or markers
   - If tracker fails: auto mode will take over

---

**Version**: 3.0 (Two-Stage Detection with Road Constraint)
**Status**: ✅ Fully Implemented & Tested
