#!/usr/bin/env python3
"""
Extract clean track boundaries from adaptive threshold by filtering noise.
"""

import cv2
import numpy as np

def extract_clean_boundaries(image_path):
    """Extract only the track boundaries, filtering out noise."""
    
    frame = cv2.imread(image_path)
    if frame is None:
        print(f"ERROR: Could not load {image_path}")
        return
    
    height, width = frame.shape[:2]
    
    print(f"\n{'='*70}")
    print(f"CLEAN BOUNDARY EXTRACTION")
    print(f"{'='*70}")
    print(f"Image: {width}×{height}")
    
    # ---- Convert to grayscale ----
    gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
    
    # ---- Blur ----
    gray = cv2.GaussianBlur(gray, (5, 5), 0)
    
    # ---- Adaptive threshold ----
    binary = cv2.adaptiveThreshold(
        gray, 
        255,
        cv2.ADAPTIVE_THRESH_GAUSSIAN_C,
        cv2.THRESH_BINARY_INV,
        11,
        2
    )
    
    # ---- Apply VERY gentle morphology - just remove tiny noise ----
    kernel_tiny = np.ones((2, 2), np.uint8)
    
    # Remove isolated pixels
    binary = cv2.morphologyEx(binary, cv2.MORPH_OPEN, kernel_tiny, iterations=1)
    
    # Close very small gaps only
    binary = cv2.morphologyEx(binary, cv2.MORPH_CLOSE, kernel_tiny, iterations=1)
    
    print(f"✓ Adaptive threshold applied with minimal morphology")
    
    # ---- Find ALL contours ----
    contours, _ = cv2.findContours(binary, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
    
    print(f"✓ Found {len(contours)} total contours")
    
    # ---- Filter contours by size to get only track boundaries ----
    # Keep only the largest contours (track boundaries)
    all_contours_with_area = [(cv2.contourArea(c), c) for c in contours]
    all_contours_with_area.sort(reverse=True, key=lambda x: x[0])
    
    print(f"\nTop 10 contour areas:")
    for i in range(min(10, len(all_contours_with_area))):
        print(f"  {i+1}. {all_contours_with_area[i][0]:,.0f} px²")
    
    # Keep top 10 largest contours (should include inner/outer boundaries and segments)
    boundary_contours = all_contours_with_area[:10]
    boundary_contours = [item for item in boundary_contours if item[0] >= 700]  # Filter tiny ones
    
    print(f"\n{'='*70}")
    print(f"FILTERING CONTOURS (top 10, minimum 700 px²)")
    print(f"{'='*70}")
    
    print(f"✓ Found {len(boundary_contours)} boundary contours")
    
    # Show top 10
    for idx in range(min(10, len(boundary_contours))):
        area = boundary_contours[idx][0]
        print(f"  {idx+1}. Area: {area:,.0f} px²")
    
    # ---- Create visualization ----
    result = frame.copy()
    clean_boundaries = np.zeros_like(frame)
    
    # ---- Draw ALL boundary contours (not just top 4) ----
    print(f"\n{'='*70}")
    print(f"DRAWING ALL BOUNDARY SEGMENTS")
    print(f"{'='*70}")
    
    # Draw all in green
    for idx, (area, contour) in enumerate(boundary_contours):
        cv2.drawContours(result, [contour], 0, (0, 255, 0), 2)
        cv2.drawContours(clean_boundaries, [contour], 0, (0, 255, 0), 2)
    
    print(f"✓ Drew {len(boundary_contours)} boundary segments in green")
    
    # ---- Save results ----
    cv2.imwrite("Path-image/clean_boundaries_on_image.png", result)
    cv2.imwrite("Path-image/clean_boundaries_only.png", clean_boundaries)
    cv2.imwrite("Path-image/adaptive_binary_cleaned.png", binary)
    
    print(f"\n{'='*70}")
    print(f"SAVED")
    print(f"{'='*70}")
    print(f"✓ Path-image/clean_boundaries_on_image.png")
    print(f"✓ Path-image/clean_boundaries_only.png (boundaries on black)")
    print(f"✓ Path-image/adaptive_binary_cleaned.png (cleaned binary)")
    print(f"{'='*70}\n")

if __name__ == "__main__":
    extract_clean_boundaries("Simulation/image-1.png")
