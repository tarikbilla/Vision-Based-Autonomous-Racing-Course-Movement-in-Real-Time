#!/usr/bin/env python3
"""
Road Detection Debugging Tool

Specifically designed to debug why road detection is failing on your track.
Shows what the system is seeing at each stage of color detection.
"""

import sys
import cv2
import numpy as np
from pathlib import Path

project_root = Path(__file__).parent
sys.path.insert(0, str(project_root))

from rc_autonomy.config import Config


def debug_road_detection():
    """Interactive road detection debugging."""
    
    config = Config.load('config/default.json')
    cap = cv2.VideoCapture(config.camera.source)
    
    print("=" * 70)
    print("ROAD DETECTION DEBUG TOOL")
    print("=" * 70)
    print("\nShowing detection at each HSV color range...")
    print("\nControls:")
    print("  [Space] - Pause/Resume")
    print("  [r]     - Show RED detection only")
    print("  [w]     - Show WHITE detection only")
    print("  [o]     - Show ORANGE detection only")
    print("  [a]     - Show ALL combined")
    print("  [h]     - Show HSV image itself")
    print("  [c]     - Show combined mask")
    print("  [m]     - Show morphology processing stages")
    print("  [q]     - Quit")
    print("\n" + "=" * 70 + "\n")
    
    mode = 'all'
    paused = False
    frame_count = 0
    
    while True:
        if not paused:
            ret, frame = cap.read()
            if not ret:
                print("Failed to read frame")
                break
            
            frame_count += 1
            hsv = cv2.cvtColor(frame, cv2.COLOR_BGR2HSV)
            height, width = frame.shape[:2]
            
            # Create detection masks
            red1 = cv2.inRange(hsv, np.array([0, 40, 30]), np.array([15, 255, 255]))
            red2 = cv2.inRange(hsv, np.array([160, 40, 30]), np.array([180, 255, 255]))
            red_mask = cv2.bitwise_or(red1, red2)
            
            white1 = cv2.inRange(hsv, np.array([0, 0, 200]), np.array([180, 25, 255]))
            white2 = cv2.inRange(hsv, np.array([0, 0, 150]), np.array([180, 50, 200]))
            white_mask = cv2.bitwise_or(white1, white2)
            
            orange_mask = cv2.inRange(hsv, np.array([5, 80, 50]), np.array([20, 255, 255]))
            
            combined_mask = cv2.bitwise_or(cv2.bitwise_or(red_mask, white_mask), orange_mask)
            
            # Morphology
            kernel_small = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (3, 3))
            kernel_medium = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (5, 5))
            
            cleaned1 = cv2.morphologyEx(combined_mask, cv2.MORPH_OPEN, kernel_small, iterations=1)
            cleaned2 = cv2.morphologyEx(cleaned1, cv2.MORPH_CLOSE, kernel_medium, iterations=2)
            cleaned3 = cv2.dilate(cleaned2, kernel_medium, iterations=2)
            
            # Create display based on mode
            if mode == 'red':
                display = cv2.cvtColor(red_mask, cv2.COLOR_GRAY2BGR)
                title = f"RED DETECTION ({frame_count}) - Pixels: {cv2.countNonZero(red_mask)}"
            elif mode == 'white':
                display = cv2.cvtColor(white_mask, cv2.COLOR_GRAY2BGR)
                title = f"WHITE DETECTION ({frame_count}) - Pixels: {cv2.countNonZero(white_mask)}"
            elif mode == 'orange':
                display = cv2.cvtColor(orange_mask, cv2.COLOR_GRAY2BGR)
                title = f"ORANGE DETECTION ({frame_count}) - Pixels: {cv2.countNonZero(orange_mask)}"
            elif mode == 'hsv':
                # Show HSV channels
                h_channel = hsv[:, :, 0]
                s_channel = hsv[:, :, 1]
                v_channel = hsv[:, :, 2]
                h_display = (h_channel / 180 * 255).astype(np.uint8)
                display = cv2.merge([h_display, s_channel, v_channel])
                title = f"HSV CHANNELS ({frame_count})"
            elif mode == 'combined':
                display = cv2.cvtColor(combined_mask, cv2.COLOR_GRAY2BGR)
                title = f"COMBINED MASK ({frame_count}) - Red: {cv2.countNonZero(red_mask)}, White: {cv2.countNonZero(white_mask)}, Orange: {cv2.countNonZero(orange_mask)}"
            elif mode == 'morph':
                # Show 4 stages
                h, w = frame.shape[:2]
                display = np.zeros((h * 2, w * 2, 3), dtype=np.uint8)
                display[0:h, 0:w] = cv2.cvtColor(combined_mask, cv2.COLOR_GRAY2BGR)
                display[0:h, w:2*w] = cv2.cvtColor(cleaned1, cv2.COLOR_GRAY2BGR)
                display[h:2*h, 0:w] = cv2.cvtColor(cleaned2, cv2.COLOR_GRAY2BGR)
                display[h:2*h, w:2*w] = cv2.cvtColor(cleaned3, cv2.COLOR_GRAY2BGR)
                cv2.putText(display, "Original", (10, 30), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 0), 2)
                cv2.putText(display, "Open", (w+10, 30), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 0), 2)
                cv2.putText(display, "Close", (10, h+30), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 0), 2)
                cv2.putText(display, "Dilate", (w+10, h+30), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 0), 2)
                title = f"MORPHOLOGY STAGES ({frame_count})"
            else:  # 'all'
                display = cv2.cvtColor(combined_mask, cv2.COLOR_GRAY2BGR)
                # Add statistics
                title = f"ALL COLORS ({frame_count}) - Red: {cv2.countNonZero(red_mask):6d}px, White: {cv2.countNonZero(white_mask):6d}px, Orange: {cv2.countNonZero(orange_mask):6d}px"
                
                # Draw threshold lines on original frame
                display = frame.copy()
                # Show color ranges as rectangles
                cv2.rectangle(display, (10, 10), (100, 40), (0, 0, 255), -1)  # Red
                cv2.rectangle(display, (110, 10), (200, 40), (255, 255, 255), -1)  # White
                cv2.rectangle(display, (210, 10), (300, 40), (0, 165, 255), -1)  # Orange
                
                cv2.putText(display, "Red", (15, 30), cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 255, 255), 1)
                cv2.putText(display, "White", (115, 30), cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 0, 0), 1)
                cv2.putText(display, "Orange", (215, 30), cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 255, 255), 1)
            
            cv2.imshow(title, display)
        
        key = cv2.waitKey(1) & 0xFF
        if key == ord('q'):
            break
        elif key == ord(' '):
            paused = not paused
            print(f"\n{'PAUSED' if paused else 'RESUMED'}")
        elif key == ord('r'):
            mode = 'red'
            print("\nMode: RED DETECTION")
        elif key == ord('w'):
            mode = 'white'
            print("\nMode: WHITE DETECTION")
        elif key == ord('o'):
            mode = 'orange'
            print("\nMode: ORANGE DETECTION")
        elif key == ord('a'):
            mode = 'all'
            print("\nMode: ALL COLORS")
        elif key == ord('h'):
            mode = 'hsv'
            print("\nMode: HSV CHANNELS")
        elif key == ord('c'):
            mode = 'combined'
            print("\nMode: COMBINED MASK")
        elif key == ord('m'):
            mode = 'morph'
            print("\nMode: MORPHOLOGY STAGES")
    
    cap.release()
    cv2.destroyAllWindows()
    print("\nDone!")


if __name__ == '__main__':
    debug_road_detection()
