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

    def detect_road_edges(self, frame: np.ndarray) -> Tuple[Optional[int], Optional[int], Optional[int]]:
        """Detect road edges (red/white markers) and return left edge x, center x, right edge x."""
        if frame.ndim != 3:
            return None, None, None
        
        import cv2
        
        height, width = frame.shape[:2]
        hsv = cv2.cvtColor(frame, cv2.COLOR_BGR2HSV)
        
        # Detect red and white markers
        red_mask1 = cv2.inRange(hsv, (0, 100, 50), (10, 255, 255))
        red_mask2 = cv2.inRange(hsv, (170, 100, 50), (180, 255, 255))
        red_mask = cv2.bitwise_or(red_mask1, red_mask2)
        white_mask = cv2.inRange(hsv, (0, 0, 180), (180, 30, 255))
        
        edge_mask = cv2.bitwise_or(red_mask, white_mask)
        
        # Look at lower half of frame for road edges
        band_start = int(height * 0.5)
        band = edge_mask[band_start:, :]
        
        # Find left and right edges
        edge_xs = np.where(np.any(band > 0, axis=0))[0]
        if len(edge_xs) < 2:
            return None, None, None
        
        left_edge = int(np.percentile(edge_xs, 10))
        right_edge = int(np.percentile(edge_xs, 90))
        center_x = (left_edge + right_edge) // 2
        
        return left_edge, center_x, right_edge

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
            left_edge, road_center, right_edge = self.detect_road_edges(frame)
            
            if left_edge is not None and right_edge is not None:
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
        
        # Always drive at default speed
        speed = self.default_speed
        left_turn = 0
        right_turn = 0
        
        # Steer towards the farthest direction (away from boundaries)
        if min_distance < self.evasive_distance:
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
