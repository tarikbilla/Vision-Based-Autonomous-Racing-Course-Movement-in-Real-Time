# Vision-Based Autonomous RC Car Control System

Complete implementation of an autonomous RC car control system using computer vision. Now available in both **Python** and **C++**.

## 🎯 Project Structure

```
IoT-Project-Vision-based-autonomous-RC-car-control-system/
├── Python_project/              # ✅ Python Implementation (Working)
│   ├── rc_autonomy/             # Main Python module
│   ├── run_autonomy.py          # Entry point
│   ├── requirements.txt
│   ├── config/
│   ├── tests/
│   └── README.md
│
├── CPP_Complete/                # ✅ C++ Implementation (Ready to Build)
│   ├── include/                 # Header files (6)
│   ├── src/                     # Source files (8)
│   ├── config/
│   ├── docs/                    # Comprehensive guides (3)
│   ├── CMakeLists.txt
│   ├── build.sh
│   └── README.md
│
├── CPP_Project/                 # Reference C++ project
└── PROJECT_REORGANIZATION_SUMMARY.md
```

## 🚀 Quick Start

### Python Version (Immediately Ready)

```bash
cd Python_project
python -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt

# Run simulation (no hardware)
python run_autonomy.py --simulate --duration 10

# Run with hardware
python run_autonomy.py --device f9:af:3c:e2:d2:f5
```

### C++ Version (Build Required)

```bash
cd CPP_Complete
chmod +x build.sh
./build.sh              # Builds Release binary

# Run simulation
./build/VisionBasedRCCarControl --simulate

# Run with hardware
./build/VisionBasedRCCarControl --device f9:af:3c:e2:d2:f5
```

## ✨ Features

### Core Functionality
- ✅ Real-time video capture from Sony Alpha 73 camera
- ✅ Object tracking using CSRT/KCF/GOTURN algorithms
- ✅ Adaptive boundary detection with ray-casting
- ✅ BLE communication with DRIFT RC car
- ✅ Multi-threaded architecture for real-time processing
- ✅ Live visualization and control
- ✅ Configurable parameters via JSON

### Supported Platforms
- ✅ macOS (Xcode/Clang)
- ✅ Ubuntu/Debian (GCC)
- ✅ Raspberry Pi 4 (Kali/Ubuntu)

### Both Implementations Feature Parity
- Same camera capture pipeline
- Same tracking algorithms
- Same boundary detection
- Same BLE protocol
- Same configuration system
- Same performance characteristics

## 📋 Configuration

Edit configuration files to customize:

**Python**: `Python_project/config/default.json`
**C++**: `CPP_Complete/config/config.json`

Parameters include:
- Camera settings (index, resolution, FPS)
- BLE device MAC and UUID
- Boundary detection thresholds
- Tracker algorithm selection
- UI options

## 🔧 Requirements

### Python Version
- Python 3.8+
- OpenCV (contrib)
- simplepyble
- numpy

### C++ Version
- C++17 compiler (g++, clang, MSVC)
- CMake 3.10+
- OpenCV 4.0+
- nlohmann_json

## 📖 Documentation

### Python Project
- `Python_project/README.md` - Python-specific guide
- Test files demonstrating functionality

### C++ Project
- `CPP_Complete/README.md` - C++ overview
- `CPP_Complete/docs/BUILD_INSTRUCTIONS.md` - Detailed build guide
- `CPP_Complete/docs/IMPLEMENTATION_GUIDE.md` - Architecture & design
- `CPP_Complete/docs/API_REFERENCE.md` - Complete API documentation

### Project Summary
- `PROJECT_REORGANIZATION_SUMMARY.md` - Complete reorganization details

## 🏗️ Architecture

### Control Pipeline
```
Camera → Tracker → Boundary Detector → BLE Handler → RC Car
         ↓              ↓
    Object Position  Steering/Speed
```

### Threading Model
- **Camera Thread**: Continuous frame capture
- **Tracking Thread**: Object detection and analysis
- **BLE Thread**: Command transmission  
- **UI Thread**: Visualization (optional)

## 🛠️ Building C++ Version

### macOS
```bash
brew install cmake opencv nlohmann-json
cd CPP_Complete && ./build.sh
```

### Ubuntu/Debian
```bash
sudo apt-get install cmake libopencv-dev nlohmann-json3-dev
cd CPP_Complete && ./build.sh
```

### Raspberry Pi
```bash
sudo apt-get install cmake libopencv-dev nlohmann-json3-dev
cd CPP_Complete && ./build.sh Release
```

See `CPP_Complete/docs/BUILD_INSTRUCTIONS.md` for detailed steps.

## 🎮 Usage

### Python
```bash
# Simulation mode
python run_autonomy.py --simulate

# With hardware
python run_autonomy.py --device f9:af:3c:e2:d2:f5

# List devices
python run_autonomy.py --list-devices

# Help
python run_autonomy.py --help
```

### C++
```bash
# Simulation mode
./VisionBasedRCCarControl --simulate

# With hardware  
./VisionBasedRCCarControl --device f9:af:3c:e2:d2:f5

# Custom config
./VisionBasedRCCarControl --config config/config.json

# Help
./VisionBasedRCCarControl --help
```

## 📊 Performance

### Python Implementation
- Camera: 30 FPS
- Tracking analysis: 6-7 FPS (real-time detection)
- BLE commands: 20 Hz
- Latency: <200ms

### C++ Implementation (Target)
- Camera: 30 FPS
- Tracking analysis: 10+ FPS (optimized)
- BLE commands: 20+ Hz
- Latency: <100ms

## 🔍 Testing

### Python Tests
```bash
cd Python_project
python -m pytest tests/
```

### C++ Simulation
```bash
./CPP_Complete/build/VisionBasedRCCarControl --simulate
```

## 🐛 Troubleshooting

### Common Issues
- **Camera not found**: Check camera index in config
- **BLE connection failed**: Verify device MAC and ensure device is powered
- **Tracking lost**: Adjust black_threshold in config
- **Build errors**: Ensure all dependencies are installed

See respective README files for platform-specific troubleshooting.

## 🚀 Next Steps

1. **Choose Implementation**:
   - Python: Ready to use immediately
   - C++: Build with `./build.sh` then run

2. **Configure**:
   - Edit `config/config.json` or `config/default.json`
   - Set camera index and BLE device MAC

3. **Test**:
   - Run with `--simulate` flag first
   - Connect hardware and test with real camera

4. **Deploy**:
   - Python: Run on any Python 3.8+ system
   - C++: Copy binary to Raspberry Pi

## 📝 License

Educational and research purposes only.

## 📚 References

- **Python Module**: `Python_project/` directory
- **C++ Implementation**: `CPP_Complete/` directory  
- **Project Details**: `PROJECT_REORGANIZATION_SUMMARY.md`
- **PRD**: `docs/PRD.md`

---

**Status**: ✅ Both implementations ready for use and deployment
- `test_ble_python.py` remains available as a standalone BLE reference script.
