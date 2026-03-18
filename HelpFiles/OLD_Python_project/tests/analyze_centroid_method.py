#!/usr/bin/env python3
"""
Centroid-based line following analysis for circular track.

This script implements a classic centroid-based approach:
1. Convert to grayscale
2. Crop ROI (bottom region)
3. Threshold to binary
4. Find centroid of white pixels
5. Calculate error from center
6. Apply P controller

Parameters:
- Ts = 0.5 (sampling time)
- Kp = 0.005 (proportional gain)
"""

import cv2
import numpy as np
import time

def analyze_centroid_approach(image_path):
    """Analyze image using centroid-based line following."""
    
    print(f"\n{'='*70}")
    print(f"CENTROID-BASED LINE FOLLOWING ANALYSIS")
    print(f"{'='*70}")
    
    # STEP 1: Capture Image (Load from file)
    frame = cv2.imread(image_path)
    if frame is None:
        print(f"ERROR: Could not load image from {image_path}")
        return
    
    print(f"✓ STEP 1: Image loaded - {frame.shape[1]}×{frame.shape[0]} (WxH)")
    cv2.imwrite("Path-image/step1_original.png", frame)
    
    # STEP 2: Convert to Grayscale
    gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
    print(f"✓ STEP 2: Converted to grayscale - {gray.shape[1]}×{gray.shape[0]}")
    cv2.imwrite("Path-image/step2_grayscale.png", gray)
    
    # STEP 3: Crop Bottom Region (ROI)
    height, width = gray.shape
    roi_start = int(height * 2/3)  # Bottom third
    roi = gray[roi_start:height, :]
    print(f"✓ STEP 3: Cropped ROI - {roi.shape[1]}×{roi.shape[0]} (bottom 1/3)")
    cv2.imwrite("Path-image/step3_roi.png", roi)
    
    # STEP 4: Resize (Optional)
    small = cv2.resize(roi, (320, 120))
    print(f"✓ STEP 4: Resized to - 320×120")
    cv2.imwrite("Path-image/step4_resized.png", small)
    
    # STEP 5: Threshold (Binary Segmentation)
    # Try multiple thresholds to find best one
    thresholds = [100, 127, 150, 180]
    best_threshold = 100
    best_white_pixels = 0
    
    print(f"\n{'='*70}")
    print(f"STEP 5: THRESHOLD ANALYSIS")
    print(f"{'='*70}")
    
    for thresh_val in thresholds:
        _, binary = cv2.threshold(small, thresh_val, 255, cv2.THRESH_BINARY_INV)
        white_pixels = np.count_nonzero(binary)
        print(f"  Threshold={thresh_val:3d} → {white_pixels:6,} white pixels ({100*white_pixels/(320*120):.1f}%)")
        
        cv2.imwrite(f"Path-image/step5_threshold_{thresh_val}.png", binary)
        
        # Best is the one with reasonable amount of white (10-50%)
        if 0.10 <= white_pixels/(320*120) <= 0.50 and white_pixels > best_white_pixels:
            best_white_pixels = white_pixels
            best_threshold = thresh_val
    
    # Use best threshold
    _, binary = cv2.threshold(small, best_threshold, 255, cv2.THRESH_BINARY_INV)
    print(f"\n✓ Selected threshold: {best_threshold}")
    cv2.imwrite("Path-image/step5_binary_final.png", binary)
    
    # STEP 6: Remove Noise (Optional)
    kernel = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (5, 5))
    denoised = cv2.morphologyEx(binary, cv2.MORPH_OPEN, kernel, iterations=2)
    denoised = cv2.morphologyEx(denoised, cv2.MORPH_CLOSE, kernel, iterations=2)
    
    white_before = np.count_nonzero(binary)
    white_after = np.count_nonzero(denoised)
    print(f"✓ STEP 6: Denoised - {white_before:,} → {white_after:,} white pixels")
    cv2.imwrite("Path-image/step6_denoised.png", denoised)
    
    # STEP 7: Compute Centroid
    print(f"\n{'='*70}")
    print(f"STEP 7: CENTROID COMPUTATION")
    print(f"{'='*70}")
    
    M = cv2.moments(denoised)
    
    if M["m00"] > 0:
        cx = int(M["m10"] / M["m00"])
        cy = int(M["m01"] / M["m00"])
        print(f"✓ Centroid found at: ({cx}, {cy})")
    else:
        cx = small.shape[1] // 2
        cy = small.shape[0] // 2
        print(f"⚠ No white pixels - using image center: ({cx}, {cy})")
    
    # Visualize centroid
    centroid_vis = cv2.cvtColor(denoised, cv2.COLOR_GRAY2BGR)
    cv2.circle(centroid_vis, (cx, cy), 5, (0, 0, 255), -1)  # Red dot
    cv2.line(centroid_vis, (cx, 0), (cx, centroid_vis.shape[0]), (0, 255, 255), 2)  # Cyan line
    cv2.imwrite("Path-image/step7_centroid.png", centroid_vis)
    
    # STEP 8: Control Law (P Controller)
    print(f"\n{'='*70}")
    print(f"STEP 8: P CONTROLLER")
    print(f"{'='*70}")
    
    image_center = small.shape[1] // 2
    error = cx - image_center
    
    Kp = 0.005
    control = -Kp * error
    
    print(f"Image center:     {image_center} px")
    print(f"Centroid x:       {cx} px")
    print(f"Error:            {error} px")
    print(f"Kp:               {Kp}")
    print(f"Control output:   {control:.6f}")
    
    if error > 0:
        direction = "RIGHT (centroid right of center)"
    elif error < 0:
        direction = "LEFT (centroid left of center)"
    else:
        direction = "STRAIGHT (centered)"
    
    print(f"Correction:       {direction}")
    
    # Visualize control
    control_vis = cv2.cvtColor(small, cv2.COLOR_GRAY2BGR)
    
    # Draw image center (green)
    cv2.line(control_vis, (image_center, 0), (image_center, control_vis.shape[0]), (0, 255, 0), 2)
    
    # Draw centroid (red)
    cv2.circle(control_vis, (cx, cy), 5, (0, 0, 255), -1)
    cv2.line(control_vis, (cx, 0), (cx, control_vis.shape[0]), (0, 0, 255), 2)
    
    # Draw error arrow
    arrow_y = control_vis.shape[0] // 2
    cv2.arrowedLine(control_vis, (image_center, arrow_y), (cx, arrow_y), (255, 0, 255), 2)
    
    # Add text
    cv2.putText(control_vis, f"Error: {error}", (10, 20), 
                cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255, 255, 255), 1)
    cv2.putText(control_vis, f"Control: {control:.4f}", (10, 40), 
                cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255, 255, 255), 1)
    
    cv2.imwrite("Path-image/step8_control.png", control_vis)
    
    # STEP 9: Apply Motor Control (Simulation)
    print(f"\n{'='*70}")
    print(f"STEP 9: MOTOR CONTROL (SIMULATED)")
    print(f"{'='*70}")
    
    # Map control to steering angle (assuming range -30 to +30)
    steering_limit = 30
    steering = np.clip(control * 1000, -steering_limit, steering_limit)  # Scale up
    
    # Map control to left/right motor speeds
    base_speed = 10
    left_speed = base_speed - steering
    right_speed = base_speed + steering
    
    print(f"Base speed:       {base_speed}")
    print(f"Steering angle:   {steering:.2f}°")
    print(f"Left motor:       {left_speed:.2f}")
    print(f"Right motor:      {right_speed:.2f}")
    
    # STEP 10: Sampling Time
    print(f"\n{'='*70}")
    print(f"STEP 10: SAMPLING TIME")
    print(f"{'='*70}")
    
    Ts = 0.5
    print(f"Target sampling time: {Ts} seconds")
    print(f"(In real-time loop, would sleep to maintain {Ts}s)")
    
    # Summary
    print(f"\n{'='*70}")
    print(f"SUMMARY")
    print(f"{'='*70}")
    print(f"✓ All 10 steps completed successfully")
    print(f"✓ Generated {len(thresholds) + 9} visualization images in Path-image/")
    print(f"\nKey Findings:")
    print(f"  - Best threshold: {best_threshold}")
    print(f"  - Centroid at: ({cx}, {cy})")
    print(f"  - Error from center: {error} px")
    print(f"  - Control output: {control:.6f}")
    print(f"  - Steering correction: {direction}")
    print(f"\n{'='*70}\n")

if __name__ == "__main__":
    analyze_centroid_approach("Path-image/image-1.png")
