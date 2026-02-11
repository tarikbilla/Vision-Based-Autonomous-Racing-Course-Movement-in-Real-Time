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
python -m rc_autonomy run --simulate --duration 10
```

Run with the car and camera:

```bash
python -m rc_autonomy run --device f9:af:3c:e2:d2:f5
```

Scan/connect to verify BLE:

```bash
python -m rc_autonomy scan --device f9:af:3c:e2:d2:f5
```

## Configuration

Edit `config/default.json` to tune camera, tracking, guidance, and BLE settings.

## Notes

- The C++ implementation and related docs are in `CPP_Project/`.
- `test_ble_python.py` remains available as a standalone BLE reference script.
