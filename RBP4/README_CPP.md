# Autonomous Centerline Controller — Pure Pursuit (C++)

> **Target platform: Raspberry Pi 4 (Linux/BlueZ)**
> C++ BLE backends: optional **SimpleBLE** (preferred) with automatic fallback to `bluetoothctl` (BlueZ).

---

## Files

| File | Purpose |
|---|---|
| `main.cpp` | C++ source |
| `CMakeLists.txt` | CMake build config |
| `build.sh` | One-shot install + build script |
| `centerline.csv` | Centerline waypoints (loaded if present; can be rebuilt at runtime with `M`) |

---

## Dependencies

| Package | Install |
|---|---|
| CMake ≥ 3.16 | `apt install cmake` |
| C++17 compiler | `apt install build-essential` |
| OpenCV 4 | `apt install libopencv-dev` |
| BlueZ (BLE) | `apt install bluez` |

---

## Quick Start (Raspberry Pi 4)

### 1. Install dependencies + build

```bash
chmod +x build.sh
./build.sh
```

Or manually:

```bash
sudo apt-get update -y
sudo apt-get install -y cmake build-essential libopencv-dev bluez

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

### 2. Run

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
| `--name <device-name>` | string | BLE device name to scan and connect to |
| `--address <MAC>` | string | BLE device MAC address (e.g. `AA:BB:CC:DD:EE:FF`) |
| `--discover` | flag | Scan and print all nearby BLE devices, then exit |
| `--scan-timeout <sec>` | float | BLE scan duration in seconds (default: 6) |
| `--max-deg <deg>` | float | Maximum steering angle in degrees |
| `--steer-smooth <val>` | float | Steering smoothing factor (0–100) |
| `--ratio <val>` | float | Pure-pursuit lookahead ratio (0–1) |
| `--c-sign <val>` | float | Centerline sign correction (`+1` or `-1`) |
| `--speed-k <val>` | float | Speed proportional gain |
| `--auto-steer-sign <val>` | float | Autonomous steering sign override |
| `--flip-centerline` | flag | Flip the loaded centerline direction |
| `--no-ble` | flag | Run vision/control loop without BLE (camera only) |

---

## Examples

**Connect by device name (scans then connects):**
```bash
./build/autonomous_centerline_controller_pure_pursuit_delay_fixed_cpp \
    --name DRiFT-ED5C2384488D \
    --max-deg 24 --steer-smooth 70 \
    --ratio 0.9 --c-sign -1 --speed-k 0.08
```

**Connect by MAC address directly:**
```bash
./build/autonomous_centerline_controller_pure_pursuit_delay_fixed_cpp \
    --address ED:5C:23:84:48:8D \
    --max-deg 24 --steer-smooth 70 \
    --ratio 0.9 --c-sign -1 --speed-k 0.08
```

**Discover nearby BLE devices:**
```bash
./build/autonomous_centerline_controller_pure_pursuit_delay_fixed_cpp \
    --discover --scan-timeout 6
```

**No-BLE — vision only (no car connected):**
```bash
./build/autonomous_centerline_controller_pure_pursuit_delay_fixed_cpp --no-ble
```

**No-BLE with full tuning parameters:**
```bash
./build/autonomous_centerline_controller_pure_pursuit_delay_fixed_cpp --no-ble \
    --max-deg 24 --steer-smooth 70 \
    --ratio 0.9 --c-sign -1 --speed-k 0.08
```

---

## BLE Notes

- BLE is **C++ only**. Runtime tries **SimpleBLE** first (if compiled), then falls back to **BlueZ** (`bluetoothctl`).
- `--name` triggers a BLE scan for `<scan-timeout>` seconds, resolves the device name to a MAC, then connects.
- `--address` connects directly without scanning.
- Make sure the Bluetooth adapter is powered on: `bluetoothctl power on`
- `gatttool` is not required (deprecated).
- If permissions fail, run with `sudo`.
- For BlueZ write mode issues, set `BLUEZ_WRITE_TYPE`:
    - `BLUEZ_WRITE_TYPE=command` (default in current code)
    - `BLUEZ_WRITE_TYPE=request`
    - `BLUEZ_WRITE_TYPE=default`
- To force BlueZ backend even when SimpleBLE is available: `FORCE_BLUEZ_CLI=1`.

---

## CPU / Camera Performance Tuning (Raspberry Pi)

- Default Linux camera mode is low-definition for lower CPU: **`640x360 @ 20 FPS`**.
- You can override at runtime with environment variables:
    - `CAM_FRAME_W` (range `160..1920`)
    - `CAM_FRAME_H` (range `120..1080`)
    - `CAM_FPS` (range `5..60`)

Example (very low CPU mode):

```bash
CAM_FRAME_W=424 CAM_FRAME_H=240 CAM_FPS=12 \
./build/autonomous_centerline_controller_pure_pursuit_delay_fixed_cpp \
        --name DRiFT-ED5C2384488D \
        --max-deg 24 --steer-smooth 70 \
        --ratio 0.9 --c-sign -1 --speed-k 0.08
```

Balanced mode:

```bash
CAM_FRAME_W=640 CAM_FRAME_H=360 CAM_FPS=15 \
./build/autonomous_centerline_controller_pure_pursuit_delay_fixed_cpp \
        --name DRiFT-ED5C2384488D \
        --max-deg 24 --steer-smooth 70 \
        --ratio 0.9 --c-sign -1 --speed-k 0.08
```

---

## Raspberry Pi BLE Troubleshooting

If macOS works but Raspberry Pi does not, validate Linux BLE first:

```bash
bluetoothctl power on
bluetoothctl scan on
# wait until your car appears, then Ctrl+C
bluetoothctl connect ED:5C:23:84:48:8D
bluetoothctl info ED:5C:23:84:48:8D
```

Expect `ServicesResolved: yes` before control writes.

If you still get write errors:

```bash
sudo systemctl restart bluetooth
sudo rfkill unblock bluetooth
```

Then rerun the app with a longer scan timeout:

```bash
./build/autonomous_centerline_controller_pure_pursuit_delay_fixed_cpp \
    --name DRiFT-ED5C2384488D \
    --scan-timeout 10
```

When running over SSH without desktop, GUI windows are automatically disabled (headless mode).
