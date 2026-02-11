# Complete Raspberry Pi 4 Kali OS Setup Guide

## Overview

This guide provides step-by-step instructions to set up and run the Vision-Based Autonomous RC Car Control System on a **Raspberry Pi 4** running **Kali Linux** from scratch. This assumes a fresh installation with no prior software installed.

## Prerequisites

### Hardware Required
- **Raspberry Pi 4** (4GB or 8GB RAM recommended)
- **MicroSD Card** (32GB or larger, Class 10 or better)
- **Power Supply** (5V 3A USB-C for Pi 4)
- **Sony Alpha 73 4K Camera** (or compatible USB/webcam)
- **DRIFT RC Car** (BLE-enabled)
- **Ethernet cable** or **WiFi** for internet connection
- **HDMI cable and monitor** (for initial setup)
- **USB keyboard and mouse** (for initial setup)

### Software Required
- **Kali Linux ARM image** for Raspberry Pi 4
- **Raspberry Pi Imager** (on your PC) or `dd` command

## Step 1: Install Kali Linux on Raspberry Pi 4

### 1.1 Download Kali Linux ARM Image

```bash
# On your PC, download Kali Linux ARM image for Raspberry Pi 4
# Visit: https://www.kali.org/get-kali/#kali-arm
# Download: "Kali Linux Raspberry Pi 4 (64-bit)" image
# File will be something like: kali-linux-2024.x-rpi4-64.img.xz
```

### 1.2 Flash Image to MicroSD Card

**Option A: Using Raspberry Pi Imager (Recommended)**
1. Download and install [Raspberry Pi Imager](https://www.raspberrypi.org/software/)
2. Insert MicroSD card into your PC
3. Open Raspberry Pi Imager
4. Choose "Use custom image" and select the downloaded `.img.xz` file
5. Select your MicroSD card
6. Click "Write" and wait for completion

**Option B: Using Command Line (Linux/Mac)**
```bash
# Extract the image
xz -d kali-linux-2024.x-rpi4-64.img.xz

# Find your SD card device (be careful!)
lsblk  # or diskutil list on Mac

# Flash (replace /dev/sdX with your SD card device)
sudo dd if=kali-linux-2024.x-rpi4-64.img of=/dev/sdX bs=4M status=progress
sudo sync
```

### 1.3 Enable SSH (Optional but Recommended)

Before ejecting the SD card, create an empty file named `ssh` in the boot partition:
```bash
# On Linux/Mac
touch /path/to/boot/partition/ssh

# This enables SSH on first boot
```

### 1.4 Boot Raspberry Pi

1. Insert MicroSD card into Raspberry Pi 4
2. Connect HDMI monitor, keyboard, mouse
3. Connect power supply
4. Wait for Kali Linux to boot (first boot may take 2-3 minutes)

### 1.5 Initial Login

- **Default username**: `kali`
- **Default password**: `kali`
- You'll be prompted to change password on first login

## Step 2: Update System

```bash
# Update package lists
sudo apt update

# Upgrade all packages
sudo apt upgrade -y

# Install essential tools
sudo apt install -y curl wget git vim nano
```

## Step 3: Install Build Tools and Dependencies

### 3.1 Install Build Essentials

```bash
sudo apt install -y build-essential cmake pkg-config
```

### 3.2 Install OpenCV Dependencies

```bash
# Install OpenCV and development libraries
sudo apt install -y libopencv-dev libopencv-contrib-dev

# Verify installation
pkg-config --modversion opencv4
```

**Note**: If OpenCV is not available in Kali repositories, you may need to build from source (see Step 3.3).

### 3.3 Build OpenCV from Source (If Needed)

If `libopencv-dev` doesn't include tracking modules or you need a specific version:

```bash
# Install dependencies
sudo apt install -y \
    libjpeg-dev libpng-dev libtiff-dev \
    libavcodec-dev libavformat-dev libswscale-dev libv4l-dev \
    libxvidcore-dev libx264-dev \
    libgtk-3-dev libatlas-base-dev gfortran \
    python3-dev python3-numpy \
    libtbb2 libtbb-dev libdc1394-dev

# Download OpenCV
cd ~
git clone https://github.com/opencv/opencv.git
git clone https://github.com/opencv/opencv_contrib.git

# Build OpenCV (this takes 1-2 hours on Pi 4)
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
      -D BUILD_PERF_TESTS=OFF \
      -D BUILD_opencv_python3=ON \
      ..

# Compile (use all cores)
make -j4

# Install
sudo make install
sudo ldconfig

# Verify
pkg-config --modversion opencv4
```

### 3.4 Install Bluetooth Development Libraries

```bash
# Install BlueZ and development libraries
sudo apt install -y libbluetooth-dev bluetooth bluez

# Enable Bluetooth service
sudo systemctl enable bluetooth
sudo systemctl start bluetooth

# Verify Bluetooth is working
hciconfig
```

### 3.5 Install Additional Dependencies

```bash
# Install JSON library (if config uses JSON)
sudo apt install -y libjsoncpp-dev

# Install threading library (usually included)
sudo apt install -y libpthread-stubs0-dev
```

## Step 4: Clone the Project

### 4.1 Clone Repository

```bash
# Navigate to home directory
cd ~

# Clone the project (replace with your repository URL)
git clone https://github.com/yourusername/IoT-Project-Vision-based-autonomous-RC-car-control-system.git

# Navigate to project directory
cd IoT-Project-Vision-based-autonomous-RC-car-control-system/apl
```

**Or if you have the project files locally:**
```bash
# Copy project files to Raspberry Pi using SCP
# From your PC:
scp -r /path/to/project kali@raspberry-pi-ip:~/IoT-Project-Vision-based-autonomous-RC-car-control-system
```

## Step 5: Build the Project

### 5.1 Create Build Directory

```bash
cd ~/IoT-Project-Vision-based-autonomous-RC-car-control-system/apl
mkdir -p build
ccd build
```

### 5.2 Configure with CMake

```bash
# Basic configuration
cmake ..

# Or for Release build with optimizations
cmake -DCMAKE_BUILD_TYPE=Release ..
```

**Expected output:**
```
-- The CXX compiler identification is GNU 11.x.x
-- OpenCV version: 4.x.x
-- Configuring done
-- Generating done
-- Build files have been written to: /home/kali/.../build
```

### 5.3 Compile

```bash
# Compile using all available cores
make -j4

# Or use single core if you encounter memory issues
make -j1
```

**Expected output:**
```
[  5%] Building CXX object CMakeFiles/VisionBasedRCCarControl.dir/src/main.cpp.o
[ 10%] Building CXX object CMakeFiles/VisionBasedRCCarControl.dir/src/camera_capture.cpp.o
...
[100%] Built target VisionBasedRCCarControl
```

### 5.4 Verify Build

```bash
# Check if executable was created
ls -lh VisionBasedRCCarControl

# Should show something like:
# -rwxr-xr-x 1 kali kali 2.5M ... VisionBasedRCCarControl
```

## Step 6: Configure the System

### 6.1 Edit Configuration File

```bash
# Edit config file
nano ../config/config.json
```

**Key settings to configure:**

```json
{
  "camera": {
    "index": 0,           // Camera device index (usually 0)
    "width": 1920,        // Resolution width (1920 for 1080p)
    "height": 1080,       // Resolution height (1080 for 1080p)
    "fps": 30            // Frame rate
  },
  "ble": {
    "device_mac": "ed:5c:23:84:48:8d",  // Your car's MAC address
    "characteristic_uuid": "6e400002-b5a3-f393-e0a9-e50e24dcca9e",
    "command_rate_hz": 200
  },
  "boundary": {
    "black_threshold": 50,    // Adjust based on your track
    "ray_max_length": 200,
    "evasive_threshold": 80,
    "base_speed": 10
  },
  "system": {
    "show_ui": true,
    "autonomous_mode": false   // Set to true for autonomous control
  }
}
```

### 6.2 Test Camera Connection

```bash
# List video devices
v4l2-ctl --list-devices

# Test camera capture (if using USB webcam)
v4l2-ctl --device=/dev/video0 --all

# For Sony Alpha 73, you may need:
# - HDMI capture card (USB3.0)
# - Network streaming setup
# - Check camera documentation for connection method
```

### 6.3 Test Bluetooth Connection

```bash
# Check Bluetooth adapter
hciconfig

# Scan for BLE devices
sudo hcitool lescan

# Or use bluetoothctl
bluetoothctl
> scan on
> devices
> quit
```

## Step 7: Run the System

### 7.1 Basic Run

```bash
cd ~/IoT-Project-Vision-based-autonomous-RC-car-control-system/apl/build
./VisionBasedRCCarControl
```

### 7.2 With Custom Config

```bash
./VisionBasedRCCarControl -c /path/to/config.json
```

### 7.3 Manual Mode (Testing Without Autonomous Control)

```bash
./VisionBasedRCCarControl --manual
```

### 7.4 Headless Mode (No UI)

```bash
./VisionBasedRCCarControl --no-ui
```

## Step 8: First Run Workflow

### 8.1 Initial Setup Sequence

1. **Start the program**
   ```bash
   ./VisionBasedRCCarControl
   ```

2. **Select ROI (Region of Interest)**
   - A window will appear showing camera feed
   - Click and drag to draw a bounding box around the car
   - Press SPACE or ENTER to confirm

3. **System will initialize**
   - Tracker initializes
   - BLE connection attempts
   - Tracking starts

4. **Monitor output**
   - Watch tracking window
   - Check console for status messages
   - Verify BLE connection status

5. **Stop the system**
   - Press Ctrl+C or 'q' key
   - System will send STOP command to car
   - Graceful shutdown

## Step 9: Troubleshooting

### 9.1 Build Issues

**Problem: OpenCV not found**
```bash
# Check if OpenCV is installed
pkg-config --modversion opencv4

# If not found, install or build OpenCV (see Step 3.3)
```

**Problem: CMake errors**
```bash
# Update CMake
sudo apt install --upgrade cmake

# Clear build directory and retry
cd build
rm -rf *
cmake ..
make -j4
```

**Problem: Compilation errors**
```bash
# Check compiler version
g++ --version  # Should be GCC 8+ or Clang

# Install/update build tools
sudo apt install --upgrade build-essential
```

### 9.2 Runtime Issues

**Problem: Camera not found**
```bash
# Check camera permissions
sudo usermod -a -G video $USER
# Log out and log back in

# List video devices
ls -l /dev/video*

# Test with v4l2-ctl
v4l2-ctl --device=/dev/video0 --all
```

**Problem: BLE connection failed**
```bash
# Check Bluetooth is enabled
sudo systemctl status bluetooth

# Enable if needed
sudo systemctl enable bluetooth
sudo systemctl start bluetooth

# Check adapter
hciconfig

# Scan for devices
sudo hcitool lescan
```

**Problem: Low frame rate**
```bash
# Reduce resolution in config
# Change width/height to 1280x720 or 640x480

# Use simpler tracker (KCF instead of CSRT)
# In config: tracker.type = "KCF"

# Disable UI
./VisionBasedRCCarControl --no-ui
```

**Problem: Out of memory**
```bash
# Check memory usage
free -h

# Increase swap space
sudo dphys-swapfile swapoff
sudo nano /etc/dphys-swapfile  # Set CONF_SWAPSIZE=2048
sudo dphys-swapfile setup
sudo dphys-swapfile swapon

# Or compile with single core
make -j1
```

### 9.3 Performance Optimization

**For better performance on Raspberry Pi 4:**

1. **Overclock (optional, at your own risk)**
   ```bash
   sudo nano /boot/config.txt
   # Add:
   # over_voltage=2
   # arm_freq=2000
   # gpu_freq=750
   ```

2. **Use faster SD card** (Class 10 or better)

3. **Reduce camera resolution** (1920x1080 or lower)

4. **Disable unnecessary services**
   ```bash
   sudo systemctl disable bluetooth  # If not using BLE
   sudo systemctl disable avahi-daemon
   ```

5. **Use simpler tracker** (KCF is faster than CSRT)

## Step 10: System Service (Optional)

### 10.1 Create Systemd Service

Create a service file to run the system on boot:

```bash
sudo nano /etc/systemd/system/rc-car-control.service
```

**Add:**
```ini
[Unit]
Description=Vision-Based RC Car Control System
After=network.target bluetooth.target

[Service]
Type=simple
User=kali
WorkingDirectory=/home/kali/IoT-Project-Vision-based-autonomous-RC-car-control-system/apl/build
ExecStart=/home/kali/IoT-Project-Vision-based-autonomous-RC-car-control-system/apl/build/VisionBasedRCCarControl
Restart=on-failure
RestartSec=5

[Install]
WantedBy=multi-user.target
```

### 10.2 Enable Service

```bash
# Reload systemd
sudo systemctl daemon-reload

# Enable service (starts on boot)
sudo systemctl enable rc-car-control.service

# Start service
sudo systemctl start rc-car-control.service

# Check status
sudo systemctl status rc-car-control.service

# View logs
sudo journalctl -u rc-car-control.service -f
```

## Step 11: Testing Checklist

Before running autonomous mode:

- [ ] System boots successfully
- [ ] OpenCV is installed and working
- [ ] Camera captures frames
- [ ] Bluetooth adapter is detected
- [ ] RC car is discoverable via BLE
- [ ] Project compiles without errors
- [ ] Configuration file is correct
- [ ] ROI selection works
- [ ] Tracking initializes successfully
- [ ] BLE connection establishes
- [ ] Commands can be sent to car
- [ ] Car responds to commands
- [ ] Emergency stop works (Ctrl+C)

## Step 12: Next Steps

1. **Tune Parameters**
   - Adjust `black_threshold` for your track
   - Tune `base_speed` and steering limits
   - Test different tracker types

2. **Test on Track**
   - Start with manual mode
   - Test tracking accuracy
   - Test boundary detection
   - Gradually enable autonomous mode

3. **Optimize Performance**
   - Profile with `perf` or `gprof`
   - Optimize hot paths
   - Adjust frame rate and resolution

4. **Implement BLE Library**
   - Choose library (simpleble-cpp or BlueZ)
   - Implement actual BLE calls in `ble_handler.cpp`
   - Test connection stability

## Additional Resources

- **Kali Linux Documentation**: https://www.kali.org/docs/
- **Raspberry Pi Documentation**: https://www.raspberrypi.org/documentation/
- **OpenCV Documentation**: https://docs.opencv.org/
- **BlueZ Documentation**: http://www.bluez.org/

## Support

If you encounter issues:

1. Check logs: `journalctl -u rc-car-control.service`
2. Review configuration: `cat config/config.json`
3. Test components individually (camera, BLE, tracking)
4. Check system resources: `htop`, `free -h`
5. Review project documentation in `docs/` directory

## Summary

You should now have:
- ✅ Kali Linux installed on Raspberry Pi 4
- ✅ All dependencies installed
- ✅ Project compiled and ready to run
- ✅ System configured for your hardware
- ✅ Basic testing completed

The system is ready for autonomous RC car control!
