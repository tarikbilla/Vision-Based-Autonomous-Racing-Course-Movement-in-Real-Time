#!/bin/bash
# Quick build for Raspberry Pi 4 (when dependencies already installed)

set -e

echo "=========================================="
echo "Building for Raspberry Pi 4"
echo "=========================================="

# Set CPU governor to performance
echo "[1/2] Setting CPU to performance mode..."
for cpu in /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor; do
    if [ -w $cpu ]; then
        echo performance | sudo tee $cpu > /dev/null
    fi
done

# Build project
echo "[2/2] Building project..."
rm -rf build
mkdir -p build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
cd ..

echo ""
echo "=========================================="
echo "Build Complete!"
echo "=========================================="
echo ""
echo "Run with:"
echo "  ./build/VisionBasedRCCarControl --rpi"
echo ""
