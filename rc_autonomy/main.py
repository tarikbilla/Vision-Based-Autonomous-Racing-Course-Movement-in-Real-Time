from __future__ import annotations

import argparse
import signal
import sys
import time

from .ble import BLETarget, FakeBLEClient, SimplePyBLEClient
from .commands import ControlVector
from .config import load_config
from .orchestrator import build_orchestrator


def _build_ble_client(config, simulate: bool, device_hint: str | None):
    target = BLETarget(
        device_mac=config.ble.device_mac,
        characteristic_uuid=config.ble.characteristic_uuid,
        device_identifier=config.ble.device_identifier,
    )
    if simulate:
        return FakeBLEClient(target)
    return SimplePyBLEClient(target, device_name_or_mac=device_hint)


def run_app(args: argparse.Namespace) -> int:
    config = load_config(args.config)
    ble = _build_ble_client(config, args.simulate, args.device)
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

    def _shutdown(*_):
        orchestrator.stop()
        ble.send_control(stop_control)
        ble.disconnect()

    signal.signal(signal.SIGINT, _shutdown)
    signal.signal(signal.SIGTERM, _shutdown)

    orchestrator.start()
    if args.duration > 0:
        time.sleep(args.duration)
        _shutdown()
        return 0
    while True:
        time.sleep(1)


def scan(args: argparse.Namespace) -> int:
    config = load_config(args.config)
    ble = _build_ble_client(config, False, args.device)
    try:
        ble.connect()
        ble.disconnect()
        print("BLE scan/connect succeeded")
        return 0
    except Exception as exc:
        print(f"BLE scan/connect failed: {exc}")
        return 1


def cli() -> None:
    parser = argparse.ArgumentParser(description="RC Autonomy Control")
    parser.add_argument("--config", default=None, help="Path to config JSON")
    subparsers = parser.add_subparsers(dest="command", required=True)

    run_parser = subparsers.add_parser("run", help="Run autonomy pipeline")
    run_parser.add_argument("--simulate", action="store_true", help="Run in simulation mode")
    run_parser.add_argument("--device", default=None, help="Device name or MAC override")
    run_parser.add_argument("--duration", type=int, default=0, help="Run duration in seconds (0 = unlimited)")

    scan_parser = subparsers.add_parser("scan", help="Scan/connect BLE device")
    scan_parser.add_argument("--device", default=None, help="Device name or MAC override")

    args = parser.parse_args()
    if args.command == "run":
        sys.exit(run_app(args))
    if args.command == "scan":
        sys.exit(scan(args))


if __name__ == "__main__":
    cli()
