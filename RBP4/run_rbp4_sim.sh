#!/bin/bash

# RBP4 Simulation Script - NO WINDOW
# For testing without any hardware (camera or BLE)

DIR="$(cd "$(dirname "$0")" && pwd)"
BIN="$DIR/build/VisionBasedRCCarControl_RBP4"
CFG="$DIR/config/config_rbp4_gui.json"

echo "========================================="
echo "RBP4 - Vision RC Car (SIMULATION MODE)"
echo "========================================="
echo "Binary: $BIN"
echo "Config: $CFG"
echo "Resolution: 640x480 @ 15fps"
echo "GUI: DISABLED"
echo "Hardware: SIMULATED (no camera/BLE needed)"
echo ""
echo "Perfect for:"
echo "  - Testing without physical hardware"
echo "  - Algorithm development"
echo "  - SSH/remote testing"
echo ""
echo "Press Ctrl+C to stop"
echo ""

if [ ! -f "$BIN" ]; then
    echo "[!] Binary not found. Run ./RBP4/build_rbp4.sh first"
    exit 1
fi

if [ ! -f "$CFG" ]; then
    echo "[!] Config not found: $CFG"
    exit 1
fi

# Ensure no Qt display issues
export QT_QPA_PLATFORM=offscreen

# Run in simulation mode (no camera, no BLE)
"$BIN" --config "$CFG" --simulate "$@"
