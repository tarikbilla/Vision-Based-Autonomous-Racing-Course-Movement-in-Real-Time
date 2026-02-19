# Vision-Based Autonomous RC Car Control System (C++)

Complete C++ implementation of vision-based autonomous RC car control system matching the Python prototype functionality.

## Project Structure

```
CPP_Complete/
├── include/                          # Header files
│   ├── config_manager.hpp            # Configuration loader
│   ├── camera_capture.hpp            # Camera capture implementation
│   ├── object_tracker.hpp            # Object tracking (CSRT/KCF/GOTURN)
│   ├── boundary_detection.hpp        # Road/boundary detection
│   ├── ble_handler.hpp               # Bluetooth Low Energy communication
│   ├── commands.hpp                  # Control command structures
│   └── control_orchestrator.hpp      # Main control loop orchestrator
│
├── src/                              # Source files
│   ├── main.cpp                      # Application entry point
│   ├── config_manager.cpp            # Configuration implementation
│   ├── camera_capture.cpp            # Camera capture implementation
│   ├── object_tracker.cpp            # Tracker implementation
│   ├── boundary_detection.cpp        # Boundary detection implementation
│   ├── ble_handler.cpp               # BLE communication implementation
│   ├── commands.cpp                  # Command building logic
│   └── control_orchestrator.cpp      # Orchestrator implementation
│
├── config/
│   └── config.json                   # Configuration file
│
├── build/                            # Build output directory
│
├── CMakeLists.txt                    # CMake build configuration
├── build.sh                          # Build script
└── README.md                         # This file
```

## Requirements

### System Requirements
- macOS 10.14+ / Linux (Ubuntu 18.04+) / Raspberry Pi 4 (Kali OS or Ubuntu)
- C++17 compiler (clang, gcc, or MSVC)
- CMake 3.10+

### Dependencies
- **OpenCV 4.5+** (opencv-contrib-python for C++)
- **nlohmann/json** (JSON parsing library)
- **pthreads** (for threading)

## Installation & Setup

### On macOS

```bash
# Install dependencies
brew install opencv nlohmann-json cmake

# Build
cd CPP_Complete
chmod +x build.sh
./build.sh
```

### On Linux (Ubuntu/Debian)

```bash
# Install dependencies
sudo apt-get update
sudo apt-get install -y cmake libopencv-dev nlohmann-json3-dev

# Build
cd CPP_Complete
chmod +x build.sh
./build.sh
```

### On Raspberry Pi (Kali/Ubuntu)

```bash
# Install dependencies
sudo apt-get update
sudo apt-get install -y cmake libopencv-dev nlohmann-json3-dev

# If OpenCV not available, build from source:
# See CPP_Project/docs/README_BUILD.md

# Build
cd CPP_Complete
chmod +x build.sh
./build.sh Release
```

## Building

### Quick Build

```bash
cd CPP_Complete
./build.sh
```

### Manual Build

```bash
cd CPP_Complete
mkdir -p build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j4
```

The executable will be created as `build/VisionBasedRCCarControl`

## Running

### Basic Usage

```bash
cd build
./VisionBasedRCCarControl
```

### Simulation Mode (No Hardware)

```bash
./VisionBasedRCCarControl --simulate
```

### Custom Configuration

```bash
./VisionBasedRCCarControl --config /path/to/config.json
```

### With Custom BLE Device

```bash
./VisionBasedRCCarControl --device "f9:af:3c:e2:d2:f5"
```

### Help

```bash
./VisionBasedRCCarControl --help
```

## Configuration

Edit `config/config.json` to customize:

- **Camera**: Index, resolution, FPS
- **BLE**: Device MAC, UUID, connection parameters
- **Boundary Detection**: Thresholds, ray angles, speed control
- **Tracker**: Algorithm selection (CSRT, KCF, GOTURN)
- **UI**: Display options, command rate

## System Architecture

### Components

1. **ConfigManager**: Loads and validates configuration
2. **CameraCapture**: Captures video frames (real or simulated)
3. **Tracker**: Tracks objects (CSRT/KCF/GOTURN algorithms)
4. **BoundaryDetector**: Detects road edges and generates guidance
5. **BLEHandler**: BLE communication with RC car
6. **ControlOrchestrator**: Coordinates all components

### Threading Model

- **Camera Thread**: Continuous frame capture
- **Tracking Thread**: Object detection and analysis
- **BLE Thread**: Command transmission
- **UI Thread**: Display and visualization

### Control Flow

```
Camera → Tracker → Boundary Detector → BLE Handler → RC Car
                        ↓
                   Control Vector
                        ↓
                   Steering & Speed
```

## Features Implemented

✓ Real-time camera capture
✓ Multi-threaded architecture
✓ CSRT/KCF/GOTURN object tracking
✓ Adaptive boundary detection
✓ Ray-casting for road edge detection
✓ Proportional steering control
✓ BLE communication framework
✓ Configuration management
✓ Simulation mode for testing
✓ Live visualization

## Platform-Specific Notes

### macOS
- Uses AVFoundation for camera
- Requires CoreBluetooth framework for BLE
- Build with: `clang++ -std=c++17`

### Linux / Raspberry Pi
- Uses V4L2 for camera
- Requires BlueZ for BLE
- Build with: `g++ -std=c++17`

## BLE Implementation Notes

The BLE handler provides two modes:

1. **FakeBLEClient**: For simulation/testing (no hardware required)
2. **RealBLEClient**: For actual BLE communication

Platform-specific BLE implementation is required for production:
- **macOS**: Use CoreBluetooth APIs
- **Linux/Pi**: Use BlueZ D-Bus interface or bluepy library

## Troubleshooting

### Build Failures

**Missing OpenCV:**
```bash
# macOS
brew install opencv

# Linux
sudo apt-get install libopencv-dev

# Raspberry Pi - Build from source (takes ~1 hour)
```

**Missing nlohmann_json:**
```bash
# macOS
brew install nlohmann-json

# Linux
sudo apt-get install nlohmann-json3-dev
```

### Runtime Issues

**Camera not found:**
- Check camera index in config (usually 0)
- Use simulation mode: `--simulate`

**BLE connection failed:**
- Verify device MAC address
- Ensure device is powered and discoverable
- Check Bluetooth adapter is working

**Tracking lost:**
- Ensure good lighting
- Adjust black_threshold in config
- Use simulation mode to verify image pipeline

## Performance Optimization

For Raspberry Pi 4:

1. Use lower resolution (1280x720 or 960x540)
2. Reduce FPS to 15-20
3. Use Release build: `./build.sh Release`
4. Enable hardware acceleration if available

## Next Steps

1. **Platform-Specific BLE**: Implement CoreBluetooth (macOS) or BlueZ (Linux)
2. **Performance**: Optimize for Raspberry Pi 4 embedded deployment
3. **Testing**: Add comprehensive unit and integration tests
4. **Documentation**: Add API documentation and examples
5. **Advanced Features**: Multi-car tracking, path planning, sensor fusion

## License

Educational and research purposes only.

## References

- Python prototype: `Python_project/`
- C++ PRD: `CPP_Project/docs/PRD.md`
- Build guide: `CPP_Project/docs/README_BUILD.md`
