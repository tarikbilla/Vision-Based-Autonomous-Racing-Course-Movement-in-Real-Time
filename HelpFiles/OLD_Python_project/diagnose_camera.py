#!/usr/bin/env python3
"""Diagnostic script to find available cameras."""

import cv2

print("Scanning for available cameras...")
found = False
for i in range(20):
    cap = cv2.VideoCapture(i)
    if cap.isOpened():
        w = cap.get(cv2.CAP_PROP_FRAME_WIDTH)
        h = cap.get(cv2.CAP_PROP_FRAME_HEIGHT)
        fps = cap.get(cv2.CAP_PROP_FPS)
        print(f"  ✓ Camera {i}: {int(w)}x{int(h)} @ {int(fps)} FPS")
        found = True
        cap.release()

if not found:
    print("  ✗ No cameras found via OpenCV")
    print("\nTroubleshooting:")
    print("  1. Check if Sony DSLR is connected via USB")
    print("  2. Check System Preferences > Security & Privacy > Camera access")
    print("  3. Try: ffmpeg -f avfoundation -list_devices true -i ''")
    print("  4. Try: python -c 'import cv2; print(cv2.__version__)'")
