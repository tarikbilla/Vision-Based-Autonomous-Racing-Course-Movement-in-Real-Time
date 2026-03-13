#!/usr/bin/env bash
# ============================================================
# build.sh — Build the autonomous controller on macOS
# Requires: Xcode Command Line Tools, Homebrew, OpenCV 4
# ============================================================
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# ── 1. Check Xcode Command Line Tools ────────────────────────
if ! xcode-select -p &>/dev/null; then
    echo "==> Installing Xcode Command Line Tools..."
    xcode-select --install
    echo "    Re-run this script after installation completes."
    exit 1
fi

# ── 2. Check / install Homebrew deps ─────────────────────────
if ! command -v brew &>/dev/null; then
    echo "ERROR: Homebrew not found. Install from https://brew.sh"
    exit 1
fi

echo "==> Installing dependencies via Homebrew..."
brew install cmake opencv 2>/dev/null || true

# ── 3. Build C++ binary ──────────────────────────────────────
echo "==> Building with CMake..."
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(sysctl -n hw.logicalcpu)"

echo ""
echo "======================================================"
echo "  Build complete!"
echo "======================================================"
echo ""
echo "Run:"
echo ""
echo "  ./build/autonomous_centerline_controller_pure_pursuit_delay_fixed_cpp \\"
echo "      --name DRiFT-ED5C2384488D \\"
echo "      --max-deg 24 --steer-smooth 70 \\"
echo "      --ratio 0.9 --c-sign -1 --speed-k 0.08"
echo ""
echo "Discover BLE devices:"
echo "  ./build/autonomous_centerline_controller_pure_pursuit_delay_fixed_cpp --discover --scan-timeout 6"
echo ""
echo "No-BLE (vision only):"
echo "  ./build/autonomous_centerline_controller_pure_pursuit_delay_fixed_cpp --no-ble"
echo "======================================================"
