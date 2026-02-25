# Road Detection Fix Guide - For Your Track

## Problem
Road boundaries are not being detected, causing the car to spin in circles using only ray-casting fallback.

**Console shows:** No "Road: L=... C=... R=..." output

## Solution Steps

### Step 1: Run the Detailed Debug Tool
This tool shows exactly what the system sees at each color detection stage.

```bash
python debug_road_detection_detailed.py
```

**Controls in debug tool:**
- **[r]** - Show RED detection only
- **[w]** - Show WHITE detection only
- **[o]** - Show ORANGE detection only
- **[a]** - Show ALL COLORS combined (default)
- **[h]** - Show HSV channels
- **[c]** - Show final combined mask
- **[m]** - Show morphology processing steps
- **[Space]** - Pause/Resume
- **[q]** - Quit

### Step 2: Identify Your Track Marker Colors

**What to look for in the debug output:**

1. **Press [r]** - Are the red/white track markers showing as WHITE pixels (detected)?
   - If YES: Red detection is working ✓
   - If NO: Red markers might be different color → Go to Step 3

2. **Press [w]** - Are the white markers showing as WHITE pixels?
   - If YES: White detection is working ✓
   - If NO: White might be very bright or different tone → Go to Step 4

3. **Press [a]** - Is there any WHITE in the combined mask where markers should be?
   - If YES: At least some markers are detected ✓
   - If NO: Need to adjust HSV thresholds → Go to Step 5

### Step 3: Markers Are Different Color?

If markers aren't red/white, identify the actual color:

**Press [h]** to see HSV channels:
- Left channel = HUE (color type)
- Middle channel = SATURATION (color intensity)
- Right channel = VALUE (brightness)

**Look at marker pixels in HSV view:**
- Note the H value (0-180)
- Note the S value (0-255)
- Note the V value (0-255)

**Then email me the values and I'll add custom detection**

### Step 4: White Detection Not Working

If white markers aren't showing up in [w] mode:

**File to edit:** `rc_autonomy/boundary.py` line ~78

**Current ranges:**
```python
white_mask1 = cv2.inRange(hsv, np.array([0, 0, 200]), np.array([180, 25, 255]))  # Very bright
white_mask2 = cv2.inRange(hsv, np.array([0, 0, 150]), np.array([180, 50, 200]))  # Light gray
```

**Try these adjustments:**

If white is dimmer:
```python
white_mask1 = cv2.inRange(hsv, np.array([0, 0, 100]), np.array([180, 50, 255]))  # More permissive
```

If white has more saturation:
```python
white_mask2 = cv2.inRange(hsv, np.array([0, 20, 100]), np.array([180, 100, 200]))  # Allow higher sat
```

### Step 5: Red Detection Not Working

If red markers aren't showing up in [r] mode:

**File to edit:** `rc_autonomy/boundary.py` line ~75

**Current ranges:**
```python
red_mask1 = cv2.inRange(hsv, np.array([0, 40, 30]), np.array([15, 255, 255]))     # Bright reds
red_mask2 = cv2.inRange(hsv, np.array([160, 40, 30]), np.array([180, 255, 255])) # Darker reds
```

**Try these adjustments:**

If red is darker/less saturated:
```python
red_mask1 = cv2.inRange(hsv, np.array([0, 20, 20]), np.array([15, 255, 255]))     # More permissive
red_mask2 = cv2.inRange(hsv, np.array([160, 20, 20]), np.array([180, 255, 255]))
```

If red is more orange-ish:
```python
orange_mask = cv2.inRange(hsv, np.array([0, 50, 30]), np.array([25, 255, 255]))  # Expand orange range
```

### Step 6: Verify Fix

After adjusting thresholds:

1. **Run debug tool again:**
   ```bash
   python debug_road_detection_detailed.py
   ```

2. **Press [a]** or [c]** - Look for WHITE pixels where your markers are
   - You should see a mask showing detected markers

3. **Run validation:**
   ```bash
   python validate_system.py
   ```
   - Should show 5/5 tests passing

4. **Run the system:**
   ```bash
   python run_autonomy.py --device f9:af:3c:e2:d2:f5
   ```
   - Press 'a' for auto tracking
   - Look for **GREEN LINES** on screen (road edges)
   - Should see **BLUE CENTERLINE**

### Step 7: If Still Not Working

**Check these things:**

1. **Lighting?**
   - Is the track well-lit?
   - Are there harsh shadows?
   - Try adjusting room lighting

2. **Camera focus?**
   - Is the image blurry?
   - Try manually focusing if possible

3. **Marker visibility?**
   - Can you clearly see red/white stripes in raw camera feed?
   - Run: `python diagnose_camera.py`

4. **Marker size?**
   - Are stripes large enough (>50 pixels wide)?
   - Very small markers might not be detected

---

## Quick Reference: HSV Color Ranges

### Red (Current)
```
Lower: H=0-15, S=40-255, V=30-255
Upper: H=160-180, S=40-255, V=30-255
```

### White (Current)
```
Very Bright: H=0-180, S=0-25, V=200-255
Light Gray: H=0-180, S=0-50, V=150-200
```

### Orange (Current)
```
Orange: H=5-20, S=80-255, V=50-255
```

---

## Common Issues & Solutions

### Issue: Red/white detected but no road edges shown on camera

**Cause:** Detected markers too small or fragmented

**Fix:** Increase morphological dilation
```python
# Line ~88 in boundary.py
edge_mask = cv2.dilate(edge_mask, kernel_medium, iterations=3)  # Change 2 → 3
```

### Issue: Too many false detections (noise showing as markers)

**Cause:** HSV thresholds too permissive

**Fix:** Tighten the ranges:
```python
# Increase saturation minimum
red_mask1 = cv2.inRange(hsv, np.array([0, 60, 30]), np.array([15, 255, 255]))  # 40 → 60
```

### Issue: Road edges detected but car still spins

**Cause:** Markers too close together or separated incorrectly

**Fix:** Increase minimum edge separation
```python
# Line ~102 in boundary.py
min_separation = max(100, width * 0.25)  # Change 0.15 → 0.25
```

---

## Testing Checklist

```
After making changes:

□ Run debug_road_detection_detailed.py
□ Check [a] or [c] mode shows markers as white pixels
□ Check [m] mode shows morphology processing
□ Run validate_system.py (5/5 tests pass)
□ Run diagnose_camera.py (camera working)
□ Run python run_autonomy.py
□ Press 'a' for auto tracking
□ Look for GREEN boundary lines
□ Look for BLUE centerline
□ Console shows "Road: L=... C=... R=..."
□ Car follows center without spinning
```

---

## Getting More Help

**If you get stuck:**

1. Run: `python debug_road_detection_detailed.py`
2. Try each mode: [r], [w], [o], [a], [h], [c], [m]
3. Screenshot or note what you see
4. Share what you observe

**Key info to provide:**
- Which markers are detected? (Red? White? Both?)
- What happens in [h] (HSV) view?
- What do HSV values look like for your markers?
- What happens in [c] (combined) view?

---

**Next: Try the debug tool and see what your track markers look like!** 🔍
