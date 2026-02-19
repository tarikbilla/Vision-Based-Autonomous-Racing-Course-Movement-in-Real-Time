# Building the C++ Project

## Prerequisites

### macOS
```bash
# Install Xcode Command Line Tools
xcode-select --install

# Install dependencies via Homebrew
brew install cmake opencv nlohmann-json
```

### Ubuntu/Debian
```bash
sudo apt-get update
sudo apt-get install -y cmake g++ libopencv-dev nlohmann-json3-dev build-essential
```

### Raspberry Pi (Kali/Ubuntu)

#### Option 1: Quick Install (Recommended)
```bash
sudo apt-get update
sudo apt-get install -y cmake g++ libopencv-dev nlohmann-json3-dev
```

#### Option 2: Build OpenCV from Source
If precompiled OpenCV is not available:

```bash
# Install dependencies
sudo apt-get install -y build-essential cmake git libgtk-3-dev pkg-config libavcodec-dev libavformat-dev libswscale-dev

# Clone OpenCV
cd ~
git clone https://github.com/opencv/opencv.git
git clone https://github.com/opencv/opencv_contrib.git
cd opencv
mkdir build && cd build

# Configure
cmake -D CMAKE_BUILD_TYPE=Release \
      -D CMAKE_INSTALL_PREFIX=/usr/local \
      -D OPENCV_EXTRA_MODULES_PATH=~/opencv_contrib/modules \
      -D ENABLE_NEON=ON \
      -D ENABLE_VFPV3=ON \
      -D BUILD_EXAMPLES=OFF \
      -D BUILD_DOCS=OFF \
      -D BUILD_TESTS=OFF \
      ..

# Build (this takes 1-2 hours on Pi 4)
make -j4

# Install
sudo make install
sudo ldconfig
```

## Building

### Step 1: Navigate to Project
```bash
cd CPP_Complete
```

### Step 2: Run Build Script
```bash
chmod +x build.sh
./build.sh              # Release build (default)
./build.sh Debug        # Debug build with symbols
```

### Step 3: Alternative Manual Build
```bash
mkdir -p build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j4
```

## Build Output

The compiled executable will be at:
```
CPP_Complete/build/VisionBasedRCCarControl
```

## Verifying Installation

### Test Compilation
```bash
cd CPP_Complete/build
./VisionBasedRCCarControl --help
```

Should output:
```
Usage: ./VisionBasedRCCarControl [options]
Options:
  --config <path>      Path to config file (default: config/config.json)
  --simulate          Run in simulation mode
  --device <mac>      BLE device MAC address
  --help              Show this help message
```

### Test in Simulation Mode
```bash
cd CPP_Complete/build
./VisionBasedRCCarControl --simulate
```

Should show:
- Configuration loading
- Component initialization
- Camera, tracker, BLE startup messages
- ROI selection window (if --simulate and UI enabled)

## Troubleshooting Build Issues

### CMake Not Found
```bash
# macOS
brew install cmake

# Linux
sudo apt-get install cmake
```

### OpenCV Not Found
```bash
# macOS
brew install opencv

# Linux
sudo apt-get install libopencv-dev

# Check installation
pkg-config --modversion opencv4
```

### nlohmann_json Not Found
```bash
# macOS
brew install nlohmann-json

# Linux
sudo apt-get install nlohmann-json3-dev

# Or download header-only version
# Place nlohmann/json.hpp in include directory
```

### Compiler Errors

#### "undefined reference to cv::..."
- OpenCV library not linked
- Check CMakeLists.txt has correct OpenCV_LIBS
- Reinstall OpenCV

#### "no member named 'TrackerCSRT_create'..."
- Using opencv-python instead of opencv-contrib-python
- Install full opencv-contrib-cpp development files

#### "json.hpp not found"
- nlohmann_json header-only library not installed
- Install via package manager or add to include path

### Linker Errors
```bash
# Check OpenCV configuration
pkg-config --cflags --libs opencv4

# If missing, rebuild with correct PKG_CONFIG_PATH
export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH
cmake ..
```

## Performance Build

For release/production builds with optimizations:

```bash
cd CPP_Complete
./build.sh Release

# Or manually:
cd build
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS="-O3 -march=native" ..
make -j4
```

## Debug Build

For debugging with GDB:

```bash
cd CPP_Complete
./build.sh Debug

# Run with debugger
gdb ./build/VisionBasedRCCarControl
```

## Cross-Compilation

For building on macOS/Ubuntu for Raspberry Pi:

```bash
# Install cross-compiler
sudo apt-get install gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf cmake

# Build
mkdir -p build-rpi
cd build-rpi
cmake -DCMAKE_TOOLCHAIN_FILE=../toolchain-rpi.cmake ..
make -j4
```

Note: Need to create `toolchain-rpi.cmake` with ARM compiler settings.

## Installation (Optional)

To install the binary system-wide:

```bash
cd CPP_Complete/build
sudo make install

# Binary installed to /usr/local/bin
# Config installed to /usr/local/config
```

## Verification Checklist

- [ ] CMake version 3.10+
- [ ] C++17 compiler available
- [ ] OpenCV 4.0+ installed
- [ ] nlohmann_json available
- [ ] No build errors
- [ ] Executable created in build directory
- [ ] Help message displays correctly
- [ ] Simulation mode runs without errors

## Next Steps

1. **Configure**: Edit `config/config.json`
2. **Run**: `./VisionBasedRCCarControl --simulate`
3. **Connect Hardware**: Add device MAC, connect camera and BLE
4. **Deploy**: Copy to Raspberry Pi and run

## Support

For build issues:
1. Check all dependencies are installed
2. Verify OpenCV installation: `pkg-config --modversion opencv4`
3. Check CMake output for missing packages
4. Review compilation errors carefully (often helpful)
5. Try clean rebuild: `rm -rf build && ./build.sh`
