# Professional Road Detection System - Quick Start Guide

## Setup (30 seconds)

```bash
# Activate environment
source .venv/bin/activate

# Start the system
python run_autonomy.py --device f9:af:3c:e2:d2:f5
```

---

## What You'll See

When the system starts, you should see:

```
┌──────────────────────────────────────────────────────┐
│  RC Autonomy - Road Detection                        │
├──────────────────────────────────────────────────────┤
│                                                      │
│  [GREEN INDICATOR] ← Road detected ✓                 │
│  ROAD DETECTED | L:150 | C:320 | R:490 | W:340px    │
│                                                      │
│   GREEN LINE          GREEN LINE                     │
│      ↓                    ↓                          │
│   ━━━◎━━━  ← Blue dashed centerline                │
│      ↑                                               │
│    Car position (cyan circle)                        │
│                                                      │
│  Car Offset: -10px (-2.9%)  ← Tracking offset       │
│                                                      │
│  Speed:  10 | Left:  0 | Right:  0  ← Commands     │
└──────────────────────────────────────────────────────┘
```

### Visual Guide

| Element | Color | Meaning |
|---------|-------|---------|
| **Side Lines** | 🟢 Green | Track boundaries detected |
| **Center Line** | 🔵 Blue | Optimal path (dashed) |
| **Road Overlay** | 🟢 Green (semi) | Confirmed road area |
| **Car Circle** | 🔷 Cyan | Your car's current position |
| **Offset Line** | 🟠 Orange | Distance from centerline |
| **Top Indicator** | 🟢 Green or 🔴 Red | Detection status |

---

## How It Works

### 1. **On Startup** (5 seconds)

System activates and begins searching for:
- ✓ Red boundary markers (`/` and `\` stripes)
- ✓ White boundary markers
- ✓ Light gray edge markers

### 2. **When Road is Detected**

Once boundaries are found:
1. Calculate **left edge** position (green line)
2. Calculate **right edge** position (green line)
3. Calculate **centerline** (blue dashed line)
4. Draw **road region** (green overlay)

### 3. **Tracking the Car**

The system:
1. Locates your red car
2. Calculates offset from centerline
3. Commands steering proportionally
4. Adjusts speed based on position

### 4. **Steering Commands**

```
Car centered (±20px):
  → Speed: normal (10)
  → Steering: none (0)

Car drifting left (-100px):
  → Speed: reduced (5)
  → Steering: turn right (+20)

Car drifting right (+100px):
  → Speed: reduced (5)
  → Steering: turn left (+20)
```

---

## Keyboard Controls

| Key | Action |
|-----|--------|
| **'a'** | Auto red-car tracking (recommended) |
| **'s'** | Manual ROI selection |
| **'q'** | Quit system |

---

## Troubleshooting: Road Not Detected?

### ✓ Step 1: Check Camera Feed
```bash
python diagnose_camera.py
```
- Verify you can see the track
- Check red and white markers are visible
- Ensure adequate lighting

### ✓ Step 2: Test Road Detection
```bash
python diagnose_road_detection.py
```

**Watch the output:**
```
[0001] Road: L=150 C=320 R=490 W=340px | Detection: 100.0%  ✓ Good
[0002] ✗ Road NOT detected | Detection: 50.0%            ✗ Problem
```

**Good:** You see left/center/right coordinates
**Bad:** See "Road NOT detected"

### ✓ Step 3: Check Lighting

- Room should be well-lit (but not direct sunlight on camera)
- Avoid shadows on track
- Verify red markers aren't too dark

### ✓ Step 4: Verify Track Colors

Use diagnostic tool's features:
```
In diagnose_road_detection.py:
  Press [h] for HSV histogram
  Press [c] for color space view
  Press [d] for debug statistics
```

This shows:
- Red pixel count
- White pixel count
- HSV distribution

---

## Tuning the System

### If Road Detection Works but Steering is Bad

**Edit `config/default.json`:**

```json
{
  "guidance": {
    "default_speed": 10      // Reduce if jerky: 10 → 5
  },
  "control": {
    "steering_limit": 30     // Reduce if overshooting: 30 → 20
  }
}
```

### If Road Not Detected with Good Lighting

**Edit color detection in `rc_autonomy/boundary.py`:**

Find line: `red_mask1 = cv2.inRange(hsv, np.array([0, 50, 40])`

**If red markers too dark:**
```python
# Change FROM:  np.array([0, 50, 40])
# Change TO:    np.array([0, 30, 20])  # Accept darker reds
```

**If white markers not bright enough:**
```python
# Change FROM:  np.array([0, 0, 180])
# Change TO:    np.array([0, 0, 150])  # Accept darker whites
```

---

## Performance Expectations

| Metric | Value |
|--------|-------|
| **Road Detection Rate** | >95% (good lighting) |
| **Detection Latency** | ~20-30ms |
| **Steering Response** | <100ms total |
| **Centerline Accuracy** | ±20px (±5% of road width) |
| **Frame Rate** | 30 FPS (configurable) |

---

## Real-World Testing Steps

### 1. Start with Low Speed
```json
{
  "guidance": {
    "default_speed": 5    // Start slow
  }
}
```

### 2. Test on Straight Section First
- Verify car follows centerline
- Watch offset values (should be ±20px)
- Check that speed is constant

### 3. Increase to Curves
- Car should lean into turns slightly
- Steering should be smooth, not jerky
- Speed should only reduce if drifting far

### 4. Gradually Increase Speed
```json
{
  "guidance": {
    "default_speed": 5 → 10 → 15    // Increase gradually
  }
}
```

### 5. Fine-Tune Steering
If overshooting centerline:
```json
{
  "control": {
    "steering_limit": 30 → 20        // More gentle
  }
}
```

---

## Common Questions

### Q: Why is detection sometimes failing?

**A:** Road detection depends on:
- ✓ Lighting (shadows cause failures)
- ✓ Color contrast (markers must be distinct)
- ✓ Camera angle (should be centered over track)
- ✓ Marker visibility (should fill >2% of frame)

**Solution:** Improve one factor at a time.

### Q: Car drifts instead of following center

**A:** Steering is not aggressive enough.

**Solutions:**
1. Increase `steering_limit`: 30 → 40
2. Check `default_speed` isn't too high
3. Verify centerline is calculated correctly (use `diagnose_road_detection.py`)

### Q: Car overshoots centerline

**A:** Steering is too aggressive.

**Solutions:**
1. Decrease `steering_limit`: 30 → 20
2. Decrease `default_speed`: 10 → 5
3. Verify track width is sufficient (>100px)

### Q: Green boundaries don't appear on camera

**A:** Road detection is failing silently.

**Solutions:**
1. Run `diagnose_road_detection.py` to see detection rate
2. Check camera can see track (use `diagnose_camera.py`)
3. Verify lighting is adequate
4. Adjust HSV color thresholds (see tuning section)

### Q: Can I use different colored markers?

**A:** Yes! Modify `boundary.py` to detect your colors.

**Example for blue markers:**
```python
blue_mask = cv2.inRange(hsv, np.array([100, 100, 50]), np.array([130, 255, 255]))
edge_mask = cv2.bitwise_or(edge_mask, blue_mask)
```

---

## Debug Output Reference

### When Road is Detected

```
[0001] Road: L=150 C=320 R=490 W=340px | Detection: 100.0%
       └─┬──┘ └─┘ └──┘ └──┘ └──────┘
         │     │   │    │    └─ Road width (pixels)
         │     │   │    └──── Right boundary
         │     │   └───────── Centerline
         │     └──────────── Left boundary
         └─────────────────── Frame count
```

### When Road Not Detected

```
[0002] ✗ Road NOT detected | Detection: 50.0%
       └──────────────────────────────┬─────────┘
                                      └─ Success rate so far
```

---

## Files to Know

| File | Purpose |
|------|---------|
| `config/default.json` | All tuning parameters |
| `rc_autonomy/boundary.py` | Road detection algorithm |
| `rc_autonomy/orchestrator.py` | Main control loop |
| `diagnose_camera.py` | Raw camera feed viewer |
| `diagnose_road_detection.py` | Road detection analyzer |
| `run_autonomy.py` | Main entry point |

---

## Next Steps

1. ✓ Run `diagnose_camera.py` - verify camera works
2. ✓ Run `diagnose_road_detection.py` - check detection
3. ✓ Start system with `python run_autonomy.py --simulate` (simulation mode)
4. ✓ Test with actual car and track
5. ✓ Adjust config as needed
6. ✓ Run multiple laps to verify consistency

---

## Pro Tips

**Tip 1:** Record a video for later analysis
```bash
python run_autonomy.py --record video.mp4
```

**Tip 2:** Test in simulation to verify steering logic
```bash
python run_autonomy.py --simulate --duration 60
```

**Tip 3:** Use multiple camera angles during testing
```bash
# Test with different camera sources
python run_autonomy.py --camera 0  # Default
python run_autonomy.py --camera 1  # USB camera
```

**Tip 4:** Log all detections for analysis
```bash
python run_autonomy.py --log detections.txt
```

---

## Support

If you encounter issues:

1. Check `ROAD_DETECTION_TUNING.md` for detailed parameter guide
2. Review `IMPLEMENTATION_CHANGES.md` for recent updates
3. Run diagnostic tools:
   - `python validate_system.py`
   - `python diagnose_camera.py`
   - `python diagnose_road_detection.py`
4. Check console output for specific errors

---

**Happy Autonomous Racing! 🏁**
