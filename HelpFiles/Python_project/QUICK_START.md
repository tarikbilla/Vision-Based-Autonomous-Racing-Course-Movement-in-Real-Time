# Quick Start Guide - Road Edge Detection System

## In One Minute

1. **Start the system**:
   ```bash
   cd /Users/tarikbilla/Projects/IoT-Project-Vision-based-autonomous-RC-car-control-system
   source .venv/bin/activate
   python run_autonomy.py
   ```

2. **Choose tracking mode**:
   - Press **'a'** → Auto red-car tracking with road edge guidance
   - Press **'s'** → Manual ROI selection (click and drag to select car)
   - Press **'q'** → Quit

3. **Watch the display**:
   - **Green lines** = Road edges detected
   - **Blue line** = Center path to follow
   - **Red box** = Car being tracked

## What Each Keypress Does

| Key | Action |
|-----|--------|
| `a` | **Auto tracking**: Find red car automatically, use road edges for guidance |
| `s` | **Manual ROI**: Freeze frame, select car by dragging, then track it |
| `q` | **Quit**: Stop system and exit |

## The System in Action

```
Press 'a' for auto tracking:
  ↓
System scans for road edges (red/white markers)
  ↓
If edges found:
  - Draws green lines at edge positions
  - Draws blue line at center
  - Tracks red car within the road
  - Steers to keep car centered
  ↓
If edges not found:
  - Looks for red car by color
  - Uses generic ray-casting for obstacles
  - Still tracks and controls the car
```

## Configuration Changes

Camera is now set to **index 0** (was 1).
To verify which camera works on your system:

```bash
python diagnose_camera.py
```

## File Changes Summary

### Modified Files
- `rc_autonomy/boundary.py` - Added `detect_road_edges()` method
- `rc_autonomy/orchestrator.py` - Improved ROI validation and edge visualization
- `config/default.json` - Changed camera source to 0

### New Files
- `EDGE_DETECTION_GUIDE.md` - Complete usage guide
- `IMPLEMENTATION_CHANGES.md` - Technical details of changes
- `validate_system.py` - System validation script

## Visual Output

### Live Camera Window
```
Road Center: 320
┌─────────────────────────────────────────────────┐
│                                                 │
│  |      ║      ║      ║      |                 │ ← Green = edges
│  |      ║      ║      ║      |                 │ ← Blue = center
│  |      ║ [car]║      ║      |                 │ ← Red box = car
│  |      ║      ║      ║      |                 │
│  |______║______║______║______|                 │
│                                                 │
└─────────────────────────────────────────────────┘
Speed: 10 | Left: 0 | Right: 0 | Rays: L=150 C=180 R=160
```

### Analysis Window
Shows: Car bounding box, center point, and distance rays

## Key Features

✅ **Road Edge Detection** - Finds red/white markers automatically
✅ **Centerline Guidance** - Shows optimal path down the road
✅ **Auto Color Tracking** - Finds red car without manual selection
✅ **Smart ROI Selection** - Up to 5 attempts with fallback to auto mode
✅ **Graceful Fallbacks** - Works even if edges aren't detected
✅ **Visual Feedback** - See exactly what the system is detecting

## Troubleshooting

### "Camera failed"
```bash
python diagnose_camera.py
# Update config/default.json with correct camera index
```

### "No road edges detected"
- Check that track has red/white boundary markers
- Ensure good lighting
- Adjust `black_threshold` in `config/default.json`

### "Car not tracked"
- Press 's' to manually select ROI instead
- Make sure car is clearly visible
- Try 'a' for auto color tracking

## Technical Validation

All 5 system tests pass ✓:
- Imports working
- Configuration loaded
- Road edge detection functional
- ROI validation correct
- Type annotations present

Run `python validate_system.py` anytime to verify system health.

---

**Ready to go!** Just run `python run_autonomy.py` and press 'a' for auto-tracking.
