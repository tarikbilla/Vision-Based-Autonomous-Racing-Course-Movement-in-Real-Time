#!/usr/bin/env python3
"""
Detect circular road boundaries and centerline for overhead camera view
The camera captures the entire circular track from above
"""

import cv2
import numpy as np

def detect_circular_road_boundaries(frame):
    """
    Detect inner and outer boundaries of circular track using white/red markers
    
    Args:
        frame: Camera image showing full circular track
    
    Returns:
        inner_boundary_contour: Inner white/red boundary
        outer_boundary_contour: Outer white/red boundary
        centerline_points: Points along circular centerline
        debug_image: Visualization with boundaries
    """
    height, width = frame.shape[:2]
    
    # ---- Detect WHITE boundaries ----
    # Convert to HSV for better color detection
    hsv = cv2.cvtColor(frame, cv2.COLOR_BGR2HSV)
    
    # White color range (more permissive)
    lower_white = np.array([0, 0, 150])
    upper_white = np.array([180, 60, 255])
    white_mask = cv2.inRange(hsv, lower_white, upper_white)
    
    # Red color range (red has two ranges in HSV) - more permissive
    lower_red1 = np.array([0, 70, 50])
    upper_red1 = np.array([15, 255, 255])
    lower_red2 = np.array([165, 70, 50])
    upper_red2 = np.array([180, 255, 255])
    red_mask1 = cv2.inRange(hsv, lower_red1, upper_red1)
    red_mask2 = cv2.inRange(hsv, lower_red2, upper_red2)
    red_mask = cv2.bitwise_or(red_mask1, red_mask2)
    
    # Combine white and red to get all boundary markers
    boundary_mask = cv2.bitwise_or(white_mask, red_mask)
    
    # Clean up the mask - gentle morphology to connect segments without merging inner/outer
    kernel_small = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (2, 2))
    kernel_medium = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (4, 4))
    
    # Remove tiny noise first
    boundary_mask = cv2.morphologyEx(boundary_mask, cv2.MORPH_OPEN, kernel_small, iterations=1)
    # Close small gaps
    boundary_mask = cv2.morphologyEx(boundary_mask, cv2.MORPH_CLOSE, kernel_medium, iterations=1)
    
    # ---- Find contours of white/red boundaries ----
    contours, _ = cv2.findContours(boundary_mask, cv2.RETR_LIST, cv2.CHAIN_APPROX_SIMPLE)
    
    # ---- Instead of filled contours, find EDGES of boundaries ----
    # Use Canny edge detection on the boundary mask
    edges = cv2.Canny(boundary_mask, 50, 150)
    
    # Dilate edges slightly to make them continuous
    kernel_line = cv2.getStructuringElement(cv2.MORPH_RECT, (2, 2))
    edges = cv2.dilate(edges, kernel_line, iterations=1)
    
    # Find edge contours
    edge_contours, _ = cv2.findContours(edges, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
    
    # ---- Sort by area and arc length (perimeter) ----
    # For circular tracks, we want the longest continuous boundaries
    contours_with_metrics = []
    for c in edge_contours:
        area = cv2.contourArea(c)
        perimeter = cv2.arcLength(c, True)
        if area >= 1000 and perimeter >= 100:  # Must be substantial
            contours_with_metrics.append((perimeter, area, c))  # Sort by perimeter first
    
    contours_with_metrics.sort(reverse=True, key=lambda x: x[0])  # Longest perimeter first
    
    # ---- Get top contours (should be inner and outer boundaries) ----
    boundary_contours = [(item[1], item[2]) for item in contours_with_metrics[:20]]  # (area, contour)
    
    print(f"\nTop boundary edge contours found:")
    for i in range(min(5, len(contours_with_metrics))):
        print(f"  {i+1}. Perimeter: {contours_with_metrics[i][0]:,.0f} px, Area: {contours_with_metrics[i][1]:,.0f} px²")
    
    inner_boundary_contour = None
    outer_boundary_contour = None
    centerline_points = []
    
    debug_image = frame.copy()
    
    if len(boundary_contours) >= 2:
        # For a circular track: need to identify which is inner vs outer
        # Outer boundary should be larger area
        outer_area, outer_boundary_contour = boundary_contours[0]
        inner_area, inner_boundary_contour = boundary_contours[1]
        
        print(f"✓ Outer boundary (white/red): {outer_area:,.0f} px²")
        print(f"✓ Inner boundary (white/red): {inner_area:,.0f} px²")
        
        # ---- Draw the actual boundary contours ----
        # Outer boundary in yellow
        cv2.drawContours(debug_image, [outer_boundary_contour], 0, (0, 255, 255), 3)
        
        # Inner boundary in blue
        cv2.drawContours(debug_image, [inner_boundary_contour], 0, (255, 0, 0), 3)
        
        # ---- Create masks for calculating centerline ----
        outer_mask = np.zeros((height, width), dtype=np.uint8)
        inner_mask = np.zeros((height, width), dtype=np.uint8)
        cv2.drawContours(outer_mask, [outer_boundary_contour], 0, 255, -1)
        cv2.drawContours(inner_mask, [inner_boundary_contour], 0, 255, -1)
        
        # Road area (ring between boundaries)
        road_mask = cv2.bitwise_xor(outer_mask, inner_mask)
        
        # ---- Calculate centerline using distance transform ----
        # Distance from outer boundary (going inward)
        dist_from_outer = cv2.distanceTransform(cv2.bitwise_not(outer_mask), cv2.DIST_L2, 5)
        # Distance from inner boundary (going outward)
        dist_from_inner = cv2.distanceTransform(cv2.bitwise_not(inner_mask), cv2.DIST_L2, 5)
        
        # On the road, find points equidistant from both boundaries
        # This creates a proper centerline
        centerline_mask = np.zeros((height, width), dtype=np.uint8)
        
        # For each pixel on the road, check if it's roughly equidistant from both boundaries
        for y in range(height):
            for x in range(width):
                if road_mask[y, x] > 0:  # Only on road
                    d_outer = dist_from_outer[y, x]
                    d_inner = dist_from_inner[y, x]
                    # If distances are similar (within 20% tolerance)
                    if d_outer > 0 and d_inner > 0:
                        ratio = min(d_outer, d_inner) / max(d_outer, d_inner)
                        if ratio > 0.7:  # Within 30% of each other
                            centerline_mask[y, x] = 255
        
        # Extract centerline points
        y_coords, x_coords = np.where(centerline_mask > 0)
        if len(x_coords) > 0:
            centerline_points = list(zip(x_coords, y_coords))
        
        # ---- Draw circular centerline (green) ----
        if len(centerline_points) > 1:
            centerline_pts = np.array(centerline_points, dtype=np.int32)
            cv2.polylines(debug_image, [centerline_pts], False, (0, 255, 0), 4)
            print(f"✓ Circular centerline: {len(centerline_points)} points")
        
        # ---- Add labels ----
        cv2.putText(debug_image, "LEFT road edge (blue)", (20, 40),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.9, (255, 0, 0), 2)
        cv2.putText(debug_image, "OUTER white/red boundary (yellow)", (20, 40),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.8, (0, 255, 255), 2)
        cv2.putText(debug_image, "INNER white/red boundary (blue)", (20, 75),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.8, (255, 0, 0), 2)
        cv2.putText(debug_image, "CENTERLINE path (green)", (20, 110),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.8, (0, 255, 0), 2)
    
    # Return centerline_mask for debugging
    centerline_debug_mask = centerline_mask if len(boundary_contours) >= 2 else None
    return inner_boundary_contour, outer_boundary_contour, centerline_points, debug_image, boundary_mask, road_mask if len(boundary_contours) >= 2 else None, centerline_debug_mask


def calculate_car_steering_circular(car_position, centerline_points, image_width):
    """
        # ---- Add labels ----
        cv2.putText(debug_image, "OUTER white/red boundary (yellow)", (20, 40),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.8, (0, 255, 255), 2)
        cv2.putText(debug_image, "INNER white/red boundary (blue)", (20, 75),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.8, (255, 0, 0), 2)
        cv2.putText(debug_image, "CENTERLINE path (green)", (20, 110),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.8, (0, 255, 0), 2)
    
    return inner_boundary_contour, outer_boundary_contour, centerline_points, debug_image, boundary_mask, road_mask if len(boundary_contours) >= 2 else None
        distance_from_center: pixels away from centerline
    """
    if len(centerline_points) == 0 or car_position is None:
        return 0, 0
    
    car_x, car_y = car_position
    
    # Find closest centerline point to car
    centerline_array = np.array(centerline_points)
    distances = np.sqrt((centerline_array[:, 0] - car_x)**2 + 
                       (centerline_array[:, 1] - car_y)**2)
    closest_idx = np.argmin(distances)
    closest_point = centerline_points[closest_idx]
    
    # Distance from centerline
    distance_from_center = int(distances[closest_idx])
    
    # Error in x direction
    error_x = car_x - closest_point[0]
    
    # Steering: negative = move left, positive = move right
    max_deviation = image_width / 4
    steering = int((error_x / max_deviation) * 100)
    steering = max(-100, min(100, steering))
    
    return -steering, distance_from_center  # Invert to correct direction


def test_circular_detection(image_path, reference_path=None):
    """Test with static image"""
    print(f"{'='*70}")
    print(f"CIRCULAR TRACK DETECTION")
    print(f"{'='*70}")
    
    frame = cv2.imread(image_path)
    if frame is None:
        print(f"✗ Could not load: {image_path}")
        return
    
    height, width = frame.shape[:2]
    print(f"Image: {width}×{height}")
    
    # Load reference if provided
    if reference_path:
        reference = cv2.imread(reference_path)
        if reference is not None:
            print(f"Reference: {reference_path}")
            cv2.imwrite("Path-image/reference_centerline.png", reference)
    
    # Detect boundaries and centerline
    inner, outer, centerline, debug_image, boundary_mask, road_mask, centerline_mask = detect_circular_road_boundaries(frame)
    
    # Simulate car at bottom center
    car_position = (width // 2, int(height * 0.8))
    
    # Calculate steering
    steering, distance = calculate_car_steering_circular(car_position, centerline, width)
    
    print(f"\nCar steering calculation:")
    print(f"  Car position: {car_position}")
    print(f"  Distance from centerline: {distance} px")
    print(f"  Steering: {steering} (-100=left, 0=straight, 100=right)")
    
    # Draw car position
    cv2.circle(debug_image, car_position, 15, (255, 0, 255), -1)  # Magenta
    cv2.putText(debug_image, "CAR", (car_position[0] - 25, car_position[1] - 25),
                cv2.FONT_HERSHEY_SIMPLEX, 0.8, (255, 0, 255), 2)
    
    # Add steering info
    cv2.putText(debug_image, f"Distance: {distance}px", (20, height - 80),
                cv2.FONT_HERSHEY_SIMPLEX, 0.9, (255, 255, 255), 2)
    cv2.putText(debug_image, f"Steering: {steering}", (20, height - 40),
                cv2.FONT_HERSHEY_SIMPLEX, 0.9, (255, 255, 255), 2)
    
    # Save results
    cv2.imwrite("Path-image/circular_track_detection.png", debug_image)
    cv2.imwrite("Path-image/circular_boundary_mask.png", boundary_mask)
    if road_mask is not None:
        cv2.imwrite("Path-image/circular_road_mask.png", road_mask)
    if centerline_mask is not None:
        cv2.imwrite("Path-image/circular_centerline_mask.png", centerline_mask)
    print(f"\n{'='*70}")
    print(f"SAVED")
    print(f"{'='*70}")
    print(f"✓ Path-image/circular_track_detection.png (boundaries + centerline)")
    print(f"✓ Path-image/circular_boundary_mask.png (white/red detection)")
    print(f"✓ Path-image/circular_road_mask.png (road area)")
    print(f"✓ Path-image/circular_centerline_mask.png (centerline)")
    print(f"{'='*70}\n")


def test_with_camera(camera_index=0):
    """Real-time circular track detection"""
    print(f"{'='*70}")
    print(f"LIVE CIRCULAR TRACK DETECTION")
    print(f"{'='*70}")
    print("Opening camera... Press 'q' to quit, 's' to save")
    
    cap = cv2.VideoCapture(camera_index)
    if not cap.isOpened():
        print(f"✗ Could not open camera")
        return
    
    frame_count = 0
    
    while True:
        ret, frame = cap.read()
        if not ret:
            break
        
        frame_count += 1
        height, width = frame.shape[:2]
        
        # Detect circular track
        inner, outer, centerline, debug_image, boundary_mask, road_mask, centerline_mask = detect_circular_road_boundaries(frame)
        
        # Assume car at bottom center (or detect it)
        car_position = (width // 2, int(height * 0.8))
        steering, distance = calculate_car_steering_circular(car_position, centerline, width)
        
        # Draw car
        cv2.circle(debug_image, car_position, 15, (255, 0, 255), -1)
        
        # Add info
        cv2.putText(debug_image, f"Frame: {frame_count}", (width - 180, 40),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 255, 255), 2)
        cv2.putText(debug_image, f"Dist: {distance}px", (width - 180, 70),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 255, 255), 2)
        cv2.putText(debug_image, f"Steer: {steering}", (width - 180, 100),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 255, 255), 2)
        
        # Show
        cv2.imshow('Circular Track Detection', debug_image)
        cv2.imshow('Boundary Mask', boundary_mask)
        
        key = cv2.waitKey(1) & 0xFF
        if key == ord('q'):
            break
        elif key == ord('s'):
            cv2.imwrite(f"Path-image/camera_circular_{frame_count}.png", debug_image)
            print(f"Saved frame {frame_count}")
    
    cap.release()
    cv2.destroyAllWindows()
    print(f"\n✓ Closed after {frame_count} frames")


if __name__ == "__main__":
    import sys
    
    if len(sys.argv) > 1 and sys.argv[1] == "--camera":
        test_with_camera()
    else:
        test_circular_detection(
            "Simulation/image-1.png",
            reference_path="Simulation/path-2-centerline.png"
        )
        print("\nTo test with live camera:")
        print("  python tests/detect_circular_road_path.py --camera")
