#!/bin/bash

# Vision-Based RC Car Autonomous Control - Quick Start
# This script builds and runs the complete autonomous control system

set -e

PROJECT_DIR="/Users/tarikbilla/Projects/IoT-Project-Vision-based-autonomous-RC-car-control-system/CPP_Complete"
BUILD_DIR="$PROJECT_DIR/build"

echo "=================================================="
echo "Vision-Based RC Car Autonomous Control System"
echo "=================================================="
echo ""

# Step 1: Check dependencies
echo "[1/4] Checking dependencies..."
if ! command -v cmake &> /dev/null; then
    echo "❌ cmake not found. Install with: brew install cmake"
    exit 1
fi

if ! command -v clang++ &> /dev/null; then
    echo "❌ clang++ not found. Install Xcode command line tools."
    exit 1
fi

# Check OpenCV
if ! pkg-config --exists opencv4; then
    echo "❌ OpenCV4 not found. Install with: brew install opencv"
    exit 1
fi

echo "✓ All dependencies found"
echo ""

# Step 2: Build project
echo "[2/4] Building C++ project..."
if [ ! -d "$BUILD_DIR" ]; then
    mkdir -p "$BUILD_DIR"
fi

cd "$BUILD_DIR"
cmake .. > /dev/null 2>&1
make -j4 > /dev/null 2>&1

if [ ! -f "$BUILD_DIR/VisionBasedRCCarControl" ]; then
    echo "❌ Build failed"
    exit 1
fi

echo "✓ Build successful"
echo ""

# Step 3: Verify configuration
echo "[3/4] Checking configuration..."
if [ ! -f "$PROJECT_DIR/config/config.json" ]; then
    echo "❌ config/config.json not found"
    echo "   Create it with your car's BLE MAC address"
    exit 1
fi

CONFIG_MAC=$(grep -o '"device_mac": "[^"]*"' "$PROJECT_DIR/config/config.json" | cut -d'"' -f4)
if [ -z "$CONFIG_MAC" ] || [ "$CONFIG_MAC" = "XX:XX:XX:XX:XX:XX" ]; then
    echo "⚠️  Warning: Car MAC address not configured"
    echo "   Update config/config.json with your car's BLE MAC"
    echo "   Use 'python3 scan_and_connect.py' to find it"
else
    echo "✓ Configured for car: $CONFIG_MAC"
fi
echo ""

# Step 4: Run system
echo "[4/4] Starting autonomous control system..."
echo ""
echo "Commands:"
echo "  'a' - Auto car detection (motion-based)"
echo "  's' - Manual car selection (click ROI)"
echo "  'q' - Quit visualization"
echo "  Ctrl+C - Stop car and disconnect"
echo ""
echo "=================================================="
echo ""

cd "$PROJECT_DIR"
./build/VisionBasedRCCarControl

echo ""
echo "=================================================="
echo "System stopped"
echo "=================================================="
