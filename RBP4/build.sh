#!/usr/bin/env bash
# ============================================================
# build.sh — Build & run the autonomous controller on
#            Raspberry Pi 4 (Raspberry Pi OS / Ubuntu 22.04+)
# ============================================================
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# ── 1. System dependencies ───────────────────────────────────
echo "==> Updating apt and installing system dependencies..."
sudo apt-get update -y
sudo apt-get install -y \
    cmake \
    build-essential \
    git \
    libopencv-dev \
    libdbus-1-dev \
    libbluetooth-dev \
    bluez \
    bluez-tools

# ── 1.5. Ensure SimpleBLE is available (preferred backend) ──
if [ ! -f /usr/local/lib/cmake/simpleble/simplebleConfig.cmake ] && [ ! -f /usr/lib/cmake/simpleble/simplebleConfig.cmake ]; then
    echo "==> SimpleBLE not found. Building SimpleBLE from source..."
    TMP_DIR="/tmp/simpleble_build_$$"
    rm -rf "$TMP_DIR"
    git clone --depth 1 https://github.com/OpenBluetoothToolbox/SimpleBLE.git "$TMP_DIR"
    cmake -S "$TMP_DIR" -B "$TMP_DIR/build" -DCMAKE_BUILD_TYPE=Release
    cmake --build "$TMP_DIR/build" -j"$(nproc)"
    sudo cmake --install "$TMP_DIR/build"
    rm -rf "$TMP_DIR"
else
    echo "==> SimpleBLE detected."
fi

# ── 2. Build C++ binary ──────────────────────────────────────
echo "==> Building C++ controller with CMake..."
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/usr/local
cmake --build build -j"$(nproc)"

echo ""
echo "======================================================"
echo "  Build complete!"
echo "======================================================"
echo ""
echo "Run with BLE:"
echo ""
echo "  ./build/autonomous_centerline_controller_pure_pursuit_delay_fixed_cpp \\"
echo "      --name DRiFT-ED5C2384488D \\"
echo "      --max-deg 24 --steer-smooth 70 \\"
echo "      --ratio 0.9 --c-sign -1 --speed-k 0.08"
echo ""
echo "Discover nearby BLE devices:"
echo ""
echo "  ./build/autonomous_centerline_controller_pure_pursuit_delay_fixed_cpp --discover --scan-timeout 6"
echo ""
echo "Run with no-BLE (camera + vision only):"
echo ""
echo "  ./build/autonomous_centerline_controller_pure_pursuit_delay_fixed_cpp --no-ble"
echo "======================================================"
