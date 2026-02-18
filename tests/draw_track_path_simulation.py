#!/usr/bin/env python3
"""
Draw track path boundaries based on white/red borders
Following the reference path shown in Simulation/path-1.jpg
"""

import cv2
import numpy as np

def draw_track_path(image_path, reference_path=None):
    """
    Detect track boundaries and draw path lines similar to reference image
    
    Args:
        image_path: Path to the track image to analyze
        reference_path: Optional path to reference image showing desired output
    """
    print(f"{'='*70}")
    print(f"TRACK PATH DETECTION")
    print(f"{'='*70}")
    
    # Load the track image
    image = cv2.imread(image_path)
    if image is None:
        print(f"✗ Could not load image: {image_path}")
        return
    
    height, width = image.shape[:2]
    print(f"Image: {width}×{height}")
    
    # Also load reference if provided
    if reference_path:
        reference = cv2.imread(reference_path)
        if reference is not None:
            print(f"Reference loaded: {reference_path}")
            cv2.imwrite("Path-image/reference_path.png", reference)
    
    # ---- Convert to grayscale ----
    gray = cv2.cvtColor(image, cv2.COLOR_BGR2GRAY)
    
    # ---- Apply Gaussian blur ----
    blurred = cv2.GaussianBlur(gray, (5, 5), 0)
    
    # ---- Adaptive threshold to detect white/red boundaries ----
    binary = cv2.adaptiveThreshold(
        blurred, 
        255,
        cv2.ADAPTIVE_THRESH_GAUSSIAN_C,
        cv2.THRESH_BINARY_INV,
        11,
        2
    )
    
    # ---- Apply VERY gentle morphology - preserve boundary separation ----
    kernel_tiny = np.ones((2, 2), np.uint8)
    
    # Remove isolated pixels
    binary = cv2.morphologyEx(binary, cv2.MORPH_OPEN, kernel_tiny, iterations=1)
    
    # Close very small gaps only
    binary = cv2.morphologyEx(binary, cv2.MORPH_CLOSE, kernel_tiny, iterations=1)
    
    print(f"✓ Adaptive threshold applied")
    
    # ---- Find contours ----
    contours, _ = cv2.findContours(binary, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
    print(f"✓ Found {len(contours)} total contours")
    
    # ---- Filter and sort contours by area ----
    contours_with_area = [(cv2.contourArea(c), c) for c in contours]
    contours_with_area.sort(reverse=True, key=lambda x: x[0])
    
    print(f"\nTop 10 contour areas:")
    for i in range(min(10, len(contours_with_area))):
        print(f"  {i+1}. {contours_with_area[i][0]:,.0f} px²")
    
    # Keep top contours that are likely track boundaries (filter tiny noise)
    boundary_contours = [item for item in contours_with_area[:15] if item[0] >= 500]
    print(f"\n✓ Selected {len(boundary_contours)} boundary contours")
    
    # ---- Identify inner and outer boundaries ----
    # Typically: largest = outer boundary, second largest = inner boundary
    if len(boundary_contours) >= 2:
        outer_boundary = boundary_contours[0][1]
        inner_boundary = boundary_contours[1][1]
        
        print(f"\nBoundary identification:")
        print(f"  Outer boundary: {boundary_contours[0][0]:,.0f} px²")
        print(f"  Inner boundary: {boundary_contours[1][0]:,.0f} px²")
        
        # ---- Draw boundaries on original image ----
        result = image.copy()
        
        # Draw all detected boundaries in green (thin lines)
        for area, contour in boundary_contours:
            cv2.drawContours(result, [contour], 0, (0, 255, 0), 2)
        
        # ---- Calculate path centerline (between inner and outer) ----
        # Create filled masks for inner and outer boundaries
        outer_mask = np.zeros((height, width), dtype=np.uint8)
        inner_mask = np.zeros((height, width), dtype=np.uint8)
        cv2.drawContours(outer_mask, [outer_boundary], 0, 255, -1)
        cv2.drawContours(inner_mask, [inner_boundary], 0, 255, -1)
        
        # Road area is between outer and inner boundaries
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
        
        # Draw the centerline path in red (thick line like reference)
        if len(centerline_points) > 1:
            centerline_points = np.array(centerline_points, dtype=np.int32)
            cv2.polylines(result, [centerline_points], False, (0, 0, 255), 4)
            print(f"✓ Drew centerline path with {len(centerline_points)} points")
        
        # Add labels similar to reference style
        cv2.putText(result, "Track Boundaries (green)", (20, 40),
                    cv2.FONT_HERSHEY_SIMPLEX, 1.0, (0, 255, 0), 2)
        cv2.putText(result, "Optimal Path (red)", (20, 80),
                    cv2.FONT_HERSHEY_SIMPLEX, 1.0, (0, 0, 255), 2)
        
        # ---- Save results ----
        cv2.imwrite("Path-image/simulation_track_path.png", result)
        cv2.imwrite("Path-image/simulation_binary.png", binary)
        cv2.imwrite("Path-image/simulation_road_mask.png", road_mask)
        
        print(f"\n{'='*70}")
        print(f"SAVED")
        print(f"{'='*70}")
        print(f"✓ Path-image/simulation_track_path.png (with boundaries and centerline)")
        print(f"✓ Path-image/simulation_binary.png (binary threshold)")
        print(f"✓ Path-image/simulation_road_mask.png (road area)")
        print(f"{'='*70}\n")
        
    else:
        print(f"✗ Need at least 2 boundaries (inner + outer) to calculate path")

if __name__ == "__main__":
    draw_track_path(
        "Simulation/image-1.png",
        reference_path="Simulation/path-1.jpg"
    )
