# PRD — Vision-Based Autonomous RC Car Control System (C++ Implementation)

## 1) Summary
Build a **vision-based autonomous control system** for a small RC car on a racetrack. The system runs as a **C++17 application**, captures video from a camera (USB camera or capture card), processes frames in **OpenCV**, tracks the car in real time, computes guidance using boundary detection and centerline following, and transmits **steering + speed + lights** commands over **Bluetooth Low Energy (BLE)**.

The `old_project/` folder is a legacy Python prototype used for algorithm reference only. The production system is the C++ implementation located in `CPP_Complete/`.

## 2) Problem & Goals

### Problem
Autonomously driving a small RC car on a marked track requires:
- **Low-latency processing** (camera → compute → BLE commands)
- **Robust tracking** under motion blur and variable lighting
- **Reliable BLE control** at a stable command rate
- **Efficient compute** to support 15+ FPS analysis rates

### Goals
- **End-to-end autonomous pipeline**: Camera → C++ processing → BLE → car motion
- **Real-time operation**: stable tracking and responsive steering
- **Modular architecture**: camera, tracking, boundary detection, BLE, and UI are separable
- **Robustness**: tolerate tracking dropouts and lighting variation
- **Config-driven tuning**: adjust camera, BLE, thresholds, and control limits via JSON

### Non-goals (current scope)
- Full SLAM / global mapping
- Multi-car coordination
- Dynamic obstacle avoidance beyond track boundaries
- On-device model training

## 3) Technology Choices

### C++ Implementation Rationale
- **Performance and latency**: native C++ processing with OpenCV
- **Predictable timing**: stable control loop behavior
- **Portability**: macOS, Linux, Raspberry Pi
- **BLE integration**: SimpleBLE for cross-platform BLE operations

### Key Libraries
- **OpenCV 4.x** for image processing and tracking
- **SimpleBLE** for BLE connection and command transmission
- **nlohmann_json** for configuration loading

## 4) Users & Use Cases

### Primary Users
- **Developer/Researcher**: tunes detection and control parameters
- **Operator**: starts/stops the system, monitors the UI, selects ROI when needed

### Core Use Cases
- **UC1 Autonomous drive**: car follows track without manual input
- **UC2 Assisted drive**: user monitors control output and overrides if needed
- **UC3 Evaluation**: log/debug behavior, adjust parameters, repeat

## 5) System Architecture

### Hardware Components
- **Camera**: USB camera or capture card input (configurable resolution/FPS)
- **Compute**: macOS/Linux/Raspberry Pi 4 running C++ app
- **RC Car**: BLE-controlled car (MAC address configurable)

### Software Components

#### 1) Camera Capture
- **Input**: video frames from camera
- **Output**: `Frame` objects (image + timestamp)
- **Responsibilities**: device init, frame acquisition, failure handling

#### 2) Car Detection / Tracking
- **Primary**: motion detection (MOG2) for robust tracking
- **Fallback**: HSV color detection (red car)
- **Manual**: ROI tracker (CSRT/KCF) if needed

#### 3) Boundary Detection & Centerline
- **Ray casting** on grayscale for immediate boundary guidance
- **Track mask + centerline** from red/white barriers (flood fill + distance transform)

#### 4) Control Generation
- **Ray-based steering** when centerline unavailable
- **Pure pursuit** when centerline available
- **Speed modulation** based on steering strength

#### 5) BLE Communication
- **Connection management**: scan/connect/retry
- **Command formatting**: packed byte protocol
- **Transmission**: stable command rate (default 50Hz)

#### 6) Orchestrator + UI
- **Threads**: camera, tracking/control, BLE, UI loop
- **UI**: overlays for car, rays, centerline, and control values

### Data Flow
1. **Camera thread** captures frames → queue
2. **Tracking thread** detects car → computes guidance → publishes `ControlVector`
3. **BLE thread** sends commands at fixed rate
4. **UI loop** renders overlays and handles key input

## 6) Functional Requirements

### FR1 — Camera Input
- Support configurable resolution and FPS
- Graceful handling of camera disconnects

### FR2 — Car Detection
- Motion detection as primary method
- HSV color detection fallback for red cars
- Manual ROI tracker for recovery

### FR3 — Guidance / Steering
- Ray casting (-45°, 0°, +45° or configured) for boundary-based steering
- Evasive action when obstacles are too close
- Pure pursuit steering when centerline is available

### FR4 — BLE Connection
- Connect by MAC address
- Retry on failure (configurable attempts)
- Use characteristic UUID from config

### FR5 — BLE Command Format
- `ControlVector = {light_on, speed, right_turn, left_turn}`
- Commands encoded with device identifier + payload
- Send at fixed command rate (default 50Hz)

### FR6 — UI & Operator Controls
- Display detection overlays (car, rays, centerline)
- Allow ROI selection and mode switching
- Support quit via 'q' or Ctrl+C

### FR7 — Configuration & Tuning
- `config/config.json` controls camera, BLE, boundary thresholds, and UI
- Parameters must be reloadable by restart

## 7) Performance Targets
- **Analysis rate**: ~15 FPS
- **BLE command rate**: 50 Hz
- **Latency**: < 150 ms camera-to-command

## 8) Constraints
- Lighting variance and motion blur
- BLE reliability and interference
- CPU limits on Raspberry Pi

## 9) Risks & Mitigations
- **Tracking loss** → fallback to color or ROI tracking
- **BLE dropouts** → reconnect attempts and STOP on shutdown
- **Lighting changes** → adjustable HSV thresholds and grayscale threshold

## 10) Configuration Parameters (Key)
```json
{
  "camera": { "index": 0, "width": 1280, "height": 720, "fps": 15 },
  "ble": {
    "device_mac": "f9:af:3c:e2:d2:f5",
    "characteristic_uuid": "6e400002-b5a3-f393-e0a9-e50e24dcca9e",
    "device_identifier": "bf0a00082800",
    "connection_timeout_s": 10,
    "reconnect_attempts": 3
  },
  "boundary": {
    "black_threshold": 50,
    "ray_max_length": 200,
    "evasive_distance": 100,
    "default_speed": 50,
    "steering_limit": 1,
    "light_on": true
  },
  "ui": { "show_window": true, "command_rate_hz": 50 }
}
```

## 11) Acceptance Criteria
- Car connects via BLE and responds to commands
- Car is tracked and steering responds to track geometry
- Centerline can be built on tracks with red/white boundaries
- UI shows live overlays and control values
  - BLE settings (MAC address, characteristic UUID, command rate)
  - Control parameters (speed limits, steering sensitivity)

## 7) Non-Functional Requirements (NFRs)

### NFR1 — Real-time Performance
- The system shall process frames at a minimum of **15 FPS** (target: 30 FPS) on Raspberry Pi 4
- The end-to-end latency (camera frame → control decision → BLE send) shall be **< 100 ms**
- The system shall maintain BLE command transmission at **~200 Hz** (5 ms intervals)
- Frame processing shall be optimized for Raspberry Pi 4 CPU (may require downscaling 4K frames)

### NFR2 — Resource Efficiency
- The system shall minimize memory usage (target: < 2GB RAM on Raspberry Pi 4)
- The system shall utilize CPU efficiently (multithreading, SIMD optimizations where possible)
- The system shall handle 4K frame processing efficiently (downscaling or ROI-based processing)

### NFR3 — Reliability & Fault Handling
- If camera capture fails, the system must stop safely (stop sending motion commands)
- If BLE connection drops, the system must stop sending and attempt reconnection
- If tracker loses the target, the system should:
  - Degrade safely (reduce speed, attempt recovery, or stop)
  - Log tracking loss events
- The system shall handle thread exceptions gracefully

### NFR4 — Safety
- Provide an emergency stop mechanism (SIGINT handler, UI stop button)
- Default to "stop/no steering" when in unknown state or missing inputs
- Send explicit STOP command on shutdown
- Validate control values before sending (prevent out-of-range commands)

### NFR5 — Maintainability
- Modular python architecture with clear separation:
  - Camera capture module
  - Tracking module
  - Guidance/boundary detection module
  - BLE communication module
  - Control orchestrator
- Well-documented code with clear interfaces
- Configuration-driven (avoid hardcoded values)

### NFR6 — Portability
- Code shall compile on Raspberry Pi OS (Debian-based)
- Dependencies shall be clearly documented (CMake build system recommended)
- Cross-compilation support for development on PC

## 8) Hardware & Software Requirements

### Hardware
- **Raspberry Pi 4**: 
  - Model B (4GB or 8GB RAM recommended)
  - Raspberry Pi OS (64-bit recommended for better performance)
  - Bluetooth 5.0 support
  - USB 3.0 for camera connection (if USB capture)
- **Sony Alpha 73 4K Camera**: 
  - 4K video output (3840x2160)
  - Connection method: USB/HDMI capture card or network streaming (to be determined)
- **RC Car**: BLE-controlled DR!FT car
  - MAC address: configurable (e.g., 'f9:af:3c:e2:d2:f5')
  - BLE characteristic UUID: '6e400002-b5a3-f393-e0a9-e50e24dcca9e'

### Software / Libraries (python Implementation)
- **python Compiler**: GCC 8+ or Clang (python17 standard)
- **OpenCV**: 4.x (python API)
  - Built with tracking module support (GOTURN, CSRT, KCF)
  - Built with GUI support (`highgui` module)
- **BLE Library**: 
  - Option 1: BlueZ (Linux native BLE stack) via D-Bus or direct API
  - Option 2: `simpleble-python` (cross-platform python wrapper)
  - Option 3: Custom BlueZ integration using `bluetoothctl` or `gatttool`
- **Build System**: CMake 3.10+
- **Threading**: python standard library (`std::thread`, `std::mutex`, `std::queue`)
- **Optional**: 
  - GStreamer (for advanced camera pipeline)
  - Eigen3 (for matrix operations if needed)
  - spdlog (for logging)

### Development Environment
- **PC/Laptop**: For cross-compilation and testing (optional)
- **Raspberry Pi 4**: Primary development and deployment target

## 9) Configurations & Tunable Parameters

### Camera Settings
- Resolution: 1920x1080 (downscaled from 4K) or 3840x2160 (full 4K, if performance allows)
- Frame rate: 30 FPS (configurable)
- Camera connection method: USB/HDMI capture or network streaming

### Tracking Parameters
- Tracker type: GOTURN (default), CSRT, or KCF
- ROI selection: Manual (user-selected bounding box)
- Movement vector calculation: Based on last N midpoints (default: 10)

### Guidance Parameters
- `BLACK_THRESHOLD`: Grayscale threshold for boundary detection (default: 50, range: 0-255)
- Ray angles: −60°, 0°, +60° (relative to car heading)
- Ray max length: 200 pixels (configurable)
- Evasive threshold distance: 80 pixels (configurable)
- Speed: Autonomous speed value (default: 10, range: 0-255)

### BLE Parameters
- `DEVICE_MAC`: Car MAC address (configurable)
- `DEVICE_CHARACTERISTIC`: UUID '6e400002-b5a3-f393-e0a9-e50e24dcca9e'
- `DEVICE_IDENTIFIER`: 'bf0a00082800'
- Command send rate: 200 Hz (5 ms sleep interval)
- Connection timeout: 5 seconds
- Reconnection attempts: 3 (configurable)

### Control Parameters
- Speed limits: Forward (0-100), Reverse (0-100)
- Steering limits: Left/Right (0-30, as per prototype)
- Light control: On ('0200') / Off ('0000')

## 10) Implementation Plan

### Phase 1: Foundation (Week 1-2)
- Set up Raspberry Pi 4 development environment
- Install dependencies (OpenCV python, BLE library)
- Create CMake build system
- Implement camera capture module (Sony Alpha 73 connection)
- Test frame capture and display

### Phase 2: Tracking Module (Week 2-3)
- Implement ROI selection interface
- Integrate OpenCV tracker (GOTURN or CSRT)
- Implement movement vector calculation
- Test tracking on recorded video

### Phase 3: Guidance Module (Week 3-4)
- Implement boundary detection (grayscale thresholding)
- Implement ray casting algorithm
- Implement steering decision logic
- Test guidance on recorded video with known car positions

### Phase 4: BLE Communication (Week 4-5)
- Implement BLE scanning and connection
- Implement command formatting and transmission
- Test BLE communication with RC car (manual commands)
- Verify command protocol matches Python prototype

### Phase 5: Integration & Optimization (Week 5-6)
- Integrate all modules with multithreading
- Implement frame queues and synchronization
- Optimize for Raspberry Pi 4 (downscaling, SIMD if applicable)
- Performance profiling and tuning

### Phase 6: Testing & Refinement (Week 6-7)
- End-to-end testing on real track
- Tune parameters (thresholds, speeds, steering sensitivity)
- Handle edge cases (tracking loss, connection drops)
- Documentation and code cleanup

## 11) Acceptance Criteria

### AC1 — End-to-end Autonomy Demo
- Given a visible car and track, when the operator selects the ROI and starts the system, the car receives BLE commands continuously and moves while attempting to stay within track boundaries
- System maintains tracking for at least 30 seconds without manual intervention
- Car successfully navigates at least one complete lap (if track is closed loop)

### AC2 — Performance Targets
- Frame processing rate: ≥ 15 FPS on Raspberry Pi 4
- End-to-end latency: < 100 ms (frame capture → BLE send)
- BLE command rate: ~200 Hz (5 ms intervals)
- Memory usage: < 2GB RAM

### AC3 — Robust Stop Behavior
- When the operator stops the program (SIGINT or UI stop), threads terminate cleanly
- System sends explicit STOP command (speed=0, steering=0) to car
- No resource leaks (verified with valgrind or similar)

### AC4 — Basic Observability
- Tracking overlay displays bounding box, midpoint, and movement vector
- Console logs indicate BLE connection status and command transmission
- System logs tracking loss events and connection errors

### AC5 — Configuration & Tuning
- System reads configuration from file or command-line arguments
- All tunable parameters are configurable without code changes
- Configuration file format is documented

## 12) Testing Plan

### Unit Tests
- Command generation formatting for BLE packets
- Boundary detection pixel thresholding logic
- Ray casting distance calculations
- Movement vector calculations

### Integration Tests
- Camera capture + tracker initialization
- Guidance output stability on recorded video (offline)
- BLE connectivity and command sending rate
- Thread synchronization and queue management

### Field Tests
- Different track images/layouts and lighting conditions
- Varying car speeds; measure lap stability and failure modes
- Tracking robustness under motion blur and occlusion
- BLE connection stability and reconnection behavior
- Performance profiling on Raspberry Pi 4 under load

## 13) Risks & Mitigation

### Key Risks

1. **4K Processing Performance on Raspberry Pi 4**
   - **Risk**: Processing 4K frames may be too slow for real-time operation
   - **Mitigation**: 
     - Downscale frames to 1920x1080 or lower for processing
     - Use ROI-based processing (only process relevant region)
     - Optimize with SIMD/NEON instructions
     - Profile and optimize hot paths

2. **BLE Library Compatibility**
   - **Risk**: python BLE library may not match Python prototype's behavior
   - **Mitigation**: 
     - Test BLE command format matches Python prototype exactly
     - Use BlueZ directly if wrapper libraries have issues
     - Verify command protocol with car manufacturer documentation

3. **Tracking Robustness**
   - **Risk**: GOTURN/CSRT may lose track under motion blur or lighting changes
   - **Mitigation**: 
     - Implement tracking loss detection and recovery
     - Fallback to simpler trackers (KCF) if needed
     - Consider hybrid approach (detector + tracker)

4. **Camera Connection Method**
   - **Risk**: Sony Alpha 73 connection method (USB/HDMI/network) may have latency or compatibility issues
   - **Mitigation**: 
     - Test multiple connection methods
     - Use GStreamer pipeline for flexible camera integration
     - Consider frame buffering to handle variable latency

5. **Resource Constraints**
   - **Risk**: Raspberry Pi 4 may run out of memory or CPU under load
   - **Mitigation**: 
     - Monitor resource usage during development
     - Implement frame dropping if processing falls behind
     - Optimize memory usage (avoid unnecessary copies)

## 14) Future Improvements

- **Deep Learning Integration**: Replace/augment tracking with YOLO or similar object detection
- **Advanced Guidance**: Lane centerline estimation, PID control for smoother steering
- **Sensor Fusion**: Add ultrasonic/LiDAR sensors for obstacle avoidance
- **Edge AI**: Deploy optimized neural networks on Raspberry Pi GPU (if available)
- **Multi-threaded Optimization**: Parallelize boundary detection across multiple threads
- **Adaptive Thresholding**: Dynamic boundary detection based on lighting conditions
- **Path Planning**: Global path planning for optimal lap times

## 15) Reference: Python Prototype

The `old_project/` folder contains a working Python prototype that demonstrates:
- GOTURN tracking implementation (`old_project/Tracking/testgoturn.py`)
- Boundary detection with ray casting (`old_project/Python/boundaryDetection.py`)
- BLE communication (`old_project/Python/BTLEHandler.py`)
- Main control loop (`old_project/Python/main.py`)
- Simulation environment (`old_project/Simulation/simulation.py`)

**Note**: The `old_project/` folder is **read-only** and serves as a reference for algorithm logic, BLE command format, and system architecture. The python implementation should replicate this functionality with performance optimizations for Raspberry Pi 4.
