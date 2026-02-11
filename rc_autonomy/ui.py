from __future__ import annotations

from typing import Iterable, Tuple

import numpy as np

from .boundary import RayResult


def draw_overlay(
    frame: np.ndarray,
    bbox: Tuple[int, int, int, int],
    center: Tuple[int, int],
    rays: Iterable[RayResult],
    heading_deg: float,
) -> np.ndarray:
    import cv2

    x, y, w, h = bbox
    cv2.rectangle(frame, (x, y), (x + w, y + h), (0, 255, 255), 2)
    cv2.circle(frame, center, 4, (255, 0, 0), -1)
    for ray in rays:
        angle = heading_deg + ray.angle_deg
        end_x = int(center[0] + ray.distance * np.cos(np.deg2rad(angle)))
        end_y = int(center[1] + ray.distance * np.sin(np.deg2rad(angle)))
        cv2.line(frame, center, (end_x, end_y), (0, 0, 255), 1)
    return frame
