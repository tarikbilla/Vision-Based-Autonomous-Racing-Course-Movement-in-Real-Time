# 🚀 Quick Reference Guide

## Project Overview

**Vision-Based Autonomous RC Car Control System** - Now available in both **Python** and **C++**

- **Status**: ✅ Complete and ready for use
- **Python**: Working, tested, ready to run
- **C++**: Complete implementation, ready to build
- **Documentation**: Comprehensive guides included

---

## Directory Structure

```
📁 Project Root
├── 📁 Python_project/          ← Python implementation (ready to use)
│   ├── rc_autonomy/
│   ├── run_autonomy.py
│   ├── requirements.txt
│   ├── config/
│   └── tests/
│
├── 📁 CPP_Complete/            ← C++ implementation (ready to build)
│   ├── include/                (7 header files)
│   ├── src/                    (8 source files)
│   ├── config/
│   ├── docs/                   (3 documentation files)
│   ├── CMakeLists.txt
│   ├── build.sh
│   └── README.md
│
├── README.md                    ← Start here!
├── COMPLETION_REPORT.md
└── PROJECT_REORGANIZATION_SUMMARY.md
```

---

## 🎯 Getting Started (Choose One)

### Option A: Run Python Now
```bash
cd Python_project
python -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
python run_autonomy.py --simulate
```

**Output**: Live window showing simulated RC car tracking

### Option B: Build C++
```bash
cd CPP_Complete
chmod +x build.sh
./build.sh
./build/VisionBasedRCCarControl --simulate
```

**Output**: Same functionality as Python version

---

## 📖 Documentation

| Location | Content |
|----------|---------|
| `README.md` | Project overview, all implementations |
| `Python_project/README.md` | Python-specific guide |
| `CPP_Complete/README.md` | C++ overview |
| `CPP_Complete/docs/BUILD_INSTRUCTIONS.md` | Build guide (macOS/Linux/Pi) |
| `CPP_Complete/docs/API_REFERENCE.md` | Complete API documentation |
| `CPP_Complete/docs/IMPLEMENTATION_GUIDE.md` | Architecture & design |
| `COMPLETION_REPORT.md` | Project completion details |
| `PROJECT_REORGANIZATION_SUMMARY.md` | Reorganization details |

---

## 🛠️ Build Instructions

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

---

## 🎮 Usage Examples

### Python
```bash
# Simulation (no hardware)
python run_autonomy.py --simulate

# With camera and BLE
python run_autonomy.py --device f9:af:3c:e2:d2:f5

# Custom configuration
python run_autonomy.py --config config/custom.json

# Help
python run_autonomy.py --help
```

### C++
```bash
# Simulation (no hardware)
./VisionBasedRCCarControl --simulate

# With camera and BLE
./VisionBasedRCCarControl --device f9:af:3c:e2:d2:f5

# Custom configuration
./VisionBasedRCCarControl --config config/custom.json

# Help
./VisionBasedRCCarControl --help
```

---

## ⚙️ Configuration

### Python: `Python_project/config/default.json`
```json
{
  "camera": {"index": 0, "width": 1920, "height": 1080, "fps": 30},
  "ble": {"device_mac": "f9:af:3c:e2:d2:f5", ...},
  "boundary": {"black_threshold": 50, ...},
  "tracker": {"tracker_type": "CSRT"},
  "ui": {"show_window": true, "command_rate_hz": 20}
}
```

### C++: `CPP_Complete/config/config.json`
Same format as Python - both use same configuration system

---

## 🔧 Requirements

### Python
- Python 3.8+
- OpenCV (contrib)
- simplepyble
- numpy

### C++
- C++17 compiler
- CMake 3.10+
- OpenCV 4.0+
- nlohmann_json

---

## 📊 Performance

| Metric | Python | C++ Target |
|--------|--------|-----------|
| Camera FPS | 30 | 30 |
| Tracking Analysis | 6-7 FPS | 15+ FPS |
| BLE Command Rate | 20 Hz | 20+ Hz |
| Latency | <200ms | <100ms |

---

## 🚀 Deployment

### On Raspberry Pi

**Step 1**: Copy project
```bash
scp -r CPP_Complete/ pi@raspberrypi:~/
```

**Step 2**: SSH to Pi and build
```bash
ssh pi@raspberrypi
cd CPP_Complete
./build.sh Release
```

**Step 3**: Run
```bash
./build/VisionBasedRCCarControl --device f9:af:3c:e2:d2:f5
```

---

## 🐛 Troubleshooting

### Camera Not Found
- Check camera index in config (usually 0)
- Try simulation mode first: `--simulate`

### BLE Connection Failed
- Verify device MAC address
- Ensure device is powered and nearby
- Check Bluetooth is enabled

### Build Errors
- Ensure all dependencies installed
- Check OpenCV: `pkg-config --modversion opencv4`
- See `CPP_Complete/docs/BUILD_INSTRUCTIONS.md`

### Tracking Lost
- Ensure good lighting
- Adjust `black_threshold` in config
- Use simulation mode to test pipeline

---

## 📁 File Statistics

| Component | Count | Lines |
|-----------|-------|-------|
| C++ Headers | 7 | ~397 |
| C++ Sources | 8 | ~1,197 |
| Documentation | 7 | ~2,200+ |
| Config Files | 2 | JSON |
| Build Files | 2 | CMake + Script |

**Total**: 25+ files, 2,500+ lines of code

---

## 🎯 Key Features

✅ Real-time camera capture (30 FPS)
✅ CSRT/KCF object tracking
✅ Adaptive boundary detection
✅ Ray-casting for obstacles
✅ BLE RC car communication
✅ Multi-threaded architecture
✅ Configuration management
✅ Simulation mode
✅ Live visualization
✅ Cross-platform (macOS/Linux/Pi)

---

## 📚 Implementation Details

### Architecture
- **Camera Thread**: Captures frames continuously
- **Tracking Thread**: Processes at 6-7 FPS
- **BLE Thread**: Sends commands at 20 Hz
- **UI Thread**: Optional visualization

### Data Flow
```
Camera → Tracker → Boundary Detector → BLE Handler → RC Car
```

### Thread Safety
- Producer-consumer queues
- Mutex-protected shared state
- Condition variables for synchronization

---

## 🔗 Quick Links

- **Start Here**: `README.md`
- **Python Guide**: `Python_project/README.md`
- **C++ Guide**: `CPP_Complete/README.md`
- **Build Guide**: `CPP_Complete/docs/BUILD_INSTRUCTIONS.md`
- **API Reference**: `CPP_Complete/docs/API_REFERENCE.md`
- **Complete Report**: `COMPLETION_REPORT.md`

---

## ✅ Verification Checklist

- [ ] Python project found at `Python_project/`
- [ ] C++ project found at `CPP_Complete/`
- [ ] Can run Python with `--simulate` flag
- [ ] C++ builds without errors
- [ ] C++ runs with `--simulate` flag
- [ ] Configuration files present and valid
- [ ] Documentation files readable
- [ ] All 25 files created successfully

---

## 🚀 Next Steps

1. **Try Python**: Run in simulation mode first
2. **Build C++**: Follow build instructions for your platform
3. **Connect Hardware**: Add camera and BLE device MAC
4. **Test**: Use `--simulate` flag before connecting
5. **Deploy**: Copy to Raspberry Pi when ready

---

## 📝 Support Resources

### Documentation
- See `CPP_Complete/docs/` for comprehensive guides
- See `README.md` for project overview
- See `COMPLETION_REPORT.md` for detailed implementation

### Troubleshooting
- Build issues: `CPP_Complete/docs/BUILD_INSTRUCTIONS.md`
- API questions: `CPP_Complete/docs/API_REFERENCE.md`
- Architecture questions: `CPP_Complete/docs/IMPLEMENTATION_GUIDE.md`

---

**Status**: ✅ **Ready for immediate use**

**Python**: Ready to run now
**C++**: Ready to build and deploy

**Questions?** Check the documentation files listed above.
