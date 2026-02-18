from __future__ import annotations

import math
from dataclasses import dataclass
from typing import List, Tuple, Optional

import numpy as np

from .commands import ControlVector, clamp


@dataclass
class RayResult:
    angle_deg: int
    distance: int


class BoundaryDetector:
    def __init__(
        self,
        black_threshold: int,
        ray_angles_deg: List[int],
        ray_max_length: int,
        evasive_distance: int,
        default_speed: int,
        steering_limit: int,
        light_on: bool,
    ) -> None:
        self.black_threshold = black_threshold
        self.ray_angles_deg = ray_angles_deg
        self.ray_max_length = ray_max_length
        self.evasive_distance = evasive_distance
        self.default_speed = default_speed
        self.steering_limit = steering_limit
        self.light_on = light_on
        self._last_heading = 0.0

    def _heading_from_movement(self, movement: Tuple[int, int]) -> float:
        dx, dy = movement
        if dx == 0 and dy == 0:
            return self._last_heading
        self._last_heading = math.degrees(math.atan2(dy, dx))
        return self._last_heading

    def _cast_ray(self, gray: np.ndarray, origin: Tuple[int, int], angle_deg: float) -> int:
        height, width = gray.shape
        angle_rad = math.radians(angle_deg)
        ox, oy = origin
        for dist in range(1, self.ray_max_length + 1):
            x = int(ox + dist * math.cos(angle_rad))
            y = int(oy + dist * math.sin(angle_rad))
            if x < 0 or x >= width or y < 0 or y >= height:
                return dist
            if gray[y, x] < self.black_threshold:
                return dist
        return self.ray_max_length

    def detect_road_edges(self, frame: np.ndarray) -> Tuple[Optional[int], Optional[int], Optional[int], Optional[np.ndarray]]:
        """
        Detect road edges (red/white markers) for curved roads using advanced color detection.
        Returns: (left_edge_x, center_x, right_edge_x, road_mask)
        road_mask is a binary mask showing the detected road region.
        
        Professional algorithm for reliable boundary detection on racing tracks.
        """
        if frame.ndim != 3 or frame.shape[2] != 3:
            return None, None, None, None
        
        import cv2
        
        height, width = frame.shape[:2]
        
        # Convert to HSV for robust color detection
        hsv = cv2.cvtColor(frame, cv2.COLOR_BGR2HSV)
        
        # ===== OPTIMIZED WHITE DETECTION =====
        # Track has white markers - focus detection on white only
        # Range 1: Very bright white (minimal saturation, maximum brightness)
        white_mask1 = cv2.inRange(hsv, np.array([0, 0, 180]), np.array([180, 30, 255]))  # Bright white
        # Range 2: Medium brightness white (low saturation, medium-high value)
        white_mask2 = cv2.inRange(hsv, np.array([0, 0, 120]), np.array([180, 60, 200]))  # Light gray-white
        # Range 3: Slightly tinted white (minimal saturation, medium value)
        white_mask3 = cv2.inRange(hsv, np.array([0, 0, 100]), np.array([180, 80, 180]))  # Pale white-gray
        
        # Combine all white detection ranges for comprehensive coverage
        edge_mask = cv2.bitwise_or(cv2.bitwise_or(white_mask1, white_mask2), white_mask3)
        
        # ===== MORPHOLOGICAL CLEANING =====
        kernel_small = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (3, 3))
        kernel_medium = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (5, 5))
        
        # Remove noise (open operation)
        edge_mask = cv2.morphologyEx(edge_mask, cv2.MORPH_OPEN, kernel_small, iterations=1)
        
        # Fill small holes (close operation)
        edge_mask = cv2.morphologyEx(edge_mask, cv2.MORPH_CLOSE, kernel_medium, iterations=2)
        
        # Dilate to enhance, connect boundary parts, and fill gaps between white markers
        edge_mask = cv2.dilate(edge_mask, kernel_medium, iterations=3)
        
        # ===== FIND CONTOURS =====
        contours, _ = cv2.findContours(edge_mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
        
        if len(contours) == 0:
            return None, None, None, None
        
        # ===== HANDLE CIRCULAR TRACKS WITH INNER AND OUTER BOUNDARIES =====
        # For circular/round tracks:
        # - There are TWO boundaries: inner (donut hole) and outer (track edge)
        # - The road is between these two boundaries
        # - Find leftmost/rightmost points on BOTH to define the track
        
        # Sort contours by area (largest first)
        contours_sorted = sorted(contours, key=cv2.contourArea, reverse=True)
        
        # We need at least 2 significant contours for a circular track
        if len(contours_sorted) >= 2:
            # Get the two largest contours (outer and inner boundaries)
            outer_contour = contours_sorted[0]
            inner_contour = contours_sorted[1]
            
            outer_area = cv2.contourArea(outer_contour)
            inner_area = cv2.contourArea(inner_contour)
            
            # Verify they're reasonably sized
            if outer_area > width * height * 0.01 and inner_area > width * height * 0.01:
                try:
                    # Get bounds for outer boundary
                    outer_left = int(outer_contour[:, :, 0].min())
                    outer_right = int(outer_contour[:, :, 0].max())
                    
                    # Get bounds for inner boundary
                    inner_left = int(inner_contour[:, :, 0].min())
                    inner_right = int(inner_contour[:, :, 0].max())
                    
                    # For a circular track, the road is between inner and outer
                    # Left edge: use the outer left (where road starts)
                    # Right edge: use the outer right (where road ends)
                    # Center: between inner right and outer right (on the road)
                    
                    left_edge_x = outer_left
                    right_edge_x = outer_right
                    
                    # Center should be roughly in the middle of the road width
                    # For this track: inner boundary is at center, outer at edges
                    road_center_left = (outer_left + inner_left) // 2  # Between outer-left and inner-left
                    road_center_right = (inner_right + outer_right) // 2  # Between inner-right and outer-right
                    
                    # Use the right side center as the main centerline
                    center_x = road_center_right
                    
                    # Verify reasonable separation
                    min_separation = max(50, width * 0.15)
                    if right_edge_x - left_edge_x >= min_separation:
                        # Create road mask between the boundaries
                        road_mask = np.zeros((height, width), dtype=np.uint8)
                        cv2.rectangle(road_mask, (inner_left, 0), (outer_right, height), 255, -1)
                        
                        return left_edge_x, center_x, right_edge_x, road_mask
                except:
                    pass  # Fall through to fallback
        
        # ===== FALLBACK: SINGLE LARGE CONTOUR =====
        # If only one large contour (complete ring), extract extremes
        largest_contour = max(contours, key=cv2.contourArea)
        largest_area = cv2.contourArea(largest_contour)
        
        if largest_area > width * height * 0.05:
            try:
                left_edge_x = int(largest_contour[:, :, 0].min())
                right_edge_x = int(largest_contour[:, :, 0].max())
                
                min_separation = max(50, width * 0.15)
                if right_edge_x - left_edge_x >= min_separation:
                    center_x = (left_edge_x + right_edge_x) // 2
                    
                    road_mask = np.zeros((height, width), dtype=np.uint8)
                    cv2.rectangle(road_mask, (left_edge_x, 0), (right_edge_x, height), 255, -1)
                    
                    return left_edge_x, center_x, right_edge_x, road_mask
            except:
                pass
        
        # ===== FALLBACK: IDENTIFY SEPARATE LEFT AND RIGHT EDGES =====
        edge_candidates = []
        
        for contour in contours:
            area = cv2.contourArea(contour)
            
            if area < 50:
                continue
            
            M = cv2.moments(contour)
            if M["m00"] <= 0:
                continue
            
            cx = int(M["m10"] / M["m00"])
            
            if area < width * height * 0.3:
                edge_candidates.append((cx, area, contour))
        
        if len(edge_candidates) < 2:
            return None, None, None, None
        
        edge_candidates.sort(key=lambda x: x[0])
        
        left_edge_x = edge_candidates[0][0]
        right_edge_x = edge_candidates[-1][0]
        
        min_separation = max(50, width * 0.15)
        if right_edge_x - left_edge_x < min_separation:
            return None, None, None, None
        
        road_mask = np.zeros((height, width), dtype=np.uint8)
        cv2.rectangle(road_mask, (left_edge_x, 0), (right_edge_x, height), 255, -1)
        
        center_x = (left_edge_x + right_edge_x) // 2
        
        return left_edge_x, center_x, right_edge_x, road_mask

    def analyze(self, frame: np.ndarray, center: Tuple[int, int], movement: Tuple[int, int]) -> Tuple[List[RayResult], ControlVector]:
        gray = frame
        if frame.ndim == 3:
            gray = np.dot(frame[..., :3], [0.114, 0.587, 0.299]).astype(np.uint8)
        height, width = gray.shape
        heading = self._heading_from_movement(movement)
        rays = []
        for angle in self.ray_angles_deg:
            distance = self._cast_ray(gray, center, heading + angle)
            rays.append(RayResult(angle_deg=angle, distance=distance))

        if frame.ndim == 3:
            import cv2

            # First try to detect road edges (red/white markers)
            left_edge, road_center, right_edge, road_mask = self.detect_road_edges(frame)
            
            if left_edge is not None and right_edge is not None and road_mask is not None:
                # Road edges detected: use them for guidance
                error = center[0] - road_center
                tolerance = 20
                
                steer_mag = int(
                    min(
                        self.steering_limit,
                        abs(error) / max(1, (right_edge - left_edge) / 2) * self.steering_limit,
                    )
                )
                left_turn = 0
                right_turn = 0
                if error < -tolerance:
                    right_turn = max(1, steer_mag)
                elif error > tolerance:
                    left_turn = max(1, steer_mag)

                speed = self.default_speed
                if abs(error) > (right_edge - left_edge) * 0.3:
                    speed = max(5, self.default_speed - 10)

                control = ControlVector(
                    light_on=self.light_on,
                    speed=clamp(speed, 0, 255),
                    right_turn_value=clamp(right_turn, 0, 255),
                    left_turn_value=clamp(left_turn, 0, 255),
                )
                return rays, control
            
            # Fallback: try red/white lane detection
            hsv = cv2.cvtColor(frame, cv2.COLOR_BGR2HSV)
            red_mask1 = cv2.inRange(hsv, (0, 120, 50), (10, 255, 255))
            red_mask2 = cv2.inRange(hsv, (170, 120, 50), (180, 255, 255))
            red_mask = cv2.bitwise_or(red_mask1, red_mask2)
            white_mask = cv2.inRange(hsv, (0, 0, 200), (180, 40, 255))

            band_y = int(height * 0.6)
            band = slice(max(0, band_y - 5), min(height, band_y + 5))
            red_xs = np.where(red_mask[band, :] > 0)[1]
            white_xs = np.where(white_mask[band, :] > 0)[1]

            if red_xs.size > 0 and white_xs.size > 0:
                red_x = int(np.mean(red_xs))
                white_x = int(np.mean(white_xs))
                lane_center_x = int((red_x + white_x) / 2)
                error = center[0] - lane_center_x
                tolerance = 15

                steer_mag = int(
                    min(
                        self.steering_limit,
                        abs(error) / max(1, width / 2) * self.steering_limit * 1.5,
                    )
                )
                left_turn = 0
                right_turn = 0
                if error < -tolerance:
                    right_turn = max(1, steer_mag)
                elif error > tolerance:
                    left_turn = max(1, steer_mag)

                speed = self.default_speed
                if abs(error) > width * 0.25:
                    speed = max(5, self.default_speed - 10)

                control = ControlVector(
                    light_on=self.light_on,
                    speed=clamp(speed, 0, 255),
                    right_turn_value=clamp(right_turn, 0, 255),
                    left_turn_value=clamp(left_turn, 0, 255),
                )
                return rays, control
        
        min_distance = min(r.distance for r in rays)
        left_distance = rays[0].distance
        center_distance = rays[1].distance
        right_distance = rays[2].distance
        
        # Avoid continuous spinning when surrounded by obstacles
        # If all rays are very short, reduce speed and try to escape
        speed = self.default_speed
        left_turn = 0
        right_turn = 0
        
        # If ALL rays are extremely short, the car is completely surrounded
        # This happens when standing still on very dark surface
        if max(left_distance, center_distance, right_distance) < 10:
            # Car is stuck/surrounded - move slowly forward
            speed = max(2, self.default_speed // 2)
            # Slight turn to try to break free
            if left_distance > right_distance:
                left_turn = self.steering_limit // 2
            else:
                right_turn = self.steering_limit // 2
        # Normal boundary avoidance
        elif min_distance < self.evasive_distance:
            # Emergency: boundary detected, reduce speed
            speed = max(5, self.default_speed - 15)
            
            # Steer away from the closest boundary
            if left_distance > center_distance and left_distance > right_distance:
                # Turn left (farthest distance is left)
                left_turn = self.steering_limit
            elif right_distance > center_distance and right_distance > left_distance:
                # Turn right (farthest distance is right)
                right_turn = self.steering_limit
            elif center_distance < self.evasive_distance:
                # Boundary ahead and close: make a stronger turn
                if left_distance > right_distance:
                    left_turn = self.steering_limit
                else:
                    right_turn = self.steering_limit
        else:
            # No immediate boundary: steer gently towards wider path
            if left_distance > right_distance + 30:
                left_turn = max(0, self.steering_limit // 3)
            elif right_distance > left_distance + 30:
                right_turn = max(0, self.steering_limit // 3)
        
        control = ControlVector(
            light_on=self.light_on,
            speed=clamp(speed, 0, 255),
            right_turn_value=clamp(right_turn, 0, 255),
            left_turn_value=clamp(left_turn, 0, 255),
        )
        return rays, control
