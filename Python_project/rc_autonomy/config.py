from __future__ import annotations

import json
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Optional


@dataclass
class CameraConfig:
    source: int | str
    width: int
    height: int
    fps: int


@dataclass
class TrackingConfig:
    tracker_type: str
    roi: Optional[list[int]]
    color_tracking: bool = False


@dataclass
class GuidanceConfig:
    black_threshold: int
    ray_angles_deg: list[int]
    ray_max_length: int
    evasive_distance: int
    default_speed: int


@dataclass
class BLEConfig:
    device_mac: str
    characteristic_uuid: str
    device_identifier: str
    command_rate_hz: int
    connection_timeout_s: int
    reconnect_attempts: int


@dataclass
class ControlConfig:
    speed_min: int
    speed_max: int
    steering_limit: int
    light_on: bool


@dataclass
class UIConfig:
    show_window: bool


@dataclass
class AppConfig:
    camera: CameraConfig
    tracking: TrackingConfig
    guidance: GuidanceConfig
    ble: BLEConfig
    control: ControlConfig
    ui: UIConfig


DEFAULT_CONFIG_PATH = Path(__file__).resolve().parents[1] / "config" / "default.json"


def _load_json(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as file:
        return json.load(file)


def load_config(path: Optional[str] = None) -> AppConfig:
    config_path = Path(path) if path else DEFAULT_CONFIG_PATH
    raw = _load_json(config_path)
    return AppConfig(
        camera=CameraConfig(**raw["camera"]),
        tracking=TrackingConfig(**raw["tracking"]),
        guidance=GuidanceConfig(**raw["guidance"]),
        ble=BLEConfig(**raw["ble"]),
        control=ControlConfig(**raw["control"]),
        ui=UIConfig(**raw["ui"]),
    )
