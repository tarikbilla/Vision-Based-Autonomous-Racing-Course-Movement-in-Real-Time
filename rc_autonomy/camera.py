from __future__ import annotations

import time
from dataclasses import dataclass
from typing import Iterator, Optional

import numpy as np


@dataclass
class Frame:
    image: np.ndarray
    timestamp: float


class CameraCapture:
    def __init__(self, source: int | str, width: int, height: int, fps: int) -> None:
        self.source = source
        self.width = width
        self.height = height
        self.fps = fps
        self._cap = None

    def open(self) -> None:
        import cv2

        cap = cv2.VideoCapture(self.source)
        cap.set(cv2.CAP_PROP_FRAME_WIDTH, self.width)
        cap.set(cv2.CAP_PROP_FRAME_HEIGHT, self.height)
        cap.set(cv2.CAP_PROP_FPS, self.fps)
        if not cap.isOpened():
            # Try to find an open camera
            print(f"[!] Camera source {self.source} failed. Searching for available cameras...")
            for i in range(10):
                test_cap = cv2.VideoCapture(i)
                if test_cap.isOpened():
                    print(f"[✓] Found camera at index {i}")
                    test_cap.release()
                    cap = cv2.VideoCapture(i)
                    cap.set(cv2.CAP_PROP_FRAME_WIDTH, self.width)
                    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, self.height)
                    cap.set(cv2.CAP_PROP_FPS, self.fps)
                    self.source = i
                    break
                test_cap.release()
            if not cap.isOpened():
                raise RuntimeError(f"Camera capture failed to open (tried indices 0-9)")
        self._cap = cap

    def close(self) -> None:
        if self._cap:
            self._cap.release()
        self._cap = None

    def frames(self) -> Iterator[Frame]:
        if not self._cap:
            self.open()
        while True:
            ok, frame = self._cap.read()
            if not ok:
                raise RuntimeError("Camera capture failed")
            yield Frame(image=frame, timestamp=time.time())


class SimulatedCamera:
    def __init__(self, width: int, height: int, fps: int) -> None:
        self.width = width
        self.height = height
        self.fps = fps
        self._running = False

    def open(self) -> None:
        self._running = True

    def close(self) -> None:
        self._running = False

    def frames(self) -> Iterator[Frame]:
        if not self._running:
            self.open()
        t = 0
        while self._running:
            image = np.zeros((self.height, self.width, 3), dtype=np.uint8)
            cx = int((self.width / 2) + (self.width / 4) * np.sin(t))
            cy = int((self.height / 2) + (self.height / 6) * np.cos(t))
            rr = 12
            y, x = np.ogrid[: self.height, : self.width]
            mask = (x - cx) ** 2 + (y - cy) ** 2 <= rr ** 2
            image[mask] = (0, 255, 0)
            yield Frame(image=image, timestamp=time.time())
            t += 0.08
            time.sleep(1 / max(1, self.fps))
