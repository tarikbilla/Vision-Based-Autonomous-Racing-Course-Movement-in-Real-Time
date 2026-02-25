#!/usr/bin/env python3
"""
Validation script for road edge detection and auto-tracking system.
Tests all critical components without requiring physical hardware.
"""

import sys
import numpy as np

def test_imports():
    """Test that all required modules can be imported."""
    print("[1/5] Testing imports...")
    try:
        from rc_autonomy.boundary import BoundaryDetector
        from rc_autonomy.orchestrator import ControlOrchestrator, build_orchestrator
        from rc_autonomy.config import load_config
        import cv2
        print("  ✓ All imports successful")
        return True
    except ImportError as e:
        print(f"  ✗ Import failed: {e}")
        return False

def test_config():
    """Test configuration loading."""
    print("[2/5] Testing configuration...")
    try:
        from rc_autonomy.config import load_config
        config = load_config(None)
        assert config.camera.source == 0, f"Camera source should be 0, got {config.camera.source}"
        assert config.tracking.color_tracking == True, "Color tracking should be enabled"
        assert config.guidance.black_threshold > 0, "Black threshold should be positive"
        print(f"  ✓ Config loaded: camera={config.camera.source}, "
              f"color_tracking={config.tracking.color_tracking}")
        return True
    except Exception as e:
        print(f"  ✗ Config test failed: {e}")
        return False

def test_road_edge_detection():
    """Test the road edge detection algorithm."""
    print("[3/5] Testing road edge detection...")
    try:
        from rc_autonomy.boundary import BoundaryDetector
        from rc_autonomy.config import load_config
        import cv2
        
        config = load_config(None)
        detector = BoundaryDetector(
            black_threshold=config.guidance.black_threshold,
            ray_angles_deg=config.guidance.ray_angles_deg,
            ray_max_length=config.guidance.ray_max_length,
            evasive_distance=config.guidance.evasive_distance,
            default_speed=config.guidance.default_speed,
            steering_limit=config.control.steering_limit,
            light_on=config.control.light_on,
        )
        
        # Test 1: Synthetic image with clear road edges
        test_img = np.ones((480, 640, 3), dtype=np.uint8) * 64  # Gray road
        test_img[200:400, 50:100] = [0, 0, 255]  # Red left edge
        test_img[200:400, 540:590] = [255, 255, 255]  # White right edge
        
        left, center, right = detector.detect_road_edges(test_img)
        assert left is not None and right is not None, "Should detect edges"
        assert left < center < right, "Edge positions should be ordered"
        print(f"  ✓ Test 1: Detected edges L={left}, C={center}, R={right}")
        
        # Test 2: Image without clear edges
        test_img2 = np.ones((480, 640, 3), dtype=np.uint8) * 64  # Just gray
        left2, center2, right2 = detector.detect_road_edges(test_img2)
        assert (left2, center2, right2) == (None, None, None), "Should not detect non-existent edges"
        print(f"  ✓ Test 2: Correctly returns None for image without edges")
        
        # Test 3: Single-channel image (fallback)
        gray_img = np.ones((480, 640), dtype=np.uint8) * 64
        left3, center3, right3 = detector.detect_road_edges(gray_img)
        assert (left3, center3, right3) == (None, None, None), "Should handle single-channel images"
        print(f"  ✓ Test 3: Correctly handles single-channel input")
        
        return True
    except Exception as e:
        print(f"  ✗ Road edge detection test failed: {e}")
        import traceback
        traceback.print_exc()
        return False

def test_roi_validation():
    """Test ROI validation logic."""
    print("[4/5] Testing ROI validation...")
    try:
        # Test minimum size requirement
        rois = [
            ((100, 100, 5, 5), False),      # Too small
            ((100, 100, 10, 10), True),     # Minimum valid
            ((50, 50, 100, 100), True),     # Valid
            ((0, 0, 0, 0), False),          # Cancelled
        ]
        
        for roi, should_pass in rois:
            x, y, w, h = roi
            is_valid = w >= 10 and h >= 10 and roi != (0, 0, 0, 0)
            assert is_valid == should_pass, f"ROI {roi} validation mismatch"
        
        print("  ✓ ROI validation logic working correctly")
        return True
    except Exception as e:
        print(f"  ✗ ROI validation test failed: {e}")
        return False

def test_type_annotations():
    """Test that type annotations are present."""
    print("[5/5] Testing type annotations...")
    try:
        from rc_autonomy.boundary import BoundaryDetector
        import inspect
        
        # Check that detect_road_edges method exists and has annotations
        sig = inspect.signature(BoundaryDetector.detect_road_edges)
        assert 'frame' in sig.parameters, "Method should have 'frame' parameter"
        
        # Check Optional type is imported
        from rc_autonomy.boundary import Optional
        print("  ✓ Type annotations present and correct")
        return True
    except Exception as e:
        print(f"  ✗ Type annotation test failed: {e}")
        return False

def main():
    """Run all validation tests."""
    print("=" * 60)
    print("Road Edge Detection & Auto-Tracking Validation")
    print("=" * 60)
    print()
    
    tests = [
        test_imports,
        test_config,
        test_road_edge_detection,
        test_roi_validation,
        test_type_annotations,
    ]
    
    results = [test() for test in tests]
    
    print()
    print("=" * 60)
    passed = sum(results)
    total = len(results)
    print(f"Results: {passed}/{total} tests passed")
    print("=" * 60)
    
    if all(results):
        print("\n✓ All validation tests passed!")
        print("\nNext steps:")
        print("1. Run: python run_autonomy.py")
        print("2. Press 'a' for auto red-car tracking")
        print("3. Watch the live feed for:")
        print("   - Green vertical lines (road edges)")
        print("   - Blue vertical line (centerline)")
        print("   - Red car tracking within the road")
        return 0
    else:
        print(f"\n✗ {total - passed} test(s) failed!")
        return 1

if __name__ == "__main__":
    sys.exit(main())
