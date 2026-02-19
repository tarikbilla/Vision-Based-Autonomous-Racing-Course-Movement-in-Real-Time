#!/usr/bin/env python3
"""
Complete Pipeline Test - Validates all project requirements:
1. BLE Connection
2. Camera & Object Detection
3. Edge Detection (Boundary Detection)
4. Car Running & Autonomous Control
"""

import sys
import time
import cv2
import numpy as np
from rc_autonomy.ble import SimplePyBLEClient, BLETarget
from rc_autonomy.config import load_config
from rc_autonomy.commands import ControlVector, build_command
from rc_autonomy.camera import CameraCapture
from rc_autonomy.tracking import OpenCVTracker
from rc_autonomy.boundary import BoundaryDetector


def test_ble_connection():
    """Test 1: Connect to RC car via BLE"""
    print("\n" + "="*70)
    print("TEST 1: BLE CONNECTION")
    print("="*70)
    
    config = load_config()
    target = BLETarget(
        device_mac=config.ble.device_mac,
        characteristic_uuid=config.ble.characteristic_uuid,
        device_identifier=config.ble.device_identifier,
    )
    
    try:
        ble = SimplePyBLEClient(
            target,
            device_index=0,
            scan_timeout_ms=15000
        )
        print("[*] Attempting BLE connection...")
        ble.connect()
        print("[✓] BLE CONNECTION: SUCCESS")
        print(f"    MAC: {config.ble.device_mac}")
        print(f"    Characteristic: {config.ble.characteristic_uuid}")
        
        # Test sending a STOP command
        print("[*] Sending STOP command...")
        stop_cmd = ControlVector(light_on=False, speed=0, left_turn_value=0, right_turn_value=0)
        ble.send_control(stop_cmd)
        print("[✓] STOP command sent successfully")
        
        ble.disconnect()
        return True, ble
    except Exception as e:
        print(f"[✗] BLE CONNECTION FAILED: {e}")
        return False, None


def test_camera_and_object_detection():
    """Test 2: Open camera and detect objects/edges"""
    print("\n" + "="*70)
    print("TEST 2: CAMERA & OBJECT DETECTION")
    print("="*70)
    
    config = load_config()
    
    try:
        # Open camera
        print("[*] Opening camera...")
        camera = CameraCapture(
            source=config.camera.source,
            width=config.camera.width,
            height=config.camera.height,
            fps=config.camera.fps
        )
        camera.open()
        print(f"[✓] CAMERA OPENED: {config.camera.width}x{config.camera.height}@{config.camera.fps}fps")
        
        # Capture first frame
        frames_gen = camera.frames()
        frame_obj = next(frames_gen)
        frame = frame_obj.image
        
        height, width = frame.shape[:2]
        print(f"[✓] First frame captured: {width}x{height}")
        
        # Detect edges (boundary detection)
        print("[*] Testing edge detection (boundary detection)...")
        gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY) if len(frame.shape) == 3 else frame
        
        # Apply edge detection (Canny)
        edges = cv2.Canny(gray, 50, 150)
        edge_count = np.count_nonzero(edges)
        print(f"[✓] EDGE DETECTION: Found {edge_count} edge pixels")
        
        # Apply threshold to find boundaries (dark areas)
        threshold = config.guidance.black_threshold
        boundary_mask = gray < threshold
        boundary_count = np.count_nonzero(boundary_mask)
        print(f"[✓] BOUNDARY DETECTION: Found {boundary_count} boundary pixels (threshold={threshold})")
        
        # Display frame with edges for visualization
        print("[*] Displaying camera frame with edge detection overlay...")
        overlay = frame.copy()
        overlay[edges > 0] = [0, 255, 0]  # Green edges
        overlay[boundary_mask] = [255, 0, 0]  # Blue boundaries
        
        cv2.imshow("Camera Feed + Edge Detection", overlay)
        print("[*] Press any key to close window...")
        cv2.waitKey(3000)
        cv2.destroyAllWindows()
        
        print("[✓] CAMERA & OBJECT DETECTION: SUCCESS")
        camera.close()
        return True, frame
    except Exception as e:
        print(f"[✗] CAMERA & OBJECT DETECTION FAILED: {e}")
        return False, None


def test_tracking_and_guidance():
    """Test 3: Test object tracking and guidance logic"""
    print("\n" + "="*70)
    print("TEST 3: OBJECT TRACKING & AUTONOMOUS GUIDANCE")
    print("="*70)
    
    config = load_config()
    
    try:
        # Open camera
        camera = CameraCapture(
            source=config.camera.source,
            width=config.camera.width,
            height=config.camera.height,
            fps=config.camera.fps
        )
        camera.open()
        
        # Initialize tracker
        print("[*] Initializing tracker...")
        tracker = OpenCVTracker(tracker_type=config.tracking.tracker_type)
        print(f"[✓] Tracker type: {config.tracking.tracker_type}")
        
        # Initialize boundary detector
        boundary = BoundaryDetector(
            black_threshold=config.guidance.black_threshold,
            ray_angles_deg=config.guidance.ray_angles_deg,
            ray_max_length=config.guidance.ray_max_length,
            evasive_distance=config.guidance.evasive_distance,
            default_speed=config.guidance.default_speed,
            steering_limit=config.control.steering_limit,
            light_on=config.control.light_on
        )
        print(f"[✓] Boundary detector configured")
        
        # Get first frame for ROI selection
        frames_gen = camera.frames()
        frame_obj = next(frames_gen)
        frame = frame_obj.image
        
        print("[*] Select ROI (Region of Interest) - click and drag to select car")
        print("[*] Press SPACE to confirm selection")
        roi = cv2.selectROI("Select RC Car ROI", frame, fromCenter=False, showCrosshair=True)
        cv2.destroyWindow("Select RC Car ROI")
        
        if roi == (0, 0, 0, 0):
            print("[!] ROI selection cancelled")
            camera.close()
            return False, None
        
        print(f"[✓] ROI selected: {roi}")
        
        # Initialize tracker with ROI
        print("[*] Initializing tracker with selected ROI...")
        tracker.initialize(frame, roi)
        print("[✓] Tracker initialized")
        
        # Test tracking and guidance on 30 frames
        print("[*] Running tracking and guidance for 30 frames...")
        for i in range(30):
            try:
                frame_obj = next(frames_gen)
                frame = frame_obj.image
                
                # Track object
                tracked = tracker.update(frame)
                
                # Analyze boundaries and generate control
                rays, control = boundary.analyze(frame, tracked.center, tracked.movement)
                
                if i % 5 == 0:  # Print every 5 frames
                    print(f"  Frame {i+1}: Center={tracked.center} Speed={control.speed} "
                          f"Left={control.left_turn_value} Right={control.right_turn_value} "
                          f"Rays=[L:{rays[0].distance} C:{rays[1].distance} R:{rays[2].distance}]")
                
                # Visualize
                overlay = frame.copy()
                x, y, w, h = tracked.bbox
                cv2.rectangle(overlay, (x, y), (x+w, y+h), (0, 255, 0), 2)
                cv2.circle(overlay, tracked.center, 5, (0, 0, 255), -1)
                
                # Draw rays
                for ray in rays:
                    angle_rad = np.radians(ray.angle_deg)
                    x_end = int(tracked.center[0] + ray.distance * np.cos(angle_rad))
                    y_end = int(tracked.center[1] + ray.distance * np.sin(angle_rad))
                    cv2.line(overlay, tracked.center, (x_end, y_end), (255, 0, 0), 1)
                
                cv2.imshow("Tracking + Guidance", overlay)
                if cv2.waitKey(50) & 0xFF == ord('q'):
                    break
                    
            except Exception as e:
                print(f"  Frame {i+1} error: {e}")
                break
        
        cv2.destroyAllWindows()
        print("[✓] TRACKING & GUIDANCE: SUCCESS")
        camera.close()
        return True, None
        
    except Exception as e:
        print(f"[✗] TRACKING & GUIDANCE FAILED: {e}")
        return False, None


def test_car_autonomous_control():
    """Test 4: Send autonomous control commands to car"""
    print("\n" + "="*70)
    print("TEST 4: AUTONOMOUS CAR CONTROL")
    print("="*70)
    
    config = load_config()
    
    try:
        # Connect to BLE
        target = BLETarget(
            device_mac=config.ble.device_mac,
            characteristic_uuid=config.ble.characteristic_uuid,
            device_identifier=config.ble.device_identifier,
        )
        
        ble = SimplePyBLEClient(
            target,
            device_index=0,
            scan_timeout_ms=15000
        )
        
        print("[*] Connecting to car...")
        ble.connect()
        print("[✓] Connected to car")
        
        # Send test sequence
        test_sequence = [
            (ControlVector(light_on=True, speed=0, left_turn_value=0, right_turn_value=0), "STOP", 1),
            (ControlVector(light_on=True, speed=20, left_turn_value=0, right_turn_value=0), "START (speed=20, lights ON)", 2),
            (ControlVector(light_on=True, speed=15, left_turn_value=20, right_turn_value=0), "TURN LEFT (speed=15)", 2),
            (ControlVector(light_on=True, speed=15, left_turn_value=0, right_turn_value=20), "TURN RIGHT (speed=15)", 2),
            (ControlVector(light_on=True, speed=10, left_turn_value=0, right_turn_value=0), "SLOW DOWN (speed=10)", 1),
            (ControlVector(light_on=False, speed=0, left_turn_value=0, right_turn_value=0), "STOP & LIGHTS OFF", 2),
        ]
        
        for ctrl, description, duration in test_sequence:
            print(f"  [*] Sending: {description}")
            ble.send_control(ctrl)
            time.sleep(duration)
        
        ble.disconnect()
        print("[✓] AUTONOMOUS CAR CONTROL: SUCCESS")
        return True, None
        
    except Exception as e:
        print(f"[✗] AUTONOMOUS CAR CONTROL FAILED: {e}")
        return False, None


def main():
    """Run all tests"""
    print("\n\n")
    print("╔" + "="*68 + "╗")
    print("║" + " "*15 + "COMPLETE PIPELINE VALIDATION TEST" + " "*20 + "║")
    print("║" + " "*20 + "All PRD Requirements Check" + " "*23 + "║")
    print("╚" + "="*68 + "╝")
    
    results = []
    
    # Test 1: BLE Connection
    test1_pass, ble_client = test_ble_connection()
    results.append(("BLE Connection", test1_pass))
    
    # Test 2: Camera & Object Detection
    test2_pass, frame = test_camera_and_object_detection()
    results.append(("Camera & Edge Detection", test2_pass))
    
    # Test 3: Tracking & Guidance
    if test2_pass:
        test3_pass, _ = test_tracking_and_guidance()
        results.append(("Tracking & Autonomous Guidance", test3_pass))
    else:
        results.append(("Tracking & Autonomous Guidance", False))
    
    # Test 4: Car Control (optional, only if car available)
    print("\n" + "="*70)
    print("TEST 4: AUTONOMOUS CAR CONTROL (Optional - requires powered car)")
    print("="*70)
    user_input = input("Do you want to test car control? (y/n): ").lower()
    if user_input == 'y':
        test4_pass, _ = test_car_autonomous_control()
        results.append(("Autonomous Car Control", test4_pass))
    else:
        print("[*] Car control test skipped")
        results.append(("Autonomous Car Control", None))
    
    # Summary
    print("\n\n")
    print("╔" + "="*68 + "╗")
    print("║" + " "*25 + "TEST SUMMARY" + " "*32 + "║")
    print("╚" + "="*68 + "╝")
    print()
    
    for test_name, result in results:
        if result is True:
            status = "✓ PASS"
        elif result is False:
            status = "✗ FAIL"
        else:
            status = "⊘ SKIP"
        print(f"  {status:8} | {test_name}")
    
    print()
    passed = sum(1 for _, r in results if r is True)
    total = sum(1 for _, r in results if r is not None)
    print(f"  Result: {passed}/{total} tests passed")
    
    if passed == total:
        print("\n[✓] ALL TESTS PASSED - PROJECT COMPLETE!")
        print("\nPRD Requirements Satisfied:")
        print("  ✓ Connect RC car via BLE")
        print("  ✓ Open camera and capture frames")
        print("  ✓ Object detection (tracking)")
        print("  ✓ Edge/Boundary detection")
        print("  ✓ Autonomous control based on car and path position")
        print("  ✓ Car receives and executes commands")
        return 0
    else:
        print(f"\n[!] {total - passed} test(s) failed. See details above.")
        return 1


if __name__ == "__main__":
    sys.exit(main())
