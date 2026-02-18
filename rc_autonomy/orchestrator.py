from __future__ import annotations

import threading
import time
from dataclasses import dataclass
from queue import Queue, Empty
from typing import Optional

import numpy as np

from .ble import BLEClient
from .boundary import BoundaryDetector
from .camera import CameraCapture, Frame, SimulatedCamera
from .commands import ControlVector
from .tracking import OpenCVTracker, SimulatedTracker, TrackedObject


@dataclass
class OrchestratorOptions:
    show_window: bool
    command_rate_hz: int
    color_tracking: bool = False


class ControlOrchestrator:
    def __init__(
        self,
        camera,
        tracker,
        boundary: BoundaryDetector,
        ble: BLEClient,
        options: OrchestratorOptions,
    ) -> None:
        self.camera = camera
        self.tracker = tracker
        self.boundary = boundary
        self.ble = ble
        self.options = options
        self.frame_queue: Queue[Frame] = Queue(maxsize=5)
        self.render_queue: Queue[tuple] = Queue(maxsize=1)
        self.stop_event = threading.Event()
        self.latest_control = ControlVector(light_on=True, speed=0, right_turn_value=0, left_turn_value=0)
        self.latest_heading = 0.0
        self._threads: list[threading.Thread] = []
        self.roi_selected = False
        self.selected_roi: Optional[tuple[int, int, int, int]] = None
        self.tracker_ready = threading.Event()
        self.color_tracking_enabled = self.options.color_tracking
        self._last_center: Optional[tuple[int, int]] = None
        self._latest_frame: Optional[np.ndarray] = None
        self._latest_frame_lock = threading.Lock()
        self._road_mask: Optional[np.ndarray] = None  # Cache road mask for car detection

    def start(self) -> None:
        self.stop_event.clear()
        self.roi_selected = False
        self.tracker_ready.clear()
        if self.options.show_window:
            self._select_roi_live()
        self._threads = [
            threading.Thread(target=self._camera_loop, daemon=True),
            threading.Thread(target=self._tracking_loop, daemon=True),
            threading.Thread(target=self._ble_loop, daemon=True),
        ]
        for thread in self._threads:
            thread.start()
        if self.options.show_window:
            self._ui_loop()

    def stop(self) -> None:
        self.stop_event.set()
        self.camera.close()
        for thread in self._threads:
            thread.join(timeout=2)

    def _camera_loop(self) -> None:
        try:
            for frame in self.camera.frames():
                if self.stop_event.is_set():
                    break
                with self._latest_frame_lock:
                    self._latest_frame = frame.image.copy()
                try:
                    self.frame_queue.put(frame, timeout=0.1)
                except Exception:
                    continue
        except Exception:
            self.stop_event.set()
            self.latest_control = ControlVector(light_on=True, speed=0, right_turn_value=0, left_turn_value=0)

    def _tracking_loop(self) -> None:
        frame_count = 0
        last_processed = 0.0
        analysis_interval = 0.15  # ~6-7 FPS analysis rate
        self._road_mask = None  # Cache road mask
        while not self.stop_event.is_set():
            if not self.tracker_ready.is_set():
                time.sleep(0.05)
                continue
            try:
                frame = self.frame_queue.get(timeout=0.2)
            except Empty:
                continue
            now = time.time()
            if now - last_processed < analysis_interval:
                continue
            last_processed = now
            image = frame.image
            
            # Get road mask for constrained car detection
            _, _, _, road_mask = self.boundary.detect_road_edges(image)
            self._road_mask = road_mask
            
            try:
                tracked = self.tracker.update(image)
            except Exception as exc:
                if self.color_tracking_enabled:
                    # Detect car constrained to road region
                    detected = self._detect_red_car(image, self._road_mask)
                    if detected is None:
                        print(f"[!] Tracking error: {exc}")
                        self.latest_control = ControlVector(light_on=True, speed=0, right_turn_value=0, left_turn_value=0)
                        continue
                    tracked = detected
                else:
                    print(f"[!] Tracking error: {exc}")
                    self.latest_control = ControlVector(light_on=True, speed=0, right_turn_value=0, left_turn_value=0)
                    continue

            rays, control = self.boundary.analyze(image, tracked.center, tracked.movement)
            self.latest_control = control
            self.latest_heading = self.boundary._heading_from_movement(tracked.movement)
            frame_count += 1
            if frame_count % 30 == 0:  # Log every 30 frames (approx 1/sec @ 30fps)
                print(f"[{frame_count}] Center: {tracked.center} | "
                      f"Speed: {control.speed:3d} | "
                      f"Left: {control.left_turn_value:2d} | "
                      f"Right: {control.right_turn_value:2d} | "
                      f"Rays: L={rays[0].distance:3d} C={rays[1].distance:3d} R={rays[2].distance:3d}")
            # Queue render data for main thread
            try:
                self.render_queue.put_nowait((image.copy(), tracked, rays))
            except Exception:
                pass

    def _ble_loop(self) -> None:
        interval = 1.0 / max(1, self.options.command_rate_hz)
        while not self.stop_event.is_set():
            self.ble.send_control(self.latest_control)
            time.sleep(interval)

    def _render(self, image: np.ndarray, tracked: TrackedObject, rays) -> None:
        import cv2

        from .ui import draw_overlay

        # Create main visualization frame
        display = image.copy()
        height, width = image.shape[:2]
        
        # ===== DETECT AND DRAW ROAD BOUNDARIES =====
        left_edge, road_center, right_edge, road_mask = self.boundary.detect_road_edges(image)
        
        if left_edge is not None and right_edge is not None:
            # ===== DRAW ROAD REGION OVERLAY =====
            if road_mask is not None:
                # Create green overlay for road area
                overlay = display.copy()
                overlay[road_mask > 0] = [0, 200, 0]  # Green tint
                cv2.addWeighted(overlay, 0.15, display, 0.85, 0, display)
            
            # ===== DRAW BOUNDARY LINES =====
            line_thickness = 4
            edge_color = (0, 255, 0)  # Green for edges
            center_color = (255, 0, 0)  # Blue for centerline
            
            # Draw left boundary (full height)
            cv2.line(display, (left_edge, 0), (left_edge, height), edge_color, line_thickness)
            
            # Draw right boundary (full height)
            cv2.line(display, (right_edge, 0), (right_edge, height), edge_color, line_thickness)
            
            # Draw centerline (blue, dashed effect using multiple short lines)
            dash_length = 15
            gap_length = 10
            for y in range(0, height, dash_length + gap_length):
                y_end = min(y + dash_length, height)
                cv2.line(display, (road_center, y), (road_center, y_end), center_color, 3)
            
            # ===== DRAW ROAD INFORMATION TEXT =====
            road_width = right_edge - left_edge
            info_text = f"ROAD DETECTED | L:{left_edge:3d} | C:{road_center:3d} | R:{right_edge:3d} | W:{road_width:3d}px"
            cv2.putText(display, info_text, (10, 30), cv2.FONT_HERSHEY_SIMPLEX, 
                       0.7, (255, 0, 0), 2)
            
            # Draw status indicator (green rectangle if road detected)
            cv2.rectangle(display, (5, 5), (25, 25), (0, 255, 0), 3)
            
            # ===== IF TRACKING, SHOW CAR POSITION RELATIVE TO ROAD =====
            if tracked is not None:
                car_x = tracked.center[0]
                error_from_center = car_x - road_center
                error_percent = (error_from_center / (road_width / 2)) * 100 if road_width > 0 else 0
                
                # Draw car position indicator
                cv2.circle(display, tracked.center, 10, (0, 255, 255), 2)  # Cyan circle
                
                # Draw line from car to centerline
                cv2.line(display, tracked.center, (road_center, tracked.center[1]), 
                        (200, 100, 0), 2)  # Orange line showing offset
                
                # Show alignment info
                align_text = f"Car Offset: {error_from_center:+.0f}px ({error_percent:+.1f}%)"
                cv2.putText(display, align_text, (10, 60), cv2.FONT_HERSHEY_SIMPLEX,
                           0.6, (0, 255, 255), 2)
                
                # Show warning if car is too far from center
                tolerance = road_width * 0.2
                if abs(error_from_center) > tolerance:
                    cv2.putText(display, "WARNING: Car drifting!", (10, 90), 
                               cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 0, 255), 2)
        else:
            # Road not detected - show status
            cv2.putText(display, "NO ROAD DETECTED - Searching...", (10, 30), 
                       cv2.FONT_HERSHEY_SIMPLEX, 0.8, (0, 0, 255), 2)
            cv2.rectangle(display, (5, 5), (25, 25), (0, 0, 255), 3)  # Red indicator
        
        # ===== DRAW TRACKING OVERLAY =====
        if tracked is not None and rays is not None:
            display = draw_overlay(display, tracked.bbox, tracked.center, rays, 
                                  self.latest_heading, draw_tracking=True)
            
            # Draw bounding box
            if tracked.bbox is not None:
                x, y, w, h = tracked.bbox
                cv2.rectangle(display, (x, y), (x + w, y + h), (255, 255, 0), 2)
        
        # ===== DRAW CONTROL INFORMATION =====
        control_y = height - 50
        control_info = (f"Speed: {self.latest_control.speed:3d} | "
                       f"Left: {self.latest_control.left_turn_value:2d} | "
                       f"Right: {self.latest_control.right_turn_value:2d}")
        cv2.putText(display, control_info, (10, control_y), cv2.FONT_HERSHEY_SIMPLEX,
                   0.6, (255, 255, 255), 2)
        
        # Display the result
        cv2.imshow("RC Autonomy - Road Detection", display)
        
        if cv2.waitKey(1) & 0xFF == ord("q"):
            self.stop_event.set()

    def _detect_red_car(self, image: np.ndarray, road_region: Optional[np.ndarray] = None) -> Optional[TrackedObject]:
        """Detect red car, optionally constrained to road_region."""
        import cv2

        hsv = cv2.cvtColor(image, cv2.COLOR_BGR2HSV)
        
        # Detect red (not pure red - car is darker red)
        mask1 = cv2.inRange(hsv, (0, 100, 40), (12, 255, 255))
        mask2 = cv2.inRange(hsv, (168, 100, 40), (180, 255, 255))
        mask = cv2.bitwise_or(mask1, mask2)
        
        # Clean up
        kernel = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (3, 3))
        mask = cv2.morphologyEx(mask, cv2.MORPH_OPEN, kernel, iterations=1)
        mask = cv2.morphologyEx(mask, cv2.MORPH_DILATE, kernel, iterations=1)
        
        # Constrain to road region if provided
        if road_region is not None:
            mask = cv2.bitwise_and(mask, road_region)

        contours, _ = cv2.findContours(mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
        if not contours:
            return None
        
        # Find contour with area between 40 and 5000 (car-sized, not noise or edges)
        valid_contours = [c for c in contours if 40 < cv2.contourArea(c) < 5000]
        if not valid_contours:
            return None
        
        largest = max(valid_contours, key=cv2.contourArea)
        x, y, w, h = cv2.boundingRect(largest)
        center = (x + w // 2, y + h // 2)
        
        if self._last_center is None:
            movement = (0, 0)
        else:
            movement = (center[0] - self._last_center[0], center[1] - self._last_center[1])
        self._last_center = center
        return TrackedObject(bbox=(x, y, w, h), center=center, movement=movement)

    def _close_window(self) -> None:
        import cv2

        cv2.destroyAllWindows()

    def _select_roi_live(self) -> None:
        """Show live camera and allow ROI or auto color tracking (main thread)."""
        import cv2

        print("[*] Waiting for first camera frame...")
        print("[*] Press 's' to select ROI, 'a' for auto red-car tracking, or 'q' to quit.")
        while not self.stop_event.is_set() and not self.tracker_ready.is_set():
            try:
                self.camera.open()
                frame_iter = self.camera.frames()
                for frame in frame_iter:
                    if self.stop_event.is_set() or self.tracker_ready.is_set():
                        break
                    image = frame.image
                    self._latest_frame = image.copy()
                    cv2.imshow("Camera Live", image)
                    key = cv2.waitKey(1) & 0xFF
                    if key == ord("q"):
                        self.stop_event.set()
                        break
                    if key == ord("a"):
                        self.tracker_ready.set()
                        print("[✓] Auto color tracking enabled (no ROI).")
                        break
                    if key == ord("s"):
                        frozen = image.copy()
                        cv2.imshow("ROI Select", frozen)
                        cv2.waitKey(1)
                        roi_retry_count = 0
                        while not self.stop_event.is_set() and roi_retry_count < 5:
                            roi = cv2.selectROI("ROI Select", frozen, fromCenter=False, showCrosshair=True)
                            if roi == (0, 0, 0, 0):
                                print("[*] ROI cancelled. Press 's' again to reselect or 'a' for auto tracking.")
                                cv2.destroyWindow("ROI Select")
                                break
                            x, y, w, h = roi
                            # Validate ROI: must have minimum size
                            if w < 10 or h < 10:
                                print(f"[!] ROI too small ({w}x{h}). Please select a larger area.")
                                roi_retry_count += 1
                                continue
                            self.selected_roi = (int(x), int(y), int(w), int(h))
                            print(f"[*] Selected ROI: {self.selected_roi}")
                            print("[*] Initializing tracker with selected ROI...")
                            try:
                                self.tracker.initialize(frozen, self.selected_roi)
                                self.tracker_ready.set()
                                print("[✓] Tracker initialized. Starting autonomous control...")
                                cv2.destroyWindow("ROI Select")
                                break
                            except Exception as exc:
                                roi_retry_count += 1
                                if roi_retry_count >= 5:
                                    print(f"[!] Tracker failed after {roi_retry_count} attempts. Falling back to auto color tracking.")
                                    self.color_tracking_enabled = True
                                    self.tracker_ready.set()
                                    cv2.destroyWindow("ROI Select")
                                    break
                                print(f"[!] Tracker initialization failed: {exc}")
                                print(f"[*] Please select ROI again (attempt {roi_retry_count}/5).")
                        if self.tracker_ready.is_set():
                            break
                if self.tracker_ready.is_set() or self.stop_event.is_set():
                    break
            except Exception as exc:
                print(f"[!] Camera/ROI error: {exc}")
                time.sleep(0.5)

    def _ui_loop(self) -> None:
        """Main thread UI loop for OpenCV display and ROI selection."""
        import cv2

        self.roi_selected = True
        print("[*] Tracking started.")

        while not self.stop_event.is_set():
            try:
                image, tracked, rays = self.render_queue.get(timeout=0.1)
                self._render(image, tracked, rays)
            except Empty:
                # Keep window responsive
                with self._latest_frame_lock:
                    latest = self._latest_frame.copy() if self._latest_frame is not None else None
                if latest is not None:
                    self._render(latest, None, None)
                if cv2.waitKey(1) & 0xFF == ord("q"):
                    self.stop_event.set()
            except Exception:
                pass

        self._close_window()


def build_orchestrator(simulate: bool, config, ble: BLEClient) -> ControlOrchestrator:
    if simulate:
        camera = SimulatedCamera(config.camera.width, config.camera.height, config.camera.fps)
        tracker = SimulatedTracker()
    else:
        camera = CameraCapture(config.camera.source, config.camera.width, config.camera.height, config.camera.fps)
        tracker = OpenCVTracker(config.tracking.tracker_type)
    boundary = BoundaryDetector(
        black_threshold=config.guidance.black_threshold,
        ray_angles_deg=config.guidance.ray_angles_deg,
        ray_max_length=config.guidance.ray_max_length,
        evasive_distance=config.guidance.evasive_distance,
        default_speed=config.guidance.default_speed,
        steering_limit=config.control.steering_limit,
        light_on=config.control.light_on,
    )
    options = OrchestratorOptions(
        show_window=config.ui.show_window and not simulate,
        command_rate_hz=config.ble.command_rate_hz,
        color_tracking=config.tracking.color_tracking,
    )
    return ControlOrchestrator(camera, tracker, boundary, ble, options)
