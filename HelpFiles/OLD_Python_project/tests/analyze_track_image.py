#!/usr/bin/env python3
"""
Analyze the track image to understand what colors and patterns are present.
This helps calibrate the detection algorithm for the actual track.
"""

import cv2
import numpy as np
import sys

def analyze_image(image_path):
    """Analyze an image to find color information about track markers."""
    img = cv2.imread(image_path)
    
    if img is None:
        print(f"ERROR: Could not load image from {image_path}")
        return
    
    print(f"\n{'='*70}")
    print(f"TRACK IMAGE ANALYSIS")
    print(f"{'='*70}")
    print(f"Image: {image_path}")
    print(f"Dimensions: {img.shape[1]}×{img.shape[0]} (WxH)")
    
    # Convert to HSV for analysis
    hsv = cv2.cvtColor(img, cv2.COLOR_BGR2HSV)
    
    # ===== ANALYZE WHITE MARKERS =====
    print(f"\n{'-'*70}")
    print("WHITE MARKER DETECTION")
    print(f"{'-'*70}")
    
    white_mask1 = cv2.inRange(hsv, np.array([0, 0, 180]), np.array([180, 30, 255]))
    white_mask2 = cv2.inRange(hsv, np.array([0, 0, 120]), np.array([180, 60, 200]))
    white_mask3 = cv2.inRange(hsv, np.array([0, 0, 100]), np.array([180, 80, 180]))
    
    white_combined = cv2.bitwise_or(cv2.bitwise_or(white_mask1, white_mask2), white_mask3)
    white_pixels = np.count_nonzero(white_combined)
    
    print(f"Range 1 (Bright): V=180-255, S=0-30     → {np.count_nonzero(white_mask1):,} pixels")
    print(f"Range 2 (Medium): V=120-200, S=0-60     → {np.count_nonzero(white_mask2):,} pixels")
    print(f"Range 3 (Pale):   V=100-180, S=0-80     → {np.count_nonzero(white_mask3):,} pixels")
    print(f"Total White:                            → {white_pixels:,} pixels")
    
    # ===== ANALYZE RED MARKERS =====
    print(f"\n{'-'*70}")
    print("RED MARKER DETECTION")
    print(f"{'-'*70}")
    
    red_mask1 = cv2.inRange(hsv, np.array([0, 40, 30]), np.array([15, 255, 255]))
    red_mask2 = cv2.inRange(hsv, np.array([160, 40, 30]), np.array([180, 255, 255]))
    red_combined = cv2.bitwise_or(red_mask1, red_mask2)
    red_pixels = np.count_nonzero(red_combined)
    
    print(f"Lower Red (H=0-15):    → {np.count_nonzero(red_mask1):,} pixels")
    print(f"Upper Red (H=160-180): → {np.count_nonzero(red_mask2):,} pixels")
    print(f"Total Red:             → {red_pixels:,} pixels")
    
    # ===== ANALYZE ORANGE/YELLOW =====
    print(f"\n{'-'*70}")
    print("ORANGE/YELLOW MARKER DETECTION")
    print(f"{'-'*70}")
    
    orange_mask = cv2.inRange(hsv, np.array([5, 80, 50]), np.array([20, 255, 255]))
    orange_pixels = np.count_nonzero(orange_mask)
    
    print(f"Orange (H=5-20, S=80-255, V=50-255): → {orange_pixels:,} pixels")
    
    # ===== COMBINED TRACK MARKERS =====
    print(f"\n{'-'*70}")
    print("COMBINED TRACK MARKERS")
    print(f"{'-'*70}")
    
    all_markers = cv2.bitwise_or(cv2.bitwise_or(white_combined, red_combined), orange_mask)
    total_markers = np.count_nonzero(all_markers)
    
    print(f"White + Red + Orange: → {total_markers:,} pixels")
    print(f"Percentage of image:  → {100*total_markers/(img.shape[0]*img.shape[1]):.1f}%")
    
    # ===== SAMPLE PIXEL VALUES =====
    print(f"\n{'-'*70}")
    print("SAMPLE PIXEL ANALYSIS (at image center)")
    print(f"{'-'*70}")
    
    h, w = img.shape[:2]
    center_y, center_x = h//2, w//2
    
    # Sample from center and nearby points
    sample_points = [
        (center_x, center_y, "Center"),
        (center_x + 100, center_y, "Center+100 right"),
        (center_x - 100, center_y, "Center-100 left"),
        (center_x, center_y + 100, "Center+100 down"),
        (center_x, center_y - 100, "Center-100 up"),
    ]
    
    for x, y, label in sample_points:
        if 0 <= x < w and 0 <= y < h:
            b, g, r = img[y, x]
            h_val, s_val, v_val = hsv[y, x]
            print(f"{label:25} | BGR=({b:3d},{g:3d},{r:3d}) | HSV=({h_val:3d},{s_val:3d},{v_val:3d})")
    
    # ===== RECOMMENDATIONS =====
    print(f"\n{'-'*70}")
    print("RECOMMENDATIONS")
    print(f"{'-'*70}")
    
    if white_pixels > 5000:
        print("✓ White markers detected strongly - detection should work")
    elif white_pixels > 1000:
        print("⚠ White markers detected but weak - may need threshold adjustment")
    else:
        print("✗ White markers not detected - need to adjust ranges")
    
    if red_pixels > 5000:
        print("✓ Red markers detected strongly")
    elif red_pixels > 1000:
        print("⚠ Red markers detected but weak")
    else:
        print("✗ Red markers not detected")
    
    if total_markers < 1000:
        print("\n⚠ CRITICAL: Very few track markers detected!")
        print("  Try running: python debug_road_detection_detailed.py")
        print("  And check [h] mode to see actual HSV values of markers")
    
    # ===== SAVE VISUALIZATION =====
    print(f"\n{'-'*70}")
    print("SAVING VISUALIZATIONS")
    print(f"{'-'*70}")
    
    # White detection visualization
    cv2.imwrite("Path-image/white_detection.png", white_combined)
    print("✓ Saved: Path-image/white_detection.png (white markers only)")
    
    # Red detection visualization
    cv2.imwrite("Path-image/red_detection.png", red_combined)
    print("✓ Saved: Path-image/red_detection.png (red markers only)")
    
    # Orange detection visualization
    cv2.imwrite("Path-image/orange_detection.png", orange_mask)
    print("✓ Saved: Path-image/orange_detection.png (orange markers only)")
    
    # Combined visualization
    cv2.imwrite("Path-image/combined_detection.png", all_markers)
    print("✓ Saved: Path-image/combined_detection.png (all markers)")
    
    # Color-coded visualization (white=blue, red=red, orange=green)
    colored = np.zeros_like(img)
    colored[white_combined > 0] = [255, 0, 0]      # White as blue
    colored[red_combined > 0] = [0, 0, 255]        # Red as red
    colored[orange_mask > 0] = [0, 255, 0]         # Orange as green
    cv2.imwrite("Path-image/colored_markers.png", colored)
    print("✓ Saved: Path-image/colored_markers.png (color-coded by type)")
    
    # Morphology processing stages
    kernel_small = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (3, 3))
    kernel_medium = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (5, 5))
    
    opened = cv2.morphologyEx(all_markers, cv2.MORPH_OPEN, kernel_small, iterations=1)
    cv2.imwrite("Path-image/morph_opened.png", opened)
    print("✓ Saved: Path-image/morph_opened.png (after open)")
    
    closed = cv2.morphologyEx(opened, cv2.MORPH_CLOSE, kernel_medium, iterations=2)
    cv2.imwrite("Path-image/morph_closed.png", closed)
    print("✓ Saved: Path-image/morph_closed.png (after close)")
    
    dilated = cv2.dilate(closed, kernel_medium, iterations=3)
    cv2.imwrite("Path-image/morph_dilated.png", dilated)
    print("✓ Saved: Path-image/morph_dilated.png (after dilate)")
    
    # Find contours
    contours, _ = cv2.findContours(dilated, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
    contour_img = img.copy()
    cv2.drawContours(contour_img, contours, -1, (0, 255, 0), 2)
    cv2.imwrite("Path-image/detected_contours.png", contour_img)
    print(f"✓ Saved: Path-image/detected_contours.png (found {len(contours)} contours)")
    
    print(f"\n{'='*70}")
    print(f"Analysis complete! Check the saved PNG files in this directory.")
    print(f"{'='*70}\n")

if __name__ == "__main__":
    if len(sys.argv) > 1:
        image_path = sys.argv[1]
    else:
        image_path = "Path-image/image-1.png"
    
    analyze_image(image_path)
