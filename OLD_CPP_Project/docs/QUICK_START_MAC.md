# Quick Start Guide - Test BLE Car Control on Mac

## 🚀 Fastest Way to Test (Python - Recommended)

### Step 1: Install simplepyble
```bash
pip3 install simplepyble
```

### Step 2: Run the Test Script
```bash
python3 test_ble_python.py
```

### Step 3: Use Interactive Commands
When connected, you'll see:
```
> s    # Start car (forward, speed 20)
> t    # Stop car
> l    # Turn left
> r    # Turn right
> q    # Quit
```

## 📋 Before Running

1. **Turn on your RC car** and make sure it's in pairing mode
2. **Enable Bluetooth** on your Mac (System Settings → Bluetooth)
3. **Know your car's MAC address**:
   - Gray car: `f9:af:3c:e2:d2:f5`
   - Red car: `ed:5c:23:84:48:8d`
   - Or check your car's manual

## 🔧 If Python Doesn't Work

### Option A: Test Command Generation Only (C++)
```bash
# Compile
make -f Makefile.test

# Run (shows commands but doesn't send)
./test_ble_mac
```

### Option B: Check Python Installation
```bash
# Check Python version
python3 --version

# Install pip if needed
python3 -m ensurepip --upgrade

# Try installing simplepyble again
pip3 install --upgrade simplepyble
```

## 🎯 Expected Output

When you run `python3 test_ble_python.py`:

```
==================================================
BLE RC Car Control Test - Mac
==================================================
Target MAC: f9:af:3c:e2:d2:f5
Characteristic UUID: 6e400002-b5a3-f393-e0a9-e50e24dcca9e
==================================================
Scanning for BLE devices...
Using adapter: MacBook Pro
Found 3 devices:
  0: DR!FT Car [f9:af:3c:e2:d2:f5]
Found device: DR!FT Car [f9:af:3c:e2:d2:f5]
Connecting...
Connected successfully!
Found 2 services:
  Service: 6e400001-b5a3-f393-e0a9-e50e24dcca9e
    Characteristic: 6e400002-b5a3-f393-e0a9-e50e24dcca9e
Found target characteristic: 6e400002-b5a3-f393-e0a9-e50e24dcca9e

--- Testing Commands ---

STOP command: bf0a00082800000000000000000000000000
✓ Command sent successfully

START command: bf0a00082800001400000000000000020000
✓ Command sent successfully

[Car should move forward for 2 seconds]

STOP command: bf0a00082800000000000000000000000000
✓ Command sent successfully

Entering interactive mode...
==================================================
Interactive BLE Control Mode
==================================================
Commands:
  s - Start (forward, speed 20)
  t - Stop
  l - Left turn
  r - Right turn
  f - Forward straight
  b - Reverse
  + - Increase speed
  - - Decrease speed
  q - Quit
==================================================

> s
START: Speed=20, Lights ON
Command: bf0a00082800001400000000000000020000
✓ Command sent successfully

> t
STOP
Command: bf0a00082800000000000000000000000000
✓ Command sent successfully

> q
Quitting...
Disconnecting...
Disconnected.
```

## ⚠️ Troubleshooting

### "No BLE adapters found"
- Enable Bluetooth in System Settings
- Restart Bluetooth: `sudo pkill bluetoothd`

### "Device not found"
- Make sure car is ON and in pairing mode
- Check MAC address is correct
- Move car closer to Mac

### "Permission denied"
- System Settings → Privacy & Security → Bluetooth
- Grant permission to Terminal/your app

### "simplepyble installation fails"
```bash
pip3 install --upgrade pip
pip3 install simplepyble==0.6.1
```

## 📝 Custom MAC Address

If your car has a different MAC address:

```bash
python3 test_ble_python.py YOUR_MAC_ADDRESS
```

Example:
```bash
python3 test_ble_python.py ed:5c:23:84:48:8d
```

## ✅ Success Checklist

- [ ] Python 3 installed
- [ ] simplepyble installed (`pip3 install simplepyble`)
- [ ] Car powered on and in pairing mode
- [ ] Bluetooth enabled on Mac
- [ ] Script runs and finds device
- [ ] Connection successful
- [ ] Start command moves car
- [ ] Stop command stops car

## 🎉 Next Steps

Once BLE connection works:
1. Test all commands (forward, reverse, left, right)
2. Note any issues or unexpected behavior
3. Integrate into main C++ project
4. Test with camera and tracking system

## 📚 Full Documentation

See `MAC_SETUP_INSTRUCTIONS.md` for detailed setup and troubleshooting.
