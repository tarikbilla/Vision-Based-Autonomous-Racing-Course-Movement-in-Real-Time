# DRIFT Car Information

## Your Car Details

**Bluetooth Name**: `DRiFT-ED5C2384488D`  
**MAC Address**: `ed:5c:23:84:48:8d` (extracted from name)  
**Car Type**: Red car (based on MAC address pattern)

## Quick Connection

### Option 1: Auto-detect (Recommended)
```bash
python3 scan_and_connect.py
```
The script will automatically detect and mark your car with 🚗

### Option 2: Specify name
```bash
python3 test_ble_python.py "DRiFT-ED5C2384488D"
```

### Option 3: Specify MAC address
```bash
python3 test_ble_python.py "ed:5c:23:84:48:8d"
```

## Expected Detection

When scanning, you should see:
```
🚗  1. Name: 'DRiFT-ED5C2384488D'
      MAC: ed:5c:23:84:48:8d [DRIFT CAR?]
```

## Characteristic UUID

Default DRIFT characteristic: `6e400002-b5a3-f393-e0a9-e50e24dcca9e`

## Command Format

All commands use this format:
```
bf0a00082800  [Device identifier]
000a          [Speed: 0-255]
0000          [Drift: 0]
0000          [Steering: 0-255]
0200          [Lights: 0200=ON, 0000=OFF]
00            [Checksum]
```

## Notes

- The MAC address `ed:5c:23:84:48:8d` matches the "red car" from the old project code
- The name format `DRiFT-ED5C2384488D` embeds the MAC address
- Scripts now detect this pattern automatically
