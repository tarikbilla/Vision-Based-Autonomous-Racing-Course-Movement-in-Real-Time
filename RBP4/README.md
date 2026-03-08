# RBP4 (Raspberry Pi 4 Optimized Version) ✅

**Status:** Complete | **Same as Main Project** - Just pick your mode!

This folder contains the **exact same functionality as the main project**, optimized for Raspberry Pi 4, with a simple choice between window and windowless modes.

## 🎯 WHICH SETUP DO YOU WANT?

### ✅ "I want to see the camera feed & controls" → **USE GUI MODE**
- **OS:** Raspberry Pi OS Desktop (64-bit) with HDMI display
- **Command:** `./RBP4/run_rbp4.sh`
- **Benefits:** See detection in real-time, debug easily, visual feedback
- **Requirements:** HDMI monitor connected to Pi

**Setup Steps:**
```bash
# 1. Flash Raspberry Pi OS Desktop (64-bit) to SD card
# 2. Enable camera: sudo raspi-config → Interface Options → Camera → Enable
# 3. Build: ./RBP4/build_rbp4.sh
# 4. Run: ./RBP4/run_rbp4.sh
# 5. Watch the live detection window!
```

---

### ✅ "I control via SSH, no display needed" → **USE HEADLESS MODE**
- **OS:** Raspberry Pi OS Lite (64-bit) OR Ubuntu Server 22.04 LTS
- **Command:** `./RBP4/run_rbp4_headless.sh`
- **Benefits:** Lower CPU usage, remote control, no display overhead
- **Requirements:** SSH access to Pi

**Setup Steps:**
```bash
# 1. Flash Raspberry Pi OS Lite (64-bit) to SD card
# 2. Enable camera: sudo raspi-config → Interface Options → Camera → Enable
# 3. Build: ./RBP4/build_rbp4.sh
# 4. Run: ./RBP4/run_rbp4_headless.sh
# 5. Watch terminal output for detection status
```

---

### ❓ "I'm not sure / Having camera issues?" → **USE UBUNTU SERVER 22.04 LTS**
- **OS:** Ubuntu Server 22.04 LTS for Raspberry Pi
- **Why:** Better camera driver support, more reliable
- **Default:** Headless (CLI only) - can add GUI later
- **Command:** `./RBP4/run_rbp4_headless.sh` (or `./RBP4/run_rbp4.sh` if you install GUI)
- **Download:** https://cdimage.ubuntu.com/releases/jammy/release/

**Setup Steps:**
```bash
# 1. Flash Ubuntu Server 22.04 LTS (ARMv8) to SD card
# 2. SSH to your Pi:
#    ssh ubuntu@raspberrypi.local
#    Default password: ubuntu

# 3. Update system
sudo apt update && sudo apt upgrade -y

# 4. Install dependencies
sudo apt install -y build-essential cmake libopencv-dev bluetooth libbluetooth-dev

# 5. (OPTIONAL) Add GUI if you want to see detection window
#    sudo apt install -y lubuntu-desktop
#    sudo reboot
#    (Then connect HDMI monitor after reboot)

# 6. Build project
./RBP4/build_rbp4.sh

# 7. Run (choose one):
./RBP4/run_rbp4_headless.sh  # Works with or without GUI
# OR if GUI is installed and monitor connected:
./RBP4/run_rbp4.sh

# Camera works out-of-the-box on Ubuntu!
```

**Why Ubuntu is Best:**
- ✅ Excellent camera support (95%+)
- ✅ Works out-of-the-box (no configuration)
- ✅ Perfect for vision-based projects
- ✅ Same lightweight footprint as RPi OS Lite
- ✅ Can add GUI anytime if needed
- ✅ Better community support
- ✅ Professional, production-ready

---

## 🎨 UI SUPPORT GUIDE

### Does Ubuntu Server Support UI?
**YES!** ✅ But it depends on what you want:

| OS/Setup | Default GUI | Can Add GUI | Command |
|----------|-----------|-----------|---------|
| **RPi OS Lite** | ❌ No | ⚠️ Hard | `./RBP4/run_rbp4_headless.sh` |
| **RPi OS Desktop** | ✅ Yes | N/A | `./RBP4/run_rbp4.sh` |
| **Ubuntu Server** | ❌ No | ✅ Easy | `./RBP4/run_rbp4_headless.sh` |
| **Ubuntu Server + Lubuntu** | ✅ Yes | ✅ Already done | `./RBP4/run_rbp4.sh` |

### Ubuntu Server UI Options

#### Option 1: Headless Only (Default, Lightweight)
- **RAM:** ~200-400MB
- **CPU:** ~5% overhead
- **Use:** SSH remote control
- **Run:** `./RBP4/run_rbp4_headless.sh`
- **Best for:** Production, SSH access, minimal resources

#### Option 2: Add Lubuntu Desktop (5 minutes, Recommended)
```bash
sudo apt install -y lubuntu-desktop
sudo reboot
```
- **RAM:** ~800-1000MB
- **CPU:** ~15% overhead  
- **Use:** HDMI monitor + GUI window
- **Run:** `./RBP4/run_rbp4.sh` (with display) or headless
- **Best for:** Testing, debugging, development

### Recommendation
Start with **Ubuntu Server (headless)**, add Lubuntu GUI later if you need it:
1. **Initially:** Fast, lightweight, perfect for SSH
2. **If needed:** Add GUI in 5 minutes with one command
3. **Best of both:** Use headless OR GUI mode, your choice

---

| Use Case | OS | Command | Notes |
|----------|----|---------|----|
| **See car detection + controls** | RPi OS Desktop | `./RBP4/run_rbp4.sh` | Need HDMI |
| **Remote SSH operation** | RPi OS Lite | `./RBP4/run_rbp4_headless.sh` | Terminal output |
| **Camera problems / Unsure** | Ubuntu Server 22.04 | Either command | Most reliable |

---

## 🎯 Two Simple Modes

### Mode 1: WITH WINDOW (Shows Detection) - **RECOMMENDED WITH UI**
```bash
./RBP4/run_rbp4.sh
```
- ✅ **Live window showing car detection**
- ✅ **Interactive keyboard controls**
- ✅ See steering/speed in real-time
- ✅ Perfect for Raspberry Pi Desktop
- ❌ Needs HDMI display or X11 forwarding

**Best for:** Testing, debugging, visualization

### Mode 2: WITHOUT WINDOW (Headless) - **RECOMMENDED FOR SSH/REMOTE**
```bash
./RBP4/run_rbp4_headless.sh
```
- ✅ No display needed
- ✅ Perfect for SSH (remote control)
- ✅ Lower CPU usage
- ✅ Same control as Mode 1
- ❌ No visual feedback (terminal output only)

**Best for:** Production, remote operation, autonomous runs

## 📁 Structure

```
RBP4/
├── src/               # Synced with main project source
├── include/           # Synced with main project headers
├── config/
│   ├── config_rbp4_headless.json  # Headless config (show_window: false)
│   ├── config_rbp4_gui.json       # GUI config (show_window: true)
│   └── config_rbp4_ultra_low.json # Legacy ultra-low settings
├── build/             # Build output directory
├── CMakeLists.txt     # Separate CMake project
├── build_rbp4.sh      # Build script
├── run_rbp4.sh        # Run with GUI (requires display)
└── run_rbp4_headless.sh  # Run headless (no display needed)
```

## 🚀 Quick Start

### Option 1: Headless Mode (Recommended for SSH/Remote)

**For Raspberry Pi OS Lite or headless setup:**

```bash
cd /path/to/IoT-Project-Vision-based-autonomous-RC-car-control-system
./RBP4/build_rbp4.sh
./RBP4/run_rbp4_headless.sh
```

**Features:**
- ✅ No display required
- ✅ Works over SSH
- ✅ Press `a` then Enter to start auto red car detection
- ✅ Lower CPU usage (no GUI rendering)
- ✅ `show_window: false` in config

### Option 2: GUI Mode (Requires Display)

**For Raspberry Pi with desktop environment:**

```bash
cd /path/to/IoT-Project-Vision-based-autonomous-RC-car-control-system
./RBP4/build_rbp4.sh
./RBP4/run_rbp4.sh
```

**Features:**
- ✅ Live visualization window
- ✅ Shows car detection, boundaries, centerline
- ✅ Can press 'q' to quit
- ❌ Requires X11/Wayland display server

## ⚙️ Configuration Files

### config_rbp4_headless.json
```json
{
  "camera": {
    "width": 640,
    "height": 480,
    "fps": 15
  },
  "ui": {
    "show_window": false,  ← No GUI
    "color_tracking": true  ← Auto detection
  }
}
```

### config_rbp4_gui.json
```json
{
  "camera": {
    "width": 640,
    "height": 480,
    "fps": 15
  },
  "ui": {
    "show_window": true,   ← Enable GUI
    "color_tracking": true
  }
}
```

## 🔧 Build

```bash
chmod +x RBP4/build_rbp4.sh RBP4/run_rbp4.sh RBP4/run_rbp4_headless.sh
./RBP4/build_rbp4.sh
```

## 🏃 Run Options

### Headless (No Display)
```bash
./RBP4/run_rbp4_headless.sh
```

### With GUI
```bash
./RBP4/run_rbp4.sh
```

### Direct Binary Execution
```bash
# Headless
./RBP4/build/VisionBasedRCCarControl_RBP4 --config ./RBP4/config/config_rbp4_headless.json

# GUI
./RBP4/build/VisionBasedRCCarControl_RBP4 --config ./RBP4/config/config_rbp4_gui.json
```

## 🐛 Troubleshooting

### Error: "could not connect to display"
**Problem:** Running GUI version without X11 display

**Solution:** Use headless mode instead:
```bash
./RBP4/run_rbp4_headless.sh
```

### Error: "Qt platform plugin could not be initialized"
**Problem:** Qt trying to use GUI on headless system

**Solution:** The headless script sets `QT_QPA_PLATFORM=offscreen` automatically, but you can also:
```bash
export QT_QPA_PLATFORM=offscreen
./RBP4/build/VisionBasedRCCarControl_RBP4 --config ./RBP4/config/config_rbp4_headless.json
```

### Camera Not Opening
**Problem:** Camera index incorrect or permissions

**Solution:**
```bash
# List video devices
ls -l /dev/video*

# Give camera permissions
sudo usermod -a -G video $USER

# Edit config camera index if needed
nano RBP4/config/config_rbp4_headless.json
```

## 📊 Performance Settings

| Setting | Headless | GUI | Notes |
|---------|----------|-----|-------|
| Resolution | 640×480 | 640×480 | Lower than main (1280×720) |
| FPS | 15 | 15 | Optimized for RPi4 |
| Tracker | KCF | KCF | Faster than CSRT |
| BLE Rate | 30Hz | 30Hz | Lower than main (50Hz) |
| Show Window | ❌ | ✅ | Headless saves CPU |
| Auto Detection | ✅ | ✅ | Enabled by default |

## 🔄 Syncing with Main Project

RBP4 code is synced with main project. To update:

```bash
# Sync source files
rsync -av src/ RBP4/src/
rsync -av include/ RBP4/include/

# Rebuild
./RBP4/build_rbp4.sh
```

## 📝 Recommended Raspberry Pi Setup

### For Headless Operation
```bash
# 1. Install Raspberry Pi OS Lite (no desktop)
# 2. Enable camera
sudo raspi-config
# Interface Options -> Camera -> Enable

# 3. Install dependencies
sudo apt update
sudo apt install -y cmake build-essential pkg-config \
    libopencv-dev bluetooth libbluetooth-dev

# 4. Build and run
git clone <your-repo>
cd IoT-Project-Vision-based-autonomous-RC-car-control-system
./RBP4/build_rbp4.sh
./RBP4/run_rbp4_headless.sh
```

### For GUI/UI Mode (WITH DISPLAY)
```bash
# 1. Install Raspberry Pi OS Desktop (64-bit)
#    - Comes with GUI, X11, and full camera support
#    - Download from: https://www.raspberrypi.com/software/

# 2. After installation, enable camera:
sudo raspi-config
# Interface Options -> Camera -> Enable

# 3. Install dependencies (same as above)
sudo apt update
sudo apt install -y cmake build-essential pkg-config \
    libopencv-dev bluetooth libbluetooth-dev

# 4. Build
./RBP4/build_rbp4.sh

# 5. Run WITH GUI window
./RBP4/run_rbp4.sh

# You'll see a live window showing:
# - Red car detection box
# - Steering/speed values
# - Boundary detection rays
# - Path centerline
```

### For Ubuntu Server 22.04 LTS (BEST FOR CAMERA)
```bash
# 1. Install Ubuntu Server 22.04 LTS (ARMv8/64-bit)
#    Download from: https://cdimage.ubuntu.com/releases/jammy/release/
#    Look for: ubuntu-22.04.X-preinstalled-server-arm64+raspi.img.xz
#    Flash with Raspberry Pi Imager or Balena Etcher

# 2. SSH to your Pi (after first boot, ~60 seconds)
ssh ubuntu@raspberrypi.local
# Default password: ubuntu (you'll be forced to change it)

# 3. System update
sudo apt update
sudo apt upgrade -y
sudo apt install -y build-essential cmake pkg-config

# 4. Install camera support (Ubuntu has excellent camera drivers)
sudo apt install -y libraspberrypi-bin libraspberrypi0 libopencv-dev

# 5. Verify camera works
vcgencmd get_camera
# Output should show: supported=1 detected=1

libcamera-hello --list-cameras
# Output should show your camera model

# 6. Clone and build your project
git clone <your-repo-url>
cd IoT-Project-Vision-based-autonomous-RC-car-control-system
./RBP4/build_rbp4.sh

# 7. Run headless or with display
./RBP4/run_rbp4_headless.sh   # SSH mode (no display needed)
# OR
./RBP4/run_rbp4.sh            # With HDMI monitor
```

## 🎮 Controls

### Headless Mode
- **Start:** `./RBP4/run_rbp4_headless.sh`
- **After BLE connects:** Press `a` then Enter to start auto detection
- **Stop:** Press `Ctrl+C` (sends STOP commands to car)
- If terminal input is not available, headless mode auto-starts detection

### GUI Mode
- **Start:** `./RBP4/run_rbp4.sh`
- **Stop:** Press `q` in window or `Ctrl+C`
- **Select ROI:** Press `s` (manual tracking)
- **Auto detect:** Press `a` (color detection)

## 📌 Notes

- Headless mode uses no camera feed window (`show_window: false`)
- Press `a` then Enter to start detection after connection (or auto-start if no terminal input)
- BLE MAC address can be configured in config files
- All settings are JSON-based, no hardcoding

## 🔍 Understanding the Output

When you run headless mode, you'll see detection feedback:

### Good Detection (Car Found):
```
[MOTION] [45] Car: (320,240) | Speed: 15 | L:0 R:15 | Rays: L=120 C=200 R=180
[COLOR] [46] Car: (325,238) | Speed: 12 | L:5 R:0 | Rays: L=115 C=195 R=185
```
- **MOTION/COLOR**: Detection method used
- **Car: (x,y)**: Car position in frame
- **Speed**: Forward speed (0-15)
- **L/R**: Left/Right turn values
- **Rays**: Distance to boundaries

### Failed Detection (Car Not Found):
```
[FAIL] [47] Car not detected - STOPPING (motion:Y color:Y warmup:done)
```
- **FAIL**: No car detected
- **STOPPING**: Car receives stop command for safety
- **motion:Y/N**: Motion detection enabled?
- **color:Y/N**: Color detection enabled?
- **warmup:done/wait**: System warm-up status

### What the BLE commands mean:
```
[*] BLE -> write (hex): bf0a00082800000f00000000020000
                                   ^^^^^^^^^^^^
                                   Speed: 0x0f (15)
                                       Left: 0x00 (straight)
                                           Right: 0x00 (straight)
```

### Troubleshooting Detection Issues:

**If you see many `[FAIL]` messages:**

1. **Check lighting** - Red car detection needs good lighting
2. **Check camera angle** - Car should be clearly visible
3. **Verify red color** - Camera must see red surface of car
4. **Motion detection warmup** - First 30 frames are for initialization

**Commands to adjust if detection fails:**

Edit `RBP4/config/config_rbp4_headless.json`:
```json
{
  "detection": {
    "red_car": {
      "hue_min": 0,      // Lower = more red colors
      "hue_max": 10,     // Raise if car appears orange
      "sat_min": 100,    // Lower if color is washed out
      "val_min": 100     // Lower for darker environments
    }
  }
}
```


## Run direct binary (simulation)
```bash
./RBP4/build/VisionBasedRCCarControl_RBP4 --config ./RBP4/config/config_rbp4_ultra_low.json --simulate
```

## Why `--config` is required for direct run
- Direct run without `--config` uses default `config/config.json` (root project config).
- Use `--config ./RBP4/config/config_rbp4_ultra_low.json` to force RBP4 optimized settings.

## Notes
- If Pi still hangs, lower to `240x180` and `6 FPS` in `RBP4/config/config_rbp4_ultra_low.json`.
- Keep using only scripts inside `RBP4` for this isolated version.
