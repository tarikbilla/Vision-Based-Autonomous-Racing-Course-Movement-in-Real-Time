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
    libopencv-dev \
    python3 \
    python3-venv \
    python3-pip \
    git

# ── 2. Python virtual-env setup ──────────────────────────────
if [ ! -d ".venv" ]; then
    echo "==> Creating Python virtual environment..."
    python3 -m venv .venv
fi

echo "==> Installing Python packages into .venv..."
.venv/bin/pip install --upgrade pip --quiet
.venv/bin/pip install opencv-python-headless numpy scipy bleak --quiet

# ── 3. Build C++ binary ──────────────────────────────────────
echo "==> Building C++ controller with CMake..."
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"

echo ""
echo "======================================================"
echo "  Build complete!"
echo "======================================================"
echo ""
echo "Run with BLE (delegates to Python on macOS / runs C++ natively):"
echo ""
echo "  ./build/autonomous_centerline_controller_pure_pursuit_delay_fixed_cpp \\"
echo "      --name DRiFT-ED5C2384488D \\"
echo "      --max-deg 24 --steer-smooth 70 \\"
echo "      --ratio 0.9 --c-sign -1 --speed-k 0.08"
echo ""
echo "Or run the Python script directly:"
echo ""
echo "  .venv/bin/python autonomous_centerline_controller_pure_pursuit_delay_fixed.py \\"
echo "      --name DRiFT-ED5C2384488D \\"
echo "      --max-deg 24 --steer-smooth 70 \\"
echo "      --ratio 0.9 --c-sign -1 --speed-k 0.08"
echo ""
echo "Or run with no-BLE (camera + vision only, no car):"
echo ""
echo "  ./build/autonomous_centerline_controller_pure_pursuit_delay_fixed_cpp --no-ble"
echo "======================================================"
