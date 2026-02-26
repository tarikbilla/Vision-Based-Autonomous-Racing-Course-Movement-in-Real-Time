#!/bin/bash
# Quick test of live boundary detection with car detection improvement

echo "=========================================="
echo "LIVE BOUNDARY DETECTION TEST"
echo "=========================================="
echo ""
echo "This script tests the improved boundary detection"
echo "using adaptive threshold (not color-based)"
echo ""
echo "What to expect:"
echo "  1. Camera window opens"
echo "  2. Green lines show track boundaries (left + right)"
echo "  3. Blue dashed line shows road center"
echo "  4. Orange box shows detected car"
echo "  5. Green tint shows road area"
echo ""
echo "Controls:"
echo "  q - Quit"
echo "  s - Save raw frame"
echo "  d - Save display frame"
echo ""
echo "=========================================="
echo ""

# Activate virtual environment
source .venv/bin/activate

# Run the test
python test_live_boundaries.py --camera 0 --width 640 --height 480 --fps 30

echo ""
echo "=========================================="
echo "Test complete!"
echo "=========================================="
