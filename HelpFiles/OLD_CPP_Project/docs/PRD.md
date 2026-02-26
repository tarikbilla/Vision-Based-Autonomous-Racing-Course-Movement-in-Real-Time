# PRD — Vision-Based Autonomous RC Car Control System

## 1) Summary
Build a **vision-based autonomous control system** for a small RC car on a racetrack. The system runs on **Raspberry Pi 4**, captures video from a **Sony Alpha 73 4K camera**, processes frames in **C++** to track the car in real time, derives motion/heading, performs track-boundary guidance, and transmits steering + speed + lights commands to the car over **Bluetooth Low Energy (BLE)**. 

The `old_project/` folder contains a Python prototype (read-only reference) that demonstrates the core concepts: GOTURN tracking, ray-based boundary detection, and BLE command transmission.

## 2) Problem & Goals

### Problem
Autonomously driving a small RC car on a marked track requires:
- **Real-time processing** of high-resolution 4K video (Sony Alpha 73) on embedded hardware (Raspberry Pi 4)
- **Low-latency control loop** (camera → processing → BLE commands → car response)
- **Robust tracking** under varying lighting and motion blur
- **Efficient resource usage** on Raspberry Pi 4 (limited CPU/RAM)

### Goals
- **End-to-end autonomous pipeline**: Sony Alpha 73 → Raspberry Pi 4 → C++ processing → BLE commands → car motion
- **Real-time operation**: maintain stable tracking and responsive steering decisions at acceptable frame rates
- **Modular C++ architecture**: separable tracking, guidance, and BLE command layers
- **Robustness**: tolerate track variations, lighting changes, and brief tracking degradation
- **Performance optimization**: leverage C++ for efficient processing of 4K frames on Raspberry Pi 4

### Non-goals (current scope)
- Full SLAM / global path planning
- Multi-car coordination
- Obstacle avoidance beyond boundary/track avoidance (future improvement)
- Full neural network training on-device (inference only)

## 3) Technology Choice: C++ vs Python

### Recommendation: **C++** for Production Implementation

#### C++ Advantages (Critical for this project):
1. **Performance**: 
   - Native compilation → faster execution, lower CPU usage
   - Better memory management → critical for 4K frame processing on Raspberry Pi 4
   - Direct hardware access → optimized for embedded systems
   - Lower latency → essential for real-time control loops

2. **Resource Efficiency**:
   - Lower memory footprint → important for Raspberry Pi 4 (4-8GB RAM)
   - Better CPU utilization → can process 4K frames more efficiently
   - Predictable performance → no garbage collection pauses

3. **Real-time Constraints**:
   - Deterministic execution times → critical for control systems
   - Better threading control → fine-grained control over processing pipeline
   - Hardware acceleration → easier integration with GPU/NEON SIMD on Raspberry Pi

4. **Production Readiness**:
   - Industry standard for embedded vision systems
   - Better integration with OpenCV C++ API (more features, better performance)
   - Easier deployment (single binary, no Python runtime dependencies)

#### Python Advantages (Why it was used in prototype):
1. **Rapid Prototyping**: Faster development, easier debugging
2. **Rich Libraries**: Simple integration with OpenCV, BLE libraries
3. **Flexibility**: Easy to modify and experiment with algorithms

#### Conclusion:
For **Raspberry Pi 4 + 4K camera + real-time control**, **C++ is the better choice** due to performance, resource efficiency, and real-time requirements. The Python prototype in `old_project/` serves as a reference for algorithm logic and system architecture.

## 4) Users & Use Cases

### Primary Users
- **Developer/Researcher**: develops and tunes the C++ pipeline, evaluates autonomy on different tracks
- **Operator**: starts/stops the system, selects the tracked object (ROI), monitors system status

### Core Use Cases
- **UC1 Manual driving (baseline)**: operator controls car speed/steering and lights via UI/input device
- **UC2 Assisted driving**: tracking runs; guidance computed; operator can observe decisions
- **UC3 Autonomous lap**: system tracks the car and sends steering/speed commands continuously

## 5) System Architecture

### Hardware Components
- **Sony Alpha 73 4K Camera**: 
  - Captures high-resolution video (3840x2160 or downscaled)
  - Connected to Raspberry Pi 4 via USB/HDMI capture card or network streaming
- **Raspberry Pi 4**: 
  - Processes video frames in C++
  - Runs tracking, boundary detection, and control logic
  - Manages BLE communication with RC car
- **RC Car**: BLE-controlled DR!FT car (MAC address configurable)

### Software Architecture (C++ Implementation)

#### Component 1: Camera Capture Module
- **Input**: Sony Alpha 73 4K camera stream
- **Technology**: OpenCV C++ (`VideoCapture` or GStreamer pipeline)
- **Output**: Raw frames (may need downscaling for processing efficiency)
- **Responsibilities**:
  - Initialize camera connection
  - Capture frames at target frame rate
  - Handle frame buffering and synchronization
  - Error handling for camera disconnection

#### Component 2: Object Tracking Module
- **Input**: Camera frames + initial ROI selection
- **Technology**: OpenCV C++ tracking API (GOTURN, CSRT, or KCF)
- **Output**: `(position, movement_vector)` tuples
- **Responsibilities**:
  - Initialize tracker with user-selected ROI
  - Track object across frames
  - Calculate midpoint and movement vector (dx, dy)
  - Handle tracking loss and recovery

#### Component 3: Boundary Detection & Guidance Module
- **Input**: Camera frames + tracked car position + movement vector
- **Technology**: OpenCV C++ image processing
- **Output**: Control vector `[light_on, speed, right_turn_value, left_turn_value]`
- **Responsibilities**:
  - Convert frames to grayscale
  - Cast three rays (-60°, 0°, +60°) from car position
  - Detect boundary pixels (threshold-based)
  - Compute steering adjustments based on ray distances
  - Generate control commands

#### Component 4: BLE Communication Module
- **Input**: Control vector from guidance module
- **Technology**: BlueZ (Linux BLE stack) or C++ BLE library (e.g., `simpleble-cpp`)
- **Output**: BLE commands sent to RC car
- **Responsibilities**:
  - Scan and connect to RC car by MAC address
  - Discover services and characteristics
  - Format commands (hexadecimal string format)
  - Send commands continuously at target rate (~200 Hz)
  - Handle connection errors and reconnection

#### Component 5: Control Orchestrator
- **Technology**: C++ multithreading (`std::thread`, `std::mutex`, `std::queue`)
- **Responsibilities**:
  - Coordinate camera capture, tracking, guidance, and BLE threads
  - Manage frame queues between modules
  - Handle graceful shutdown
  - Emergency stop mechanism

#### Component 6: UI/Monitoring (Optional)
- **Technology**: OpenCV GUI (`cv::imshow`) or lightweight C++ GUI framework
- **Responsibilities**:
  - Display tracking overlay (bounding box, rays, control values)
  - ROI selection interface
  - System status indicators
  - Start/stop controls

### Data Flow (C++ Implementation)
1. **Camera thread**: Captures frames from Sony Alpha 73 → pushes to frame queue
2. **Tracking thread**: Consumes frames → tracks car → outputs `(position, movement_vector)` → pushes to position queue
3. **Guidance thread**: Consumes frames + positions → runs boundary detection → outputs control vector → pushes to control queue
4. **BLE thread**: Consumes control commands → formats BLE packets → sends to car continuously
5. **UI thread** (optional): Displays frames with overlays, handles user input

### Threading Model
- **Camera capture**: Dedicated thread (I/O bound)
- **Tracking**: Dedicated thread (CPU intensive)
- **Guidance**: Dedicated thread (CPU intensive)
- **BLE sending**: Dedicated thread (I/O bound, high frequency)
- **UI**: Main thread or dedicated thread (if using GUI)

## 6) Functional Requirements

### FR1 — Camera Input (Sony Alpha 73)
- The system shall connect to Sony Alpha 73 4K camera via USB/HDMI capture or network streaming
- The system shall capture frames at a configurable resolution (may downscale from 4K for processing)
- The system shall handle camera initialization errors gracefully
- The system shall support frame rate configuration (target: 30 FPS or higher if downscaled)

### FR2 — Target Selection & Initialization
- The system shall allow the operator to select the object/car to track using a **ROI selection** interface
- The tracking pipeline shall initialize the tracker from the first frame and selected ROI
- The system shall support tracker type selection (GOTURN, CSRT, KCF, or other OpenCV trackers)

### FR3 — Real-time Tracking
- The system shall track the object in real time and output:
  - Current midpoint position `(x, y)` in image coordinates
  - Last movement vector `(dx, dy)` between consecutive midpoints
- The system shall render a tracking overlay (bounding box, midpoint, movement arrow) for monitoring
- The system shall handle tracking loss (e.g., reduce speed, attempt recovery, or stop)

### FR4 — Track/Boundary Representation
- The system shall treat "track boundaries" as **dark pixels** on the image
- Boundary detection shall use a grayscale threshold:
  - A pixel is boundary if `gray_value < BLACK_THRESHOLD` (configurable, default: 50)
- The system shall support adaptive thresholding for varying lighting conditions (future enhancement)

### FR5 — Guidance / Steering Decision
- The system shall project **three rays** from the car position at relative angles:
  - −60°, 0°, +60° (relative to car direction/heading)
- The system shall compute each ray's distance to the first detected boundary pixel (or max length)
- The system shall trigger an evasive action when the closest ray distance is below a safety margin (configurable, default: 80 px)
- The system shall output a standardized control vector:
  - `control = [light_on, speed, right_turn_value, left_turn_value]`
  - `light_on`: boolean/int (1 = on, 0 = off)
  - `speed`: integer value (0-255, mapped by BLE layer)
  - `right_turn_value` and `left_turn_value`: turning magnitude (0-255)

### FR6 — BLE Connection & Identification
- The system shall scan for BLE devices and connect to the car by configured MAC address
- The system shall list available services/characteristics and choose the correct characteristic UUID
- The system shall support configuration for:
  - `DEVICE_MAC` (e.g., 'f9:af:3c:e2:d2:f5' for gray car, 'ed:5c:23:84:48:8d' for red car)
  - `DEVICE_CHARACTERISTIC` (default: '6e400002-b5a3-f393-e0a9-e50e24dcca9e')
  - Connection timeout and retry logic

### FR7 — BLE Command Format & Transmission
- The system shall generate a command string composed of:
  - Device identifier: `'bf0a00082800'`
  - Speed value (4-digit hex): `twoDigitHex(speed)`
  - Drift value (4-digit hex): `twoDigitHex(drift)` (typically 0)
  - Steering value (4-digit hex): `twoDigitHex(steering)`
  - Light value: `'0200'` (on) or `'0000'` (off)
  - Checksum: `'00'` (or calculated)
- The system shall send commands using BLE write request to the selected characteristic
- The system shall support:
  - Forward speed control (0-255)
  - Reverse speed control (implemented as `255 - speed` mapping)
  - Steering control (left: `255 - leftValue`, right: `rightValue`)
  - Light control

### FR8 — Continuous Command Sending
- The system shall transmit commands continuously while driving to maintain smooth control
- The system shall send commands at a target rate (~200 Hz, configurable)
- The system shall allow stopping the system and terminating threads cleanly
- The system shall send a STOP command (speed=0, steering=0) on shutdown

### FR9 — Operator Interface
- The system shall provide a UI/monitoring interface to:
  - Start the system (connect to car + start processing pipeline)
  - Stop the system (graceful shutdown)
  - Select ROI for tracking
  - Display live tracking overlay
  - Display control values (speed, steering)
- The system shall not block processing threads with UI operations

### FR10 — Configuration & Tuning
- The system shall support configuration files or command-line parameters for:
  - Camera settings (resolution, frame rate)
  - Tracking parameters (tracker type, ROI)
  - Boundary detection (threshold, ray angles, safety margin)
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
- Modular C++ architecture with clear separation:
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

### Software / Libraries (C++ Implementation)
- **C++ Compiler**: GCC 8+ or Clang (C++17 standard)
- **OpenCV**: 4.x (C++ API)
  - Built with tracking module support (GOTURN, CSRT, KCF)
  - Built with GUI support (`highgui` module)
- **BLE Library**: 
  - Option 1: BlueZ (Linux native BLE stack) via D-Bus or direct API
  - Option 2: `simpleble-cpp` (cross-platform C++ wrapper)
  - Option 3: Custom BlueZ integration using `bluetoothctl` or `gatttool`
- **Build System**: CMake 3.10+
- **Threading**: C++ standard library (`std::thread`, `std::mutex`, `std::queue`)
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
- Install dependencies (OpenCV C++, BLE library)
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
   - **Risk**: C++ BLE library may not match Python prototype's behavior
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

**Note**: The `old_project/` folder is **read-only** and serves as a reference for algorithm logic, BLE command format, and system architecture. The C++ implementation should replicate this functionality with performance optimizations for Raspberry Pi 4.
