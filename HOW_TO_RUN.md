# 🚀 How to Run This Project

## ✅ Recommended: Python Version (Production Ready)

The **Python implementation is fully working** and ready to use immediately.

### Quick Start

```bash
cd Python_project
source ../.venv/bin/activate  # Or create new: python3 -m venv .venv && source .venv/bin/activate
pip install -r requirements.txt

# Test in simulation mode (no hardware needed)
python run_autonomy.py --simulate --duration 10

# Run with real hardware
python run_autonomy.py --device f9:af:3c:e2:d2:f5
```

### All Python Options

```bash
# Show help
python run_autonomy.py --help

# List available BLE devices
python run_autonomy.py --list-devices

# Run with custom config
python run_autonomy.py --config config/custom.json --simulate

# Select device by index
python run_autonomy.py --device-index 0

# Run for specific duration (seconds)
python run_autonomy.py --simulate --duration 30
```

### Python Features
✅ Real-time camera capture  
✅ Object tracking (CSRT/KCF/GOTURN)  
✅ Road boundary detection  
✅ BLE communication with RC car  
✅ Live visualization  
✅ Simulation mode  
✅ **WORKS ON ALL PLATFORMS**  

---

## ⚠️ C++ Version (Has Issues on macOS)

The C++ version **compiles successfully** but has runtime issues on macOS due to GUI threading constraints.

### Status
- ✅ Builds successfully (263 KB executable)
- ✅ Help works: `./CPP_Complete/build/VisionBasedRCCarControl --help`
- ⚠️ Simulation mode crashes (NSWindow threading issue on macOS)
- ⚠️ Real hardware mode untested

### Known Issue
```
NSInternalInconsistencyException: NSWindow should only be instantiated on the main thread
```

This is a macOS-specific limitation where OpenCV GUI functions must run on the main thread, but the current C++ implementation uses multi-threading.

### If You Want to Try C++

```bash
cd CPP_Complete

# Help (works)
./build/VisionBasedRCCarControl --help

# Simulation (may crash on macOS)
./build/VisionBasedRCCarControl --simulate

# Rebuild if needed
./build.sh
```

### C++ Would Work Better On
- ✅ Linux (Ubuntu/Debian)
- ✅ Raspberry Pi (target platform)
- ⚠️ macOS (GUI issues)

---

## 📊 Feature Comparison

| Feature | Python | C++ |
|---------|--------|-----|
| **Status** | ✅ Working | ⚠️ Issues on macOS |
| **Build** | No build needed | Requires compilation |
| **Performance** | Good (6-7 FPS) | Better (10+ FPS expected) |
| **Simulation** | ✅ Works | ⚠️ Crashes on macOS |
| **Real Hardware** | ✅ Works | ⚠️ Untested |
| **Platform** | All platforms | Linux/Pi better |
| **Development** | Faster iteration | Slower iteration |

---

## 🎯 Recommendations

### For Testing & Development
**Use Python**
```bash
cd Python_project
source ../.venv/bin/activate
python run_autonomy.py --simulate
```

### For Production on Raspberry Pi
**Use Python** (until C++ GUI issues are fixed)
```bash
cd Python_project
python run_autonomy.py --device f9:af:3c:e2:d2:f5
```

### For Performance-Critical Applications on Linux
**Use C++** (after fixing GUI or disabling it)
```bash
cd CPP_Complete
./build/VisionBasedRCCarControl --device f9:af:3c:e2:d2:f5
```

---

## 🔧 Setup Instructions

### Python Setup (Recommended)

1. **Navigate to Python project**
   ```bash
   cd Python_project
   ```

2. **Create virtual environment** (if not exists)
   ```bash
   python3 -m venv .venv
   source .venv/bin/activate
   ```

3. **Install dependencies**
   ```bash
   pip install -r requirements.txt
   ```

4. **Run**
   ```bash
   python run_autonomy.py --simulate
   ```

### C++ Setup (Advanced Users)

1. **Install dependencies** (macOS)
   ```bash
   brew install opencv nlohmann-json cmake
   ```

2. **Build**
   ```bash
   cd CPP_Complete
   ./build.sh
   ```

3. **Run** (may have GUI issues on macOS)
   ```bash
   ./build/VisionBasedRCCarControl --help
   ```

---

## 🐛 Troubleshooting

### Python Issues

**Issue: ModuleNotFoundError**
```bash
# Solution: Install requirements
pip install -r requirements.txt
```

**Issue: Camera not found**
```bash
# Solution: Check camera index in config/default.json
# Try different camera indices: 0, 1, 2
```

**Issue: BLE connection failed**
```bash
# Solution: Use simulation mode for testing
python run_autonomy.py --simulate

# Or verify device MAC address
python run_autonomy.py --list-devices
```

### C++ Issues

**Issue: Build fails**
```bash
# Solution: Install dependencies
brew install opencv nlohmann-json cmake
cd CPP_Complete && ./build.sh
```

**Issue: Runtime crash (macOS)**
```
Current Status: Known issue with GUI threading on macOS
Workaround: Use Python version instead
Fix Required: Disable GUI or refactor threading model
```

---

## 📁 Project Structure

```
IoT-Project-Vision-based-autonomous-RC-car-control-system/
│
├── Python_project/          ← USE THIS (Working)
│   ├── rc_autonomy/         # Python modules
│   ├── run_autonomy.py      # Main entry point ✅
│   ├── requirements.txt     # Dependencies
│   └── config/              # Configuration
│
├── CPP_Complete/            ← C++ version (GUI issues on macOS)
│   ├── build/
│   │   └── VisionBasedRCCarControl  # Executable (263 KB)
│   ├── src/                 # C++ source files
│   ├── include/             # Header files
│   └── build.sh             # Build script
│
└── .venv/                   # Python virtual environment
```

---

## ✅ Summary

**START HERE:**
```bash
cd Python_project
source ../.venv/bin/activate
pip install -r requirements.txt
python run_autonomy.py --simulate --duration 10
```

This will run the fully working Python implementation in simulation mode for 10 seconds.

**For Production:**
Use the Python version until C++ GUI issues are resolved.

**Questions?**
- Python docs: `Python_project/README.md`
- C++ docs: `CPP_Complete/README.md`
- Build details: `CPP_BUILD_SUCCESS.md`

---

**Status as of February 18, 2026:**
- ✅ Python: **Production Ready**
- ⚠️ C++: **Builds OK, Runtime Issues on macOS**
