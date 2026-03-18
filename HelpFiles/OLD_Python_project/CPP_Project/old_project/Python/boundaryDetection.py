import math
from random import randint
import numpy as np

BLACK_THRESHOLD = 50  # Threshold for detecting black pixels (0 to 255)

class Car:
    def __init__(self, position, speed, max_angle):
        self.pos = position
        self.dir = 0
        self.olddir = 0
        self.speed = speed
        self.max_angle = max_angle
        self.decision_counter = 0

        self.rays = []
        for angle in range(-60, 70, 60):  # Three rays (-60, 0, +60 degrees)
            ray = Ray(self.pos, angle, 200)
            self.rays.append(ray)

    def update(self, track):
        self.update_position()
        self.update_rays(track)
        self.update_guidance()

    def update_real(self, last_movement, position, track):
        """Update car direction based on real movement vector."""
        dx, dy = last_movement
        if dx == 0 and dy == 0:
            return  # No movement, keep the current direction
        self.olddir = self.dir
        self.dir = math.degrees(math.atan2(dy, dx))  # Convert to degrees
        self.pos = position

        self.update_rays(track)
        self.update_guidance()


    def get_boundary_data(self, car):
        """Returns an array [1, speed, left_value, right_value]"""
        
        # Calculate the difference in angles (delta_angle)
        
        # Calculate the right and left values based on the angle difference
        if self.dir > 0:  # Right turn
            right_value = int(np.interp(self.dir, [0, 60], [0, 255]))
            left_value = 0
        elif self.dir < 0:  # Left turn
            left_value = int(np.interp(self.dir, [0, -60], [0, 255]))
            right_value = 0
        else:
            # No change in direction (straight)
            right_value = 0
            left_value = 0
        
        # Return the result as an array
        return [1, car.speed, left_value, right_value]


    def update_position(self):
        self.pos = self.get_position(self.dir, self.speed)

    def update_rays(self, track):
        for ray in self.rays:
            ray.update(self.pos, self.dir, track)

    def update_guidance(self):
        min_ray, max_ray = self.find_min_max_rays()
        if self.rays[min_ray].distance < 80:  # If too close to a boundary
            self.evasive_action(max_ray)
        else:
            self.decision_counter_check()

    def find_min_max_rays(self):
        max_distance, max_index = 0, 0
        min_distance, min_index = self.rays[0].distance, 0
        for index, ray in enumerate(self.rays):
            if ray.distance > max_distance:
                max_distance, max_index = ray.distance, index
            if ray.distance < min_distance:
                min_distance, min_index = ray.distance, index
        return min_index, max_index

    def evasive_action(self, max_ray):
        if max_ray == 0:
            self.dir = randint(-self.max_angle, 0)
        elif max_ray == 1:
            if self.rays[0].distance > self.rays[2].distance:
                self.dir = randint(-self.max_angle, 0)
            else:
                self.dir = randint(0, self.max_angle)
        else:
            self.dir = randint(0, self.max_angle)

    def decision_counter_check(self):
        if self.decision_counter >= 10:
            self.dir += randint(-self.max_angle, self.max_angle)
            self.decision_counter = 0
        else:
            self.decision_counter += 1

    def get_position(self, base_angle, length):
        angle = math.radians(base_angle)
        x = self.pos[0] + length * math.cos(angle)
        y = self.pos[1] + length * math.sin(angle)
        return (x, y)

class Ray:
    def __init__(self, position, angle, max_length):
        self.pos = position
        self.init_angle = angle
        self.length = max_length
        self.dir = None
        self.terminus = None
        self.distance = self.length

    def update(self, point, direction, track):
        self.pos = point
        self.update_direction(direction)
        self.update_terminus(track)

    def update_direction(self, direction):
        a = self.init_angle + direction
        angle = math.radians(a)
        self.dir = (math.cos(angle), math.sin(angle))

    def update_terminus(self, track):
        distance = self.length
        terminus = None
        for i in range(self.length):
            if (i > 20):    #to avoid detection of own Widows as boundary
                test_x = int(self.pos[0] + self.dir[0] * i)
                test_y = int(self.pos[1] + self.dir[1] * i)
                if 0 <= test_x < track.get_width() and 0 <= test_y < track.get_height():
                    # Check for dark pixels (boundary detection)
                    pixel_color = track.get_at((test_x, test_y))[:3]  # Ignore alpha channel
                    gray_value = sum(pixel_color) // 3  # Convert to grayscale
                    if gray_value < BLACK_THRESHOLD:  # If grayscale value is below threshold, consider it part of the boundary
                        distance = i
                        terminus = (test_x, test_y)
                        break
        self.distance = distance
        self.terminus = terminus if terminus else (
            self.pos[0] + self.dir[0] * self.length,
            self.pos[1] + self.dir[1] * self.length,
        )