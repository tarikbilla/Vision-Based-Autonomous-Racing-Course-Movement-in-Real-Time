import pygame
import math
from random import randint

# Constants
TRACK_IMAGE_PATH = "Simulation/Image2.jpg"  # Replace with your track image
CAR_IMAGE_PATH = "Simulation/car.png"  # Replace with your car image
BLACK_THRESHOLD = 50  # Threshold for detecting black pixels (0 to 255)
DOT_COLOR = (255, 255, 255)  # Color of the white dot (white)

class Car:
    def __init__(self, position, direction, speed, max_angle):
        self.pos = position
        self.dir = direction
        self.speed = speed
        self.max_angle = max_angle
        self.decision_counter = 0

        self.rays = []
        for angle in range(-60, 70, 60):  # Three rays (-60, 0, +60 degrees)
            ray = Ray(self.pos, angle, 200)
            self.rays.append(ray)

    def update(self, track, screen):
        self.update_position()
        self.update_rays(track)
        self.update_guidance()

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
            self.dir += randint(-self.max_angle, 0)
        elif max_ray == 1:
            if self.rays[0].distance > self.rays[2].distance:
                self.dir += randint(-self.max_angle, 0)
            else:
                self.dir += randint(0, self.max_angle)
        else:
            self.dir += randint(0, self.max_angle)

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

def find_white_dot_position(track):
    """Find the position of the white dot on the track."""
    for x in range(track.get_width()):
        for y in range(track.get_height()):
            pixel_color = track.get_at((x, y))[:3]  # Ignore alpha channel
            # Check for white color with slight tolerance for other colors
            if all(abs(pixel_color[i] - DOT_COLOR[i]) < 10 for i in range(3)):
                return x, y
    return None

def resize_image_to_fit(image, max_width, max_height):
    """Resize the image to fit within the given width and height while maintaining the aspect ratio."""
    image_width, image_height = image.get_width(), image.get_height()
    aspect_ratio = image_width / image_height

    if image_width > max_width or image_height > max_height:
        if image_width / max_width > image_height / max_height:
            new_width = max_width
            new_height = int(max_width / aspect_ratio)
        else:
            new_height = max_height
            new_width = int(max_height * aspect_ratio)
        
        image = pygame.transform.scale(image, (new_width, new_height))

    return image

def main():
    pygame.init()
    screen_width, screen_height = 800, 600  # Screen dimensions
    screen = pygame.display.set_mode((screen_width, screen_height))
    clock = pygame.time.Clock()

    # Load track
    try:
        track = pygame.image.load(TRACK_IMAGE_PATH).convert()
        print("Track loaded successfully.")
    except pygame.error as e:
        print(f"Error loading track: {e}")
        return

    # Resize track image to fit the screen
    track = resize_image_to_fit(track, screen_width, screen_height)

    screen = pygame.display.set_mode((track.get_width(), track.get_height()))
    pygame.display.set_caption("Autonomous Car with Boundary Detection")

    # Load car
    try:
        car_image = pygame.image.load(CAR_IMAGE_PATH).convert_alpha()
        car_image = pygame.transform.scale(car_image, (42, 20))
    except pygame.error as e:
        print(f"Error loading car image: {e}")
        return

    # Try to find the white dot position
    dot_position = find_white_dot_position(track)
    if dot_position is None:
        print("White dot not found on the track. Using default starting position.")
        dot_position = (200, 200)  # Use a default starting position for the car
    car = Car(dot_position, 90, 2, 6)

    running = True
    while running:
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                running = False

        # Update car
        car.update(track, screen)

        # Render
        screen.blit(track, (0, 0))
        car_rotated = pygame.transform.rotate(car_image, -car.dir)
        car_rect = car_rotated.get_rect(center=car.pos)
        screen.blit(car_rotated, car_rect)

        # Draw rays
        for ray in car.rays:
            pygame.draw.line(screen, (255, 0, 0), ray.pos, ray.terminus, 1)

        pygame.display.flip()
        clock.tick(60)

    pygame.quit()

if __name__ == "__main__":
    main()
