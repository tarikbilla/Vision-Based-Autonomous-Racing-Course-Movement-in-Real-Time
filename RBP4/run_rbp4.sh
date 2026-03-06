#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
BIN="$ROOT_DIR/build/VisionBasedRCCarControl_RBP4"
CFG="$ROOT_DIR/config/config_rbp4_gui.json"

if [[ ! -x "$BIN" ]]; then
  echo "[RBP4] Binary not found. Build first: ./RBP4/build_rbp4.sh"
  exit 1
fi

echo "=========================================="
echo "RBP4 - Vision RC Car (WITH WINDOW)"
echo "=========================================="
echo "Config: $CFG"
echo "Resolution: 640x480 @ 15fps"
echo "GUI: ENABLED (shows live detection)"
echo ""
echo "Controls:"
echo "  q = Quit"
echo "  s = Manual tracking (select ROI)"
echo "  a = Auto detection (red car)"
echo ""
echo "Note: Requires HDMI display or X11 forwarding"
echo "      For headless RPi: ./RBP4/run_rbp4_headless.sh"
echo ""

cd "$ROOT_DIR"
"$BIN" --config "$CFG" "$@"
