# VisionRC_FINAL

`VisionRC_FINAL` combines:
- Smooth vision + pure-pursuit control pipeline (from `MAC/main.mm`)
- Robust BLE scan/connect (SimpleBLE backend from root project)

This folder is a pure C++ (`.cpp`) implementation.

## 1) Prerequisites

### macOS (Homebrew)

```bash
brew install cmake opencv simpleble
```

### Linux (Ubuntu/Debian, example)

```bash
sudo apt update
sudo apt install -y build-essential cmake libopencv-dev
# SimpleBLE may need to be built from source:
# https://github.com/OpenBluetoothToolbox/SimpleBLE
```

## 2) Build

From this `FINAL/` directory:

```bash
chmod +x build.sh
./build.sh
```

Binary output:
- `./build/VisionRC_FINAL`

## 3) Quick Start

```bash
./build/VisionRC_FINAL \
  --name DRiFT-ED5C2384488D \
  --max-deg 24 \
  --steer-smooth 70 \
  --ratio 0.9 \
  --c-sign -1 \
  --speed-k 0.08
```

### A) Discover BLE devices

```bash
./build/VisionRC_FINAL --discover
```

### B) Run with BLE by name

```bash
./build/VisionRC_FINAL --name DRiFT
```

### C) Run with BLE by address

```bash
./build/VisionRC_FINAL --address ED5C2384488D
```

### D) Vision-only mode (no BLE)

```bash
./build/VisionRC_FINAL --no-ble
```

## 4) Useful Runtime Options

### Your current setup (stable baseline)

```bash
./build/VisionRC_FINAL --name DRiFT-ED5C2384488D \
  --max-deg 24 \
  --steer-smooth 70 \
  --ratio 0.9 \
  --c-sign -1 \
  --speed-k 0.08
```

### Driving style variants

#### 1) Faster (higher speed in turns)
Lower `--speed-k` so less speed is subtracted while steering.

```bash
./build/VisionRC_FINAL --name DRiFT-ED5C2384488D \
  --max-deg 24 \
  --steer-smooth 70 \
  --ratio 0.9 \
  --c-sign -1 \
  --speed-k 0.03
```

#### 2) Very fast / aggressive (test carefully)

```bash
./build/VisionRC_FINAL --name DRiFT-ED5C2384488D \
  --max-deg 26 \
  --steer-smooth 68 \
  --ratio 0.92 \
  --c-sign -1 \
  --speed-k 0.02
```

#### 3) Smooth & safe (less twitchy)

```bash
./build/VisionRC_FINAL --name DRiFT-ED5C2384488D \
  --max-deg 20 \
  --steer-smooth 78 \
  --ratio 0.85 \
  --c-sign -1 \
  --speed-k 0.10
```

#### 4) Sharp corner grip (stronger C)

```bash
./build/VisionRC_FINAL --name DRiFT-ED5C2384488D \
  --max-deg 24 \
  --steer-smooth 70 \
  --ratio 1.00 \
  --c-sign -1 \
  --speed-k 0.05
```

#### 5) Debug / vision only (no BLE)

```bash
./build/VisionRC_FINAL --no-ble \
  --max-deg 24 \
  --steer-smooth 70 \
  --ratio 0.9 \
  --c-sign -1 \
  --speed-k 0.08
```

### Speed tuning note

- To increase speed with your current setup, reduce `--speed-k` from `0.08` to `0.03` or `0.02`.
- Auto top speed is still limited by code constants (current schedule cap is `AUTO_MAX_SPEED_RAW = 8200`).

Other options:
- `--scan-timeout <s>`
- `--auto-steer-sign <v>`
- `--flip-centerline`
- `--ble-backend auto|simpleble`

Get usage anytime:

```bash
./build/VisionRC_FINAL --help
```

## 5) Keyboard Controls (OpenCV window)

- `Q`: Quit
- `A`: Toggle auto mode
- `M`: Rebuild centerline from current frame
- `R`: Start/stop CSV logging
- `V`: Show/hide foreground mask window
- `L`: Cycle lighting preprocessing mode

## 6) Logs and Data

- CSV logs are saved in current working directory as:
  - `auto_collect_YYYYMMDD_HHMMSS.csv`
- Existing path file:
  - `centerline.csv`

## 7) Troubleshooting

- If BLE does not connect:
  - Run `--discover` first and verify device name/address.
  - Try forcing backend:
    - `./build/VisionRC_FINAL --name DRiFT --ble-backend simpleble`
- If camera window does not appear:
  - Check camera permissions for Terminal/VS Code.
  - Ensure no other app is locking the camera.
- If `simpleble` is missing:
  - Build still succeeds, but real BLE mode will not work (use `--no-ble`).
