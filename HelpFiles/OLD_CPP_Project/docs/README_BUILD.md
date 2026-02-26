# Vision-Based RC Car Control System - Build Instructions

## Prerequisites

### Hardware
- Raspberry Pi 4 (4GB or 8GB RAM recommended)
- Sony Alpha 73 4K Camera (or compatible USB/webcam)
- BLE-enabled RC Car (DR!FT car)

### Software Dependencies

#### On Raspberry Pi OS (Debian-based):

```bash
# Update system
sudo apt-get update
sudo apt-get upgrade -y

# Install build tools
sudo apt-get install -y build-essential cmake git pkg-config

# Install OpenCV dependencies
sudo apt-get install -y libopencv-dev libopencv-contrib-dev

# Install BLE development libraries
sudo apt-get install -y libbluetooth-dev

# Optional: Install GStreamer for advanced camera support
sudo apt-get install -y libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev
```

#### Alternative: Build OpenCV from source (if needed)

If the packaged OpenCV doesn't include tracking modules:

```bash
# Install OpenCV dependencies
sudo apt-get install -y libjpeg-dev libpng-dev libtiff-dev
sudo apt-get install -y libavcodec-dev libavformat-dev libswscale-dev libv4l-dev
sudo apt-get install -y libxvidcore-dev libx264-dev
sudo apt-get install -y libgtk-3-dev libatlas-base-dev gfortran
sudo apt-get install -y python3-dev

# Download and build OpenCV (this takes a while)
cd ~
git clone https://github.com/opencv/opencv.git
git clone https://github.com/opencv/opencv_contrib.git
cd opencv
mkdir build && cd build
cmake -D CMAKE_BUILD_TYPE=RELEASE \
      -D CMAKE_INSTALL_PREFIX=/usr/local \
      -D OPENCV_EXTRA_MODULES_PATH=~/opencv_contrib/modules \
      -D ENABLE_NEON=ON \
      -D ENABLE_VFPV3=ON \
      -D BUILD_EXAMPLES=OFF \
      -D BUILD_DOCS=OFF \
      -D BUILD_TESTS=OFF \
      ..
make -j4
sudo make install
sudo ldconfig
```

## Building the Project

### 1. Clone/Navigate to Project Directory

```bash
cd /path/to/IoT-Project-Vision-based-autonomous-RC-car-control-system
```

### 2. Create Build Directory

```bash
mkdir -p build
cd build
```

### 3. Configure with CMake

```bash
cmake ..
```

Or for Release build with optimizations:

```bash
cmake -DCMAKE_BUILD_TYPE=Release ..
```

### 4. Build

```bash
make -j4
```

The executable will be created as `VisionBasedRCCarControl` in the `build/` directory.

## Configuration

### 1. Edit Configuration File

Edit `config/config.json` to match your setup:

```bash
nano config/config.json
```

Key settings:
- `camera.index`: Camera device index (usually 0)
- `camera.width/height`: Resolution (1920x1080 recommended for Pi 4)
- `ble.device_mac`: Your RC car's MAC address
- `boundary.black_threshold`: Threshold for boundary detection (adjust based on track)

### 2. Test Camera Connection

```bash
# Test if camera is accessible
v4l2-ctl --list-devices
```

## Running the System

### Basic Run

```bash
cd build
./VisionBasedRCCarControl
```

### With Custom Config

```bash
./VisionBasedRCCarControl -c /path/to/config.json
```

### Manual Mode (for testing without autonomous control)

```bash
./VisionBasedRCCarControl --manual
```

### Without UI (headless mode)

```bash
./VisionBasedRCCarControl --no-ui
```

## Usage Flow

1. **Start the program**: The system will initialize camera and BLE
2. **Select ROI**: A window will appear asking you to select the object (car) to track
   - Click and drag to draw a bounding box around the car
   - Press SPACE or ENTER to confirm
3. **Autonomous Mode**: Once ROI is selected, tracking and guidance will start
4. **Monitor**: Watch the tracking and guidance windows (if UI enabled)
5. **Stop**: Press Ctrl+C or 'q' key to stop gracefully

## Troubleshooting

### Camera Issues

- **Camera not found**: Check `camera.index` in config (try 0, 1, 2...)
- **Permission denied**: Add user to `video` group: `sudo usermod -a -G video $USER`
- **Low FPS**: Reduce resolution in config (e.g., 1280x720)

### BLE Issues

- **Connection failed**: 
  - Verify MAC address is correct
  - Ensure car is powered on and in pairing mode
  - Check Bluetooth: `bluetoothctl` → `scan on`
- **Commands not sending**: Check BLE handler implementation (currently placeholder)

### Performance Issues

- **Low frame rate**: 
  - Reduce camera resolution
  - Use simpler tracker (KCF instead of CSRT)
  - Disable UI: `--no-ui`
- **High CPU usage**: Normal for real-time processing, but can optimize by:
  - Processing every Nth frame
  - Using ROI-based processing

### Build Issues

- **OpenCV not found**: 
  - Install libopencv-dev or build from source
  - Set `OpenCV_DIR` in CMake: `cmake -DOpenCV_DIR=/usr/local/lib/cmake/opencv4 ..`
- **Missing tracking modules**: Build OpenCV with contrib modules (see above)

## BLE Implementation Note

The current BLE handler (`src/ble_handler.cpp`) contains placeholder implementations for:
- `connectToDevice()`
- `disconnectFromDevice()`
- `sendCommand()`

**You need to implement these methods** using your chosen BLE library:

### Option 1: simpleble-cpp
```cpp
#include <simpleble/SimpleBLE.h>
// Implement using SimpleBLE::Adapter and SimpleBLE::Peripheral
```

### Option 2: BlueZ D-Bus
```cpp
#include <dbus/dbus.h>
// Implement using org.bluez D-Bus API
```

### Option 3: BlueZ gatttool (system calls)
```cpp
#include <cstdlib>
// Use system() calls to gatttool
```

See `src/ble_handler.cpp` for TODO comments indicating where to add implementation.

## Development

### Project Structure

```
.
├── CMakeLists.txt          # Build configuration
├── include/                # Header files
│   ├── types.h
│   ├── config_manager.h
│   ├── camera_capture.h
│   ├── object_tracker.h
│   ├── boundary_detection.h
│   ├── ble_handler.h
│   └── control_orchestrator.h
├── src/                    # Source files
│   ├── main.cpp
│   ├── config_manager.cpp
│   ├── camera_capture.cpp
│   ├── object_tracker.cpp
│   ├── boundary_detection.cpp
│   ├── ble_handler.cpp
│   └── control_orchestrator.cpp
├── config/                 # Configuration files
│   └── config.json
└── build/                  # Build output (created)
```

### Code Style

- C++17 standard
- Namespace: `rc_car`
- Naming: camelCase for functions, snake_case for variables
- Thread-safe queues for inter-thread communication

## License

This project is intended for educational and research purposes.
