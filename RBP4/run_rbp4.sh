#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
BIN="$ROOT_DIR/build/VisionBasedRCCarControl_RBP4"
CFG="$ROOT_DIR/config/config_rbp4_ultra_low.json"

if [[ ! -x "$BIN" ]]; then
  echo "[RBP4] Binary not found. Build first: ./RBP4/build_rbp4.sh"
  exit 1
fi

cd "$ROOT_DIR"
"$BIN" --config "$CFG" "$@"