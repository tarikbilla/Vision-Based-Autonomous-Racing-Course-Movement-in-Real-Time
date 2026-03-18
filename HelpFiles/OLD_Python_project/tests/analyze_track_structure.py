#!/usr/bin/env python3
"""
Analyze the circular track structure to find inner and outer boundaries.
"""

import cv2
import numpy as np

def analyze_track_structure(image_path):
    """Analyze the track to understand inner/outer boundary structure."""
    img = cv2.imread(image_path)
    h, w = img.shape[:2]
    
    print(f"\n{'='*70}")
    print(f"CIRCULAR TRACK STRUCTURE ANALYSIS")
    print(f"{'='*70}")
    print(f"Image: {image_path}")
    print(f"Dimensions: {w}×{h}")
    
    # Convert to HSV
    hsv = cv2.cvtColor(img, cv2.COLOR_BGR2HSV)
    
    # Detect white+red markers
    white_mask1 = cv2.inRange(hsv, np.array([0, 0, 180]), np.array([180, 30, 255]))
    white_mask2 = cv2.inRange(hsv, np.array([0, 0, 120]), np.array([180, 60, 200]))
    white_mask3 = cv2.inRange(hsv, np.array([0, 0, 100]), np.array([180, 80, 180]))
    white_mask = cv2.bitwise_or(cv2.bitwise_or(white_mask1, white_mask2), white_mask3)
    
    red_mask1 = cv2.inRange(hsv, np.array([0, 40, 30]), np.array([15, 255, 255]))
    red_mask2 = cv2.inRange(hsv, np.array([160, 40, 30]), np.array([180, 255, 255]))
    red_mask = cv2.bitwise_or(red_mask1, red_mask2)
    
    edge_mask = cv2.bitwise_or(white_mask, red_mask)
    
    # Morphology
    kernel_small = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (3, 3))
    kernel_medium = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (5, 5))
    
    edge_mask = cv2.morphologyEx(edge_mask, cv2.MORPH_OPEN, kernel_small, iterations=1)
    edge_mask = cv2.morphologyEx(edge_mask, cv2.MORPH_CLOSE, kernel_medium, iterations=2)
    edge_mask = cv2.dilate(edge_mask, kernel_medium, iterations=3)
    
    # Find contours
    contours, _ = cv2.findContours(edge_mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
    
    print(f"\nContours found: {len(contours)}")
    print(f"{'-'*70}")
    
    # Analyze each contour
    for idx, contour in enumerate(contours):
        area = cv2.contourArea(contour)
        x, y, cw, ch = cv2.boundingRect(contour)
        
        # Find extreme points
        leftmost = int(contour[:, :, 0].min())
        rightmost = int(contour[:, :, 0].max())
        topmost = int(contour[:, :, 1].min())
        bottommost = int(contour[:, :, 1].max())
        
        print(f"Contour {idx}: Area={area:,} px²")
        print(f"  BBox: x={x}, y={y}, w={cw}, h={ch}")
        print(f"  Extremes: L={leftmost}, R={rightmost}, T={topmost}, B={bottommost}")
        print(f"  Width span: {rightmost - leftmost} px")
        print()
    
    # Sort contours by area
    contours_sorted = sorted(contours, key=cv2.contourArea, reverse=True)
    
    print(f"{'-'*70}")
    print(f"INTERPRETATION:")
    print(f"{'-'*70}")
    
    if len(contours_sorted) >= 2:
        print(f"✓ Two boundaries detected!")
        print(f"  Contour 0 (outer): Area = {cv2.contourArea(contours_sorted[0]):,} px²")
        print(f"  Contour 1 (inner): Area = {cv2.contourArea(contours_sorted[1]):,} px²")
        
        # Get bounds for both
        outer = contours_sorted[0]
        inner = contours_sorted[1]
        
        outer_left = int(outer[:, :, 0].min())
        outer_right = int(outer[:, :, 0].max())
        
        inner_left = int(inner[:, :, 0].min())
        inner_right = int(inner[:, :, 0].max())
        
        print(f"\n  Outer boundary: L={outer_left}, R={outer_right}, width={outer_right-outer_left}")
        print(f"  Inner boundary: L={inner_left}, R={inner_right}, width={inner_right-inner_left}")
        
        print(f"\n  Road area: between x={inner_left} and x={outer_left} (left)")
        print(f"             and between x={inner_right} and x={outer_right} (right)")
        
    elif len(contours_sorted) == 1:
        print(f"⚠ Only one boundary found")
        print(f"  This is likely the COMPLETE track ring (single contour)")
        print(f"  Area: {cv2.contourArea(contours_sorted[0]):,} px²")
    else:
        print(f"✗ No contours found!")
    
    # Visualization
    print(f"\n{'='*70}")
    print(f"CREATING VISUALIZATIONS...")
    print(f"{'='*70}")
    
    # Draw all contours
    contour_img = img.copy()
    colors = [(255, 0, 0), (0, 255, 0), (0, 0, 255)]
    for idx, contour in enumerate(contours_sorted):
        color = colors[idx % len(colors)]
        cv2.drawContours(contour_img, [contour], 0, color, 3)
    
    cv2.imwrite("Path-image/contour_analysis.png", contour_img)
    print(f"✓ Saved: Path-image/contour_analysis.png")
    
    # Highlight detected markers
    markers = cv2.cvtColor(edge_mask, cv2.COLOR_GRAY2BGR)
    cv2.imwrite("Path-image/markers_only.png", markers)
    print(f"✓ Saved: Path-image/markers_only.png")
    
    return contours_sorted

if __name__ == "__main__":
    analyze_track_structure("Path-image/image-1.png")
