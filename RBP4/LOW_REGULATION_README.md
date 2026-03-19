# Low-Regulation Autonomous Controller

Ultra-low CPU mode controller for Raspberry Pi 4 with severe resource constraints.

## Key Optimizations

| Feature | Full Version | Low-Regulation |
|---------|-------------|-----------------|
| **Resolution** | 1280×720 | **480×270** |
| **FPS** | 20 | **10** |
| **MOG2 History** | 500 | **200** |
| **Frame Skip** | 0 | **1 of 3** |
| **Centerline Samples** | 1200 | **400** |
| **ROI Half-Size** | 180px | **120px** |
| **Global Search Freq** | 1/8 frames | **1/16 frames** |
| **Processing** | Full pipeline | **MOG2 + Centroid** |

## Expected CPU Impact

- **~70-80% CPU reduction** vs full version
- Pi 4 CPU stays < 30% in normal operation
- Can run alongside other services

## Building

```bash
cd /Users/tarikbilla/Projects/IoT-Project-Vision-based-autonomous-RC-car-control-system/RBP4
./build.sh
```

This compiles both full and low-regulation versions.

## Running on Pi

### Ultra-Low Mode (default):
```bash
export CAM_FRAME_W=480
export CAM_FRAME_H=270
export CAM_FPS=10

./build/autonomous_centerline_controller_low_regulation_cpp \
  --name "DRiFT-ED5C2384488D" \
  --max-deg 24 \
  --steer-smooth 70 \
  --ratio 0.9 \
  --c-sign -1 \
  --speed-k 0.08 \
  --speed 100
```

### Low-Resource but Higher Quality:
```bash
export CAM_FRAME_W=640
export CAM_FRAME_H=360
export CAM_FPS=12

./build/autonomous_centerline_controller_low_regulation_cpp \
  --name "DRiFT-ED5C2384488D" \
  --max-deg 24 \
  --steer-smooth 70 \
  --ratio 0.9 \
  --c-sign -1 \
  --speed-k 0.08 \
  --speed 100
```

## Keyboard Controls

- `w` / `s` - Speed up / down
- `a` / `d` - Steer left / right
- `b` - Brake
- `<space>` - Stop
- `q` - Quit

## Technical Changes vs Full Version

1. **Resolution Throttling**: Hardcoded defaults to 480×270, with env var override
2. **Frame Skipping**: Processes 1 of 3 frames (2 skipped)
3. **Minimal Vision Pipeline**: Only MOG2 + threshold, no complex Kalman filtering
4. **Centroid-Based Steering**: Direct centroid→steering mapping instead of path tracking
5. **Simplified State Machine**: No mode transitions, no advanced IMM filtering
6. **Reduced Morphology**: Smaller kernels (3×3 open, 5×5 close) vs (5×5, 7×7)

## When to Use

✅ **Use Low-Regulation for:**
- Raspberry Pi 4 running other services
- Limited power supply (USB powered)
- Simple lane-following without complex maneuvers
- High frame rate more important than processing quality

❌ **Use Full Version for:**
- Dedicated Pi (no other services)
- Complex path tracking required
- Need for advanced motion prediction
- Centerline-based navigation

## Centerline CSV Format

The controller still supports CSV centerline loading with automatic resolution scaling:

```csv
# W=1280 H=720
640,360
650,340
...
```

The `W=` and `H=` values indicate the source resolution. The controller will automatically scale all points to the current camera resolution at runtime.

To regenerate centerline at low resolution:
```bash
# Press 'M' key at startup to capture new centerline at current resolution
```

## Performance Notes

- **Pi CPU**: Typically 15-25% with this controller
- **Memory**: ~50 MB RSS (vs 150+ MB for full version)
- **BLE Updates**: Still 20 Hz for smooth steering
- **Camera Latency**: ~100 ms (minimal, due to frame skipping and low resolution)

## Debugging

Enable verbose logging by editing the source:
```cpp
std::cout << "[DEBUG] Frame " << frame_count_ << ": " << pts.size() << " points\n";
```

To check Pi system load:
```bash
top -b -n 1 | head -15
```

Target: `%CPU` column < 30%
