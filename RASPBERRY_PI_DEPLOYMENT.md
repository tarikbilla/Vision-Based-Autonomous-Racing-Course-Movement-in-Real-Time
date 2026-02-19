# 🚀 Raspberry Pi Deployment Guide

## Current Status

**Python Version:** ✅ Ready for deployment
**C++ Version:** ⚠️ Needs GUI fix for Mac testing, but should work on Pi

## Quick Deploy to Raspberry Pi

### 1. Transfer Project

```bash
# From your Mac, transfer only what you need
rsync -avz --exclude '.venv' --exclude '__pycache__' --exclude 'build' \
  IoT-Project-Vision-based-autonomous-RC-car-control-system/ \
  pi@raspberrypi.local:~/rc-car-project/
```

### 2. Setup on Raspberry Pi

```bash
# SSH into Pi
ssh pi@raspberrypi.local

cd ~/rc-car-project

# For Python (Recommended - Works Now)
cd Python_project
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt

# Test
python run_autonomy.py --simulate --duration 5
```

### 3. Setup Hardware

```bash
# Check camera
ls /dev/video*  # Should show /dev/video0 or similar

# Find BLE device
python run_autonomy.py --list-devices
```

### 4. Run

```bash
# With real hardware
python run_autonomy.py --device <YOUR_CAR_MAC_ADDRESS>
```

## C++ Version on Pi

The C++ version should work on Raspberry Pi (the GUI threading issue is macOS-specific).

```bash
# Install dependencies
sudo apt-get update
sudo apt-get install -y cmake build-essential libopencv-dev nlohmann-json3-dev

# Build
cd ~/rc-car-project/CPP_Complete
./build.sh

# Run
./build/VisionBasedRCCarControl --device <MAC_ADDRESS>
```

## Configuration for Pi

Edit `Python_project/config/default.json`:

```json
{
  "camera": {
    "source": 0,           // Usually 0 for first camera
    "width": 640,          // Lower resolution for better performance
    "height": 480,
    "fps": 20              // Lower FPS if needed
  },
  "ble": {
    "device_mac": "XX:XX:XX:XX:XX:XX",  // Your car's MAC
    ...
  },
  "ui": {
    "show_window": false   // Disable if running headless
  }
}
```

## Performance Tips

1. **Disable GUI** - Set `show_window: false` for headless operation
2. **Lower resolution** - Use 640x480 instead of 1920x1080
3. **Reduce FPS** - 15-20 FPS is enough for control
4. **Check CPU** - Run `htop` to monitor CPU usage

## Troubleshooting

### Camera Issues
```bash
# Test camera
raspistill -o test.jpg    # For Pi Camera
v4l2-ctl --list-devices    # For USB camera
```

### BLE Issues
```bash
# Check Bluetooth
sudo systemctl status bluetooth
hciconfig  # Should show hci0

# Scan for devices
bluetoothctl
> scan on
```

### Performance Issues
- Lower camera resolution
- Reduce FPS
- Disable UI completely
- Use C++ version for better performance

## Auto-Start on Boot (Optional)

Create systemd service:

```bash
sudo nano /etc/systemd/system/rc-car.service
```

Add:
```ini
[Unit]
Description=RC Car Autonomy
After=network.target

[Service]
Type=simple
User=pi
WorkingDirectory=/home/pi/rc-car-project/Python_project
ExecStart=/home/pi/rc-car-project/Python_project/.venv/bin/python run_autonomy.py --device XX:XX:XX:XX:XX:XX
Restart=on-failure

[Install]
WantedBy=multi-user.target
```

Enable:
```bash
sudo systemctl enable rc-car
sudo systemctl start rc-car
sudo systemctl status rc-car
```

## Summary

**For immediate use:** Deploy Python version - it's tested and working
**For performance:** Try C++ version on Pi after Python works
**For headless:** Set `show_window: false` in config

The Python version is production-ready and will work immediately on your Raspberry Pi!
