#!/bin/bash

# RBP4 Run Script - HEADLESS (No Window)
# Perfect for Raspberry Pi without display

DIR="$(cd "$(dirname "$0")" && pwd)"
BIN="$DIR/build/VisionBasedRCCarControl_RBP4"
CFG="$DIR/config/config_rbp4_headless.json"

# Ensure no Qt display issues on headless system
export QT_QPA_PLATFORM=offscreen

echo "========================================="
echo "RBP4 - Vision RC Car (HEADLESS MODE)"
echo "========================================="
echo "Binary: $BIN"
echo "Config: $CFG"
echo "Resolution: 640x480 @ 15fps"
echo "GUI: DISABLED (no display needed)"
echo "Tracker: KCF"
echo ""
echo "Features:"
echo "  - Runs without display"
echo "  - Perfect for SSH/remote"
echo "  - Press 'a' then Enter to start auto red car detection"
echo "  - Lower CPU usage"
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

# Run in headless mode (no window)
"$BIN" --config "$CFG" "$@"
