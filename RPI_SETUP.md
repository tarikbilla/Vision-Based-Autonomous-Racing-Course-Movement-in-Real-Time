# Raspberry Pi 4 Setup & Build Guide

## Complete Pre-Installation Script

Run this script on a fresh Raspberry Pi OS to install all dependencies:

```bash
#!/bin/bash
set -e

echo "=================================="
echo "Raspberry Pi 4 Setup"
echo "=================================="

# Update system
echo "[1/10] Updating system..."
sudo apt update
sudo apt upgrade -y

# Install build essentials
echo "[2/10] Installing build essentials..."
sudo apt install -y cmake build-essential pkg-config git wget curl

# Install Python dependencies
echo "[3/10] Installing Python dependencies..."
sudo apt install -y python3-dev python3-pip python3-numpy

# Install OpenCV from apt (faster than building from source)
echo "[4/10] Installing OpenCV..."
sudo apt install -y libopencv-dev python3-opencv

# Install OpenCV dev headers
sudo apt install -y libopencv-contrib-dev

# Install nlohmann-json (C++ JSON library)
echo "[5/10] Installing nlohmann-json..."
sudo apt install -y nlohmann-json3-dev

# Install SimpleBLE dependencies
echo "[6/10] Installing SimpleBLE dependencies..."
sudo apt install -y libglib2.0-dev libdbus-1-dev libudev-dev libssl-dev

# Clone and build SimpleBLE
echo "[7/10] Building SimpleBLE from source..."
cd /tmp
git clone https://github.com/OpenBluetoothToolbox/SimpleBLE.git
cd SimpleBLE
mkdir -p build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j4
sudo make install
sudo ldconfig

# Install camera utilities
echo "[8/10] Installing camera utilities..."
sudo apt install -y v4l-utils

# Enable camera interface
echo "[9/10] Configuring camera interface..."
sudo raspi-config nonint do_camera 0

# Final setup
echo "[10/10] Final cleanup..."
sudo ldconfig
cd ~

echo ""
echo "=================================="
echo "Setup Complete!"
echo "=================================="
echo ""
echo "Next steps:"
echo "1. cd ~/Project/IoT-Project-Vision-based-autonomous-RC-car-control-system"
echo "2. ./build.sh  (for main project)"
echo "3. ./RBP4/build_rbp4.sh  (for optimized RBP4 version)"
echo ""
```

## Quick Install (Copy & Paste)

```bash
# Download and run setup script
curl -fsSL https://raw.githubusercontent.com/yourusername/repo/main/rpi_setup.sh | bash
```

Or manually install missing packages:

```bash
# Just the critical missing packages
sudo apt update
sudo apt install -y nlohmann-json3-dev libglib2.0-dev libdbus-1-dev libudev-dev

# Build SimpleBLE (if not in apt repos)
cd /tmp
git clone https://github.com/OpenBluetoothToolbox/SimpleBLE.git
cd SimpleBLE && mkdir build && cd build
cmake .. && make -j4 && sudo make install
sudo ldconfig
```

## Troubleshooting

### Error: `nlohmann/json.hpp: No such file or directory`
```bash
sudo apt install -y nlohmann-json3-dev
```

### Error: `SimpleBLE not found`
```bash
# Install from source
cd /tmp
git clone https://github.com/OpenBluetoothToolbox/SimpleBLE.git
cd SimpleBLE
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j4
sudo make install
sudo ldconfig
```

### Build Memory Issues
If build fails with memory errors on Pi:
```bash
# Use single-threaded build
cmake ..
make -j1

# Or use RBP4 version (more optimized)
./RBP4/build_rbp4.sh
```

### Camera Not Detected
```bash
# Check if enabled
sudo raspi-config nonint get_camera

# Enable if needed
sudo raspi-config nonint do_camera 0

# Test camera
vcgencmd get_camera
v4l2-ctl --list-devices
```

## Build Instructions

### Build Main Project
```bash
cd ~/Project/IoT-Project-Vision-based-autonomous-RC-car-control-system
./build.sh
./build/VisionBasedRCCarControl
```

### Build RBP4 Optimized Version
```bash
cd ~/Project/IoT-Project-Vision-based-autonomous-RC-car-control-system
./RBP4/build_rbp4.sh
./RBP4/run_rbp4.sh
```

## System Specs After Setup

| Component | Version |
|-----------|---------|
| CMake | 3.27+ |
| C++ Standard | 17 |
| OpenCV | 4.10.0 |
| nlohmann_json | 3.11+ |
| SimpleBLE | Latest from GitHub |
| Python | 3.11+ |

## Performance Notes

- **First build:** 10-15 minutes (depends on Pi's CPU)
- **RBP4 version:** More optimized, faster startup
- **Memory:** Monitor with `free -h` during build
- **Cooling:** Ensure proper Pi cooling during builds

---

**Recommendation:** Use RBP4 version for better Pi4 performance.
