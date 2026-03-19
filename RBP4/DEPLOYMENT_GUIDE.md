# Deployment Guide: Low-Regulation Controller on Raspberry Pi 4

## Prerequisites

**On your development machine** (already completed):
- ✅ Both versions compiled (`build/autonomous_centerline_controller_low_regulation_cpp`, `build/autonomous_centerline_controller_pure_pursuit_delay_fixed_cpp`)
- ✅ CMakeLists.txt updated with both targets
- ✅ No compilation errors

**On Raspberry Pi 4**:
- Raspberry Pi OS (Bullseye or newer)
- OpenCV 4.x installed
- SimpleBLE or Python BLE available (build.sh handles this)
- USB camera connected
- DRIFT RC car connected and paired via Bluetooth

---

## Step 1: Prepare Pi for First Build

SSH into your Pi:
```bash
ssh pi@raspberrypi.local
```

Clone or update the project:
```bash
cd /home/pi
git clone https://github.com/tarikbilla/IoT-Project-Vision-based-autonomous-RC-car-control-system.git
# OR git pull if already cloned

cd IoT-Project-Vision-based-autonomous-RC-car-control-system/RBP4
```

---

## Step 2: Build on Pi

Run the automated build script (handles SimpleBLE auto-install):
```bash
./build.sh
```

**This will:**
- Install dependencies (`cmake`, `libopencv-dev`, `libdbus-1-dev`, etc.)
- Auto-build SimpleBLE if missing
- Compile both controller versions
- Output: `build/autonomous_centerline_controller_low_regulation_cpp`

**Time**: ~15-20 minutes first run (SimpleBLE build), ~2-3 min subsequent

---

## Step 3: Verify Build Success

Check if both executables exist:
```bash
ls -lh build/autonomous_*
```

Expected output:
```
-rwxr-xr-x 1 pi pi  143K [DATE] autonomous_centerline_controller_low_regulation_cpp
-rwxr-xr-x 1 pi pi  245K [DATE] autonomous_centerline_controller_pure_pursuit_delay_fixed_cpp
```

---

## Step 4: Test Camera

Before running the controller, verify camera works:
```bash
# List connected cameras
v4l2-ctl --list-devices

# Test with OpenCV
python3 << 'EOF'
import cv2
cap = cv2.VideoCapture(0)
if cap.isOpened():
    ret, frame = cap.read()
    print(f"Camera OK: {frame.shape}")
    cap.release()
else:
    print("Camera FAILED!")
EOF
```

---

## Step 5: Verify BLE Connection to Car

```bash
# List paired Bluetooth devices
bluetoothctl devices

# Check car is connected
bluetoothctl info DRiFT-ED5C2384488D

# If not connected:
bluetoothctl pair DRiFT-ED5C2384488D
bluetoothctl connect DRiFT-ED5C2384488D
```

---

## Step 6: First Run - Ultra-Low Mode (Recommended)

In the RBP4 directory:
```bash
cd ~/IoT-Project-Vision-based-autonomous-RC-car-control-system/RBP4

# Set ultra-low environment variables
export CAM_FRAME_W=480
export CAM_FRAME_H=270
export CAM_FPS=10

# Run the low-regulation controller
./build/autonomous_centerline_controller_low_regulation_cpp \
  --name "DRiFT-ED5C2384488D" \
  --max-deg 24 \
  --steer-smooth 70 \
  --ratio 0.9 \
  --c-sign -1 \
  --speed-k 0.08 \
  --speed 80
```

**Expected output:**
```
=== LOW-REGULATION AUTONOMOUS CONTROLLER ===
Ultra-low CPU mode: 480p, 10 FPS, minimal processing

Camera: 480x270 @ 10 FPS
Control: max_deg=24, steer_smooth=70, ratio=0.9, c_sign=-1, speed_k=0.08

[OK] Camera started
[OK] Control thread started
[OK] BLE thread started

Keyboard controls:
  'w' / 's': speed up / down
  'a' / 'd': steer left / right
  'b': brake
  'q': quit
```

**In another SSH terminal**, monitor CPU usage:
```bash
watch -n 0.5 'top -b -n 1 | head -15'
```

**Expected CPU usage**: 15-20% total

---

## Step 7: Test Keyboard Control

While running, try:
```
w  → Speed up (should see "BLE Send" messages)
s  → Speed down
a  → Steer left
d  → Steer right
b  → Brake
q  → Quit
```

---

## Step 8: Monitor CPU During Execution

In a separate terminal:
```bash
# Real-time CPU monitoring
while true; do
  clear
  ps aux | grep autonomous_centerline_controller_low_regulation_cpp
  sleep 1
done

# Or use top
top -p $(pgrep -f autonomous_centerline_controller_low_regulation_cpp)
```

**Target**: CPU% < 30% (should see 15-20%)

If CPU is >30%, reduce resolution:
```bash
export CAM_FRAME_W=320 CAM_FRAME_H=180 CAM_FPS=8
./build/autonomous_centerline_controller_low_regulation_cpp ...
```

---

## Step 9: Adjusting Resolution/Performance

### Ultra-Low (Minimum CPU)
```bash
export CAM_FRAME_W=320 CAM_FRAME_H=180 CAM_FPS=8
```
CPU: ~8-10%, Steering: Very delayed

### Default Low (Balanced)
```bash
export CAM_FRAME_W=480 CAM_FRAME_H=270 CAM_FPS=10
```
CPU: ~15-20%, Steering: Acceptable delay

### Balanced (Higher Quality)
```bash
export CAM_FRAME_W=640 CAM_FRAME_H=360 CAM_FPS=12
```
CPU: ~22-28%, Steering: Good quality

### Higher Quality (Use if CPU available)
```bash
export CAM_FRAME_W=800 CAM_FRAME_H=450 CAM_FPS=15
```
CPU: ~32-40%, Steering: Smooth

---

## Step 10: Switching to Full Version

If CPU allows and you need better steering:
```bash
export CAM_FRAME_W=960 CAM_FRAME_H=540 CAM_FPS=15

./build/autonomous_centerline_controller_pure_pursuit_delay_fixed_cpp \
  --name "DRiFT-ED5C2384488D" \
  --max-deg 24 \
  --steer-smooth 70 \
  --ratio 0.9 \
  --c-sign -1 \
  --speed-k 0.08 \
  --speed 100
```

**Expected CPU**: 45-60% (moderate load)

---

## Troubleshooting

### Problem: "Camera failed to open"

**Solution:**
```bash
# Check camera exists
v4l2-ctl --list-devices

# Check permissions
ls -l /dev/video*

# Try reopening
./build/autonomous_centerline_controller_low_regulation_cpp ...
```

### Problem: "BLE connection failed"

**Solution:**
```bash
# Verify car is paired
bluetoothctl devices

# Reconnect manually
bluetoothctl connect DRiFT-ED5C2384488D

# Check BLE backend
export FORCE_BLUEZ_CLI=1  # Force bluetoothctl (slower)
./build/autonomous_centerline_controller_low_regulation_cpp ...
```

### Problem: "CPU is still >50%"

**Solution:** Go even lower
```bash
export CAM_FRAME_W=320 CAM_FRAME_H=180 CAM_FPS=8
./build/autonomous_centerline_controller_low_regulation_cpp ...
```

### Problem: "Steering is unresponsive/jerky"

**Solution:** Increase smoothing and frame size
```bash
export CAM_FRAME_W=640 CAM_FRAME_H=360 CAM_FPS=12

./build/autonomous_centerline_controller_low_regulation_cpp \
  --steer-smooth 85 \  # Increased from 70
  ...
```

### Problem: "Car moves but doesn't steer"

**Solution:** Check BLE connection and verify steering parameters
```bash
# Check logs for BLE Send messages
./build/autonomous_centerline_controller_low_regulation_cpp ... | grep "BLE Send"

# Try different c_sign
./build/autonomous_centerline_controller_low_regulation_cpp \
  --c-sign 1 \  # Try opposite sign
  ...
```

---

## Persistent Deployment (systemd Service)

To run as a Pi service at boot:

**Create `/home/pi/start_autonomous.sh`:**
```bash
#!/bin/bash
cd /home/pi/IoT-Project-Vision-based-autonomous-RC-car-control-system/RBP4

export CAM_FRAME_W=480
export CAM_FRAME_H=270
export CAM_FPS=10

./build/autonomous_centerline_controller_low_regulation_cpp \
  --name "DRiFT-ED5C2384488D" \
  --max-deg 24 \
  --steer-smooth 70 \
  --ratio 0.9 \
  --c-sign -1 \
  --speed-k 0.08 \
  --speed 100 \
  >> /tmp/autonomous.log 2>&1
```

**Make executable:**
```bash
chmod +x /home/pi/start_autonomous.sh
```

**Create systemd service `/etc/systemd/system/autonomous.service`:**
```ini
[Unit]
Description=Autonomous DRIFT Controller (Low Regulation)
After=network.target

[Service]
Type=simple
User=pi
WorkingDirectory=/home/pi/IoT-Project-Vision-based-autonomous-RC-car-control-system/RBP4
ExecStart=/home/pi/start_autonomous.sh
Restart=on-failure
RestartSec=10

[Install]
WantedBy=multi-user.target
```

**Enable and start:**
```bash
sudo systemctl daemon-reload
sudo systemctl enable autonomous.service
sudo systemctl start autonomous.service
sudo systemctl status autonomous.service
```

**Monitor logs:**
```bash
tail -f /tmp/autonomous.log
sudo journalctl -u autonomous.service -f
```

---

## Performance Baseline

After deployment, record baseline metrics:

```bash
# 1. CPU usage (should be ~15-20%)
top -b -n 1 | grep autonomous_centerline

# 2. Memory usage
ps aux | grep autonomous_centerline

# 3. BLE packets (should see periodic "BLE Send")
./build/autonomous_centerline_controller_low_regulation_cpp ... 2>&1 | head -100

# 4. Check car responds to steering
# Manually observe car turning with 'a'/'d' keys

# 5. Measure frame latency
time ./build/autonomous_centerline_controller_low_regulation_cpp ... &
sleep 5
pkill -f autonomous_centerline_controller_low_regulation_cpp
```

---

## Performance Tuning Checklist

- [ ] Camera works: `v4l2-ctl --list-devices`
- [ ] Car paired: `bluetoothctl devices`
- [ ] Ultra-low mode runs: CPU < 20%
- [ ] Steering responds: 'a'/'d' moves car
- [ ] Speed responds: 'w'/'s' changes speed
- [ ] No crashes on 5+ minute run
- [ ] Adjust resolution to balance CPU vs quality
- [ ] Document final settings for reproducibility

---

## Final Checklist

✅ Repository cloned/updated on Pi  
✅ build.sh completed successfully  
✅ Both executables compiled  
✅ Camera verified working  
✅ BLE car connected  
✅ Low-regulation version runs  
✅ CPU < 30% during operation  
✅ Steering responds to keyboard  
✅ Speed responds to keyboard  
✅ Settings documented for future runs  

---

## Next Steps

1. **Baseline Run**: Execute Step 6 and record CPU/performance
2. **Tuning**: Adjust CAM_FRAME_W/H/FPS based on CPU available
3. **Production**: Set up systemd service if autonomous mode desired
4. **Advanced**: Integrate with existing Pi services (monitoring, logging, etc.)

---

## Support

If issues persist, check:
1. **Build errors**: Re-run `./build.sh` with logs
2. **Camera issues**: Run `v4l2-ctl` diagnostics
3. **BLE issues**: Check `bluetoothctl` device connection
4. **CPU issues**: Reduce resolution/FPS further
5. **Steering issues**: Verify `--c-sign` parameter matches car direction

For questions, refer to:
- `RBP4/LOW_REGULATION_README.md` - Full documentation
- `RBP4/COMPARISON.md` - Full vs low-regulation comparison
- `RBP4/LOW_REGULATION_QUICK_START.md` - Quick reference
