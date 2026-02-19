#!/bin/bash

# Build script for Vision-Based Autonomous RC Car Control System (C++)

set -e

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
BUILD_DIR="$SCRIPT_DIR/build"
CMAKE_BUILD_TYPE="${1:-Release}"

echo "=========================================="
echo "Building C++ Project"
echo "=========================================="
echo "Build Type: $CMAKE_BUILD_TYPE"
echo ""

# Create build directory
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Run CMake
echo "[*] Running CMake..."
cmake -DCMAKE_BUILD_TYPE="$CMAKE_BUILD_TYPE" ..

# Build
echo "[*] Building..."
make -j4

echo ""
echo "[✓] Build completed successfully!"
echo "[✓] Executable: $BUILD_DIR/VisionBasedRCCarControl"
echo ""
echo "To run the application:"
echo "  cd $BUILD_DIR"
echo "  ./VisionBasedRCCarControl"
echo ""
echo "For simulation mode (no hardware):"
echo "  ./VisionBasedRCCarControl --simulate"
echo ""
