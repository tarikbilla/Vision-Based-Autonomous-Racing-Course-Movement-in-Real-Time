# Project Structure

## Complete C++ Implementation

This document describes the complete C++ project structure for the Vision-Based Autonomous RC Car Control System.

## Directory Structure

```
.
├── CMakeLists.txt              # CMake build configuration
├── README.md                   # Main project README
├── README_BUILD.md             # Build instructions
├── PRD.md                      # Product Requirements Document
├── PROJECT_STRUCTURE.md        # This file
├── .gitignore                  # Git ignore rules
│
├── include/                    # Header files
│   ├── types.h                 # Common data structures (Position, MovementVector, ControlVector, Ray, ThreadSafeQueue)
│   ├── config_manager.h       # Configuration file management
│   ├── camera_capture.h        # Camera capture module
│   ├── object_tracker.h        # Object tracking (GOTURN, CSRT, KCF, MOSSE)
│   ├── boundary_detection.h   # Boundary detection and guidance
│   ├── ble_handler.h          # BLE communication handler
│   └── control_orchestrator.h # Main control orchestrator
│
├── src/                        # Source files
│   ├── main.cpp                # Entry point
│   ├── config_manager.cpp      # Configuration implementation
│   ├── camera_capture.cpp      # Camera capture implementation
│   ├── object_tracker.cpp      # Object tracking implementation
│   ├── boundary_detection.cpp  # Boundary detection implementation
│   ├── ble_handler.cpp         # BLE handler implementation (placeholder)
│   └── control_orchestrator.cpp # Control orchestrator implementation
│
├── config/                     # Configuration files
│   └── config.json             # Main configuration file
│
├── build/                      # Build directory (created by cmake)
│   └── VisionBasedRCCarControl # Compiled executable
│
└── old_project/               # Python prototype (read-only reference)
    ├── Python/
    ├── CarSteering/
    ├── Tracking/
    └── Simulation/
```

## Module Overview

### 1. Types (`include/types.h`, `src/types.h` - header only)
- **Position**: 2D position (x, y)
- **MovementVector**: Movement vector (dx, dy) with magnitude/angle calculations
- **TrackingResult**: Tracking output (bbox, midpoint, movement, tracking_lost flag)
- **ControlVector**: Control command [light_on, speed, right_turn, left_turn]
- **Ray**: Ray structure for boundary detection
- **ThreadSafeQueue**: Thread-safe queue wrapper for inter-thread communication

### 2. Config Manager (`config_manager.h/cpp`)
- Loads configuration from file
- Provides getters/setters for configuration values
- Default configuration values
- Supports string, int, double, bool types

### 3. Camera Capture (`camera_capture.h/cpp`)
- Initializes camera (USB/webcam or video file)
- Captures frames in separate thread
- Frame rate control
- Thread-safe frame access
- Supports resolution and FPS configuration

### 4. Object Tracker (`object_tracker.h/cpp`)
- Supports multiple tracker types: GOTURN, CSRT, KCF, MOSSE
- ROI selection helper
- Tracks object and calculates movement vector
- Maintains midpoint history for movement calculation
- Handles tracking loss

### 5. Boundary Detection (`boundary_detection.h/cpp`)
- Grayscale threshold-based boundary detection
- Ray casting algorithm (3 rays: -60°, 0°, +60°)
- Evasive action when too close to boundary
- Generates control vector based on ray distances
- Visualization support (draw rays)

### 6. BLE Handler (`ble_handler.h/cpp`)
- **NOTE**: Contains placeholder implementations
- BLE connection management
- Command formatting (hexadecimal format matching Python prototype)
- Continuous command sending at configurable rate (~200 Hz)
- Emergency stop functionality
- **TODO**: Implement actual BLE library integration (BlueZ, simpleble-cpp, etc.)

### 7. Control Orchestrator (`control_orchestrator.h/cpp`)
- Main system coordinator
- Manages all modules (camera, tracker, guidance, BLE)
- Multithreaded architecture:
  - Camera thread: Captures frames
  - Tracking thread: Tracks object
  - Guidance thread: Boundary detection and control generation
  - BLE thread: Sends commands to car
- Frame queues for inter-thread communication
- UI display (optional)
- Emergency stop handling

### 8. Main (`main.cpp`)
- Entry point
- Command-line argument parsing
- Signal handling (SIGINT, SIGTERM)
- Initializes and starts orchestrator
- Main event loop

## Data Flow

```
Camera → Frame Queue → Tracking Thread → Tracking Queue → Guidance Thread → Control Queue → BLE Thread → RC Car
         ↓                                                      ↓
      UI Display                                          UI Display
```

## Threading Model

1. **Main Thread**: Initialization, signal handling, UI (if enabled)
2. **Camera Thread**: Continuous frame capture
3. **Tracking Thread**: Object tracking, movement vector calculation
4. **Guidance Thread**: Boundary detection, control generation
5. **BLE Thread**: Command formatting and transmission

## Configuration

All settings are in `config/config.json`:
- Camera settings (index, resolution, FPS)
- Tracker type (GOTURN, CSRT, KCF, MOSSE)
- Boundary detection parameters (threshold, ray angles, evasive threshold)
- BLE settings (MAC address, characteristic UUID, command rate)
- Control limits (speed, steering)
- System settings (UI, autonomous mode)

## Build System

- **CMake 3.10+** required
- **C++17** standard
- **OpenCV 4.x** required (with tracking modules)
- **Threads** library (pthread)

## Key Features

✅ Modular architecture  
✅ Multithreaded processing  
✅ Thread-safe inter-module communication  
✅ Configuration-driven  
✅ Error handling and graceful shutdown  
✅ UI visualization (optional)  
✅ Emergency stop mechanism  
✅ Extensible design  

## Known Limitations

⚠️ **BLE Handler**: Currently contains placeholder implementations. Actual BLE library integration needed.  
⚠️ **Camera**: Assumes USB/webcam. Sony Alpha 73 may need HDMI capture card or network streaming.  
⚠️ **Performance**: 4K processing may require downscaling on Raspberry Pi 4.  

## Next Steps

1. **Implement BLE**: Choose library (simpleble-cpp, BlueZ) and implement `ble_handler.cpp`
2. **Test Camera**: Verify Sony Alpha 73 connection method
3. **Tune Parameters**: Adjust boundary detection thresholds for your track
4. **Optimize**: Profile and optimize for Raspberry Pi 4 performance
5. **Test**: End-to-end testing on real track

## Compilation Status

✅ All source files created  
✅ Headers properly structured  
✅ Includes resolved  
✅ CMake configuration complete  
⚠️ **Not yet compiled** - Requires OpenCV and BLE libraries on target system  

## Testing Recommendations

1. **Unit Tests**: Test each module independently
2. **Integration Tests**: Test module interactions
3. **Hardware Tests**: Test with actual camera and BLE car
4. **Performance Tests**: Measure FPS and latency
5. **Field Tests**: Test on actual racetrack
