from __future__ import annotations

from dataclasses import dataclass
from typing import Optional, Tuple

import numpy as np


@dataclass
class TrackedObject:
    bbox: Tuple[int, int, int, int]
    center: Tuple[int, int]
    movement: Tuple[int, int]


class Tracker:
    def initialize(self, frame: np.ndarray, roi: Optional[Tuple[int, int, int, int]]) -> None:
        raise NotImplementedError

    def update(self, frame: np.ndarray) -> TrackedObject:
        raise NotImplementedError


class OpenCVTracker(Tracker):
    def __init__(self, tracker_type: str) -> None:
        self.tracker_type = tracker_type
        self.tracker = None
        self._last_center = None

    def _create_tracker(self):
        import cv2

        name = self.tracker_type.upper()
        if name == "CSRT":
            return cv2.TrackerCSRT_create()
        if name == "KCF":
            return cv2.TrackerKCF_create()
        if name == "GOTURN":
            return cv2.TrackerGOTURN_create()
        return cv2.TrackerKCF_create()  # Default to KCF (more stable)

    def initialize(self, frame: np.ndarray, roi: Optional[Tuple[int, int, int, int]]) -> None:
        import cv2

        if roi is None:
            roi = cv2.selectROI("Select ROI", frame, fromCenter=False, showCrosshair=True)
            cv2.destroyWindow("Select ROI")
        self.tracker = self._create_tracker()
        ok = self.tracker.init(frame, roi)
        if not ok:
            raise RuntimeError("Tracker initialization failed")
        x, y, w, h = [int(v) for v in roi]
        self._last_center = (x + w // 2, y + h // 2)

    def update(self, frame: np.ndarray) -> TrackedObject:
        if not self.tracker:
            raise RuntimeError("Tracker not initialized")
        ok, bbox = self.tracker.update(frame)
        if not ok:
            raise RuntimeError("Tracking lost")
        x, y, w, h = [int(v) for v in bbox]
        center = (x + w // 2, y + h // 2)
        if self._last_center is None:
            movement = (0, 0)
        else:
            movement = (center[0] - self._last_center[0], center[1] - self._last_center[1])
        self._last_center = center
        return TrackedObject(bbox=(x, y, w, h), center=center, movement=movement)


class SimulatedTracker(Tracker):
    def __init__(self) -> None:
        self._last_center = None

    def initialize(self, frame: np.ndarray, roi: Optional[Tuple[int, int, int, int]]) -> None:
        self._last_center = None

    def update(self, frame: np.ndarray) -> TrackedObject:
        green_mask = frame[:, :, 1] > 200
        ys, xs = np.where(green_mask)
        if len(xs) == 0:
            raise RuntimeError("Simulated target not found")
        cx = int(xs.mean())
        cy = int(ys.mean())
        bbox = (cx - 12, cy - 12, 24, 24)
        if self._last_center is None:
            movement = (0, 0)
        else:
            movement = (cx - self._last_center[0], cy - self._last_center[1])
        self._last_center = (cx, cy)
        return TrackedObject(bbox=bbox, center=(cx, cy), movement=movement)
