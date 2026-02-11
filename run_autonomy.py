#!/usr/bin/env python3
from __future__ import annotations

import argparse
import signal
import time

from rc_autonomy.ble import BLETarget, FakeBLEClient, SimplePyBLEClient
from rc_autonomy.commands import ControlVector
from rc_autonomy.config import load_config
from rc_autonomy.orchestrator import build_orchestrator


def build_ble_client(config, simulate: bool, device_hint: str | None):
    target = BLETarget(
        device_mac=config.ble.device_mac,
        characteristic_uuid=config.ble.characteristic_uuid,
        device_identifier=config.ble.device_identifier,
    )
    if simulate:
        return FakeBLEClient(target)
    return SimplePyBLEClient(target, device_name_or_mac=device_hint)


def main() -> int:
    parser = argparse.ArgumentParser(description="Run RC autonomy pipeline")
    parser.add_argument("--config", default=None, help="Path to config JSON")
    parser.add_argument("--simulate", action="store_true", help="Run in simulation mode")
    parser.add_argument("--device", default=None, help="Device name or MAC override")
    parser.add_argument("--duration", type=int, default=0, help="Run duration in seconds (0 = unlimited)")
    args = parser.parse_args()

    config = load_config(args.config)
    ble = build_ble_client(config, args.simulate, args.device)
    stop_control = ControlVector(light_on=False, speed=0, right_turn_value=0, left_turn_value=0)

    for attempt in range(1, config.ble.reconnect_attempts + 1):
        try:
            ble.connect()
            break
        except Exception as exc:
            if attempt == config.ble.reconnect_attempts:
                print(f"BLE connection failed: {exc}")
                return 1
            time.sleep(1)

    orchestrator = build_orchestrator(args.simulate, config, ble)

    def shutdown(*_):
        orchestrator.stop()
        ble.send_control(stop_control)
        ble.disconnect()

    signal.signal(signal.SIGINT, shutdown)
    signal.signal(signal.SIGTERM, shutdown)

    orchestrator.start()
    if args.duration > 0:
        time.sleep(args.duration)
        shutdown()
        return 0

    while True:
        time.sleep(1)


if __name__ == "__main__":
    raise SystemExit(main())
