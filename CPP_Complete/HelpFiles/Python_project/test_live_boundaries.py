#!/usr/bin/env python3
"""
Test script to run live boundary detection with improved car detection.
Uses the adaptive threshold method from BoundaryDetector class.
"""

import sys
import cv2
import numpy as np
from rc_autonomy.boundary import BoundaryDetector

def run_live_demo(camera_index: int = 0, width: int = 640, height: int = 480, fps: int = 30):
    """
    Run live boundary and car detection demo.
    Press 'q' to quit, 's' to save frame.
    """
    
    # Open camera
    cap = cv2.VideoCapture(camera_index)
    if not cap.isOpened():
        print(f"ERROR: Could not open camera {camera_index}")
        return
    
    cap.set(cv2.CAP_PROP_FRAME_WIDTH, width)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, height)
    cap.set(cv2.CAP_PROP_FPS, fps)
    
    # Warm up
    for _ in range(3):
        cap.read()
    
    # Create detector
    detector = BoundaryDetector(
        black_threshold=40,
        ray_angles_deg=[-90, 0, 90],
        ray_max_length=100,
        evasive_distance=30,
        default_speed=50,
        steering_limit=100,
        light_on=True
    )
    
    frame_count = 0
    
    print("\n" + "="*70)
    print("LIVE BOUNDARY DETECTION WITH CAR DETECTION")
    print("="*70)
    print("Press 'q' to quit | 's' to save frame | 'd' to save display")
    print("="*70 + "\n")
    
    try:
        while True:
            ret, frame = cap.read()
            if not ret:
                print("Camera read failed")
                break
            
            frame_count += 1
            
            # Detect boundaries
            left, center, right, road_mask = detector.detect_road_edges(frame)
            
            # Detect car
            car_bbox = detector.detect_car_in_frame(frame, road_mask)
            
            # Create display
            display = frame.copy()
            
            if left is not None and right is not None:
                # Draw road overlay
                if road_mask is not None:
                    overlay = display.copy()
                    overlay[road_mask > 0] = [0, 200, 0]
                    cv2.addWeighted(overlay, 0.1, display, 0.9, 0, display)
                
                # Draw boundaries
                h = frame.shape[0]
                cv2.line(display, (left, 0), (left, h), (0, 255, 0), 3)
                cv2.line(display, (right, 0), (right, h), (0, 255, 0), 3)
                
                # Draw center dashed
                for y in range(0, h, 30):
                    cv2.line(display, (center, y), (center, min(y+20, h)), (255, 0, 0), 2)
                
                # Status
                road_width = right - left
                status = f"ROAD: L={left} C={center} R={right} W={road_width}"
                cv2.putText(display, status, (10, 30), cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 255, 0), 2)
            else:
                cv2.putText(display, "NO BOUNDARIES DETECTED", (10, 30), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 0, 255), 2)
            
            # Draw car if detected
            if car_bbox:
                x, y, w, h = car_bbox
                cv2.rectangle(display, (x, y), (x+w, y+h), (0, 165, 255), 2)
                cv2.circle(display, (x+w//2, y+h//2), 4, (0, 165, 255), -1)
                cv2.putText(display, f"CAR: {car_bbox}", (10, 60), cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 165, 255), 2)
            
            # Show FPS
            if frame_count % 30 == 0:
                print(f"[Frame {frame_count}] L={left} C={center} R={right} Car={car_bbox is not None}")
            
            # Display
            cv2.imshow("Live Boundary Detection", display)
            
            # Keyboard input
            key = cv2.waitKey(1) & 0xFF
            if key == ord('q'):
                print("\nQuitting...")
                break
            elif key == ord('s'):
                filename = f"live_frame_{frame_count}.png"
                cv2.imwrite(filename, frame)
                print(f"Saved: {filename}")
            elif key == ord('d'):
                filename = f"live_display_{frame_count}.png"
                cv2.imwrite(filename, display)
                print(f"Saved: {filename}")
    
    finally:
        cap.release()
        cv2.destroyAllWindows()
        print("Done!\n")


if __name__ == "__main__":
    import argparse
    
    parser = argparse.ArgumentParser(description="Live boundary detection with car detection")
    parser.add_argument("--camera", type=int, default=0, help="Camera index (default: 0)")
    parser.add_argument("--width", type=int, default=640, help="Frame width (default: 640)")
    parser.add_argument("--height", type=int, default=480, help="Frame height (default: 480)")
    parser.add_argument("--fps", type=int, default=30, help="Target FPS (default: 30)")
    
    args = parser.parse_args()
    
    try:
        run_live_demo(
            camera_index=args.camera,
            width=args.width,
            height=args.height,
            fps=args.fps
        )
    except KeyboardInterrupt:
        print("\nInterrupted by user")
        sys.exit(0)
    except Exception as e:
        print(f"Error: {e}")
        sys.exit(1)
