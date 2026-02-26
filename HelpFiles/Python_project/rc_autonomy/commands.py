from __future__ import annotations

from dataclasses import dataclass


@dataclass
class ControlVector:
    light_on: bool
    speed: int
    right_turn_value: int
    left_turn_value: int


def int_to_hex(value: int, digits: int = 4) -> str:
    return format(max(0, value), f"0{digits}x")


def clamp(value: int, min_value: int, max_value: int) -> int:
    return max(min_value, min(max_value, value))


def steering_value(left_turn: int, right_turn: int) -> int:
    if right_turn > 0:
        return right_turn
    if left_turn > 0:
        return 255 - left_turn
    return 0


def build_command(device_identifier: str, control: ControlVector) -> str:
    drift_value = 0
    light_value = "0200" if control.light_on else "0000"
    checksum = "00"
    steer = steering_value(control.left_turn_value, control.right_turn_value)
    return (
        device_identifier
        + int_to_hex(control.speed, 4)
        + int_to_hex(drift_value, 4)
        + int_to_hex(steer, 4)
        + light_value
        + checksum
    )
