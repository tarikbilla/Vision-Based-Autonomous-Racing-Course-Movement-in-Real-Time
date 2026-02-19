# Installing SimpleBLE for Real BLE Support

## What is SimpleBLE?

SimpleBLE is a C++ BLE library that provides cross-platform Bluetooth Low Energy support. The Python version uses `simplepyble` (Python bindings for SimpleBLE).

## Installation

### macOS

SimpleBLE is not available via Homebrew, so you need to build from source:

```bash
# Install dependencies
brew install cmake

# Clone SimpleBLE
cd /tmp
git clone https://github.com/OpenBluetoothToolbox/SimpleBLE.git
cd SimpleBLE

# Build and install
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(sysctl -n hw.ncpu)
sudo cmake --install build

# Verify installation
ls /usr/local/lib | grep simpleble
ls /usr/local/include | grep simpleble
```

### Linux / Ubuntu

```bash
# Install dependencies
sudo apt-get update
sudo apt-get install -y cmake build-essential libbluetooth-dev

# Clone SimpleBLE
cd /tmp
git clone https://github.com/OpenBluetoothToolbox/SimpleBLE.git
cd SimpleBLE

# Build and install
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
sudo cmake --install build

# Verify installation
ls /usr/local/lib | grep simpleble
ls /usr/local/include | grep simpleble
```

### Raspberry Pi

Same as Linux above, but may take longer to build (~5-10 minutes).

## After Installation

Rebuild the C++ project:

```bash
cd CPP_Complete
rm -rf build
./build.sh
```

You should see:
```
SimpleBLE found - Real BLE support enabled
```

## Without SimpleBLE

If you don't install SimpleBLE:
- **Simulation mode will still work**: `./build/VisionBasedRCCarControl --simulate`
- **Real BLE mode will show an error** and instruct you to install SimpleBLE
- The executable will still build, but BLE functionality will be disabled

## Verifying BLE Works

After installing SimpleBLE and rebuilding:

```bash
# This should now scan for real BLE devices
./build/VisionBasedRCCarControl --device f9:af:3c:e2:d2:f5
```

Expected output:
```
[1/3] Connecting to BLE car...
[*] Connecting to BLE device: f9:af:3c:e2:d2:f5
[*] Scanning for BLE devices...
[✓] Found 5 BLE devices
[*] Found candidate: DRiFT Car - f9:af:3c:e2:d2:f5
[✓] Selected device: DRiFT Car - f9:af:3c:e2:d2:f5
[*] Connecting...
[✓] Found target characteristic: 0000ffe1-0000-1000-8000-00805f9b34fb
[✓] BLE car connected!
[*] Sending short START pulse to confirm connection...
[✓] Connection pulse complete.
```

## Troubleshooting

**Error: SimpleBLE not found**
- Make sure you ran `sudo cmake --install build` after building
- Check CMake output for "SimpleBLE found"

**Error: No BLE adapters found**
- Ensure Bluetooth is enabled on your system
- On Linux: `sudo systemctl status bluetooth`
- On macOS: Check System Preferences → Bluetooth

**Error: Permission denied**
- On Linux, you may need to add your user to the `bluetooth` group:
  ```bash
  sudo usermod -a -G bluetooth $USER
  # Log out and log back in
  ```
