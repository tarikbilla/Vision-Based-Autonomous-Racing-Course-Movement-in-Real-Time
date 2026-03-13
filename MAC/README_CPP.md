# Autonomous Centerline Controller — Pure Pursuit (C++)

> **Target platform: macOS (Apple Silicon / Intel)**
> Native BLE via **CoreBluetooth** — no external tools or scripts needed.

---

## Files

| File | Purpose |
|---|---|
| `autonomous_centerline_controller_pure_pursuit_delay_fixed.mm` | Objective-C++ source (CoreBluetooth) |
| `CMakeLists.txt` | CMake build config (links CoreBluetooth + Foundation) |
| `build.sh` | One-shot build script |
| `centerline.csv` | Centerline waypoints (required at runtime) |

---

## Dependencies

| Package | Install |
|---|---|
| Xcode Command Line Tools | `xcode-select --install` |
| CMake | `brew install cmake` |
| OpenCV 4 | `brew install opencv` |
| CoreBluetooth | Built-in macOS framework — no install needed |

---

## Quick Start

### 1. Install dependencies + build

```bash
chmod +x build.sh
./build.sh
```

Or manually:

```bash
brew install cmake opencv

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(sysctl -n hw.logicalcpu)
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
| `--address <UUID>` | string | CoreBluetooth peripheral UUID (e.g. `C276DF76-6504-041B-4F60-A98479B6B867`) |
| `--discover` | flag | Scan and print all nearby BLE devices, then exit |
| `--scan-timeout <sec>` | float | BLE scan duration in seconds (default: 5) |
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

**Connect by CoreBluetooth UUID:**
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

- BLE uses **CoreBluetooth** — the same framework macOS uses internally for Bluetooth.
- `--name` triggers a BLE scan, matches the device by name, then connects.
- `--address` expects a **CoreBluetooth UUID** (not a MAC address — macOS does not expose raw MAC addresses). Use `--discover` first to find the UUID.
- On first run macOS may ask for Bluetooth permission in a system dialog — click **Allow**.
- The app must be granted Bluetooth access: **System Settings → Privacy & Security → Bluetooth**.
- GATT writes use `CBCharacteristicWriteWithoutResponse` for low-latency control packets.
