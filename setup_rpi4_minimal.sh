#!/bin/bash
# Minimal Raspberry Pi 4 Setup (without building SimpleBLE from source)

set -e

echo "=========================================="
echo "Raspberry Pi 4 Minimal Setup"
echo "=========================================="

# Update system
echo "[1/5] Updating system packages..."
sudo apt update

# Install dependencies
echo "[2/5] Installing dependencies..."
sudo apt install -y \
    cmake \
    build-essential \
    pkg-config \
    libopencv-dev \
    libbluetooth-dev \
    libdbus-1-dev \
    nlohmann-json3-dev \
    git

# Try to install SimpleBLE from system packages (if available)
echo "[3/5] Checking for SimpleBLE..."
if ! pkg-config --exists simpleble 2>/dev/null; then
    echo "[!] SimpleBLE not found in system packages"
    echo "[*] Building SimpleBLE from source..."
    
    # Clone SimpleBLE if not exists
    if [ ! -d "SimpleBLE" ]; then
        git clone https://github.com/OpenBluetoothToolbox/SimpleBLE.git
        cd SimpleBLE
        git submodule update --init --recursive
    else
        cd SimpleBLE
        git pull || true
        git submodule update --init --recursive
    fi
    
    # Build SimpleBLE (correct path to simpleble subdirectory)
    mkdir -p build_simpleble
    cd build_simpleble
    cmake ../simpleble -DCMAKE_BUILD_TYPE=Release -DLIBFMT_VENDORIZE=ON
    make -j$(nproc)
    sudo make install
    sudo ldconfig
    cd ../..
    echo "[✓] SimpleBLE installed"
else
    echo "[✓] SimpleBLE already installed"
fi

# Enable performance governor
echo "[4/5] Setting CPU governor to performance..."
for cpu in /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor; do
    echo performance | sudo tee $cpu > /dev/null
done

# Build project
echo "[5/5] Building project for Raspberry Pi 4..."
rm -rf build
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
echo "Optional: Configure GPU memory (requires reboot)"
echo "  sudo nano /boot/firmware/config.txt"
echo "  Add: gpu_mem=256"
echo "  sudo reboot"
