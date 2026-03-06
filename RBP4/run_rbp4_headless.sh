#!/bin/bash

# RBP4 Headless Run Script
# For Raspberry Pi without display (headless mode)

DIR="$(cd "$(dirname "$0")" && pwd)"
BIN="$DIR/build/VisionBasedRCCarControl_RBP4"
CFG="$DIR/config/config_rbp4_headless.json"

# Force headless mode - disable Qt GUI (backup in case show_window doesn't work)
export QT_QPA_PLATFORM=offscreen

echo "========================================="
echo "RBP4 - Vision RC Car (HEADLESS Mode)"
echo "========================================="
echo "Binary: $BIN"
echo "Config: $CFG"
echo "Resolution: 640x480 @ 15fps"
echo "GUI: DISABLED (no display required)"
echo "Detection: Auto red car color detection"
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

# Run with offscreen rendering (no X11 display needed)
"$BIN" --config "$CFG" "$@"
