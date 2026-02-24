# Real-Time Car Detection - Quick Guide

## ✅ What's New

**Motion-Based Detection System** - The car is now detected in real-time using background subtraction, similar to the reference controller.cpp implementation.

### Detection Methods (in priority order):

1. **Motion Detection** (Primary) - Detects moving objects by background subtraction
   - Most robust for cars moving left/right
   - Handles varying lighting conditions
   - Tracks movement patterns

2. **Tracker** (Fallback) - OpenCV tracking (CSRT/KCF)
   - Used if motion detection fails
   - Requires manual ROI selection

3. **Color Detection** (Last Resort) - HSV-based red car detection
   - Used when both above methods fail
   - Constrained to detected road area

## 🚀 How to Run

```bash
cd CPP_Complete
./build/VisionBasedRCCarControl --device f9:af:3c:e2:d2:f5
```

### When Program Starts:

1. **BLE Connection** (2-3 seconds)
   - Car moves briefly to verify connection

2. **Camera Window Opens**
   - Live preview appears

3. **Choose Detection Mode:**
   - Press **`a`** = **Auto Motion Detection** (RECOMMENDED for moving car)
   - Press **`s`** = Manual ROI selection
   - Press **`q`** = Quit

### If You Choose `a` (Auto Motion Detection):

**IMPORTANT:** For the first 120 frames (~4 seconds at 30fps):
- **Keep the track COMPLETELY EMPTY**
- The system learns the background
- After 4 seconds, you'll see: `[✓] Background learning complete. Car detection active.`
- **NOW place the car on track and start it moving**

The system will detect the car by motion and track it automatically.

## 📊 Console Output

```
[*] Keep track EMPTY for first 120 frames for background learning.
[120] [✓] Background learning complete. Car detection active.
[MOTION] [150] Center: (640,480) | Speed: 20 | Left: 0 | Right: 15 | Rays: L=150 C=200 R=180
[MOTION] [180] Center: (620,475) | Speed: 18 | Left: 12 | Right: 0 | Rays: L=180 C=190 R=140
```

**Detection Method Indicators:**
- `[MOTION]` = Motion detection (background subtraction)
- `[TRACK]` = OpenCV tracker
- `[COLOR]` = Color-based detection
- `[!]` = No detection - car stopped for safety

## ⚙️ Key Features

### Motion Detection Parameters (in code):
```cpp
warmupFrames_ = 120        // Background learning duration
minArea = 250.0            // Min car blob size
maxArea = 12000.0          // Max car blob size
minWH = 10                 // Min width/height
maxWH = 220                // Max width/height
minSolidity = 0.25         // Shape filter (blob compactness)
maxJump = 220.0            // Max movement between frames
```

### Steering Control:
- **Road Edge Detection**: Red/white markers define track boundaries
- **Center-based Steering**: Car steers toward road center
- **Adaptive Speed**: Slows down when far from center
- **Emergency Stop**: Stops if car not detected

## 🔧 Troubleshooting

**Car not detected after warmup:**
- Make sure car is actually moving (motion detection requires movement)
- Check lighting - ensure good contrast
- Verify car is within camera view
- Try increasing `warmupFrames_` if background not stable

**Car detected but jerky movement:**
- Road edges may not be detected - check red/white markers visibility
- Adjust `default_speed` in config.json (try lower values like 15)

**Car moves too fast left/right:**
- This is NORMAL during first ~4 seconds (background learning)
- Commands are being sent but car hasn't been detected yet
- Wait for `Background learning complete` message

## 📝 Configuration

Edit `config/config.json`:
```json
{
  "boundary": {
    "default_speed": 20,        // Base speed (try 15-25)
    "steering_limit": 50,       // Max steering angle
    "black_threshold": 50,      // Edge detection sensitivity
    "evasive_distance": 100     // Obstacle avoidance distance
  }
}
```

## 🎯 Expected Behavior

1. **0-4 seconds**: Background learning (track must be empty, car may send random commands)
2. **After 4 seconds**: Motion detection active
3. **Place car on track**: System detects moving car
4. **Autonomous driving**: Car follows track, adjusts speed/steering based on road center
5. **Real-time tracking**: Updates position every 150ms (~6-7 Hz)

## ✨ Advantages of Motion Detection

✅ **Robust to left/right movement** - Primary use case  
✅ **Lighting invariant** - Works in varying conditions  
✅ **No manual ROI needed** - Fully automatic  
✅ **Tracks moving objects naturally** - Perfect for RC cars  
✅ **Filters noise** - Only detects significant movement  
✅ **Road-constrained** - Combined with road edge detection  

The car should now track reliably even when moving rapidly left and right!
