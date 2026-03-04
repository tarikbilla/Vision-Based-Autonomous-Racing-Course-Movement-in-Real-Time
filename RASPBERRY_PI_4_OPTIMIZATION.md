# Raspberry Pi 4 Optimization Guide

## Performance Optimizations

### 1. Camera Resolution
- **PC**: 1280x720 @ 30fps
- **RPi4**: 320x240 @ 15fps
- Reduces bandwidth by ~95%
- Lower resolution maintains performance

### 2. Processing Rate
- **Tracking Loop**: 10 Hz (was 15 Hz)
- **BLE Commands**: 30 Hz (was 50 Hz)
- **UI Rendering**: Disabled by default
- Saves ~40% CPU usage

### 3. Tracker Selection
- **PC**: CSRT (high accuracy, slower)
- **RPi4**: KCF (fast, good enough)
- KCF is 3-4x faster on ARM

### 4. Centerline Building
- **PC**: Every 2 seconds (30 frames @ 15Hz)
- **RPi4**: Every 2 seconds (20 frames @ 10Hz)
- Less frequent updates save CPU

### 5. Ray Casting
- **PC**: 200 pixel max length
- **RPi4**: 100 pixel max length
- Faster boundary detection

## Quick Start on Raspberry Pi 4

### Installation
```bash
# Run setup script
chmod +x setup_rpi4.sh
./setup_rpi4.sh

# Reboot for GPU memory changes
sudo reboot
```

### Running
```bash
# Low-resource mode (automatic config)
./build/VisionBasedRCCarControl --rpi

# Or manually specify config
./build/VisionBasedRCCarControl --config config/config_rpi4.json
```

## Configuration Differences

### config_rpi4.json
```json
{
  "camera": {
    "width": 320,      // 4x smaller than PC
    "height": 240,     // 4x smaller than PC
    "fps": 15          // Half of PC FPS
  },
  "tracker": {
    "tracker_type": "KCF"  // Fast tracker
  },
  "ui": {
    "show_window": false,  // No GUI overhead
    "command_rate_hz": 30  // Lower BLE rate
  },
  "boundary": {
    "ray_max_length": 100,   // Shorter rays
    "evasive_distance": 60   // Adjusted for resolution
  }
}
```

## System Optimizations

### 1. CPU Governor
```bash
# Set to performance mode
echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor
```

### 2. GPU Memory
```bash
# Edit /boot/config.txt
sudo nano /boot/config.txt

# Add this line:
gpu_mem=256
```

### 3. Disable Desktop
```bash
# Boot to console (saves RAM)
sudo systemctl set-default multi-user.target
sudo reboot
```

### 4. Overclock (Optional)
```bash
# Edit /boot/config.txt
sudo nano /boot/config.txt

# Add these lines:
over_voltage=6
arm_freq=2000
gpu_freq=750
```

## Troubleshooting

### Camera Not Opening
```bash
# Check camera devices
ls -l /dev/video*

# Test camera
v4l2-ctl --list-devices
v4l2-ctl -d /dev/video0 --list-formats-ext
```

### Low FPS
```bash
# Check CPU usage
htop

# Check temperature
vcgencmd measure_temp

# Add cooling if > 70°C
```

### Memory Issues
```bash
# Check memory
free -h

# Increase swap
sudo dphys-swapfile swapoff
sudo nano /etc/dphys-swapfile
# Set CONF_SWAPSIZE=2048
sudo dphys-swapfile setup
sudo dphys-swapfile swapon
```

## Performance Benchmarks

### Expected Frame Rates
- Camera capture: 15 FPS
- Tracking loop: 10 Hz
- BLE commands: 30 Hz

### CPU Usage
- Idle: ~5%
- Camera + Tracking: ~60-70%
- Full system: ~75-85%

### Temperature
- Idle: 40-50°C
- Running: 60-70°C
- Throttling starts at: 80°C

## Hardware Requirements

### Minimum
- Raspberry Pi 4 (2GB RAM)
- USB Camera (720p capable)
- MicroSD Card (16GB, Class 10)
- Power Supply (5V 3A)

### Recommended
- Raspberry Pi 4 (4GB RAM)
- USB Camera with good low-light
- MicroSD Card (32GB, UHS-I)
- Power Supply (5V 3A official)
- Active cooling (fan or heatsink)

## Remote Access

### SSH Setup
```bash
# Enable SSH
sudo systemctl enable ssh
sudo systemctl start ssh

# Connect from PC
ssh pi@raspberrypi.local
```

### Run Headless
```bash
# Run without display
./build/VisionBasedRCCarControl --rpi

# Or with nohup for background
nohup ./build/VisionBasedRCCarControl --rpi > output.log 2>&1 &
```

### VNC Setup (Optional)
```bash
# Install VNC
sudo apt install realvnc-vnc-server

# Enable VNC
sudo raspi-config
# Interface Options > VNC > Enable
```

## Advanced Tuning

### Further Resolution Reduction
Edit `config/config_rpi4.json`:
```json
"camera": {
  "width": 240,
  "height": 180,
  "fps": 10
}
```

### Disable Motion Detection
If color detection works well:
```cpp
// In control_orchestrator.cpp
useMotionDetection_ = false;  // Skip motion, use color only
```

### Reduce Smoothing History
Edit `include/boundary_detection.hpp`:
```cpp
int steerHistorySize_ = 3;  // Was 5, reduce for faster response
```

## Comparison Table

| Feature | PC | Raspberry Pi 4 |
|---------|----|----|
| Resolution | 1280x720 | 320x240 |
| FPS | 30 | 15 |
| Tracking Rate | 15 Hz | 10 Hz |
| Tracker | CSRT | KCF |
| BLE Rate | 50 Hz | 30 Hz |
| Ray Length | 200px | 100px |
| UI Display | Enabled | Disabled |
| CPU Usage | ~20% | ~75% |

## Future Optimizations

1. Use OpenCV with NEON optimizations
2. Use hardware video decode (V4L2 M2M)
3. Implement multi-threading optimizations
4. Use TensorFlow Lite for detection
5. Reduce image processing to ROI only
