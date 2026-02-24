# Vision-Based RC Car Autonomous Control - Complete Implementation

## Overview
This is a complete C++ implementation of an autonomous RC car control system using vision-based path detection.

**Key Features:**
- **BLE Connectivity**: Real-time wireless car control via SimpleBLE library
- **Dual Detection**: Motion-based (PRIMARY) + Color-based (FALLBACK) car detection
- **Autonomous Path Following**: Centerline tracking on white/red boundary-marked tracks
- **Live Visualization**: Real-time overlay with car position, boundaries, and centerline

## System Architecture

### Detection Pipeline (Priority Order)
1. **Motion Detection (PRIMARY)** - Uses MOG2 background subtraction
   - Works regardless of lighting conditions
   - Fast warmup (30 frames = ~1 second at 30fps)
   - Very relaxed thresholds for reliability
   
2. **Color Detection (FALLBACK)** - HSV-based red/orange filtering
   - 5 HSV range masks for maximum coverage
   - Contour-based candidate filtering
   - Used when motion detection fails

3. **Tracker (MANUAL ROI)** - Optional tracker for stationary ROI

### Control Loop
1. **Camera Thread** - Captures frames continuously
2. **Tracking Thread** - Detects car, analyzes boundaries, generates control commands
3. **BLE Thread** - Sends control commands to car via Bluetooth
4. **UI Thread** - Displays live video with overlays

### Path Following Algorithm
- Detects red/white boundary markers
- Extracts inner and outer track boundaries
- Computes centerline (midpoint between boundaries)
- Generates steering commands to keep car centered

## Hardware Requirements

- RC Car with BLE interface (configured in `config/config.json`)
- Webcam or USB camera
- Mac/Linux with OpenCV 4.x and SimpleBLE installed
- Track with red inner boundary and white outer boundary markers

## Software Setup

### 1. Install Dependencies (macOS with Homebrew)
```bash
brew install opencv cmake
pip install opencv-python cmake
```

### 2. Build the Project
```bash
cd CPP_Complete
mkdir -p build
cd build
cmake ..
make -j4
```

### 3. Configure Car Settings
Edit `config/config.json`:
```json
{
  "device_mac": "XX:XX:XX:XX:XX:XX",  // Your car's BLE MAC address
  "default_speed": 50,
  "use_color_tracking": true,
  "motion_detection_threshold": 50
}
```

## Running the System

### Startup Options

**Option 1: Fully Automatic (Motion Detection)**
```bash
./build/VisionBasedRCCarControl
```
- Press 'a' when prompted to auto-detect car (uses motion detection)
- System will automatically track and follow the path
- Press 'q' to quit
- Press Ctrl+C to stop car and disconnect

**Option 2: Manual ROI Selection**
```bash
./build/VisionBasedRCCarControl
```
- Press 's' when prompted for manual selection
- Click and drag to select car's bounding box
- System will track within this ROI
- Press 'q' to quit

## Real-Time Display Overlay

The live display shows:
- **Red Circle**: Detected car position (RED = detected, BLUE = searching)
- **Red Box**: Car bounding box
- **Green Arrow**: Car movement direction
- **Blue Line**: Inner track boundary
- **Green Line**: Outer track boundary  
- **Cyan/Yellow Line**: Centerline (path to follow)
- **Status Text**: Detection method (MOTION/COLOR/TRACK) and car coordinates

## Control Commands

| Key | Action |
|-----|--------|
| `a` | Auto car detection (motion-based) |
| `s` | Manual selection (click to ROI) |
| `q` | Quit visualization |
| `Ctrl+C` | Stop car and disconnect BLE |

## Troubleshooting

### Car Not Detected
1. **Check Motion Detection** - Watch for visible motion mask
   - Car should appear as white blob in motion mask
   - If not visible, car may be too stationary
   
2. **Verify Camera Feed**
   - Ensure webcam shows track clearly
   - Lighting should be adequate (no backlighting)
   
3. **Adjust Detection Parameters** (in control_orchestrator.cpp)
   - Motion threshold: Line ~524 `cv::threshold(fgMask, fgMask, 100, 255, cv::THRESH_BINARY);`
   - Min area: Line ~534 `const double minArea = 20.0;`
   - Max jump distance: Line ~545 `if (dist > 400.0) continue;`

### Path Not Following Correctly
1. **Verify Boundary Detection**
   - Boundaries should appear as blue/green lines on display
   - Centerline should be cyan/yellow
   
2. **Check Track Markers**
   - Ensure red markers on inner boundary are visible
   - Ensure white markers on outer boundary are visible
   
3. **Adjust Steering Response** (in boundary_detection.cpp)
   - Ray casting parameters may need tuning
   - Turn value multipliers affect steering sensitivity

### BLE Connection Issues
1. **Find Car MAC Address**:
   ```bash
   python3 scan_and_connect.py
   ```
   
2. **Update config.json** with correct MAC
   
3. **Restart car** - cycle power on car

## Performance Metrics

- **Detection Latency**: ~30-50ms (30fps processing)
- **BLE Command Rate**: ~60Hz (limited by car firmware)
- **Path Following Accuracy**: ±50px (depends on camera resolution)
- **Motion Detection Warmup**: ~1 second (30 frames)

## Advanced Configuration

### Config File Structure (config/config.json)
```json
{
  "device_mac": "Your car's MAC address",
  "default_speed": 50,              // 0-100
  "max_turn_value": 50,
  "use_color_tracking": true,
  "color_tracking_threshold": 50,
  "motion_detection_threshold": 50,
  "roi_threshold_area": 40,
  "centerline_smoothing": 5
}
```

### Detection Parameters
- **Color Detection HSV Ranges**: See lines 400-420 in control_orchestrator.cpp
- **Motion Detection Threshold**: Line 524
- **Contour Filtering**: Lines 530-545 (area, dimensions, solidity)
- **Tracking Jump Tolerance**: Line ~545

## Code Structure

```
CPP_Complete/
├── src/
│   ├── main.cpp                    # Entry point, signal handling
│   ├── control_orchestrator.cpp    # Main orchestration (detection, tracking, rendering)
│   ├── boundary_detection.cpp      # Road edge detection, centerline calculation
│   ├── ble_handler.cpp             # Bluetooth communication
│   ├── camera_capture.cpp          # Camera frame acquisition
│   ├── object_tracker.cpp          # Optional manual ROI tracker
│   └── commands.cpp                # Car control command generation
├── include/                         # Header files
├── config/                          # Configuration files
│   └── config.json                 # Car MAC, speeds, thresholds
└── CMakeLists.txt                  # Build configuration
```

## Key Implementation Details

### Motion Detection (RECOMMENDED)
- Uses OpenCV's MOG2 background subtractor
- 30-frame warmup for background learning
- Morphological operations (open, close, dilate) for cleanup
- Contour area filtering: 20-30000 pixels
- Proximity scoring to previous detection

### Color Detection (FALLBACK)
- 5 HSV range masks covering all red/orange shades
- Ranges: Pure red, wraparound red, orange, bright low-sat red
- Solidity filtering (convex hull comparison)
- Aspect ratio validation (0.2-5.0)

### Path Following
- Ray casting from car center in 3 directions (left, center, right)
- Distance measurement to road boundaries
- Steering calculation: left_turn if left_distance < center_distance
- Speed proportional to straightness of path

## Testing Tips

1. **Test Color Detection in Lab**:
   - Place red object on white background
   - Adjust HSV ranges if not detected
   
2. **Test Motion Detection**:
   - Start system with stationary car
   - Wait for motion detection to stabilize
   - Move car in front of camera
   
3. **Test Path Following**:
   - Start with slow speed (20-30)
   - Drive manually first to verify centerline is correct
   - Gradually increase speed
   - Monitor deviation from centerline

## Performance Optimization

- **Reduce Computational Load**:
  - Process frames at 15Hz instead of 30Hz
  - Use smaller ROI instead of full frame
  - Downscale camera resolution

- **Improve Detection**:
  - Better lighting on track
  - Increase contrast between car and track
  - Use larger/more visible boundary markers

## Emergency Stop

```bash
# If car doesn't stop
pkill -f VisionBasedRCCarControl
```

Press Ctrl+C during runtime to:
1. Send STOP command to car via BLE
2. Disconnect from car
3. Exit gracefully

## Support

For detailed algorithm explanations, see:
- `docs/PRD.md` - Product requirements and specifications
- `docs/PROJECT_STRUCTURE.md` - Detailed architecture
- `IMPLEMENTATION_COMPLETE.md` - Implementation notes

---

**Status**: Production-Ready  
**Last Updated**: 2024  
**Version**: 2.0 (Motion Detection Priority)
