#!/usr/bin/env bash
# ──────────────────────────────────────────────────────────────────
#  build.sh — Build VisionRC_FINAL
# ──────────────────────────────────────────────────────────────────
set -e

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD="$DIR/build"
TYPE="${1:-Release}"

echo "══════════════════════════════════════"
echo "  VisionRC_FINAL  —  Build ($TYPE)"
echo "══════════════════════════════════════"

mkdir -p "$BUILD"
cd "$BUILD"

cmake -DCMAKE_BUILD_TYPE="$TYPE" ..
cmake --build . -j"$(nproc 2>/dev/null || sysctl -n hw.ncpu)"

echo ""
echo "══════════════════════════════════════"
echo "  ✓  Build succeeded!"
echo "  Binary: $BUILD/VisionRC_FINAL"
echo "══════════════════════════════════════"
echo ""
echo "Examples:"
echo ""
echo "  # Discover BLE devices"
echo "  ./build/VisionRC_FINAL --discover"
echo ""
echo "  # Connect by name"
echo "  ./build/VisionRC_FINAL --name DRiFT"
echo ""
echo "  # Connect by address (no camera)"
echo "  ./build/VisionRC_FINAL --address ED5C2384488D --no-ble"
echo ""
echo "  # Force a specific BLE backend"
echo "  ./build/VisionRC_FINAL --name DRiFT --ble-backend simpleble"
echo ""
