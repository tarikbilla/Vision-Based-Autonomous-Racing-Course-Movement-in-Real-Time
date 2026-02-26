from __future__ import annotations

import time
from dataclasses import dataclass
from typing import Optional

from .commands import ControlVector, build_command


@dataclass
class BLETarget:
    device_mac: str
    characteristic_uuid: str
    device_identifier: str


class BLEClient:
    def connect(self) -> None:
        raise NotImplementedError

    def disconnect(self) -> None:
        raise NotImplementedError

    def send_control(self, control: ControlVector) -> bool:
        raise NotImplementedError


class FakeBLEClient(BLEClient):
    def __init__(self, target: BLETarget) -> None:
        self.target = target
        self.connected = False

    def connect(self) -> None:
        self.connected = True

    def disconnect(self) -> None:
        self.connected = False

    def send_control(self, control: ControlVector) -> bool:
        if not self.connected:
            return False
        return True


class SimplePyBLEClient(BLEClient):
    def __init__(
        self,
        target: BLETarget,
        device_name_or_mac: Optional[str] = None,
        scan_timeout_ms: int = 10000,
        device_index: Optional[int] = None,
    ) -> None:
        self.target = target
        self.device_name_or_mac = device_name_or_mac
        self.scan_timeout_ms = scan_timeout_ms
        self.device_index = device_index
        self.peripheral = None
        self.characteristic = None

    def _get_adapter(self):
        import simplepyble

        adapters = simplepyble.Adapter.get_adapters()
        if not adapters:
            raise RuntimeError("No BLE adapters found")
        return adapters[0]

    def _scan_peripherals(self):
        adapter = self._get_adapter()
        adapter.scan_for(self.scan_timeout_ms)
        return adapter.scan_get_results()

    def list_devices(self):
        peripherals = self._scan_peripherals()
        return [(peripheral.identifier(), peripheral.address()) for peripheral in peripherals]

    def _find_candidate(self):
        peripherals = self._scan_peripherals()
        scanned_devices = [(p.identifier(), p.address()) for p in peripherals]

        if self.device_index is not None:
            if self.device_index < 0 or self.device_index >= len(peripherals):
                raise RuntimeError(
                    f"Device index {self.device_index} out of range (0-{max(0, len(peripherals) - 1)})"
                )
            return peripherals[self.device_index]

        candidates = []
        for peripheral in peripherals:
            name = peripheral.identifier()
            address = peripheral.address()
            if self.device_name_or_mac:
                name_lower = name.lower()
                name_upper = name.upper()
                address_lower = address.lower()
                if (
                    self.device_name_or_mac.lower() in name_lower
                    or self.device_name_or_mac.upper() in name_upper
                    or self.device_name_or_mac.lower() == address_lower
                ):
                    candidates.append(peripheral)
            else:
                name_lower = name.lower()
                name_upper = name.upper()
                address_lower = address.lower()
                if (
                    "drift" in name_lower
                    or "dr!ft" in name_lower
                    or name.startswith("DRiFT")
                    or name.startswith("DRIFT")
                    or name.startswith("DR!FT")
                    or "ED5C2384488D" in name_upper
                    or "F9AF3CE2D2F5" in name_upper
                    or address_lower == self.target.device_mac.lower()
                ):
                    candidates.append(peripheral)

        if not candidates:
            preview = ", ".join(f"{name} ({addr})" for name, addr in scanned_devices[:10])
            raise RuntimeError(
                "DRIFT car not found during scan. "
                f"Found {len(scanned_devices)} devices: {preview}"
            )
        return candidates[0]

    def _select_characteristic(self, peripheral) -> Optional[tuple[str, str]]:
        services = peripheral.services()
        for service in services:
            for char in service.characteristics():
                if char.uuid().lower() == self.target.characteristic_uuid.lower():
                    return (service.uuid(), char.uuid())
        for service in services:
            for char in service.characteristics():
                return (service.uuid(), char.uuid())
        return None

    def connect(self) -> None:
        peripheral = self._find_candidate()
        peripheral.connect()
        time.sleep(1)
        characteristic = self._select_characteristic(peripheral)
        if not characteristic:
            raise RuntimeError("No writable characteristic found")
        self.peripheral = peripheral
        self.characteristic = characteristic

    def disconnect(self) -> None:
        if self.peripheral:
            self.peripheral.disconnect()
        self.peripheral = None
        self.characteristic = None

    def send_control(self, control: ControlVector) -> bool:
        if not self.peripheral or not self.characteristic:
            return False
        command_hex = build_command(self.target.device_identifier, control)
        command_bytes = bytes.fromhex(command_hex)
        service_uuid, char_uuid = self.characteristic
        try:
            self.peripheral.write_request(service_uuid, char_uuid, command_bytes)
            return True
        except Exception:
            try:
                self.peripheral.write_command(service_uuid, char_uuid, command_bytes)
                return True
            except Exception:
                return False
