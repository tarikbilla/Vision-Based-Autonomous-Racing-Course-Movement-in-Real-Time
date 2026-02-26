#!/usr/bin/env python3
"""
Draw lines on both inner and outer boundaries of the circular track.
"""

import cv2
import numpy as np

def draw_both_boundaries(image_path):
    """Find and draw both inner and outer track boundaries."""
    
    img = cv2.imread(image_path)
    if img is None:
        print(f"ERROR: Could not load {image_path}")
        return
    
    h, w = img.shape[:2]
    print(f"\n{'='*70}")
    print(f"DRAWING BOTH TRACK BOUNDARIES")
    print(f"{'='*70}")
    print(f"Image: {w}×{h}")
    
    # Convert to HSV
    hsv = cv2.cvtColor(img, cv2.COLOR_BGR2HSV)
    
    # Detect white markers
    white_mask1 = cv2.inRange(hsv, np.array([0, 0, 180]), np.array([180, 30, 255]))
    white_mask2 = cv2.inRange(hsv, np.array([0, 0, 120]), np.array([180, 60, 200]))
    white_mask3 = cv2.inRange(hsv, np.array([0, 0, 100]), np.array([180, 80, 180]))
    white_mask = cv2.bitwise_or(cv2.bitwise_or(white_mask1, white_mask2), white_mask3)
    
    # Detect red markers
    red_mask1 = cv2.inRange(hsv, np.array([0, 40, 30]), np.array([15, 255, 255]))
    red_mask2 = cv2.inRange(hsv, np.array([160, 40, 30]), np.array([180, 255, 255]))
    red_mask = cv2.bitwise_or(red_mask1, red_mask2)
    
    # Combine all markers
    edge_mask = cv2.bitwise_or(white_mask, red_mask)
    
    # Morphology
    kernel_small = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (3, 3))
    kernel_medium = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (5, 5))
    
    edge_mask = cv2.morphologyEx(edge_mask, cv2.MORPH_OPEN, kernel_small, iterations=1)
    edge_mask = cv2.morphologyEx(edge_mask, cv2.MORPH_CLOSE, kernel_medium, iterations=2)
    edge_mask = cv2.dilate(edge_mask, kernel_medium, iterations=3)
    
    # Find contours
    contours, _ = cv2.findContours(edge_mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
    
    print(f"\nFound {len(contours)} contours")
    
    # Sort by area
    contours_sorted = sorted(contours, key=cv2.contourArea, reverse=True)
    
    # Display contour info
    print(f"\n{'='*70}")
    print(f"CONTOUR ANALYSIS")
    print(f"{'='*70}")
    
    for idx, contour in enumerate(contours_sorted[:5]):
        area = cv2.contourArea(contour)
        x, y, cw, ch = cv2.boundingRect(contour)
        
        left = int(contour[:, :, 0].min())
        right = int(contour[:, :, 0].max())
        top = int(contour[:, :, 1].min())
        bottom = int(contour[:, :, 1].max())
        
        print(f"\nContour {idx}:")
        print(f"  Area: {area:,.0f} px² ({100*area/(w*h):.1f}% of image)")
        print(f"  Bounds: x={left}-{right}, y={top}-{bottom}")
        print(f"  Width: {right-left}, Height: {bottom-top}")
    
    # Create visualization - draw directly on the red/white markers
    result = img.copy()
    
    # Also create a version showing just the contours on black background
    contour_only = np.zeros_like(img)
    
    # Draw ALL detected markers/contours with thick colored lines
    print(f"\n{'='*70}")
    print(f"DRAWING BOUNDARY LINES")
    print(f"{'='*70}")
    
    for idx, contour in enumerate(contours_sorted):
        area = cv2.contourArea(contour)
        
        # Skip very small noise contours
        if area < 100:
            continue
        
        # Choose color based on size
        if idx == 0:  # Largest = outer boundary
            color = (0, 255, 0)  # Green
            label = "OUTER"
            thickness = 4
        elif idx == 1:  # Second = inner boundary
            color = (255, 0, 0)  # Blue
            label = "INNER"
            thickness = 4
        else:  # Smaller contours
            color = (0, 255, 255)  # Cyan
            label = f"Small-{idx}"
            thickness = 2
        
        # Draw contour outline directly on the markers
        cv2.drawContours(result, [contour], 0, color, thickness)
        cv2.drawContours(contour_only, [contour], 0, color, thickness)
        
        print(f"  {label}: Area={area:,.0f} px², Color={color}")
    
    if len(contours_sorted) >= 2:
        # Get outer and inner boundaries
        outer = contours_sorted[0]
        inner = contours_sorted[1]
        
        # Get bounds
        outer_left = int(outer[:, :, 0].min())
        outer_right = int(outer[:, :, 0].max())
        inner_left = int(inner[:, :, 0].min())
        inner_right = int(inner[:, :, 0].max())
        
        # Calculate road centerline
        road_center = (inner_right + outer_right) // 2
        
        # Draw centerline
        cv2.line(result, (road_center, 0), (road_center, h), (0, 0, 255), 3)
        cv2.line(contour_only, (road_center, 0), (road_center, h), (0, 0, 255), 3)
        
        print(f"\nBoundary positions:")
        print(f"  Outer boundary: x={outer_left} to x={outer_right}")
        print(f"  Inner boundary: x={inner_left} to x={inner_right}")
        print(f"  Road centerline: x={road_center}")
        
        # Add labels
        cv2.putText(result, "OUTER BOUNDARY", (20, 40), 
                    cv2.FONT_HERSHEY_SIMPLEX, 1.2, (0, 255, 0), 3)
        cv2.putText(result, "INNER BOUNDARY", (20, 90), 
                    cv2.FONT_HERSHEY_SIMPLEX, 1.2, (255, 0, 0), 3)
        cv2.putText(result, f"CENTERLINE (x={road_center})", (20, 140), 
                    cv2.FONT_HERSHEY_SIMPLEX, 1.2, (0, 0, 255), 3)
        
    elif len(contours_sorted) == 1:
        print(f"\n⚠ Only 1 contour found - drawing single boundary")
        contour = contours_sorted[0]
        
        cv2.drawContours(result, [contour], 0, (0, 255, 0), 3)
        
        left = int(contour[:, :, 0].min())
        right = int(contour[:, :, 0].max())
        center = (left + right) // 2
        
        cv2.line(result, (left, 0), (left, h), (255, 0, 0), 2)
        cv2.line(result, (right, 0), (right, h), (0, 0, 255), 2)
        cv2.line(result, (center, 0), (center, h), (0, 255, 255), 2)
        
        print(f"  Left: x={left}")
        print(f"  Center: x={center}")
        print(f"  Right: x={right}")
    else:
        print(f"\n✗ No contours detected!")
    
    # Save results
    cv2.imwrite("Path-image/boundary_lines.png", result)
    cv2.imwrite("Path-image/boundary_contours_only.png", contour_only)
    cv2.imwrite("Path-image/boundary_mask.png", edge_mask)
    
    print(f"\n{'='*70}")
    print(f"SAVED")
    print(f"{'='*70}")
    print(f"✓ Path-image/boundary_lines.png - Lines drawn on boundaries")
    print(f"✓ Path-image/boundary_contours_only.png - Contours on black background")
    print(f"✓ Path-image/boundary_mask.png - Binary mask of detected markers")
    print(f"{'='*70}\n")

if __name__ == "__main__":
    draw_both_boundaries("Path-image/image-1.png")
