#!/bin/bash
# Setup and run script for Mac

set -e  # Exit on error

echo "=========================================="
echo "C++ Setup and Test Script for Mac"
echo "=========================================="
echo ""

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Check for Xcode command line tools
echo "Checking for Xcode Command Line Tools..."
if ! xcode-select -p &>/dev/null; then
    echo -e "${YELLOW}Xcode Command Line Tools not found!${NC}"
    echo "Installing Xcode Command Line Tools..."
    echo "This will open a dialog - please click 'Install' and wait..."
    xcode-select --install
    echo ""
    echo "Please wait for installation to complete, then run this script again."
    exit 1
else
    echo -e "${GREEN}✓ Xcode Command Line Tools installed${NC}"
fi

# Check for C++ compiler
echo ""
echo "Checking for C++ compiler..."
if command -v clang++ &> /dev/null; then
    COMPILER="clang++"
    echo -e "${GREEN}✓ Found clang++${NC}"
    clang++ --version | head -1
elif command -v g++ &> /dev/null; then
    COMPILER="g++"
    echo -e "${GREEN}✓ Found g++${NC}"
    g++ --version | head -1
else
    echo -e "${RED}✗ No C++ compiler found!${NC}"
    echo "Please install Xcode Command Line Tools first."
    exit 1
fi

# Check for make
echo ""
echo "Checking for make..."
if command -v make &> /dev/null; then
    echo -e "${GREEN}✓ Found make${NC}"
    make --version | head -1
else
    echo -e "${YELLOW}⚠ make not found (optional)${NC}"
fi

# Compile the test program
echo ""
echo "=========================================="
echo "Compiling test_ble_mac.cpp..."
echo "=========================================="

$COMPILER -std=c++17 -Wall -Wextra -O2 -o test_ble_mac test_ble_mac.cpp

if [ $? -eq 0 ]; then
    echo -e "${GREEN}✓ Compilation successful!${NC}"
    echo ""
    echo "=========================================="
    echo "Running test program..."
    echo "=========================================="
    echo ""
    echo "Note: This program tests command generation."
    echo "For actual BLE connection, you'll need to integrate a BLE library."
    echo ""
    ./test_ble_mac
else
    echo -e "${RED}✗ Compilation failed!${NC}"
    exit 1
fi
