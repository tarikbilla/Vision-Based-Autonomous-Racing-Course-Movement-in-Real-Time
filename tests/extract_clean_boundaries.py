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
    
    # ---- Calculate circular centerline ----
    if len(boundary_contours) >= 2:
        # For circular track: identify inner and outer boundaries
        # Assume largest contour is outer, second largest is inner
        outer_boundary = boundary_contours[0][1]
        inner_boundary = boundary_contours[1][1]
        
        print(f"\n{'='*70}")
        print(f"CALCULATING CIRCULAR CENTERLINE")
        print(f"{'='*70}")
        print(f"  Outer boundary area: {boundary_contours[0][0]:,.0f} px²")
        print(f"  Inner boundary area: {boundary_contours[1][0]:,.0f} px²")
        
        # Create masks for inner and outer boundaries
        outer_mask = np.zeros((height, width), dtype=np.uint8)
        inner_mask = np.zeros((height, width), dtype=np.uint8)
        cv2.drawContours(outer_mask, [outer_boundary], 0, 255, -1)  # Fill
        cv2.drawContours(inner_mask, [inner_boundary], 0, 255, -1)  # Fill
        
        # Road area is between outer and inner
        road_mask = cv2.bitwise_xor(outer_mask, inner_mask)
        
        # Find centerline: for each row, get midpoint between boundaries
        centerline_points = []
        for y in range(height):
            row_road = road_mask[y, :]
            if np.any(row_road):
                road_indices = np.where(row_road > 0)[0]
                if len(road_indices) > 0:
                    left_edge = road_indices[0]
                    right_edge = road_indices[-1]
                    center_x = (left_edge + right_edge) // 2
                    centerline_points.append((center_x, y))
        
        # Draw circular centerline
        if len(centerline_points) > 1:
            centerline_points = np.array(centerline_points, dtype=np.int32)
            cv2.polylines(result, [centerline_points], False, (0, 0, 255), 3)
            cv2.polylines(clean_boundaries, [centerline_points], False, (0, 0, 255), 3)
            print(f"✓ Drew circular centerline with {len(centerline_points)} points")
        else:
            print("⚠ Could not calculate centerline")
        
        # Add text labels
        cv2.putText(result, "TRACK BOUNDARIES (green)", (20, 40),
                    cv2.FONT_HERSHEY_SIMPLEX, 1.0, (0, 255, 0), 2)
        cv2.putText(result, "CIRCULAR CENTERLINE (red)", (20, 80),
                    cv2.FONT_HERSHEY_SIMPLEX, 1.0, (0, 0, 255), 2)
        cv2.putText(result, f"{len(boundary_contours)} segments", (20, 120),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.8, (255, 255, 255), 2)
    else:
        print("⚠ Need at least 2 boundaries (inner + outer) for circular centerline")
    
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
