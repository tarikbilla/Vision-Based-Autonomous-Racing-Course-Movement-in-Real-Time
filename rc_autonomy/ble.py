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
    def __init__(self, target: BLETarget, device_name_or_mac: Optional[str] = None) -> None:
        self.target = target
        self.device_name_or_mac = device_name_or_mac
        self.peripheral = None
        self.characteristic = None

    def _get_adapter(self):
        import simplepyble

        adapters = simplepyble.Adapter.get_adapters()
        if not adapters:
            raise RuntimeError("No BLE adapters found")
        return adapters[0]

    def _find_candidate(self):
        adapter = self._get_adapter()
        adapter.scan_for(8000)
        peripherals = adapter.scan_get_results()
        candidates = []
        for peripheral in peripherals:
            name = peripheral.identifier()
            address = peripheral.address()
            if self.device_name_or_mac:
                if self.device_name_or_mac.lower() in name.lower() or self.device_name_or_mac.lower() == address.lower():
                    candidates.append(peripheral)
            else:
                name_lower = name.lower()
                if "drift" in name_lower or "dr!ft" in name_lower or address.lower() == self.target.device_mac.lower():
                    candidates.append(peripheral)
        if not candidates:
            raise RuntimeError("DRIFT car not found during scan")
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
