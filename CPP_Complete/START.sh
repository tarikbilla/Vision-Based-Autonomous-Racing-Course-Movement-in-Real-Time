#!/bin/bash

# Vision-Based RC Car Autonomous Control - START SCRIPT
# This script handles everything needed to run the system

set -e

PROJECT_DIR="/Users/tarikbilla/Projects/IoT-Project-Vision-based-autonomous-RC-car-control-system/CPP_Complete"
BUILD_DIR="$PROJECT_DIR/build"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Vision-Based RC Car - Autonomous Control${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# Check if binary exists
if [ ! -f "$BUILD_DIR/VisionBasedRCCarControl" ]; then
    echo -e "${YELLOW}[*] Binary not found. Building...${NC}"
    cd "$BUILD_DIR"
    cmake .. > /dev/null 2>&1
    make -j4 > /dev/null 2>&1
    cd -
    echo -e "${GREEN}[✓] Build complete${NC}"
else
    echo -e "${GREEN}[✓] Binary found${NC}"
fi

echo ""
echo -e "${BLUE}========== SYSTEM READY ==========${NC}"
echo ""
echo -e "${YELLOW}Controls when system starts:${NC}"
echo -e "${GREEN}  Press 'a'${NC}  → ${YELLOW}AUTO DETECT car (recommended - motion-based)${NC}"
echo -e "${GREEN}  Press 's'${NC}  → ${YELLOW}Manual selection (click to select car box)${NC}"
echo -e "${GREEN}  Press 'q'${NC}  → ${YELLOW}Quit visualization${NC}"
echo -e "${GREEN}  Ctrl+C${NC}    → ${YELLOW}EMERGENCY STOP (stops car + disconnects)${NC}"
echo ""

echo -e "${YELLOW}What will happen:${NC}"
echo "  1. Camera window opens"
echo "  2. Press 'a' to auto-detect car"
echo "  3. System finds orange/red car on track"
echo "  4. Car follows centerline automatically"
echo "  5. Display shows:"
echo "     - Red circle = car position"
echo "     - Red box = car bounding box"
echo "     - Blue/Green lines = track boundaries"
echo "     - Cyan line = centerline path"
echo ""

echo -e "${YELLOW}Configuration:${NC}"
echo "  Car MAC: f9:af:3c:e2:d2:f5 (from config.json)"
echo "  Camera: Webcam index 0 (1280x720)"
echo "  Speed: 20 (configurable in config.json)"
echo "  Steering: 50 max (configurable)"
echo ""

echo -e "${BLUE}[*] Starting system...${NC}"
echo ""

cd "$PROJECT_DIR"
./build/VisionBasedRCCarControl

echo ""
echo -e "${BLUE}========== SHUTDOWN ==========${NC}"
echo "System stopped successfully"
