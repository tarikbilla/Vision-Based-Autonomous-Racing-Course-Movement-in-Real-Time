#!/usr/bin/env python3
"""
Specialized boundary detection for circular/round tracks viewed from corner angle.

For a round track:
- The entire boundary is one or two large contours
- We need to find the left and right edges by analyzing the boundary contour
- Center is roughly in the middle of the track image
"""

import cv2
import numpy as np

def detect_circular_track_edges(frame):
    """
    Detect road edges for circular/round tracks.
    
    Strategy:
    1. Find the main track boundary contour
    2. Find the leftmost and rightmost points on the boundary
    3. Use those as left/center/right positions
    
    Args:
        frame: Camera frame (BGR)
    
    Returns:
        (left_x, center_x, right_x, road_mask) or (None, None, None, None)
    """
    
    if len(frame.shape) == 2:
        # Grayscale - convert to BGR
        frame = cv2.cvtColor(frame, cv2.COLOR_GRAY2BGR)
    
    height, width = frame.shape[:2]
    hsv = cv2.cvtColor(frame, cv2.COLOR_BGR2HSV)
    
    # ===== DETECT WHITE MARKERS =====
    white_mask1 = cv2.inRange(hsv, np.array([0, 0, 180]), np.array([180, 30, 255]))
    white_mask2 = cv2.inRange(hsv, np.array([0, 0, 120]), np.array([180, 60, 200]))
    white_mask3 = cv2.inRange(hsv, np.array([0, 0, 100]), np.array([180, 80, 180]))
    white_mask = cv2.bitwise_or(cv2.bitwise_or(white_mask1, white_mask2), white_mask3)
    
    # ===== DETECT RED MARKERS =====
    red_mask1 = cv2.inRange(hsv, np.array([0, 40, 30]), np.array([15, 255, 255]))
    red_mask2 = cv2.inRange(hsv, np.array([160, 40, 30]), np.array([180, 255, 255]))
    red_mask = cv2.bitwise_or(red_mask1, red_mask2)
    
    # ===== COMBINE MARKERS =====
    edge_mask = cv2.bitwise_or(white_mask, red_mask)
    
    # ===== MORPHOLOGICAL CLEANING =====
    kernel_small = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (3, 3))
    kernel_medium = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (5, 5))
    
    edge_mask = cv2.morphologyEx(edge_mask, cv2.MORPH_OPEN, kernel_small, iterations=1)
    edge_mask = cv2.morphologyEx(edge_mask, cv2.MORPH_CLOSE, kernel_medium, iterations=2)
    edge_mask = cv2.dilate(edge_mask, kernel_medium, iterations=3)
    
    # ===== FIND CONTOURS =====
    contours, _ = cv2.findContours(edge_mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
    
    if len(contours) == 0:
        return None, None, None, None
    
    # ===== FIND LARGEST CONTOUR (the track boundary) =====
    largest_contour = max(contours, key=cv2.contourArea)
    
    if cv2.contourArea(largest_contour) < 500:  # Too small
        return None, None, None, None
    
    # ===== GET BOUNDING BOX OF TRACK =====
    x, y, w, h = cv2.boundingRect(largest_contour)
    
    # ===== GET LEFTMOST AND RIGHTMOST POINTS =====
    # For a circular track viewed from corner, the edges are on left and right sides
    leftmost_point = tuple(largest_contour[largest_contour[:, :, 0].argmin()][0])
    rightmost_point = tuple(largest_contour[largest_contour[:, :, 0].argmax()][0])
    
    left_x = leftmost_point[0]
    right_x = rightmost_point[0]
    center_x = (left_x + right_x) // 2
    
    # ===== VERIFY EDGE ORDERING =====
    if left_x >= center_x or center_x >= right_x:
        # Fallback: use bounding box
        left_x = x
        right_x = x + w
        center_x = (left_x + right_x) // 2
    
    return left_x, center_x, right_x, edge_mask


# ===== TEST THE FUNCTION =====
if __name__ == "__main__":
    test_img = cv2.imread("Path-image/image-1.png")
    
    if test_img is not None:
        left, center, right, mask = detect_circular_track_edges(test_img)
        
        print(f"Circular Track Edge Detection Results:")
        print(f"  Left:   {left}")
        print(f"  Center: {center}")
        print(f"  Right:  {right}")
        
        if left is not None:
            # Draw on image
            result = test_img.copy()
            cv2.line(result, (left, 0), (left, test_img.shape[0]), (255, 0, 0), 3)      # Blue left
            cv2.line(result, (center, 0), (center, test_img.shape[0]), (0, 255, 255), 2)  # Cyan center
            cv2.line(result, (right, 0), (right, test_img.shape[0]), (0, 0, 255), 3)    # Red right
            
            cv2.imwrite("Path-image/circular_track_edges.png", result)
            print("  ✓ Saved: Path-image/circular_track_edges.png")
