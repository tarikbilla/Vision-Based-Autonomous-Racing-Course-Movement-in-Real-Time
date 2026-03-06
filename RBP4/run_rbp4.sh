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
echo "RBP4 - Vision RC Car (GUI Mode)"
echo "=========================================="
echo "Config: $CFG"
echo "Resolution: 640x480 @ 15fps"
echo "GUI: ENABLED (requires X11/desktop)"
echo ""

cd "$ROOT_DIR"
"$BIN" --config "$CFG" "$@"
