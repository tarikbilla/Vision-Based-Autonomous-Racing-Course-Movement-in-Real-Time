#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$ROOT_DIR/build"

echo "[RBP4] Clean build started..."
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"

cd "$BUILD_DIR"
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j"$(getconf _NPROCESSORS_ONLN || echo 4)"

echo "[RBP4] Build complete: $BUILD_DIR/VisionBasedRCCarControl_RBP4"