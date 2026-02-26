# BLE Connection Details

## Overview
The system uses Bluetooth Low Energy (BLE) to communicate with the RC car. The BLE connection is established before the camera starts, ensuring reliable control from the beginning.

## Connection Process

### Step 1: Initialize BLE Client
1. **Load configuration** from `config/config.json`
   - Device MAC address: `f9:af:3c:e2:d2:f5`
   - Characteristic UUID: `6e400002-b5a3-f393-e0a9-e50e24dcca9e`
   - Device identifier: `bf0a00082800`

2. **Create BLE client instance**
   - Real mode: Uses SimpleBLE library for actual BLE communication
   - Simulation mode: Uses FakeBLEClient for testing without hardware

### Step 2: Scan for Devices
1. **Get BLE adapters** from the system
2. **Scan for peripherals** with timeout (default: 10 seconds)
3. **Filter devices** by MAC address or name
4. **Return list** of available devices with identifier and address

### Step 3: Connect to RC Car
1. **Attempt connection** up to 3 times (configurable)
2. **For each attempt:**
   - Call `peripheral.connect()`
   - Wait for connection establishment
   - Verify connection status
   - If failed, wait 1 second and retry

3. **On successful connection:**
   - Send connection pulse to verify
   - Run car forward for 1 second to test motion
   - Send stop command

### Step 4: Send Commands
Commands are sent using the characteristic UUID with the following format:

**Command Structure (15 bytes):**
```
Byte 0-3:   Device identifier (bf0a00082800)
Byte 4:     Light on/off (00=off, 01=on)
Byte 5-8:   Speed value (little-endian)
Byte 9-12:  Right turn value (little-endian)
Byte 13-14: Left turn value (little-endian)
```

**Example Commands:**
- **STOP**: `bf0a00082800` + `00` + `00000000` + `00000000` + `0000`
- **FORWARD**: `bf0a00082800` + `01` + `32000000` (speed=50) + `00000000` + `0000`
- **LEFT TURN**: `bf0a00082800` + `01` + `32000000` + `00000000` + `1E00` (left=30)
- **RIGHT TURN**: `bf0a00082800` + `01` + `32000000` + `1E000000` + `0000` (right=30)

## Command Sending Process

### Step 1: Build Command Bytes
```cpp
ControlVector control(light_on, speed, right_turn, left_turn);
std::vector<uint8_t> cmd = Commands::buildCommand(device_identifier, control);
```

### Step 2: Write to BLE Characteristic
```cpp
peripheral.write_command(service_uuid, char_uuid, data);
```

### Step 3: Verify Transmission
- Check return status from write operation
- Log success or failure
- Retry if necessary

## Graceful Shutdown

### When 'q' is Pressed or Ctrl+C
1. **Send multiple STOP commands** (10 times with 50ms delays)
2. **Wait 500ms** for car to physically stop
3. **Disconnect BLE** connection cleanly
4. **Close camera** and other resources

### Stop Command Sequence
```cpp
for (int i = 0; i < 10; ++i) {
    ble->sendControl(ControlVector(false, 0, 0, 0));
    sleep(50ms);
}
sleep(500ms);
ble->disconnect();
```

## Error Handling

### Connection Failures
- **No adapters found**: System has no BLE hardware
- **Device not found**: MAC address incorrect or car is off
- **Connection timeout**: Car is too far or signal interference
- **Write failed**: Connection lost during operation

### Recovery Strategies
1. **Retry connection** up to 3 times with 1-second delays
2. **Send emergency STOP** if connection is lost during operation
3. **Log all errors** for debugging

## Configuration Parameters

**From `config/config.json`:**
```json
{
  "ble": {
    "device_mac": "f9:af:3c:e2:d2:f5",
    "characteristic_uuid": "6e400002-b5a3-f393-e0a9-e50e24dcca9e",
    "device_identifier": "bf0a00082800",
    "connection_timeout_s": 10,
    "reconnect_attempts": 3
  }
}
```

## Platform Support
- **macOS**: Full support via SimpleBLE
- **Linux**: Full support via SimpleBLE (requires BlueZ)
- **Windows**: Full support via SimpleBLE
- **Simulation mode**: Works on all platforms without BLE hardware

## Debugging Tips

1. **Check BLE adapter**: `system_profiler SPBluetoothDataType` (macOS)
2. **Verify MAC address**: Turn on car and scan for devices
3. **Check UUID**: Confirm characteristic UUID matches car firmware
4. **Test with fake mode**: Use `--simulate` flag to test without hardware
5. **Monitor logs**: Look for `[✓]` success and `[!]` error messages
