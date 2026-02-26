#!/usr/bin/env python3
"""
Simple circular road boundary detection
Detects white/red boundaries and calculates centerline
"""

import cv2
import numpy as np

def detect_road_boundaries_simple(image_path):
    """Simple boundary detection for circular track"""
    
    print(f"{'='*70}")
    print(f"CIRCULAR TRACK BOUNDARY DETECTION")
    print(f"{'='*70}")
    
    # Load image
    frame = cv2.imread(image_path)
    if frame is None:
        print(f"✗ Could not load: {image_path}")
        return
    
    height, width = frame.shape[:2]
    print(f"Image: {width}×{height}")
    
    # Convert to grayscale
    gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
    blurred = cv2.GaussianBlur(gray, (5, 5), 0)
    
    # Adaptive threshold
    binary = cv2.adaptiveThreshold(
        blurred, 255,
        cv2.ADAPTIVE_THRESH_GAUSSIAN_C,
        cv2.THRESH_BINARY_INV,
        11, 2
    )
    
    # Minimal morphology
    kernel = np.ones((3, 3), np.uint8)
    binary = cv2.morphologyEx(binary, cv2.MORPH_OPEN, kernel, iterations=1)
    binary = cv2.morphologyEx(binary, cv2.MORPH_CLOSE, kernel, iterations=1)
    
    # Find contours
    contours, _ = cv2.findContours(binary, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
    
    # Sort by area and keep large ones
    contours_sorted = sorted(contours, key=cv2.contourArea, reverse=True)
    large_contours = [c for c in contours_sorted if cv2.contourArea(c) >= 3000]
    
    print(f"\n✓ Found {len(large_contours)} large contours")
    for i in range(min(5, len(large_contours))):
        print(f"  {i+1}. Area: {cv2.contourArea(large_contours[i]):,.0f} px²")
    
    if len(large_contours) < 2:
        print("✗ Need at least 2 boundaries")
        return
    
    # Get outer and inner boundaries
    outer = large_contours[0]
    inner = large_contours[1]
    
    # Create visualization
    result = frame.copy()
    
    # Draw boundaries
    cv2.drawContours(result, [outer], 0, (0, 255, 255), 3)  # Yellow = outer
    cv2.drawContours(result, [inner], 0, (255, 0, 0), 3)    # Blue = inner
    
    # Create masks
    outer_mask = np.zeros((height, width), dtype=np.uint8)
    inner_mask = np.zeros((height, width), dtype=np.uint8)
    cv2.drawContours(outer_mask, [outer], 0, 255, -1)
    cv2.drawContours(inner_mask, [inner], 0, 255, -1)
    
    # Road area (between boundaries)
    road_mask = cv2.bitwise_xor(outer_mask, inner_mask)
    
    # Calculate centerline (simple row-by-row)
    centerline_points = []
    for y in range(height):
        row = road_mask[y, :]
        if np.any(row):
            indices = np.where(row > 0)[0]
            if len(indices) > 0:
                center_x = (indices[0] + indices[-1]) // 2
                centerline_points.append((center_x, y))
    
    # Draw centerline
    if len(centerline_points) > 1:
        pts = np.array(centerline_points, dtype=np.int32)
        cv2.polylines(result, [pts], False, (0, 255, 0), 4)  # Green = center
        print(f"\n✓ Centerline: {len(centerline_points)} points")
    
    # Add labels
    cv2.putText(result, "OUTER (yellow)", (20, 40),
                cv2.FONT_HERSHEY_SIMPLEX, 1.0, (0, 255, 255), 2)
    cv2.putText(result, "INNER (blue)", (20, 80),
                cv2.FONT_HERSHEY_SIMPLEX, 1.0, (255, 0, 0), 2)
    cv2.putText(result, "CENTER (green)", (20, 120),
                cv2.FONT_HERSHEY_SIMPLEX, 1.0, (0, 255, 0), 2)
    
    # Save results
    cv2.imwrite("Path-image/simple_boundaries.png", result)
    cv2.imwrite("Path-image/simple_binary.png", binary)
    cv2.imwrite("Path-image/simple_road_mask.png", road_mask)
    
    print(f"\n{'='*70}")
    print(f"SAVED")
    print(f"{'='*70}")
    print(f"✓ Path-image/simple_boundaries.png")
    print(f"✓ Path-image/simple_binary.png")
    print(f"✓ Path-image/simple_road_mask.png")
    print(f"{'='*70}\n")

if __name__ == "__main__":
    detect_road_boundaries_simple("Simulation/image-1.png")
