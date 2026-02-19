#!/usr/bin/env python3
"""
Professional Road Detection Diagnostic Tool

Tests road boundary detection capabilities and provides detailed debug information.
Helps diagnose and tune HSV color thresholds for different track types.
"""

import sys
import cv2
import numpy as np
from pathlib import Path

# Add project root to path
project_root = Path(__file__).parent
sys.path.insert(0, str(project_root))

from rc_autonomy.config import Config
from rc_autonomy.boundary import BoundaryDetector


class RoadDetectionDiagnostics:
    """Comprehensive road detection testing and diagnostics."""
    
    def __init__(self, config_path: str = 'config/default.json'):
        self.config = Config.load(config_path)
        self.detector = BoundaryDetector(
            black_threshold=self.config.guidance.black_threshold,
            ray_angles_deg=self.config.guidance.ray_angles_deg,
            ray_max_length=self.config.guidance.ray_max_length,
            evasive_distance=self.config.guidance.evasive_distance,
            default_speed=self.config.guidance.default_speed,
            steering_limit=self.config.control.steering_limit,
            light_on=self.config.control.light_on,
        )
        self.cap = cv2.VideoCapture(self.config.camera.source)
        self.frame_count = 0
        self.detection_count = 0
        
    def run(self):
        """Main diagnostic loop."""
        print("=" * 70)
        print("ROAD DETECTION DIAGNOSTIC TOOL")
        print("=" * 70)
        print("\nControls:")
        print("  [Space] - Pause/Resume")
        print("  [s]     - Save current frame for analysis")
        print("  [d]     - Toggle debug visualization")
        print("  [c]     - Toggle color space view")
        print("  [h]     - Show HSV histogram")
        print("  [q]     - Quit")
        print("\n" + "=" * 70 + "\n")
        
        paused = False
        show_debug = False
        show_color_space = False
        current_frame = None
        
        while True:
            if not paused:
                ret, frame = self.cap.read()
                if not ret:
                    print("[!] Failed to read frame")
                    break
                
                current_frame = frame.copy()
                self.frame_count += 1
                
                # Detect road edges
                left, center, right, mask = self.detector.detect_road_edges(frame)
                
                # Display results
                display = self._create_display(frame, left, center, right, mask, 
                                              show_debug, show_color_space)
                
                # Update statistics
                if left is not None:
                    self.detection_count += 1
                    detection_rate = (self.detection_count / self.frame_count) * 100
                    print(f"\r[{self.frame_count:4d}] Road: L={left:3d} C={center:3d} R={right:3d} "
                          f"W={right-left:3d}px | Detection: {detection_rate:5.1f}%", end='', flush=True)
                else:
                    detection_rate = (self.detection_count / self.frame_count) * 100
                    print(f"\r[{self.frame_count:4d}] ✗ Road NOT detected | Detection: {detection_rate:5.1f}%", 
                          end='', flush=True)
            else:
                if current_frame is not None:
                    display = self._create_display(current_frame, None, None, None, None,
                                                  show_debug, show_color_space)
                else:
                    display = np.zeros((self.config.camera.height, self.config.camera.width, 3), 
                                      dtype=np.uint8)
            
            cv2.imshow("Road Detection Diagnostic", display)
            
            key = cv2.waitKey(1) & 0xFF
            if key == ord('q'):
                print("\n[*] Exiting...")
                break
            elif key == ord(' '):
                paused = not paused
                status = "PAUSED" if paused else "RUNNING"
                print(f"\n[*] {status}")
            elif key == ord('s'):
                filename = f"road_detection_frame_{self.frame_count:04d}.png"
                cv2.imwrite(filename, current_frame)
                print(f"\n[✓] Frame saved: {filename}")
            elif key == ord('d'):
                show_debug = not show_debug
                status = "ON" if show_debug else "OFF"
                print(f"\n[*] Debug visualization: {status}")
            elif key == ord('c'):
                show_color_space = not show_color_space
                status = "ON" if show_color_space else "OFF"
                print(f"\n[*] Color space view: {status}")
            elif key == ord('h'):
                self._show_hsv_histogram(current_frame)
        
        self.cleanup()
        self._print_summary()
    
    def _create_display(self, frame, left, center, right, mask, show_debug, show_color_space):
        """Create diagnostic display with overlays."""
        display = frame.copy()
        height, width = frame.shape[:2]
        
        # Draw road boundaries if detected
        if left is not None and center is not None and right is not None:
            self._draw_detection(display, left, center, right, mask)
        else:
            cv2.putText(display, "NO ROAD DETECTED", (10, 30),
                       cv2.FONT_HERSHEY_SIMPLEX, 1.0, (0, 0, 255), 2)
        
        # Debug visualization
        if show_debug:
            display = self._draw_debug_info(display, frame)
        
        # Color space visualization
        if show_color_space:
            display = self._draw_color_space(display, frame)
        
        return display
    
    def _draw_detection(self, frame, left, center, right, mask):
        """Draw detected road boundaries."""
        height = frame.shape[0]
        
        # Draw road region overlay
        if mask is not None:
            overlay = frame.copy()
            overlay[mask > 0] = [0, 200, 0]
            cv2.addWeighted(overlay, 0.15, frame, 0.85, 0, frame)
        
        # Draw boundaries
        cv2.line(frame, (left, 0), (left, height), (0, 255, 0), 4)
        cv2.line(frame, (right, 0), (right, height), (0, 255, 0), 4)
        
        # Draw centerline (dashed)
        dash_length = 15
        gap_length = 10
        for y in range(0, height, dash_length + gap_length):
            y_end = min(y + dash_length, height)
            cv2.line(frame, (center, y), (center, y_end), (255, 0, 0), 3)
        
        # Draw markers
        cv2.circle(frame, (left, height // 2), 8, (0, 255, 0), 2)
        cv2.circle(frame, (center, height // 2), 8, (255, 0, 0), 2)
        cv2.circle(frame, (right, height // 2), 8, (0, 255, 0), 2)
        
        # Draw info
        road_width = right - left
        info = f"Road Detected: L={left} C={center} R={right} W={road_width}px"
        cv2.putText(frame, info, (10, 30), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 0), 2)
    
    def _draw_debug_info(self, frame, original_frame):
        """Draw debug color masks."""
        import cv2
        
        hsv = cv2.cvtColor(original_frame, cv2.COLOR_BGR2HSV)
        
        # Create masks
        red_mask1 = cv2.inRange(hsv, np.array([0, 50, 40]), np.array([10, 255, 255]))
        red_mask2 = cv2.inRange(hsv, np.array([170, 50, 40]), np.array([180, 255, 255]))
        red_mask = cv2.bitwise_or(red_mask1, red_mask2)
        
        white_mask = cv2.inRange(hsv, np.array([0, 0, 180]), np.array([180, 30, 255]))
        
        # Show red mask stats
        red_pixels = cv2.countNonZero(red_mask)
        white_pixels = cv2.countNonZero(white_mask)
        total_pixels = frame.shape[0] * frame.shape[1]
        
        red_percent = (red_pixels / total_pixels) * 100
        white_percent = (white_pixels / total_pixels) * 100
        
        info_text = f"Red: {red_pixels:6d} ({red_percent:5.2f}%) | White: {white_pixels:6d} ({white_percent:5.2f}%)"
        cv2.putText(frame, info_text, (10, frame.shape[0] - 20),
                   cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 255, 0), 2)
        
        return frame
    
    def _draw_color_space(self, frame, original_frame):
        """Draw HSV color space visualization."""
        hsv = cv2.cvtColor(original_frame, cv2.COLOR_BGR2HSV)
        
        # Extract H channel and normalize for display
        h_channel = hsv[:, :, 0]
        h_display = (h_channel / 180 * 255).astype(np.uint8)
        h_colored = cv2.applyColorMap(h_display, cv2.COLORMAP_HSV)
        
        # Create small preview
        preview_height = 60
        preview_width = 160
        resized = cv2.resize(h_colored, (preview_width, preview_height))
        
        # Place in corner
        frame[-preview_height:, -preview_width:] = resized
        
        cv2.putText(frame, "HSV Hue Channel", 
                   (frame.shape[1] - preview_width, frame.shape[0] - preview_height - 5),
                   cv2.FONT_HERSHEY_SIMPLEX, 0.4, (255, 255, 255), 1)
        
        return frame
    
    def _show_hsv_histogram(self, frame):
        """Display HSV histogram for color analysis."""
        import cv2
        
        hsv = cv2.cvtColor(frame, cv2.COLOR_BGR2HSV)
        
        # Calculate histograms
        hist_h = cv2.calcHist([hsv], [0], None, [180], [0, 180])
        hist_s = cv2.calcHist([hsv], [1], None, [256], [0, 256])
        hist_v = cv2.calcHist([hsv], [2], None, [256], [0, 256])
        
        # Create histogram display
        height, width = 300, 400
        hist_display = np.ones((height, width, 3), dtype=np.uint8) * 255
        
        # Normalize
        hist_h = cv2.normalize(hist_h, hist_h, 0, height - 40, cv2.NORM_MINMAX)
        hist_s = cv2.normalize(hist_s, hist_s, 0, height - 40, cv2.NORM_MINMAX)
        hist_v = cv2.normalize(hist_v, hist_v, 0, height - 40, cv2.NORM_MINMAX)
        
        # Draw histograms
        bin_w = width // 180
        for i in range(1, 180):
            cv2.line(hist_display, (bin_w * (i-1), height - 30 - int(hist_h[i-1])),
                    (bin_w * i, height - 30 - int(hist_h[i])), (0, 0, 255), 2)
        
        cv2.putText(hist_display, "HSV Histograms (Red=H, Green=S, Blue=V)",
                   (10, 20), cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 0, 0), 2)
        
        cv2.imshow("HSV Histogram", hist_display)
        print("\n[*] Histogram displayed. Press any key to close.")
        cv2.waitKey(0)
    
    def cleanup(self):
        """Clean up resources."""
        self.cap.release()
        cv2.destroyAllWindows()
    
    def _print_summary(self):
        """Print diagnostic summary."""
        detection_rate = (self.detection_count / max(1, self.frame_count)) * 100
        
        print("\n" + "=" * 70)
        print("DIAGNOSTIC SUMMARY")
        print("=" * 70)
        print(f"Frames Processed:        {self.frame_count:6d}")
        print(f"Successful Detections:   {self.detection_count:6d}")
        print(f"Detection Rate:          {detection_rate:6.1f}%")
        print("=" * 70)
        
        if detection_rate > 90:
            print("✓ EXCELLENT: Road detection working reliably!")
        elif detection_rate > 70:
            print("◐ GOOD: Road detection working, but may need tuning")
        elif detection_rate > 50:
            print("⚠ FAIR: Road detection inconsistent, needs tuning")
        else:
            print("✗ POOR: Road detection failing, check lighting/colors")
        
        print("=" * 70)


if __name__ == '__main__':
    import sys
    
    config_path = sys.argv[1] if len(sys.argv) > 1 else 'config/default.json'
    
    if not Path(config_path).exists():
        print(f"[!] Config file not found: {config_path}")
        sys.exit(1)
    
    diagnostics = RoadDetectionDiagnostics(config_path)
    diagnostics.run()
