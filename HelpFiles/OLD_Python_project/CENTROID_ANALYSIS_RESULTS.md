# Centroid-Based Line Following Analysis - Results

## Analysis Complete ✅

### 10-Step Process Applied to `Path-image/image-1.png`

1. **STEP 1: Capture Image** - 1134×657 RGB
2. **STEP 2: Grayscale** - Converted successfully
3. **STEP 3: ROI Crop** - Bottom 1/3 (219 pixels height)
4. **STEP 4: Resize** - 320×120 for processing
5. **STEP 5: Threshold** - Best threshold = **127** (39.7% white pixels)
6. **STEP 6: Denoise** - Morphological operations applied
7. **STEP 7: Centroid** - Found at **(242, 54)**
8. **STEP 8: P Controller** - Error = **82 px**, Control = **-0.41**
9. **STEP 9: Motor Control** - Steering = **-30°** (max right)
10. **STEP 10: Sampling** - Ts = **0.5s**

---

## Key Results

### Centroid Detection
- **Position:** (242, 54) in 320×120 image
- **Image Center:** 160 px
- **Error:** 82 px to the RIGHT
- **Interpretation:** Track curves right, car needs to turn right

### Control Output
```
Kp = 0.005
Error = 82 px (right of center)
Control = -0.41

Steering angle = -30° (maximum right turn)
Left motor = 40
Right motor = -20
```

### Best Threshold Analysis
| Threshold | White Pixels | Percentage |
|-----------|-------------|------------|
| 100 | 5,535 | 14.4% |
| **127** | **15,236** | **39.7%** ✓ |
| 150 | 29,350 | 76.4% |
| 180 | 37,127 | 96.7% |

**Selected:** 127 (good balance - not too much noise, clear track)

---

## Generated Visualizations

All saved in **Path-image/** folder:

### Processing Steps
- `step1_original.png` - Original RGB image
- `step2_grayscale.png` - Grayscale conversion
- `step3_roi.png` - Bottom 1/3 ROI
- `step4_resized.png` - 320×120 resized
- `step5_threshold_100.png` - Threshold @ 100
- `step5_threshold_127.png` - Threshold @ 127 ✓
- `step5_threshold_150.png` - Threshold @ 150
- `step5_threshold_180.png` - Threshold @ 180
- `step5_binary_final.png` - Final binary image
- `step6_denoised.png` - After morphological cleaning
- `step7_centroid.png` - Centroid marked (red dot + cyan line)
- `step8_control.png` - Control visualization (error arrow)

---

## File Organization ✅

### `/tests` folder contains:
- `analyze_centroid_method.py` - This centroid analysis script
- `analyze_track_image.py` - Color detection analysis
- `analyze_track_structure.py` - Contour structure analysis
- `detect_circular_track.py` - Circular track detection
- `test_ble_python.py` - BLE connection test
- `test_car_control.py` - Car control test
- `test_circular_detection.py` - Circular edge detection test
- `test_complete_demo.py` - Complete demo test
- `test_full_pipeline.py` - Full pipeline test
- `test_inner_outer_detection.py` - Inner/outer boundary test

### `/Path-image` folder contains:
- `image-1.png` - Original track image
- `step1_*.png` through `step8_*.png` - Analysis visualizations

---

## How to Run

```bash
# Run centroid analysis
python tests/analyze_centroid_method.py

# Run other tests
python tests/analyze_track_image.py
python tests/test_inner_outer_detection.py
```

---

## Comparison: Centroid vs Edge Detection

### Centroid Method (Classic)
✅ Simple and fast
✅ Works well for clear straight lines
✅ Low computational cost
❌ Struggles with curved tracks
❌ Single centroid doesn't show left/right boundaries
❌ Error at 82px suggests centroid far from desired path

### Edge Detection (Current System)
✅ Detects both left and right boundaries
✅ Handles curved circular tracks
✅ Provides explicit lane boundaries
✅ More robust for complex track shapes
✅ Can follow centerline between boundaries

---

## Recommendation

For your **circular track**, the **edge detection approach** (already implemented in `rc_autonomy/boundary.py`) is superior because:

1. Circular tracks have varying curvature
2. Need to know BOTH boundaries (inner and outer ring)
3. Centroid gives one point, edges give full lane context
4. Your track: outer boundary (L=0, R=1133), centerline at x=717

The centroid method would struggle to follow the curved path correctly.

---

**Status:** Analysis complete, all files organized ✅
