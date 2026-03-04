#!/bin/bash
# Raspberry Pi 4 Setup and Optimization Script

set -e

echo "=========================================="
echo "Raspberry Pi 4 Optimization Setup"
echo "=========================================="

# Update system
echo "[1/6] Updating system packages..."
sudo apt update
sudo apt upgrade -y

# Install dependencies
echo "[2/6] Installing dependencies..."
sudo apt install -y \
    cmake \
    build-essential \
    pkg-config \
    libopencv-dev \
    libbluetooth-dev \
    libdbus-1-dev \
    nlohmann-json3-dev \
    git

# Install SimpleBLE
echo "[3/6] Installing SimpleBLE..."
if [ ! -d "SimpleBLE" ]; then
    git clone https://github.com/OpenBluetoothToolbox/SimpleBLE.git
fi
cd SimpleBLE
git submodule update --init --recursive
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
sudo make install
sudo ldconfig
cd ../..

# Enable performance governor for better CPU performance
echo "[4/6] Setting CPU governor to performance..."
echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor

# Increase GPU memory (requires reboot)
echo "[5/6] Configuring GPU memory..."
if ! grep -q "gpu_mem=256" /boot/config.txt; then
    echo "gpu_mem=256" | sudo tee -a /boot/config.txt
    echo "[!] GPU memory set to 256MB - reboot required"
fi

# Build project
echo "[6/6] Building project for Raspberry Pi 4..."
mkdir -p build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
cd ..

echo ""
echo "=========================================="
echo "Setup Complete!"
echo "=========================================="
echo ""
echo "To run the optimized version:"
echo "  ./build/VisionBasedRCCarControl --rpi"
echo ""
echo "Note: Reboot required for GPU memory changes to take effect"
echo "  sudo reboot"
