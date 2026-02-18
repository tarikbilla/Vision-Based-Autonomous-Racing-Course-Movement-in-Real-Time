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
        Detect road edges for circular track using adaptive threshold.
        Returns: (left_edge_x, center_x, right_edge_x, road_mask)
        """
        if frame.ndim != 3 or frame.shape[2] != 3:
            return None, None, None, None
        
        import cv2
        
        height, width = frame.shape[:2]
        
        # Convert to grayscale
        gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
        blurred = cv2.GaussianBlur(gray, (5, 5), 0)
        
        # Adaptive threshold
        binary = cv2.adaptiveThreshold(
            blurred, 255,
            cv2.ADAPTIVE_THRESH_GAUSSIAN_C,
            cv2.THRESH_BINARY_INV,
            11, 2
        )
        
        # Minimal morphology to remove noise
        kernel = np.ones((2, 2), np.uint8)
        binary = cv2.morphologyEx(binary, cv2.MORPH_OPEN, kernel, iterations=1)
        binary = cv2.morphologyEx(binary, cv2.MORPH_CLOSE, kernel, iterations=1)
        
        # Find contours
        contours, _ = cv2.findContours(binary, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
        
        if len(contours) == 0:
            return None, None, None, None
        
        # Sort by area and keep large contours
        contours_sorted = sorted(contours, key=cv2.contourArea, reverse=True)
        large_contours = [(cv2.contourArea(c), c) for c in contours_sorted if cv2.contourArea(c) >= 700]
        
        if len(large_contours) < 2:
            return None, None, None, None
        
        # Get top 10 contours
        boundary_contours = large_contours[:10]
        
        # Get outer and inner boundaries (largest two)
        outer_area, outer_boundary = boundary_contours[0]
        inner_area, inner_boundary = boundary_contours[1]
        
        try:
            # Get extremes from outer boundary
            left_edge_x = int(outer_boundary[:, :, 0].min())
            right_edge_x = int(outer_boundary[:, :, 0].max())
            
            # Create masks
            outer_mask = np.zeros((height, width), dtype=np.uint8)
            inner_mask = np.zeros((height, width), dtype=np.uint8)
            cv2.drawContours(outer_mask, [outer_boundary], 0, 255, -1)
            cv2.drawContours(inner_mask, [inner_boundary], 0, 255, -1)
            
            # Road area (between boundaries)
            road_mask = cv2.bitwise_xor(outer_mask, inner_mask)
            
            # Calculate center by finding midpoint of road at middle row
            mid_y = height // 2
            row = road_mask[mid_y, :]
            if np.any(row):
                indices = np.where(row > 0)[0]
                if len(indices) > 0:
                    center_x = (indices[0] + indices[-1]) // 2
                else:
                    center_x = (left_edge_x + right_edge_x) // 2
            else:
                center_x = (left_edge_x + right_edge_x) // 2
            
            # Verify reasonable separation
            min_separation = max(50, width * 0.15)
            if right_edge_x - left_edge_x >= min_separation:
                return left_edge_x, center_x, right_edge_x, road_mask
        except:
            pass
        
        return None, None, None, None
        
        return None, None, None, None

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
