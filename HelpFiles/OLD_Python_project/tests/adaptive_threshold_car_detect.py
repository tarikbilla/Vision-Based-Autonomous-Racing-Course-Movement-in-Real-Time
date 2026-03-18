#!/usr/bin/env python3
"""
Adaptive threshold binary segmentation with car tracking.
Converted from C++ OpenCV code.
"""

import cv2
import numpy as np

def main():
    # Open camera or use static image
    use_camera = False  # Set to True for live camera
    
    if use_camera:
        cap = cv2.VideoCapture(0)
        
        if not cap.isOpened():
            print("Camera not opened!")
            return -1
        
        print("Gray + Strong Binary + Car Tracking")
    else:
        # Use static image for testing
        print("Processing static image: Path-image/image-1.png")
        frame = cv2.imread("Path-image/image-1.png")
        if frame is None:
            print("Could not load image!")
            return -1
    
    iteration = 0
    
    while True:
        if use_camera:
            ret, frame = cap.read()
            if not ret or frame is None:
                break
        else:
            # For static image, process once and save
            if iteration > 0:
                break
        
        height, width = frame.shape[:2]
        
        # ---- Convert to grayscale ----
        gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
        
        # ---- Blur ----
        gray = cv2.GaussianBlur(gray, (5, 5), 0)
        
        # ---- Adaptive threshold (strong working version) ----
        binary = cv2.adaptiveThreshold(
            gray, 
            255,
            cv2.ADAPTIVE_THRESH_GAUSSIAN_C,
            cv2.THRESH_BINARY_INV,
            11,  # blockSize
            2    # C constant
        )
        
        # ---- Clean mask ----
        binary = cv2.erode(binary, None, iterations=1)
        binary = cv2.dilate(binary, None, iterations=2)
        
        # ---- Car Detection ----
        contours, _ = cv2.findContours(
            binary, 
            cv2.RETR_EXTERNAL,
            cv2.CHAIN_APPROX_SIMPLE
        )
        
        # Create a copy for drawing
        gray_with_detection = gray.copy()
        
        car_detected = False
        car_positions = []
        
        for cnt in contours:
            area = cv2.contourArea(cnt)
            
            # Adjust area thresholds if needed
            if 1000 < area < 8000:
                box = cv2.boundingRect(cnt)
                x, y, w, h = box
                
                cx = x + w // 2
                cy = y + h // 2
                
                # Draw bounding box on grayscale image
                cv2.rectangle(
                    gray_with_detection, 
                    (x, y), 
                    (x + w, y + h),
                    255,  # White color in grayscale
                    2
                )
                
                # Draw center point
                cv2.circle(
                    gray_with_detection,
                    (cx, cy),
                    5,
                    255,  # White color in grayscale
                    -1
                )
                
                print(f"Car Position -> X: {cx} Y: {cy}")
                car_detected = True
                car_positions.append((cx, cy, area))
        
        if not car_detected:
            print("No car detected in valid size range (1000-8000 px²)")
        else:
            print(f"Detected {len(car_positions)} car(s)")
        
        # ---- Convert to color for display ----
        gray_color = cv2.cvtColor(gray_with_detection, cv2.COLOR_GRAY2BGR)
        binary_color = cv2.cvtColor(binary, cv2.COLOR_GRAY2BGR)
        
        # ---- Concatenate two windows side by side ----
        display = np.hstack([gray_color, binary_color])
        
        # ---- Resize for better viewing ----
        display = cv2.resize(display, (1280, 600))
        
        # Add labels
        cv2.putText(display, "Grayscale + Detection", (10, 30),
                    cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 255, 0), 2)
        cv2.putText(display, "Binary (Adaptive Threshold)", (640 + 10, 30),
                    cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 255, 0), 2)
        
        if use_camera:
            cv2.imshow("Grayscale | Strong Binary", display)
            
            if cv2.waitKey(1) & 0xFF == ord('q'):
                break
        else:
            # Save for static image
            cv2.imwrite("Path-image/adaptive_threshold_result.png", display)
            cv2.imwrite("Path-image/adaptive_gray.png", gray_with_detection)
            cv2.imwrite("Path-image/adaptive_binary.png", binary)
            
            print(f"\n✓ Saved results to Path-image/")
            print(f"  - adaptive_threshold_result.png (combined view)")
            print(f"  - adaptive_gray.png (grayscale + detections)")
            print(f"  - adaptive_binary.png (binary mask)")
            
            # Also show contour statistics
            print(f"\nContour Statistics:")
            print(f"Total contours found: {len(contours)}")
            
            areas = [cv2.contourArea(cnt) for cnt in contours]
            areas_sorted = sorted(areas, reverse=True)
            
            print(f"Top 10 contour areas:")
            for i, area in enumerate(areas_sorted[:10]):
                in_range = "✓ CAR" if 1000 < area < 8000 else ""
                print(f"  {i+1}. {area:,.0f} px² {in_range}")
            
            break
        
        iteration += 1
    
    if use_camera:
        cap.release()
    cv2.destroyAllWindows()
    
    return 0

if __name__ == "__main__":
    main()
