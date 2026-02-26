#!/usr/bin/env python3
"""Quick test: Connect to car, make it move, then stop."""

import sys
import time

from rc_autonomy.ble import BLETarget, SimplePyBLEClient
from rc_autonomy.commands import ControlVector, build_command
from rc_autonomy.config import load_config


def main():
    config = load_config()
    target = BLETarget(
        device_mac=config.ble.device_mac,
        characteristic_uuid=config.ble.characteristic_uuid,
        device_identifier=config.ble.device_identifier,
    )
    ble = SimplePyBLEClient(target, device_index=2, scan_timeout_ms=10000)

    print("[1] Connecting to car...")
    try:
        ble.connect()
        print("[✓] Connected!")
    except Exception as exc:
        print(f"[✗] Failed: {exc}")
        return 1

    print("\n[2] Testing car response...")

    # Stop first
    print("  → Sending STOP...")
    stop = ControlVector(light_on=False, speed=0, right_turn_value=0, left_turn_value=0)
    ble.send_control(stop)
    time.sleep(0.5)

    # Start moving
    print("  → Sending START (speed=30, lights ON)...")
    start = ControlVector(light_on=True, speed=30, right_turn_value=0, left_turn_value=0)
    ble.send_control(start)
    time.sleep(2)

    # Stop
    print("  → Sending STOP...")
    ble.send_control(stop)
    time.sleep(0.5)

    # Left turn
    print("  → Sending LEFT TURN (speed=20, left=20)...")
    left = ControlVector(light_on=True, speed=20, right_turn_value=0, left_turn_value=20)
    ble.send_control(left)
    time.sleep(1)

    # Right turn
    print("  → Sending RIGHT TURN (speed=20, right=20)...")
    right = ControlVector(light_on=True, speed=20, right_turn_value=20, left_turn_value=0)
    ble.send_control(right)
    time.sleep(1)

    # Final stop
    print("  → Sending STOP...")
    ble.send_control(stop)

    print("\n[✓] Test complete!")
    ble.disconnect()
    return 0


if __name__ == "__main__":
    sys.exit(main())
