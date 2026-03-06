#!/bin/bash

# Headless Mode Run Script
# For systems without display (X11/Wayland)

DIR="$(cd "$(dirname "$0")" && pwd)"
BIN="$DIR/build/VisionBasedRCCarControl"
CFG="$DIR/config/config_headless.json"

# Force headless mode - disable Qt GUI (backup in case show_window doesn't work)
export QT_QPA_PLATFORM=offscreen

echo "========================================="
echo "Vision-Based RC Car (HEADLESS Mode)"
echo "========================================="
echo "Binary: $BIN"
echo "Config: $CFG"
echo "GUI: DISABLED (no display required)"
echo "Detection: Auto red car color detection"
echo ""
echo "Press Ctrl+C to stop"
echo ""

if [ ! -f "$BIN" ]; then
    echo "[!] Binary not found. Run ./build.sh first"
    exit 1
fi

if [ ! -f "$CFG" ]; then
    echo "[!] Config not found: $CFG"
    exit 1
fi

# Run with offscreen rendering (no X11 display needed)
"$BIN" --config "$CFG" "$@"
