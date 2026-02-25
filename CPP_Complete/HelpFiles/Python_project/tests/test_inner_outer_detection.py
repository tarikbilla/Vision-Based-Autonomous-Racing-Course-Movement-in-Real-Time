#!/usr/bin/env python3
"""
Test the updated detection on the actual circular track structure.
"""

import cv2
import numpy as np
from rc_autonomy.boundary import BoundaryDetector
from rc_autonomy.config import load_config

config = load_config(None)

detector = BoundaryDetector(
    black_threshold=config.guidance.black_threshold,
    ray_angles_deg=config.guidance.ray_angles_deg,
    ray_max_length=config.guidance.ray_max_length,
    evasive_distance=config.guidance.evasive_distance,
    default_speed=config.guidance.default_speed,
    steering_limit=config.control.steering_limit,
    light_on=config.control.light_on,
)

img = cv2.imread("Path-image/image-1.png")

print(f"\n{'='*70}")
print(f"TESTING INNER+OUTER BOUNDARY DETECTION")
print(f"{'='*70}")

left, center, right, road_mask = detector.detect_road_edges(img)

if left is None:
    print("✗ FAILED: No edges detected")
else:
    print(f"✓ SUCCESS: Circular track detected!")
    print(f"  Outer Left:  {left}")
    print(f"  Road Center: {center}")
    print(f"  Outer Right: {right}")
    print(f"  Track width: {right - left} pixels")
    
    # Draw visualization with BOTH boundaries visible
    result = img.copy()
    h, w = img.shape[:2]
    
    # Draw track boundaries
    cv2.line(result, (left, 0), (left, h), (255, 0, 0), 3)      # Blue: outer left
    cv2.line(result, (center, 0), (center, h), (0, 255, 255), 3)  # Cyan: center/road
    cv2.line(result, (right, 0), (right, h), (0, 0, 255), 3)    # Red: outer right
    
    # Overlay road area
    overlay = result.copy()
    cv2.rectangle(overlay, (left, 0), (right, h), (0, 255, 0), -1)
    cv2.addWeighted(overlay, 0.15, result, 0.85, 0, result)
    
    cv2.imwrite("Path-image/final_detection.png", result)
    print(f"\n✓ Saved: Path-image/final_detection.png")
    
    # Show contours for reference
    import cv2 as cv2_local
    hsv = cv2_local.cvtColor(img, cv2_local.COLOR_BGR2HSV)
    
    white_mask1 = cv2_local.inRange(hsv, np.array([0, 0, 180]), np.array([180, 30, 255]))
    white_mask2 = cv2_local.inRange(hsv, np.array([0, 0, 120]), np.array([180, 60, 200]))
    white_mask3 = cv2_local.inRange(hsv, np.array([0, 0, 100]), np.array([180, 80, 180]))
    white_mask = cv2_local.bitwise_or(cv2_local.bitwise_or(white_mask1, white_mask2), white_mask3)
    
    red_mask1 = cv2_local.inRange(hsv, np.array([0, 40, 30]), np.array([15, 255, 255]))
    red_mask2 = cv2_local.inRange(hsv, np.array([160, 40, 30]), np.array([180, 255, 255]))
    red_mask = cv2_local.bitwise_or(red_mask1, red_mask2)
    
    edge_mask = cv2_local.bitwise_or(white_mask, red_mask)
    
    kernel_small = cv2_local.getStructuringElement(cv2_local.MORPH_ELLIPSE, (3, 3))
    kernel_medium = cv2_local.getStructuringElement(cv2_local.MORPH_ELLIPSE, (5, 5))
    
    edge_mask = cv2_local.morphologyEx(edge_mask, cv2_local.MORPH_OPEN, kernel_small, iterations=1)
    edge_mask = cv2_local.morphologyEx(edge_mask, cv2_local.MORPH_CLOSE, kernel_medium, iterations=2)
    edge_mask = cv2_local.dilate(edge_mask, kernel_medium, iterations=3)
    
    contours, _ = cv2_local.findContours(edge_mask, cv2_local.RETR_EXTERNAL, cv2_local.CHAIN_APPROX_SIMPLE)
    
    contour_img = img.copy()
    contours_sorted = sorted(contours, key=cv2_local.contourArea, reverse=True)
    
    colors = [(255, 0, 0), (0, 255, 0), (0, 0, 255)]
    for idx, contour in enumerate(contours_sorted[:3]):
        color = colors[idx % len(colors)]
        cv2_local.drawContours(contour_img, [contour], 0, color, 3)
        area = cv2_local.contourArea(contour)
        print(f"  Contour {idx}: Area={area:,} px² Color=RGB{color}")
    
    cv2_local.imwrite("Path-image/contours_drawn.png", contour_img)
    print(f"✓ Saved: Path-image/contours_drawn.png")

print(f"\n{'='*70}\n")
