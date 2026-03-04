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
    cd SimpleBLE
    git submodule update --init --recursive
else
    cd SimpleBLE
    git pull
    git submodule update --init --recursive
fi

# Build and install SimpleBLE
mkdir -p build_simpleble && cd build_simpleble
cmake ../simpleble -DCMAKE_BUILD_TYPE=Release -DLIBFMT_VENDORIZE=ON
make -j$(nproc)
sudo make install
sudo ldconfig
cd ../..

# Enable performance governor for better CPU performance
echo "[4/6] Setting CPU governor to performance..."
echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor

# Increase GPU memory (requires reboot)
echo "[5/6] Configuring GPU memory..."
if [ -f /boot/firmware/config.txt ]; then
    # Raspberry Pi OS Bookworm and newer
    CONFIG_FILE="/boot/firmware/config.txt"
elif [ -f /boot/config.txt ]; then
    # Older Raspberry Pi OS
    CONFIG_FILE="/boot/config.txt"
else
    echo "[!] Could not find config.txt, skipping GPU memory configuration"
    CONFIG_FILE=""
fi

if [ -n "$CONFIG_FILE" ] && ! sudo grep -q "gpu_mem=256" "$CONFIG_FILE"; then
    echo "gpu_mem=256" | sudo tee -a "$CONFIG_FILE"
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
