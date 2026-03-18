# Vision-Based Autonomous RC Car Control (Python)

This Python application implements the end-to-end autonomy pipeline defined in the PRD: connect to the DR!FT RC car over BLE, then start the camera → tracking → guidance → control loop.

## Quick Start

```bash
python -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
```

Run a simulation (no hardware required):

```bash
python run_autonomy.py --simulate --duration 10
```

Run with the car and camera:

```bash
python run_autonomy.py --device f9:af:3c:e2:d2:f5
```

If your system shows UUID-style BLE addresses (macOS often does), list devices and pick an index:

```bash
python run_autonomy.py --list-devices
python run_autonomy.py --device-index 0
```

Scan/connect to verify BLE:

```bash
python -m rc_autonomy scan --device f9:af:3c:e2:d2:f5

You can also run the module directly if preferred:

```bash
python -m rc_autonomy run --simulate --duration 10
```
```

## Configuration

Edit `config/default.json` to tune camera, tracking, guidance, and BLE settings.

## Notes

- The C++ implementation and related docs are in `CPP_Project/`.
- `test_ble_python.py` remains available as a standalone BLE reference script.
