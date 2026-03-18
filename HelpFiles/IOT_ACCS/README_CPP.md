# Autonomous Centerline Controller — Pure Pursuit (C++)

## Files
- `autonomous_centerline_controller_pure_pursuit_delay_fixed.cpp`
- `CMakeLists.txt`

## Dependencies
- C++17 compiler (`g++` or `clang++`)
- CMake ≥ 3.16
- OpenCV 4

---

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

---

## Run

```bash
./build/autonomous_centerline_controller_pure_pursuit_delay_fixed_cpp \
    --name DRiFT-ED5C2384488D \
    --max-deg 24 \
    --steer-smooth 70 \
    --ratio 0.9 \
    --c-sign -1 \
    --speed-k 0.08
```

---

## CLI Parameters

| Flag | Type | Description |
|---|---|---|
| `--name <device-name>` | string | BLE device name to connect to |
| `--address <id>` | string | BLE device address / UUID |
| `--discover` | flag | Scan and list all nearby BLE devices |
| `--scan-timeout <sec>` | float | BLE scan duration in seconds (default: 5) |
| `--max-deg <deg>` | float | Maximum steering angle in degrees |
| `--steer-smooth <val>` | float | Steering smoothing factor (0–100) |
| `--ratio <val>` | float | Pure-pursuit lookahead ratio (0–1) |
| `--c-sign <val>` | float | Centerline sign correction (+1 or -1) |
| `--speed-k <val>` | float | Speed proportional gain |
| `--auto-steer-sign <val>` | float | Autonomous steering sign override |
| `--flip-centerline` | flag | Flip the loaded centerline direction |
| `--no-ble` | flag | Run vision/control loop without BLE (camera only) |

---

## Examples

**BLE connect by device name:**
```bash
./build/autonomous_centerline_controller_pure_pursuit_delay_fixed_cpp \
    --name DRiFT-ED5C2384488D \
    --max-deg 24 --steer-smooth 70 \
    --ratio 0.9 --c-sign -1 --speed-k 0.08
```

**BLE connect by address:**
```bash
./build/autonomous_centerline_controller_pure_pursuit_delay_fixed_cpp \
    --address C276DF76-6504-041B-4F60-A98479B6B867 \
    --max-deg 24 --steer-smooth 70 \
    --ratio 0.9 --c-sign -1 --speed-k 0.08
```

**Discover nearby BLE devices:**
```bash
./build/autonomous_centerline_controller_pure_pursuit_delay_fixed_cpp \
    --discover --scan-timeout 6
```

**No-BLE (vision only, no car):**
```bash
./build/autonomous_centerline_controller_pure_pursuit_delay_fixed_cpp --no-ble
```

**No-BLE with tuning parameters:**
```bash
./build/autonomous_centerline_controller_pure_pursuit_delay_fixed_cpp --no-ble \
    --max-deg 24 --steer-smooth 70 \
    --ratio 0.9 --c-sign -1 --speed-k 0.08
```

---

## Raspberry Pi 4 — Build & Run

Install dependencies:
```bash
sudo apt-get update -y
sudo apt-get install -y cmake build-essential libopencv-dev
```

Build:
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

Run:
```bash
./build/autonomous_centerline_controller_pure_pursuit_delay_fixed_cpp \
    --name DRiFT-ED5C2384488D \
    --max-deg 24 --steer-smooth 70 \
    --ratio 0.9 --c-sign -1 --speed-k 0.08
```

Or use the provided build script:
```bash
chmod +x build.sh
./build.sh
```
