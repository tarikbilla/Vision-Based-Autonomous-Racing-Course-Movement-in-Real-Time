#!/bin/bash
set -e

echo "=================================="
echo "Raspberry Pi 4 Dependency Setup"
echo "=================================="
echo ""

# Update system
echo "[1/8] Updating system packages..."
sudo apt update
sudo apt upgrade -y

# Install build essentials
echo "[2/8] Installing build tools..."
sudo apt install -y cmake build-essential pkg-config git wget

# Install OpenCV
echo "[3/8] Installing OpenCV..."
sudo apt install -y libopencv-dev python3-opencv libopencv-contrib-dev

# Install nlohmann-json (THE CRITICAL MISSING PACKAGE)
echo "[4/8] Installing nlohmann-json..."
sudo apt install -y nlohmann-json3-dev

# Install SimpleBLE dependencies
echo "[5/8] Installing SimpleBLE dependencies..."
sudo apt install -y libglib2.0-dev libdbus-1-dev libudev-dev libssl-dev

# Build and install SimpleBLE
echo "[6/8] Building SimpleBLE..."
cd /tmp
rm -rf SimpleBLE 2>/dev/null || true
git clone https://github.com/OpenBluetoothToolbox/SimpleBLE.git
cd SimpleBLE
mkdir -p build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local
make -j4 2>/dev/null || make -j2 || make -j1
sudo make install
sudo ldconfig

# Enable camera
echo "[7/8] Enabling camera interface..."
sudo raspi-config nonint do_camera 0 2>/dev/null || echo "Camera config skipped (not on RPi)"

# Verify installation
echo "[8/8] Verifying installation..."
echo ""
echo "Checking for required packages:"
pkg-config --modversion opencv4 && echo "✓ OpenCV found" || echo "✗ OpenCV NOT found"
[ -f /usr/include/nlohmann/json.hpp ] && echo "✓ nlohmann_json found" || echo "✗ nlohmann_json NOT found"
pkg-config --modversion simpleble && echo "✓ SimpleBLE found" || echo "✗ SimpleBLE NOT found"

echo ""
echo "=================================="
echo "Setup Complete!"
echo "=================================="
echo ""
echo "Next: Run the build"
echo "  cd ~/Project/IoT-Project-Vision-based-autonomous-RC-car-control-system"
echo "  ./build.sh                    # Main project"
echo "  ./RBP4/build_rbp4.sh          # Optimized RBP4 version"
echo ""
