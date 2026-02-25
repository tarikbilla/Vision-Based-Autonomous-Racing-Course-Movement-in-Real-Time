#!/usr/bin/env python3
"""
Test the updated boundary detection on your track image.
"""

import cv2
import numpy as np
from rc_autonomy.boundary import BoundaryDetector
from rc_autonomy.config import load_config

# Load config
config = load_config(None)

# Create detector
detector = BoundaryDetector(
    black_threshold=config.guidance.black_threshold,
    ray_angles_deg=config.guidance.ray_angles_deg,
    ray_max_length=config.guidance.ray_max_length,
    evasive_distance=config.guidance.evasive_distance,
    default_speed=config.guidance.default_speed,
    steering_limit=config.control.steering_limit,
    light_on=config.control.light_on,
)

# Load your track image
img = cv2.imread("Path-image/image-1.png")

print(f"\n{'='*70}")
print(f"TESTING CIRCULAR TRACK DETECTION ON YOUR IMAGE")
print(f"{'='*70}")

# Detect edges
left, center, right, road_mask = detector.detect_road_edges(img)

if left is None:
    print("✗ FAILED: No edges detected")
else:
    print(f"✓ SUCCESS: Edges detected!")
    print(f"  Left:   {left}")
    print(f"  Center: {center}")
    print(f"  Right:  {right}")
    print(f"  Width:  {right - left} pixels")
    
    # Draw visualization
    result = img.copy()
    h, w = img.shape[:2]
    
    # Draw edges
    cv2.line(result, (left, 0), (left, h), (255, 0, 0), 3)      # Blue left
    cv2.line(result, (center, 0), (center, h), (0, 255, 255), 3)  # Cyan center
    cv2.line(result, (right, 0), (right, h), (0, 0, 255), 3)    # Red right
    
    # Draw filled region between edges
    overlay = result.copy()
    cv2.rectangle(overlay, (left, 0), (right, h), (0, 255, 0), -1)
    cv2.addWeighted(overlay, 0.1, result, 0.9, 0, result)
    
    cv2.imwrite("Path-image/test_circular_detection.png", result)
    print(f"✓ Saved: Path-image/test_circular_detection.png")
    
    # Also save the road mask
    road_mask_colored = cv2.applyColorMap(road_mask, cv2.COLORMAP_JET)
    cv2.imwrite("Path-image/test_road_mask.png", road_mask_colored)
    print(f"✓ Saved: Path-image/test_road_mask.png")

print(f"\n{'='*70}")
