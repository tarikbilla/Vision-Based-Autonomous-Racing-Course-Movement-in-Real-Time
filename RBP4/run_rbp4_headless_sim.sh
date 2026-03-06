#!/bin/bash

# RBP4 Headless Simulation Mode Run Script
# For testing without BLE hardware or camera

DIR="$(cd "$(dirname "$0")" && pwd)"
BIN="$DIR/RBP4/build/VisionBasedRCCarControl_RBP4"
CFG="$DIR/RBP4/config/config_rbp4_headless_sim.json"

# Force headless mode
export QT_QPA_PLATFORM=offscreen

echo "=========================================="
echo "RBP4 - Vision RC Car (SIMULATION Mode)"
echo "=========================================="
echo "Binary: $BIN"
echo "Config: $CFG"
echo "GUI: DISABLED"
echo "Camera: SIMULATED (generates synthetic frames)"
echo "BLE: SIMULATED (no hardware needed)"
echo ""
echo "This mode is perfect for testing detection algorithms"
echo "without actual hardware."
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

# Run with --simulate flag
"$BIN" --config "$CFG" --simulate "$@"
