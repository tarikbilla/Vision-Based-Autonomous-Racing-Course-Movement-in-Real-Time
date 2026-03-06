# RBP4 (Raspberry Pi 4 Optimized Version) ✅

**Status:** Complete | **Same as Main Project** - Just pick your mode!

This folder contains the **exact same functionality as the main project**, optimized for Raspberry Pi 4, with a simple choice between window and windowless modes.

## 🎯 Two Simple Modes

### Mode 1: WITH WINDOW (Shows Detection)
```bash
./RBP4/run_rbp4.sh
```
- ✅ Live window showing detection
- ✅ Interactive controls
- ✅ Same as main project
- ❌ Needs HDMI or X11

### Mode 2: WITHOUT WINDOW (Headless)
```bash
./RBP4/run_rbp4_headless.sh
```
- ✅ No display needed
- ✅ Perfect for SSH
- ✅ Lower CPU usage
- ✅ Same control as Mode 1

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
- ✅ Auto red car detection enabled
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

## 🎮 Controls

### Headless Mode
- **Start:** `./RBP4/run_rbp4_headless.sh`
- **Stop:** Press `Ctrl+C` (sends STOP commands to car)
- **No keyboard input needed** - auto detection enabled

### GUI Mode
- **Start:** `./RBP4/run_rbp4.sh`
- **Stop:** Press `q` in window or `Ctrl+C`
- **Select ROI:** Press `s` (manual tracking)
- **Auto detect:** Press `a` (color detection)

## 📌 Notes

- Headless mode automatically enables red car color detection
- No user input required in headless mode - just start and go
- BLE MAC address can be configured in config files
- All settings are JSON-based, no hardcoding


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
