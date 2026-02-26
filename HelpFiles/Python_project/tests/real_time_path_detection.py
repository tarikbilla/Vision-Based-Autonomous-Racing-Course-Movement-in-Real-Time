#!/usr/bin/env python3
"""
Real-time track boundary detection and center path following
Detects both left and right road boundaries from camera feed
"""

import cv2
import numpy as np
import time

def detect_road_boundaries(frame):
    """
    Detect left and right road boundaries from camera frame
    
    Returns:
        left_boundary: Points defining left edge of road
        right_boundary: Points defining right edge of road
        center_path: Points defining center path between boundaries
        debug_image: Visualization with boundaries drawn
    """
    height, width = frame.shape[:2]
    
    # ---- Preprocess image ----
    gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
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
    
    # ---- Minimal morphology to remove noise ----
    kernel_tiny = np.ones((2, 2), np.uint8)
    binary = cv2.morphologyEx(binary, cv2.MORPH_OPEN, kernel_tiny, iterations=1)
    binary = cv2.morphologyEx(binary, cv2.MORPH_CLOSE, kernel_tiny, iterations=1)
    
    # ---- Find contours ----
    contours, _ = cv2.findContours(binary, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
    
    # ---- Filter to get largest contours (track boundaries) ----
    contours_with_area = [(cv2.contourArea(c), c) for c in contours]
    contours_with_area.sort(reverse=True, key=lambda x: x[0])
    
    # Keep top contours >= 500 px²
    boundary_contours = [item for item in contours_with_area[:15] if item[0] >= 500]
    
    # ---- Create visualization ----
    debug_image = frame.copy()
    
    left_boundary = []
    right_boundary = []
    center_path = []
    
    if len(boundary_contours) >= 2:
        # Get largest two contours (outer and inner boundaries)
        outer_contour = boundary_contours[0][1]
        inner_contour = boundary_contours[1][1]
        
        # ---- Create masks ----
        outer_mask = np.zeros((height, width), dtype=np.uint8)
        inner_mask = np.zeros((height, width), dtype=np.uint8)
        cv2.drawContours(outer_mask, [outer_contour], 0, 255, -1)
        cv2.drawContours(inner_mask, [inner_contour], 0, 255, -1)
        
        # Road area (between boundaries)
        road_mask = cv2.bitwise_xor(outer_mask, inner_mask)
        
        # ---- Detect left and right boundaries for each row ----
        for y in range(height):
            row_road = road_mask[y, :]
            if np.any(row_road):
                road_indices = np.where(row_road > 0)[0]
                if len(road_indices) > 0:
                    # Left boundary = leftmost edge
                    left_x = road_indices[0]
                    left_boundary.append((left_x, y))
                    
                    # Right boundary = rightmost edge
                    right_x = road_indices[-1]
                    right_boundary.append((right_x, y))
                    
                    # Center path = midpoint
                    center_x = (left_x + right_x) // 2
                    center_path.append((center_x, y))
        
        # ---- Draw boundaries on debug image ----
        if len(left_boundary) > 1:
            left_pts = np.array(left_boundary, dtype=np.int32)
            cv2.polylines(debug_image, [left_pts], False, (255, 0, 0), 3)  # Blue = left
        
        if len(right_boundary) > 1:
            right_pts = np.array(right_boundary, dtype=np.int32)
            cv2.polylines(debug_image, [right_pts], False, (0, 255, 255), 3)  # Yellow = right
        
        if len(center_path) > 1:
            center_pts = np.array(center_path, dtype=np.int32)
            cv2.polylines(debug_image, [center_pts], False, (0, 255, 0), 4)  # Green = center
        
        # Add labels
        cv2.putText(debug_image, "LEFT (blue)", (20, 40),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.8, (255, 0, 0), 2)
        cv2.putText(debug_image, "RIGHT (yellow)", (20, 75),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.8, (0, 255, 255), 2)
        cv2.putText(debug_image, "CENTER PATH (green)", (20, 110),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.8, (0, 255, 0), 2)
    
    return left_boundary, right_boundary, center_path, debug_image, binary


def calculate_steering_from_path(center_path, image_width, image_height):
    """
    Calculate steering angle based on center path deviation
    
    Args:
        center_path: List of (x, y) points defining center path
        image_width: Frame width
        image_height: Frame height
    
    Returns:
        steering_angle: -100 to 100 (negative = left, positive = right)
        error_pixels: Distance from center
    """
    if len(center_path) < 10:
        return 0, 0
    
    # Look at center path points in bottom third of image (closer to car)
    bottom_third_y = int(image_height * 2/3)
    relevant_points = [p for p in center_path if p[1] >= bottom_third_y]
    
    if len(relevant_points) == 0:
        return 0, 0
    
    # Average x position of center path in bottom third
    avg_center_x = np.mean([p[0] for p in relevant_points])
    
    # Image center
    image_center_x = image_width / 2
    
    # Error in pixels
    error_pixels = avg_center_x - image_center_x
    
    # Convert to steering angle (-100 to 100)
    # Normalize by half image width
    max_deviation = image_width / 2
    steering_angle = int((error_pixels / max_deviation) * 100)
    steering_angle = max(-100, min(100, steering_angle))
    
    return steering_angle, int(error_pixels)


def test_with_image(image_path):
    """Test boundary detection with a static image"""
    print(f"{'='*70}")
    print(f"TESTING BOUNDARY DETECTION")
    print(f"{'='*70}")
    
    frame = cv2.imread(image_path)
    if frame is None:
        print(f"✗ Could not load: {image_path}")
        return
    
    height, width = frame.shape[:2]
    print(f"Image: {width}×{height}")
    
    # Detect boundaries
    left_boundary, right_boundary, center_path, debug_image, binary = detect_road_boundaries(frame)
    
    print(f"\n✓ Left boundary points: {len(left_boundary)}")
    print(f"✓ Right boundary points: {len(right_boundary)}")
    print(f"✓ Center path points: {len(center_path)}")
    
    # Calculate steering
    steering, error = calculate_steering_from_path(center_path, width, height)
    print(f"\nSteering calculation:")
    print(f"  Error: {error} pixels from center")
    print(f"  Steering angle: {steering} (-100=full left, 0=straight, 100=full right)")
    
    # Add steering info to debug image
    cv2.putText(debug_image, f"Error: {error}px", (20, height - 80),
                cv2.FONT_HERSHEY_SIMPLEX, 1.0, (255, 255, 255), 2)
    cv2.putText(debug_image, f"Steering: {steering}", (20, height - 40),
                cv2.FONT_HERSHEY_SIMPLEX, 1.0, (255, 255, 255), 2)
    
    # Draw center reference line
    center_x = width // 2
    cv2.line(debug_image, (center_x, 0), (center_x, height), (255, 255, 255), 1)
    
    # Save results
    cv2.imwrite("Path-image/path_detection_left_right_center.png", debug_image)
    cv2.imwrite("Path-image/path_detection_binary.png", binary)
    
    print(f"\n{'='*70}")
    print(f"SAVED")
    print(f"{'='*70}")
    print(f"✓ Path-image/path_detection_left_right_center.png")
    print(f"✓ Path-image/path_detection_binary.png")
    print(f"{'='*70}\n")


def test_with_camera(camera_index=0):
    """Test with live camera feed"""
    print(f"{'='*70}")
    print(f"LIVE CAMERA BOUNDARY DETECTION")
    print(f"{'='*70}")
    print("Opening camera... Press 'q' to quit")
    
    cap = cv2.VideoCapture(camera_index)
    if not cap.isOpened():
        print(f"✗ Could not open camera {camera_index}")
        return
    
    frame_count = 0
    
    while True:
        ret, frame = cap.read()
        if not ret:
            print("✗ Failed to grab frame")
            break
        
        frame_count += 1
        height, width = frame.shape[:2]
        
        # Detect boundaries
        left_boundary, right_boundary, center_path, debug_image, binary = detect_road_boundaries(frame)
        
        # Calculate steering
        steering, error = calculate_steering_from_path(center_path, width, height)
        
        # Add info overlay
        cv2.putText(debug_image, f"Frame: {frame_count}", (width - 200, 40),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.7, (255, 255, 255), 2)
        cv2.putText(debug_image, f"Error: {error}px", (width - 200, 75),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.7, (255, 255, 255), 2)
        cv2.putText(debug_image, f"Steer: {steering}", (width - 200, 110),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.7, (255, 255, 255), 2)
        
        # Draw center reference
        center_x = width // 2
        cv2.line(debug_image, (center_x, 0), (center_x, height), (255, 255, 255), 1)
        
        # Show both original and processed
        combined = np.hstack([frame, debug_image])
        cv2.imshow('Track Detection - Original | Boundaries', combined)
        cv2.imshow('Binary Threshold', binary)
        
        # Press 'q' to quit, 's' to save frame
        key = cv2.waitKey(1) & 0xFF
        if key == ord('q'):
            break
        elif key == ord('s'):
            cv2.imwrite(f"Path-image/camera_frame_{frame_count}.png", debug_image)
            print(f"Saved frame {frame_count}")
    
    cap.release()
    cv2.destroyAllWindows()
    print(f"\n✓ Camera closed after {frame_count} frames")


if __name__ == "__main__":
    import sys
    
    if len(sys.argv) > 1 and sys.argv[1] == "--camera":
        # Live camera mode
        test_with_camera()
    else:
        # Test with static image first
        test_with_image("Simulation/image-1.png")
        print("\nTo test with live camera, run:")
        print("  python tests/real_time_path_detection.py --camera")
