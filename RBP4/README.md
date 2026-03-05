# RBP4 (Isolated Raspberry Pi 4 Version)

This folder is a fully separate low-quality build for Raspberry Pi 4.
It does not modify or depend on your root project build/output.

## Includes
- `RBP4/src/` separate source copy
- `RBP4/include/` separate header copy
- `RBP4/config/config_rbp4_ultra_low.json` ultra-low runtime config
- `RBP4/CMakeLists.txt` separate CMake project
- `RBP4/build_rbp4.sh` separate build script
- `RBP4/run_rbp4.sh` separate run script

## Ultra-Low Settings
- Resolution: `320x240`
- FPS: `8`
- Tracker: `KCF`
- UI Window: `on` (fixed small `320x240` preview)
- BLE command rate: `12Hz`
- Speed: `10`
- Forced downscale: every frame is resized to `320x240` even if camera opens at higher resolution

## Build
```bash
cd /path/to/IoT-Project-Vision-based-autonomous-RC-car-control-system
chmod +x RBP4/build_rbp4.sh RBP4/run_rbp4.sh
./RBP4/build_rbp4.sh
```

## Run (real car)
```bash
./RBP4/run_rbp4.sh
```

## Raspberry Pi 4 recommended run sequence
```bash
cd /path/to/IoT-Project-Vision-based-autonomous-RC-car-control-system
./RBP4/build_rbp4.sh
./RBP4/build/VisionBasedRCCarControl_RBP4 --config ./RBP4/config/config_rbp4_ultra_low.json
```

## Run (simulation)
```bash
./RBP4/run_rbp4.sh --simulate
```

## Run direct binary (important)
```bash
./RBP4/build/VisionBasedRCCarControl_RBP4 --config ./RBP4/config/config_rbp4_ultra_low.json
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
