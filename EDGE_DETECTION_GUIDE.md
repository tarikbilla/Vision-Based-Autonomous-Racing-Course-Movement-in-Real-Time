# Road Edge Detection & Auto-Tracking Guide

## Overview

The system now includes enhanced road edge detection and improved tracking that automatically:
1. **Detects road edges** (red/white boundary markers on gray/black road surface)
2. **Draws centerline guidance** showing the optimal path down the middle of the road
3. **Tracks the red car** inside the detected road boundaries
4. **Handles ROI selection failures** by gracefully falling back to auto color tracking

## How It Works

### Road Edge Detection
When you select **'a' for auto tracking**, the system:
- Scans the lower half of the camera frame
- Identifies red and white colored markers (lane markers)
- Calculates the left edge, right edge, and centerline of the road
- Visualizes these on the live feed as:
  - **Green vertical lines**: Left and right road edges
  - **Blue vertical line**: Road centerline (optimal path)
  - **Text display**: Shows the calculated road center position

### Edge-Based Car Control
The system uses detected edges to:
1. **Calculate steering error**: How far the car is from the road centerline
2. **Adjust steering**: Turn left/right to keep the car centered
3. **Manage speed**: Reduce speed if car is drifting far from center
4. **Stay within bounds**: Avoid hitting the road edges

### Auto Color Tracking Fallback
If road edges aren't clearly visible, the system:
- Looks for the red car by color
- Uses the car's current position as reference point
- Still applies ray-casting for obstacle avoidance

## Usage Instructions

### Option 1: Auto Red-Car + Edge-Based Guidance (Recommended)
```
1. Run: python run_autonomy.py
2. Press 'a' key to enable auto color tracking
3. System will auto-detect road edges and track the red car
4. Watch the blue centerline to see the guidance
```

### Option 2: Manual ROI Selection
```
1. Run: python run_autonomy.py
2. Press 's' key to select a ROI
3. Click and drag to select the red car in the frozen frame
4. Release to confirm (minimum 10x10 pixels required)
5. If tracker fails, press 's' again (up to 5 attempts)
6. After 5 failures, auto color tracking is enabled
```

### Exit
- Press 'q' to quit at any time

## Configuration

Edit `config/default.json` to customize:

```json
{
  "camera": {
    "source": 0,              // Camera index (0 or 1)
    "width": 640,             // Frame width
    "height": 480,            // Frame height
    "fps": 10                 // Frames per second
  },
  "tracking": {
    "tracker_type": "KCF",    // Kalman Correlation Filter
    "color_tracking": true    // Enable auto color tracking
  },
  "guidance": {
    "black_threshold": 50,    // Darkness level for road edge detection
    "ray_angles_deg": [-60, 0, 60],  // Ray casting angles
    "ray_max_length": 200,    // Max ray distance
    "evasive_distance": 80,   // Distance to trigger emergency steering
    "default_speed": 10       // Default car speed (0-255)
  },
  "control": {
    "steering_limit": 30,     // Max steering value (0-255)
    "light_on": true          // Enable car light
  }
}
```

## Troubleshooting

### "Camera source failed"
- **Problem**: System can't find the camera
- **Solution**: Check which camera is available
  ```bash
  python diagnose_camera.py
  ```
  Update `source` in `config/default.json` with available index

### "Tracker initialization failed"
- **Problem**: ROI too small or invalid for tracker
- **Solution**: 
  - Select a larger ROI (minimum 10x10 pixels)
  - Make sure the car is clearly visible
  - After 5 attempts, system automatically uses color tracking

### "Road edges not detected"
- **Problem**: Red/white markers aren't visible or properly colored
- **Solution**:
  - Check that your track has red/white edge markers
  - Adjust `black_threshold` in config if road surface is lighter/darker
  - Increase lighting on the track

### "Car tracked but driving erratically"
- **Problem**: Guidance control is too aggressive
- **Solution**:
  - Reduce `default_speed` in config
  - Increase `steering_limit` to make turns smoother
  - Ensure road edges are clearly visible

## Visual Feedback

### Live Camera Window
Shows real-time video with:
- **Green lines**: Detected road edges (when auto-tracking)
- **Blue line**: Calculated road centerline
- **Text**: Current road center position

### Analysis Window
Shows:
- **Yellow circle**: Car's center position
- **Red box**: Car bounding box
- **Rays**: Distance measurements to boundaries

## Technical Details

### Edge Detection Algorithm
1. Convert frame to HSV color space
2. Create masks for red (0-10°, 170-180°) and white (high V, low S)
3. Combine masks to find edge markers
4. Scan lower 50% of frame for edge positions
5. Use 10th and 90th percentiles to find reliable edge positions
6. Calculate centerline as midpoint between edges

### Steering Control
```
error = car_x - road_center_x
if |error| < tolerance (20px):
  steer = 0 (straight)
else if error < -tolerance:
  right_turn = proportional to error
else if error > tolerance:
  left_turn = proportional to error
```

### Speed Adjustment
```
speed = default_speed
if |error| > road_width * 0.3:
  speed = default_speed - 10 (slow down when drifting)
```

## Next Steps

1. **Test with your track**:
   ```bash
   python run_autonomy.py
   # Press 'a' for auto-tracking
   ```

2. **Observe the live feed**:
   - Check if edges are detected (green lines)
   - Verify centerline is accurate (blue line)
   - Watch car control based on centerline

3. **Adjust config if needed**:
   - Tweak thresholds and speeds in `config/default.json`
   - Re-run to test changes

4. **Monitor console output**:
   - Every 30 frames: position, speed, steering values
   - Error messages for debugging

---

**Version**: 2.0 (Road Edge Detection + Auto-Tracking)
**Last Updated**: February 17, 2026
