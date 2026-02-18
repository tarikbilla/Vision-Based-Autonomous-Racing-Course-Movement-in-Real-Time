#!/usr/bin/env python3
"""
Live camera boundary detection using adaptive threshold.
Same method as extract_clean_boundaries.py but for real-time operation.
Improved car detection that distinguishes between road boundaries and car.
"""

import cv2
import numpy as np
from typing import Tuple, Optional, List


class LiveBoundaryDetector:
    """
    Detects track boundaries and car in live camera feed using adaptive threshold.
    Prevents confusion between road markers (boundaries) and the car itself.
    """
    
    def __init__(self, frame_width: int = 640, frame_height: int = 480):
        self.frame_width = frame_width
        self.frame_height = frame_height
        self.last_car_position = None
        self.frame_buffer = []
        self.max_buffer_size = 5
        
    def detect_boundaries_and_car(self, frame: np.ndarray) -> Tuple[
        Optional[int],  # left_edge_x
        Optional[int],  # center_x
        Optional[int],  # right_edge_x
        Optional[np.ndarray],  # road_mask
        Optional[Tuple[int, int, int, int]],  # car_bbox (x, y, w, h)
        np.ndarray  # display_frame (with visualizations)
    ]:
        """
        Detect road boundaries and car position using adaptive threshold.
        
        Returns:
            - left_edge_x: X coordinate of left boundary
            - center_x: X coordinate of road center
            - right_edge_x: X coordinate of right boundary
            - road_mask: Binary mask of road area
            - car_bbox: Bounding box of detected car (x, y, width, height) or None
            - display_frame: Frame with boundary lines drawn for visualization
        """
        
        height, width = frame.shape[:2]
        display = frame.copy()
        
        # ===== STEP 1: ADAPTIVE THRESHOLD =====
        gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
        blurred = cv2.GaussianBlur(gray, (5, 5), 0)
        
        binary = cv2.adaptiveThreshold(
            blurred, 255,
            cv2.ADAPTIVE_THRESH_GAUSSIAN_C,
            cv2.THRESH_BINARY_INV,
            11, 2
        )
        
        # ===== STEP 2: MINIMAL MORPHOLOGY =====
        kernel = np.ones((2, 2), np.uint8)
        binary = cv2.morphologyEx(binary, cv2.MORPH_OPEN, kernel, iterations=1)
        binary = cv2.morphologyEx(binary, cv2.MORPH_CLOSE, kernel, iterations=1)
        
        # ===== STEP 3: FIND CONTOURS =====
        contours, _ = cv2.findContours(binary, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
        
        if len(contours) == 0:
            cv2.putText(display, "No contours detected", (10, 30), 
                       cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 0, 255), 2)
            return None, None, None, None, None, display
        
        # ===== STEP 4: FILTER AND SORT CONTOURS =====
        contours_with_area = [(cv2.contourArea(c), c) for c in contours]
        contours_sorted = sorted(contours_with_area, key=lambda x: x[0], reverse=True)
        
        # Keep top 10 with minimum area threshold
        large_contours = [(area, c) for area, c in contours_sorted if area >= 700]
        
        if len(large_contours) < 2:
            cv2.putText(display, f"Not enough boundaries (found {len(large_contours)})", 
                       (10, 30), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 0, 255), 2)
            return None, None, None, None, None, display
        
        # ===== STEP 5: IDENTIFY BOUNDARIES VS ROAD MARKERS =====
        # The two largest contours should be outer and inner track boundaries
        outer_area, outer_boundary = large_contours[0]
        inner_area, inner_boundary = large_contours[1]
        
        # Remaining contours: could be road markers, car, or road segments
        other_contours = large_contours[2:] if len(large_contours) > 2 else []
        
        try:
            # Get edge positions from outer boundary
            left_edge_x = int(outer_boundary[:, :, 0].min())
            right_edge_x = int(outer_boundary[:, :, 0].max())
            top_edge_y = int(outer_boundary[:, :, 1].min())
            bottom_edge_y = int(outer_boundary[:, :, 1].max())
            
            # Create road mask (area between outer and inner boundaries)
            outer_mask = np.zeros((height, width), dtype=np.uint8)
            inner_mask = np.zeros((height, width), dtype=np.uint8)
            cv2.drawContours(outer_mask, [outer_boundary], 0, 255, -1)
            cv2.drawContours(inner_mask, [inner_boundary], 0, 255, -1)
            road_mask = cv2.bitwise_xor(outer_mask, inner_mask)
            
            # Calculate road center
            mid_y = height // 2
            row = road_mask[mid_y, :]
            if np.any(row):
                indices = np.where(row > 0)[0]
                center_x = (indices[0] + indices[-1]) // 2 if len(indices) > 0 else (left_edge_x + right_edge_x) // 2
            else:
                center_x = (left_edge_x + right_edge_x) // 2
            
            # Verify reasonable separation
            min_separation = max(50, width * 0.15)
            if right_edge_x - left_edge_x < min_separation:
                return None, None, None, None, None, display
            
            # ===== STEP 6: DETECT CAR =====
            car_bbox = self._detect_car(
                binary, other_contours, road_mask, 
                left_edge_x, right_edge_x, center_x, height, width
            )
            
            # ===== STEP 7: DRAW VISUALIZATION =====
            self._draw_boundaries(display, left_edge_x, center_x, right_edge_x, height, road_mask)
            if car_bbox:
                self._draw_car_bbox(display, car_bbox)
            
            # Draw status text
            road_width = right_edge_x - left_edge_x
            status_text = f"BOUNDARIES: L={left_edge_x} C={center_x} R={right_edge_x} W={road_width}px"
            cv2.putText(display, status_text, (10, 30), cv2.FONT_HERSHEY_SIMPLEX, 
                       0.6, (0, 255, 0), 2)
            
            if car_bbox:
                car_text = f"CAR DETECTED at {car_bbox[:2]}"
                cv2.putText(display, car_text, (10, 60), cv2.FONT_HERSHEY_SIMPLEX, 
                           0.6, (0, 165, 255), 2)
            
            return left_edge_x, center_x, right_edge_x, road_mask, car_bbox, display
            
        except Exception as e:
            cv2.putText(display, f"Error: {str(e)[:40]}", (10, 30), 
                       cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 0, 255), 2)
            return None, None, None, None, None, display
    
    def _detect_car(
        self, 
        binary: np.ndarray,
        other_contours: List[Tuple[float, np.ndarray]],
        road_mask: np.ndarray,
        left_edge: int,
        right_edge: int,
        center_x: int,
        height: int,
        width: int
    ) -> Optional[Tuple[int, int, int, int]]:
        """
        Detect car position by filtering out road markers.
        
        Car characteristics:
        - Located in center region of frame (within road area)
        - Typically in lower half of frame
        - Moving object (small movement frame-to-frame)
        - Different size/shape than stationary road markers
        - Won't be at extreme left/right edges
        
        Returns: (x, y, width, height) or None
        """
        
        if len(other_contours) == 0:
            return None
        
        # Define car search region (center of road, lower half of frame)
        car_search_left = left_edge + int((right_edge - left_edge) * 0.2)
        car_search_right = right_edge - int((right_edge - left_edge) * 0.2)
        car_search_top = int(height * 0.4)
        car_search_bottom = height
        
        best_car_candidate = None
        best_car_score = -1
        
        for area, contour in other_contours:
            x, y, w, h = cv2.boundingRect(contour)
            cx, cy = x + w // 2, y + h // 2
            
            # Filter criteria to distinguish car from road markers
            
            # 1. Must be in car search region
            if not (car_search_left < cx < car_search_right and 
                   car_search_top < cy < car_search_bottom):
                continue
            
            # 2. Must be within road mask
            if not self._is_mostly_in_road(road_mask, x, y, w, h):
                continue
            
            # 3. Size filtering (car should be reasonable size, not too small like noise)
            min_size = 30  # Minimum width/height
            max_size = min(int((right_edge - left_edge) * 0.4), int(height * 0.3))
            if w < min_size or h < min_size or w > max_size or h > max_size:
                continue
            
            # 4. Aspect ratio filtering (car is roughly square-ish, not extreme)
            aspect = max(w, h) / max(1, min(w, h))
            if aspect > 3:  # Too elongated, likely road marker
                continue
            
            # 5. Prefer contours closer to bottom of frame (car position)
            distance_from_bottom = height - cy
            position_score = 1.0 / (distance_from_bottom + 100)  # Higher score = closer to bottom
            
            # 6. Prefer contours in center (not at edges)
            center_distance = abs(cx - center_x)
            max_center_distance = (right_edge - left_edge) * 0.3
            if center_distance > max_center_distance:
                continue
            center_score = 1.0 - (center_distance / max_center_distance)
            
            # 7. Temporal filtering (prefer contours near last position)
            temporal_score = 1.0
            if self.last_car_position:
                last_x, last_y = self.last_car_position
                dist_from_last = np.sqrt((cx - last_x)**2 + (cy - last_y)**2)
                if dist_from_last > 100:  # Too far from last position, likely false positive
                    temporal_score = 0.3
                else:
                    temporal_score = 1.0 - (dist_from_last / 100)
            
            # Combined score
            score = position_score * 0.4 + center_score * 0.4 + temporal_score * 0.2
            
            if score > best_car_score:
                best_car_score = score
                best_car_candidate = (x, y, w, h)
        
        # Update position memory for next frame
        if best_car_candidate:
            x, y, w, h = best_car_candidate
            self.last_car_position = (x + w // 2, y + h // 2)
            return best_car_candidate
        
        return None
    
    def _is_mostly_in_road(self, road_mask: np.ndarray, x: int, y: int, w: int, h: int) -> bool:
        """Check if contour is mostly within road area (not on boundary markers)."""
        # Extract region
        y1, y2 = max(0, y), min(road_mask.shape[0], y + h)
        x1, x2 = max(0, x), min(road_mask.shape[1], x + w)
        
        if y2 - y1 < 5 or x2 - x1 < 5:
            return False
        
        region = road_mask[y1:y2, x1:x2]
        total_pixels = (y2 - y1) * (x2 - x1)
        road_pixels = np.count_nonzero(region)
        
        # Must be at least 40% within road area
        return road_pixels / total_pixels >= 0.4
    
    def _draw_boundaries(self, display: np.ndarray, left: int, center: int, right: int, 
                        height: int, road_mask: np.ndarray) -> None:
        """Draw boundary lines on display frame."""
        # Road overlay (light green tint)
        overlay = display.copy()
        overlay[road_mask > 0] = [0, 200, 0]
        cv2.addWeighted(overlay, 0.1, display, 0.9, 0, display)
        
        # Boundary lines
        line_thickness = 3
        edge_color = (0, 255, 0)  # Green for edges
        center_color = (255, 0, 0)  # Blue for center
        
        # Left boundary
        cv2.line(display, (left, 0), (left, height), edge_color, line_thickness)
        
        # Right boundary
        cv2.line(display, (right, 0), (right, height), edge_color, line_thickness)
        
        # Center line (dashed)
        dash_length = 20
        gap_length = 10
        for y in range(0, height, dash_length + gap_length):
            y_end = min(y + dash_length, height)
            cv2.line(display, (center, y), (center, y_end), center_color, 2)
    
    def _draw_car_bbox(self, display: np.ndarray, bbox: Tuple[int, int, int, int]) -> None:
        """Draw car bounding box on display frame."""
        x, y, w, h = bbox
        color = (0, 165, 255)  # Orange
        thickness = 2
        
        # Draw rectangle
        cv2.rectangle(display, (x, y), (x + w, y + h), color, thickness)
        
        # Draw center point
        cx, cy = x + w // 2, y + h // 2
        cv2.circle(display, (cx, cy), 4, color, -1)


def run_live_demo(camera_index: int = 0, width: int = 640, height: int = 480, fps: int = 30):
    """
    Run live boundary and car detection demo.
    Press 'q' to quit, 's' to save frame.
    """
    import sys
    
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
    
    detector = LiveBoundaryDetector(width, height)
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
            
            # Detect boundaries and car
            left, center, right, road_mask, car_bbox, display = detector.detect_boundaries_and_car(frame)
            
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
    run_live_demo()
