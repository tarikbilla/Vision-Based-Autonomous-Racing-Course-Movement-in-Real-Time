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
        self.stop_event = threading.Event()
        self.latest_control = ControlVector(light_on=True, speed=0, right_turn_value=0, left_turn_value=0)
        self.latest_heading = 0.0
        self._threads: list[threading.Thread] = []

    def start(self) -> None:
        self.stop_event.clear()
        self._threads = [
            threading.Thread(target=self._camera_loop, daemon=True),
            threading.Thread(target=self._tracking_loop, daemon=True),
            threading.Thread(target=self._ble_loop, daemon=True),
        ]
        for thread in self._threads:
            thread.start()

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
                try:
                    self.frame_queue.put(frame, timeout=0.1)
                except Exception:
                    continue
        except Exception:
            self.stop_event.set()

    def _tracking_loop(self) -> None:
        initialized = False
        while not self.stop_event.is_set():
            try:
                frame = self.frame_queue.get(timeout=0.2)
            except Empty:
                continue
            image = frame.image
            if not initialized:
                self.tracker.initialize(image, None)
                initialized = True
            try:
                tracked = self.tracker.update(image)
                rays, control = self.boundary.analyze(image, tracked.center, tracked.movement)
                self.latest_control = control
                self.latest_heading = self.boundary._heading_from_movement(tracked.movement)
                if self.options.show_window:
                    self._render(image, tracked, rays)
            except Exception:
                self.latest_control = ControlVector(light_on=True, speed=0, right_turn_value=0, left_turn_value=0)
        if self.options.show_window:
            self._close_window()

    def _ble_loop(self) -> None:
        interval = 1.0 / max(1, self.options.command_rate_hz)
        while not self.stop_event.is_set():
            self.ble.send_control(self.latest_control)
            time.sleep(interval)

    def _render(self, image: np.ndarray, tracked: TrackedObject, rays) -> None:
        import cv2

        from .ui import draw_overlay

        overlay = draw_overlay(image.copy(), tracked.bbox, tracked.center, rays, self.latest_heading)
        cv2.imshow("Autonomy", overlay)
        if cv2.waitKey(1) & 0xFF == ord("q"):
            self.stop_event.set()

    def _close_window(self) -> None:
        import cv2

        cv2.destroyAllWindows()


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
    options = OrchestratorOptions(show_window=config.ui.show_window and not simulate, command_rate_hz=config.ble.command_rate_hz)
    return ControlOrchestrator(camera, tracker, boundary, ble, options)
