#!/usr/bin/env python3
from __future__ import annotations

import argparse
import signal
import time

from rc_autonomy.ble import BLETarget, FakeBLEClient, SimplePyBLEClient
from rc_autonomy.commands import ControlVector
from rc_autonomy.config import load_config
from rc_autonomy.orchestrator import build_orchestrator


def build_ble_client(
    config,
    simulate: bool,
    device_hint: str | None,
    device_index: int | None,
):
    target = BLETarget(
        device_mac=config.ble.device_mac,
        characteristic_uuid=config.ble.characteristic_uuid,
        device_identifier=config.ble.device_identifier,
    )
    if simulate:
        return FakeBLEClient(target)
    scan_timeout_ms = max(10000, int(config.ble.connection_timeout_s * 1000))
    return SimplePyBLEClient(
        target,
        device_name_or_mac=device_hint,
        scan_timeout_ms=scan_timeout_ms,
        device_index=device_index,
    )


def main() -> int:
    parser = argparse.ArgumentParser(description="Run RC autonomy pipeline")
    parser.add_argument("--config", default=None, help="Path to config JSON")
    parser.add_argument("--simulate", action="store_true", help="Run in simulation mode")
    parser.add_argument("--device", default=None, help="Device name or MAC override")
    parser.add_argument("--device-index", type=int, default=None, help="Select device by scan index")
    parser.add_argument("--list-devices", action="store_true", help="List scanned BLE devices and exit")
    parser.add_argument("--duration", type=int, default=0, help="Run duration in seconds (0 = unlimited)")
    args = parser.parse_args()

    config = load_config(args.config)
    ble = build_ble_client(config, args.simulate, args.device, args.device_index)
    stop_control = ControlVector(light_on=False, speed=0, right_turn_value=0, left_turn_value=0)

    if args.list_devices:
        if hasattr(ble, "list_devices"):
            devices = ble.list_devices()
            for index, (name, address) in enumerate(devices):
                label = name if name else "(Unknown)"
                print(f"[{index}] {label} - {address}")
            return 0
        print("Device listing not supported by current BLE client")
        return 1

    print("[1/3] Connecting to BLE car...")
    for attempt in range(1, config.ble.reconnect_attempts + 1):
        try:
            ble.connect()
            print("[✓] BLE car connected!")
            break
        except Exception as exc:
            if attempt == config.ble.reconnect_attempts:
                print(f"[✗] BLE connection failed: {exc}")
                return 1
            print(f"[*] Retry {attempt}/{config.ble.reconnect_attempts}...")
            time.sleep(1)

    print("[2/3] Starting camera + tracking pipeline...")
    orchestrator = build_orchestrator(args.simulate, config, ble)

    try:
        orchestrator.start()
    except Exception as exc:
        print(f"[✗] Pipeline start failed: {exc}")
        ble.send_control(stop_control)
        ble.disconnect()
        return 1
    print("[3/3] Autonomy running (press Ctrl+C to stop, or 'q' in window to quit)")

    def shutdown(*_):
        orchestrator.stop()
        ble.send_control(stop_control)
        ble.disconnect()

    signal.signal(signal.SIGINT, shutdown)
    signal.signal(signal.SIGTERM, shutdown)

    if args.duration > 0:
        time.sleep(args.duration)
        shutdown()
        return 0

    while True:
        time.sleep(1)


if __name__ == "__main__":
    raise SystemExit(main())
