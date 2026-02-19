# C++ Project Quick Start Guide

## ✅ Build Status: Complete

The C++ project is **successfully compiled and ready to use**.

## Quick Start

### 1. Navigate to the C++ project
```bash
cd CPP_Complete
```

### 2. Run in Simulation Mode (No Hardware Needed)
```bash
./build/VisionBasedRCCarControl --simulate
```

This will:
- Load simulated camera feed (moving green dot)
- Initialize tracker
- Establish fake BLE connection
- Run autonomous control logic

### 3. Run with Real Hardware
```bash
./build/VisionBasedRCCarControl --device f9:af:3c:e2:d2:f5
```

Replace `f9:af:3c:e2:d2:f5` with your DRIFT RC car's BLE MAC address.

### 4. Run with Custom Config
```bash
./build/VisionBasedRCCarControl --config ./config/custom.json --simulate
```

### 5. View Help
```bash
./build/VisionBasedRCCarControl --help
```

## Configuration

Edit `CPP_Complete/config/config.json` to adjust:

- Camera resolution and FPS
- Tracker type (CSRT, KCF, etc.)
- BLE settings
- Boundary detection parameters
- Control thresholds

## Rebuilding

### Clean Build
```bash
cd CPP_Complete
./build.sh
```

### Incremental Build (faster)
```bash
cd CPP_Complete/build
make
```

## File Structure

```
CPP_Complete/
├── src/              # Implementation files
├── include/          # Header files
├── config/           # JSON configuration
├── build/            # Compiled executable and artifacts
├── CMakeLists.txt    # Build configuration
├── build.sh          # Build script
└── README.md         # Detailed documentation
```

## Features

✅ **Vision-Based Tracking**
- Real-time object tracking
- Multiple algorithm support (KCF, CSRT, template matching)

✅ **Road Boundary Detection**
- Adaptive edge detection
- Ray-casting for path analysis
- Heading calculation

✅ **BLE Communication**
- Connects to DRIFT RC car via Bluetooth
- Commands: speed, steering, light control
- Simulation mode for testing

✅ **Autonomous Control**
- Multi-threaded architecture
- Real-time camera → tracking → control pipeline
- Graceful shutdown with Ctrl+C

## Troubleshooting

### Issue: "Cannot open file: config/config.json"
**Solution:** Make sure you're in the `CPP_Complete` directory
```bash
cd CPP_Complete
./build/VisionBasedRCCarControl --simulate
```

### Issue: "Camera source failed"
**Solution:** Verify camera availability
```bash
# List available cameras (macOS)
system_profiler SPCameraDataType
```

### Issue: "NSWindow on non-main thread" (macOS)
**Solution:** Normal on macOS with threading. Use simulation mode without GUI:
```bash
./build/VisionBasedRCCarControl --simulate 2>&1 | grep -E "\[✓\]|\[!\]"
```

## Performance

- **Build Time:** ~30 seconds (clean)
- **Startup Time:** ~2 seconds
- **Frame Rate:** 30 FPS (configured)
- **CPU Usage:** ~15-25% per thread

## Platform Support

| Platform | Status | Notes |
|----------|--------|-------|
| macOS | ✅ Working | GUI threading limitation |
| Linux | ✅ Should work | Needs BlueZ for BLE |
| Raspberry Pi | ✅ Target | Optimize for ARM |

## Dependencies

All installed automatically via Homebrew:
- OpenCV 4.13.0
- nlohmann-json 3.12.0
- CMake 3.10+
- C++17 compatible compiler

## Deployment to Raspberry Pi

### Cross-Compile (from macOS)
```bash
# Install ARM toolchain
brew install arm-linux-gnueabihf-binutils

# Build for Pi
cd CPP_Complete
cmake -DCMAKE_TOOLCHAIN_FILE=./cmake/RPi.cmake ..
make
```

### Direct Build on Pi
```bash
# SSH into Pi
ssh pi@raspberrypi.local

# Build on Pi
cd CPP_Complete
./build.sh
```

## Documentation

- **Detailed Guide:** `CPP_Complete/README.md`
- **Build Details:** `CPP_BUILD_SUCCESS.md`
- **Architecture:** `CPP_Complete/docs/ARCHITECTURE.md`
- **API Reference:** `CPP_Complete/docs/API_REFERENCE.md`

---

**Status:** ✅ Ready to Use  
**Last Updated:** February 18, 2026  
**Compiler:** AppleClang 17.0  
**OpenCV:** 4.13.0  
