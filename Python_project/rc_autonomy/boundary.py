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
        Detect road edges (red/white markers) for curved roads.
        Returns: (left_edge_x, center_x, right_edge_x, road_mask)
        road_mask is a binary mask showing the detected road region.
        """
        if frame.ndim != 3:
            return None, None, None, None
        
        import cv2
        
        height, width = frame.shape[:2]
        hsv = cv2.cvtColor(frame, cv2.COLOR_BGR2HSV)
        
        # Detect RED and WHITE markers (road boundaries)
        # Red has two ranges in HSV
        red_mask1 = cv2.inRange(hsv, (0, 80, 60), (15, 255, 255))      # Lower reds
        red_mask2 = cv2.inRange(hsv, (165, 80, 60), (180, 255, 255))   # Upper reds
        red_mask = cv2.bitwise_or(red_mask1, red_mask2)
        
        # White: high V, low S
        white_mask = cv2.inRange(hsv, (0, 0, 150), (180, 50, 255))
        
        # Combine edge markers
        edge_mask = cv2.bitwise_or(red_mask, white_mask)
        
        # Morphological operations to clean up
        kernel = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (5, 5))
        edge_mask = cv2.morphologyEx(edge_mask, cv2.MORPH_CLOSE, kernel, iterations=1)
        edge_mask = cv2.morphologyEx(edge_mask, cv2.MORPH_OPEN, kernel, iterations=1)
        
        # Find contours to identify road boundaries
        contours, _ = cv2.findContours(edge_mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
        
        if len(contours) < 2:
            return None, None, None, None
        
        # Find leftmost and rightmost contour centers
        left_most_x = width
        right_most_x = 0
        road_x_positions = []
        
        for contour in contours:
            area = cv2.contourArea(contour)
            if area < 50:  # Ignore tiny noise
                continue
            
            M = cv2.moments(contour)
            if M["m00"] > 0:
                cx = int(M["m10"] / M["m00"])
                road_x_positions.append(cx)
                left_most_x = min(left_most_x, cx)
                right_most_x = max(right_most_x, cx)
        
        if len(road_x_positions) < 2:
            return None, None, None, None
        
        # Create a binary mask of the detected road area
        # This marks all pixels between the detected edges
        road_mask = np.zeros((height, width), dtype=np.uint8)
        
        # Fill region between detected edges as "road area"
        if left_most_x < right_most_x:
            road_mask[:, left_most_x:right_most_x] = 255
        
        center_x = (left_most_x + right_most_x) // 2
        
        return left_most_x, center_x, right_most_x, road_mask

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
