"""
autonomous_centerline_controller.py
===================================
One-file autonomous controller for the BLE RC car.

What this file does:
- connects to the car over BLE (or runs with --no-ble)
- runs the same vision tracker / centerline logic as your current collector
- computes autonomous steering + speed from the live tracked pose
- sends BLE packets directly to the car
- logs both drive commands and tracker signals to CSV

Run with BLE:
    python autonomous_centerline_controller.py --address ED:5C:23:84:48:8D

Run without BLE:
    python autonomous_centerline_controller.py --no-ble

CV window keys:
    M = build centerline from current frame
    R = start/stop logging
    A = toggle autonomous mode on/off
    V = toggle mask view
    L = toggle lighting mode
    D = debug overlay
    Q = quit
"""

import argparse
import asyncio
import csv
import math
import os
import queue
import sys
import threading
import time
from pathlib import Path
from typing import Optional
import re

try:
    from bleak import BleakClient, BleakScanner
    from bleak.exc import BleakDeviceNotFoundError
except ImportError:
    BleakClient = None
    BleakScanner = None
    BleakDeviceNotFoundError = None

try:
    import cv2
    import numpy as np
    from scipy.spatial import cKDTree
    HAS_CV = True
except ImportError as _e:
    HAS_CV = False
    print(f"[WARN] Vision disabled: {_e}  (pip install opencv-python numpy scipy)")
    sys.exit(1)

# =============================================================================
# BLE packet layer
# =============================================================================

WRITE_UUID = "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
HEADER = bytes.fromhex("bf0a00082800")
TAIL = 0x4E
BRAKE_PACKET = bytes.fromhex("bf0a000828000000000000000100ba")
IDLE_PACKET = bytes.fromhex("bf0a0008280000000000000000004e")

B_PER_DEG = 16000.0 / 90.0
CONTROL_HZ = 20.0


def s16le(v: float) -> bytes:
    iv = max(-32768, min(32767, int(round(v))))
    return iv.to_bytes(2, byteorder="little", signed=True)


def build_packet(a: float, b: float, c: float) -> bytes:
    return HEADER + s16le(a) + s16le(b) + s16le(c) + s16le(0) + bytes([TAIL])


class DummyBleClient:
    def __init__(self):
        self.is_connected = True

    async def write_gatt_char(self, uuid, data, response=False):
        return None


def _looks_like_mac(s: str) -> bool:
    return bool(re.fullmatch(r"([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}", s or ""))


def _looks_like_uuid(s: str) -> bool:
    return bool(re.fullmatch(r"[0-9A-Fa-f]{8}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{12}", s or ""))


def _normalize_mac(s: str) -> str:
    return re.sub(r"[^0-9A-Fa-f]", "", s or "").lower()


async def _discover_ble_devices(scan_timeout: float):
    if BleakScanner is None:
        raise RuntimeError("bleak is not installed. Install it (pip install bleak) or run with --no-ble")
    print(f"[BLE] Scanning for {scan_timeout:.1f}s ...")
    devices = await BleakScanner.discover(timeout=scan_timeout)
    return sorted(devices, key=lambda d: ((d.name or "~").lower(), str(d.address).lower()))


def _print_ble_devices(devices):
    if not devices:
        print("[BLE] No BLE devices found")
        return
    print("[BLE] Found devices:")
    for d in devices:
        name = d.name or "<no-name>"
        print(f"  - name={name!r}  address={d.address}")


async def _resolve_ble_target(args):
    if args.discover:
        devices = await _discover_ble_devices(args.scan_timeout)
        _print_ble_devices(devices)
        return None, None

    if args.no_ble:
        return None, None

    if not args.address and not args.name:
        raise RuntimeError("BLE mode requires --address, --name, or --discover (or use --no-ble)")

    if sys.platform == "darwin" and args.address and _looks_like_mac(args.address):
        wanted = _normalize_mac(args.address)
        devices = await _discover_ble_devices(args.scan_timeout)
        matches = []
        for d in devices:
            dev_name = (d.name or "")
            dev_name_norm = _normalize_mac(dev_name)
            if wanted and wanted in dev_name_norm:
                matches.append(d)

        if len(matches) == 1:
            dev = matches[0]
            print(f"[BLE] Resolved macOS UUID for {args.address}: {dev.address} ({dev.name or '<no-name>'})")
            return dev.address, dev

        if len(matches) > 1:
            print("[BLE] Multiple macOS devices matched this MAC-derived name:")
            _print_ble_devices(matches)
            raise RuntimeError("More than one BLE device matched the requested MAC; use the UUID shown above with --address")

        _print_ble_devices(devices)
        raise RuntimeError(
            "macOS/CoreBluetooth cannot connect using BLE MAC addresses directly, and no scanned device name matched the provided MAC. "
            "Run with --discover to list devices, then reconnect using the macOS UUID from the scan output or use --name <device-name>."
        )

    if args.name:
        devices = await _discover_ble_devices(args.scan_timeout)
        wanted = args.name.strip().lower()
        exact = [d for d in devices if (d.name or "").strip().lower() == wanted]
        partial = [d for d in devices if wanted in (d.name or "").strip().lower()]
        matches = exact or partial
        if not matches:
            _print_ble_devices(devices)
            raise RuntimeError(f"No BLE device found matching name: {args.name!r}")
        if len(matches) > 1:
            print("[BLE] Multiple name matches found:")
            _print_ble_devices(matches)
            raise RuntimeError("More than one BLE device matched --name; use the macOS UUID with --address")
        dev = matches[0]
        return dev.address, dev

    return args.address, None


# =============================================================================
# Shared state
# =============================================================================

state = {
    "speed": 0.0,
    "turn_deg": 0.0,
    "target_deg": 0.0,
    "max_deg": 12.0,
    "steer_smooth": 55.0,
    "ratio": 0.80,
    "c_sign": -1.0,
    "max_speed": 6000.0,
    "speed_step": 90.0,
    "speed_k": 0.03,
    "braking": False,
    "logging": False,
    "log_path": "",
    "auto_enabled": True,
    "auto_steer_sign": 1.0,
    "last_good_theta": None,
    "v_ref_raw": 0.0,
    "delta_cmd": 0.0,
    "status": "INIT",
    "last_e_lat": 0.0,
    "last_e_head_deg": 0.0,
    "last_track_t": 0.0,
    "flip_centerline": False,
    "drive_bias_deg": 0.0,
    "last_delta_for_speed": 0.0,
    "pp_target_x": float("nan"),
    "pp_target_y": float("nan"),
    "pp_alpha_deg": 0.0,
    "pp_lookahead_px": 0.0,
    "e_lat_filt": 0.0,
    "pp_alpha_filt": 0.0,
    "e_lat_rate_filt": 0.0,
    "pp_alpha_rate_filt": 0.0,
}

ble_client = None
log_file = None
log_writer = None
_cv_stop = threading.Event()
_new_centerline = [None]


# =============================================================================
# Utility helpers
# =============================================================================

def clamp(v: float, lo: float, hi: float) -> float:
    return max(lo, min(hi, v))


def sign_nonzero(v: float) -> float:
    return -1.0 if v < 0 else 1.0


def wrap_pi(a: float) -> float:
    return (a + math.pi) % (2.0 * math.pi) - math.pi


def compute_abc():
    s = state
    b = s["turn_deg"] * B_PER_DEG
    c = s["c_sign"] * s["ratio"] * b

    speed_raw = clamp(s["speed"], -s["max_speed"], s["max_speed"])
    drop = s["speed_k"] * abs(b)
    if abs(speed_raw) <= drop:
        a_sent = 0.0
    else:
        a_sent = sign_nonzero(speed_raw) * (abs(speed_raw) - drop)

    a_sent = clamp(a_sent, -s["max_speed"], s["max_speed"])
    return a_sent, b, c, drop


# =============================================================================
# Logging
# =============================================================================

_VISION_FIELDS = [
    "t_ms", "dt_ms", "mode", "used_meas", "holding",
    "meas_cx", "meas_cy",
    "kf_x", "kf_y", "kf_vx", "kf_vy",
    "blend_x", "blend_y", "ctrl_x", "ctrl_y",
    "blend_src", "blend_a", "speed_px_s",
    "theta_deg", "theta_src", "step_px",
    "mu_cv", "mu_ct", "omega_deg_s",
    "e_lat", "e_head_deg",
]

_DRIVE_FIELDS = [
    "timestamp", "a_sent", "speed_raw", "speed_drop",
    "turn_deg", "target_deg", "b_val", "c_val", "cb_ratio_live",
    "ratio_setting", "c_sign", "max_deg", "steer_smooth",
    "max_speed", "speed_step", "speed_k", "braking",
    "auto_enabled", "auto_speed_cmd", "auto_delta_cmd", "auto_status",
]

_vision_lock = threading.Lock()
_vision = {
    "t_ms": 0, "dt_ms": 0.0, "mode": "GLOBAL",
    "used_meas": False, "holding": False,
    "meas_cx": -1, "meas_cy": -1,
    "kf_x": 0.0, "kf_y": 0.0, "kf_vx": 0.0, "kf_vy": 0.0,
    "blend_x": 0.0, "blend_y": 0.0, "ctrl_x": 0.0, "ctrl_y": 0.0,
    "blend_src": "NONE", "blend_a": 0.0, "speed_px_s": 0.0,
    "theta_deg": float("nan"), "theta_src": "NONE", "step_px": 0.0,
    "mu_cv": 0.5, "mu_ct": 0.5, "omega_deg_s": 0.0,
    "e_lat": float("nan"), "e_head_deg": float("nan"),
    "pp_target_x": float("nan"), "pp_target_y": float("nan"),
    "pp_alpha_deg": float("nan"), "pp_lookahead_px": 0.0,
}


def log_row(a_sent: float, b: float, c: float, drop: float):
    global log_writer, log_file
    if not state["logging"] or log_writer is None:
        return

    live_ratio = (c / b) if abs(b) > 1e-9 else 0.0

    row = {
        "timestamp": f"{time.time():.6f}",
        "a_sent": int(round(a_sent)),
        "speed_raw": int(round(state["speed"])),
        "speed_drop": f"{drop:.3f}",
        "turn_deg": f"{state['turn_deg']:.3f}",
        "target_deg": f"{state['target_deg']:.3f}",
        "b_val": int(round(b)),
        "c_val": int(round(c)),
        "cb_ratio_live": f"{live_ratio:.5f}",
        "ratio_setting": f"{state['ratio']:.5f}",
        "c_sign": int(state["c_sign"]),
        "max_deg": f"{state['max_deg']:.3f}",
        "steer_smooth": f"{state['steer_smooth']:.3f}",
        "max_speed": int(round(state["max_speed"])),
        "speed_step": int(round(state["speed_step"])),
        "speed_k": f"{state['speed_k']:.5f}",
        "braking": int(state["braking"]),
        "auto_enabled": int(state["auto_enabled"]),
        "auto_speed_cmd": int(round(state["v_ref_raw"])),
        "auto_delta_cmd": f"{state['delta_cmd']:.3f}",
        "auto_status": state["status"],
    }

    with _vision_lock:
        v = dict(_vision)

    row.update({
        "t_ms": v["t_ms"],
        "dt_ms": f"{v['dt_ms']:.3f}",
        "mode": v["mode"],
        "used_meas": int(v["used_meas"]),
        "holding": int(v["holding"]),
        "meas_cx": v["meas_cx"],
        "meas_cy": v["meas_cy"],
        "kf_x": f"{v['kf_x']:.3f}",
        "kf_y": f"{v['kf_y']:.3f}",
        "kf_vx": f"{v['kf_vx']:.3f}",
        "kf_vy": f"{v['kf_vy']:.3f}",
        "blend_x": f"{v['blend_x']:.3f}",
        "blend_y": f"{v['blend_y']:.3f}",
        "ctrl_x": f"{v['ctrl_x']:.3f}",
        "ctrl_y": f"{v['ctrl_y']:.3f}",
        "blend_src": v["blend_src"],
        "blend_a": f"{v['blend_a']:.3f}",
        "speed_px_s": f"{v['speed_px_s']:.3f}",
        "theta_deg": f"{v['theta_deg']:.3f}" if math.isfinite(v["theta_deg"]) else "nan",
        "theta_src": v["theta_src"],
        "step_px": f"{v['step_px']:.3f}",
        "mu_cv": f"{v['mu_cv']:.4f}",
        "mu_ct": f"{v['mu_ct']:.4f}",
        "omega_deg_s": f"{v['omega_deg_s']:.2f}",
        "e_lat": f"{v['e_lat']:.2f}" if math.isfinite(v["e_lat"]) else "nan",
        "e_head_deg": f"{v['e_head_deg']:.2f}" if math.isfinite(v["e_head_deg"]) else "nan",
    })

    log_writer.writerow(row)
    log_file.flush()


def start_logging():
    global log_file, log_writer
    if state["logging"]:
        return

    out_dir = Path.cwd()
    ts = time.strftime("%Y%m%d_%H%M%S")
    path = out_dir / f"auto_collect_{ts}.csv"

    log_file = open(path, "w", newline="", encoding="utf-8")
    log_writer = csv.DictWriter(log_file, fieldnames=_DRIVE_FIELDS + _VISION_FIELDS)
    log_writer.writeheader()

    state["logging"] = True
    state["log_path"] = str(path)


def stop_logging():
    global log_file, log_writer
    state["logging"] = False
    if log_file is not None:
        log_file.flush()
        log_file.close()
    log_file = None
    log_writer = None


# =============================================================================
# Vision settings and helpers
# =============================================================================

CAM_INDEX = 0
FRAME_W = 1280
FRAME_H = 720
VIDEO_FILE = None
CENTERLINE_CSV = "centerline.csv"

MOG2_HISTORY = 500
MOG2_VAR_THRESHOLD = 16
MOG2_SHADOWS = False
THRESH_FG = 200
OPEN_K = 5
CLOSE_K = 7
OPEN_IT = 1
CLOSE_IT = 2
MIN_AREA = 250
MAX_AREA = 250000
MIN_WH = 10
MAX_WH = 900
MIN_MOTION_PIXELS = 120
ROI_HALF_SIZE = 180
ROI_MARGIN = 20
ROI_FAIL_TO_GLOBAL = 10
GLOBAL_SEARCH_EVERY = 8
HOLD_AFTER_NO_MEAS = 3
RESET_VEL_AFTER_HOLD = True

CV_ACC_SIGMA = 200.0
CT_ACC_SIGMA = 30.0
CT_OMEGA_SIGMA = 0.3
CT_OMEGA_MAX = 6.0
CT_OMEGA_INIT_VAR = 0.01
MEAS_SIGMA_PX = 8.0
IMM_P_STAY_CV = 0.97
IMM_P_STAY_CT = 0.92
IMM_MU0 = [0.8, 0.2]

MEAS_GATE_PX = 70.0
MEAS_ALPHA_SLOW = 0.65
MEAS_ALPHA_FAST = 0.88
FAST_SPEED_PX_S = 450.0
MOTION_WINDOW = 1
MIN_STEP_DIST = 1.2
EMA_ALPHA_SLOW = 0.28
EMA_ALPHA_FAST = 0.70
PREDICT_DT_MIN = 0.060
PREDICT_DT_MAX = 0.180
MIN_SPEED_FOR_VEL_HEADING = 8.0
MIN_SPEED_FOR_PREDICT = 5.0
NEAREST_WIN = 40
HEADING_LEN = 90

MAP_N_SAMPLES = 1200
MAP_SMOOTH_WIN = 31
MAP_CLEAN_CLOSE_K = 31
MAP_CLEAN_CLOSE_ITER = 1
MAP_CLEAN_OPEN_K = 7
MAP_CLEAN_OPEN_ITER = 1
MAP_CLEAN_MAX_HOLE = 5000
MAP_CENTER_CLOSE_K = 7
MAP_CENTER_CLOSE_ITER = 2


class _LightingMode:
    MODES = ["AUTO_CLAHE", "NONE", "EQUALIZE", "GAMMA_BRIGHT", "GAMMA_DARK"]
    _idx = 0

    @classmethod
    def current(cls):
        return cls.MODES[cls._idx]

    @classmethod
    def next(cls):
        cls._idx = (cls._idx + 1) % len(cls.MODES)
        print(f"[CV] Lighting mode -> {cls.current()}")


def _preprocess_lighting(bgr):
    mode = _LightingMode.current()
    if mode == "NONE":
        return bgr
    if mode == "EQUALIZE":
        lab = cv2.cvtColor(bgr, cv2.COLOR_BGR2LAB)
        l, a, b = cv2.split(lab)
        l = cv2.equalizeHist(l)
        return cv2.cvtColor(cv2.merge([l, a, b]), cv2.COLOR_LAB2BGR)
    if mode == "GAMMA_BRIGHT":
        lut = (np.power(np.arange(256) / 255.0, 0.5) * 255).astype(np.uint8)
        return lut[bgr]
    if mode == "GAMMA_DARK":
        lut = (np.power(np.arange(256) / 255.0, 1.8) * 255).astype(np.uint8)
        return lut[bgr]
    lab = cv2.cvtColor(bgr, cv2.COLOR_BGR2LAB)
    l, a, b = cv2.split(lab)
    clahe = cv2.createCLAHE(clipLimit=3.0, tileGridSize=(8, 8))
    l = clahe.apply(l)
    return cv2.cvtColor(cv2.merge([l, a, b]), cv2.COLOR_LAB2BGR)


def _draw_arrow(img, x, y, theta, length, color, thickness=3):
    x2 = int(round(x + math.cos(theta) * length))
    y2 = int(round(y + math.sin(theta) * length))
    cv2.arrowedLine(
        img,
        (int(round(x)), int(round(y))),
        (x2, y2),
        color,
        thickness,
        line_type=cv2.LINE_AA,
        tipLength=0.25,
    )


def _ema_angle(prev, new_a, alpha):
    if prev is None:
        return new_a
    mx = (1 - alpha) * math.cos(prev) + alpha * math.cos(new_a)
    my = (1 - alpha) * math.sin(prev) + alpha * math.sin(new_a)
    return math.atan2(my, mx)


def _adaptive_alpha(speed):
    t = clamp(speed / FAST_SPEED_PX_S, 0.0, 1.0)
    return (1 - t) * EMA_ALPHA_SLOW + t * EMA_ALPHA_FAST


def _adaptive_meas_alpha(speed):
    t = clamp(speed / FAST_SPEED_PX_S, 0.0, 1.0)
    return (1 - t) * MEAS_ALPHA_SLOW + t * MEAS_ALPHA_FAST


class _CamThread:
    def __init__(self, src, w, h):
        self.src = src
        self.w = w
        self.h = h
        self.cap = None
        self.q = queue.Queue(maxsize=1)
        self.stopped = False

    def start(self):
        src = self.src
        capture_arg = src if isinstance(src, str) else int(src)

        backend_candidates = []
        if isinstance(src, str):
            backend_candidates = [cv2.CAP_ANY]
        elif sys.platform == "darwin":
            backend_candidates = [getattr(cv2, "CAP_AVFOUNDATION", cv2.CAP_ANY), cv2.CAP_ANY]
        elif sys.platform.startswith("win"):
            backend_candidates = [cv2.CAP_DSHOW, getattr(cv2, "CAP_MSMF", cv2.CAP_ANY), cv2.CAP_ANY]
        else:
            backend_candidates = [getattr(cv2, "CAP_V4L2", cv2.CAP_ANY), cv2.CAP_ANY]

        for backend in backend_candidates:
            cap = cv2.VideoCapture(capture_arg, backend)
            if cap.isOpened():
                self.cap = cap
                break
            cap.release()

        if self.cap is None or not self.cap.isOpened():
            raise RuntimeError(f"Cannot open camera: {src}")
        self.cap.set(cv2.CAP_PROP_FRAME_WIDTH, self.w)
        self.cap.set(cv2.CAP_PROP_FRAME_HEIGHT, self.h)
        self.cap.set(cv2.CAP_PROP_BUFFERSIZE, 1)
        self.cap.set(cv2.CAP_PROP_FOURCC, cv2.VideoWriter_fourcc(*"MJPG"))
        self.cap.set(cv2.CAP_PROP_AUTO_EXPOSURE, 0.25)
        self.cap.set(cv2.CAP_PROP_AUTO_WB, 0)
        threading.Thread(target=self._reader, daemon=True).start()
        return self

    def _reader(self):
        while not self.stopped:
            ok, frame = self.cap.read()
            if not ok:
                time.sleep(0.005)
                continue
            if self.q.full():
                try:
                    self.q.get_nowait()
                except queue.Empty:
                    pass
            try:
                self.q.put_nowait(frame)
            except queue.Full:
                pass

    def read_latest(self, timeout=0.5):
        try:
            return self.q.get(timeout=timeout)
        except queue.Empty:
            return None

    def stop(self):
        self.stopped = True
        if self.cap:
            self.cap.release()


def _build_mog2():
    return cv2.createBackgroundSubtractorMOG2(MOG2_HISTORY, MOG2_VAR_THRESHOLD, MOG2_SHADOWS)


def _clean_mask(mask):
    _, m = cv2.threshold(mask, THRESH_FG, 255, cv2.THRESH_BINARY)
    ko = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (OPEN_K, OPEN_K))
    kc = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (CLOSE_K, CLOSE_K))
    m = cv2.morphologyEx(m, cv2.MORPH_OPEN, ko, iterations=OPEN_IT)
    m = cv2.morphologyEx(m, cv2.MORPH_CLOSE, kc, iterations=CLOSE_IT)
    return m


def _clamp_roi(x0, y0, x1, y1, W, H):
    x0 = max(0, min(W - 1, x0))
    y0 = max(0, min(H - 1, y0))
    x1 = max(0, min(W, x1))
    y1 = max(0, min(H, y1))
    return None if x1 <= x0 or y1 <= y0 else (x0, y0, x1, y1)


def _make_roi(px, py, W, H):
    return _clamp_roi(
        int(px - ROI_HALF_SIZE - ROI_MARGIN),
        int(py - ROI_HALF_SIZE - ROI_MARGIN),
        int(px + ROI_HALF_SIZE + ROI_MARGIN),
        int(py + ROI_HALF_SIZE + ROI_MARGIN),
        W,
        H,
    )


def _pick_contour(mask, roi=None):
    if roi:
        x0, y0, x1, y1 = roi
        sub = mask[y0:y1, x0:x1]
        conts, _ = cv2.findContours(sub, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
    else:
        x0 = y0 = 0
        conts, _ = cv2.findContours(mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)

    best = None
    best_area = 0
    for cnt in conts:
        area = float(cv2.contourArea(cnt))
        if area < MIN_AREA or area > MAX_AREA:
            continue
        x, y, w, h = cv2.boundingRect(cnt)
        x += x0
        y += y0
        if w < MIN_WH or h < MIN_WH or w > MAX_WH or h > MAX_WH:
            continue
        if area > best_area:
            best_area = area
            cx = x + w // 2
            cy = y + h // 2
            bx0 = max(0, x)
            by0 = max(0, y)
            bx1 = min(mask.shape[1], x + w)
            by1 = min(mask.shape[0], y + h)
            best = {
                "bbox": (x, y, w, h),
                "cx": cx,
                "cy": cy,
                "area": best_area,
                "motion_px": int(cv2.countNonZero(mask[by0:by1, bx0:bx1])),
            }
    return best


def _ensure_closed(arr):
    if arr is None or len(arr) < 2:
        return arr
    if np.hypot(*(arr[0] - arr[-1])) > 1e-6:
        arr = np.vstack([arr, arr[0]])
    return arr


def _load_centerline(path, flip=False):
    if not path or not os.path.exists(path):
        return None
    pts = []
    with open(path, encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#") or line.lower().startswith("x"):
                continue
            parts = line.split(",")
            if len(parts) < 2:
                continue
            try:
                pts.append((float(parts[0]), float(parts[1])))
            except ValueError:
                continue
    if len(pts) < 2:
        return None
    arr = _ensure_closed(np.array(pts, dtype=np.float64))
    if flip:
        arr = _ensure_closed(arr[::-1].copy())
    return arr


def _save_centerline(path, pts, W, H):
    with open(path, "w", newline="", encoding="utf-8") as f:
        f.write(f"# W={W} H={H}\nx,y\n")
        for p in pts:
            f.write(f"{p[0]:.3f},{p[1]:.3f}\n")


def _nearest_on_polyline(p, path, last_i, win):
    nseg = len(path) - 1
    best_d2 = 1e18
    best = None
    best_i = last_i % max(1, nseg)

    for off in range(-win, win + 1):
        i = (last_i + off) % nseg
        a = path[i]
        b = path[i + 1]
        ab = b - a
        ab2 = float(ab @ ab)
        if ab2 < 1e-9:
            continue
        t = float(np.clip(((p - a) @ ab) / ab2, 0, 1))
        proj = a + t * ab
        d = p - proj
        d2 = float(d @ d)
        if d2 < best_d2:
            best_d2 = d2
            heading = float(np.arctan2(ab[1], ab[0]))
            cross = float(ab[0] * (p[1] - a[1]) - ab[1] * (p[0] - a[0]))
            best = (i, heading, float(np.sign(cross) * math.sqrt(d2)), proj)
            best_i = i

    if best is None:
        raise RuntimeError("nearest search failed")
    return best, best_i


def _avg_segment_len(path):
    if path is None or len(path) < 2:
        return 1.0
    diffs = np.diff(path, axis=0)
    lens = np.hypot(diffs[:, 0], diffs[:, 1])
    return float(max(1.0, np.mean(lens)))


def _advance_along_path(path, seg_i, proj_pt, dist_px):
    nseg = len(path) - 1
    if nseg <= 0:
        return proj_pt.copy(), seg_i, float("nan")

    dist_left = max(0.0, float(dist_px))
    cur = proj_pt.astype(np.float64).copy()
    i = int(seg_i) % nseg

    for _ in range(nseg + 1):
        nxt = path[i + 1]
        seg = nxt - cur
        seg_len = float(np.hypot(seg[0], seg[1]))
        if seg_len < 1e-9:
            i = (i + 1) % nseg
            cur = path[i].astype(np.float64).copy()
            continue

        if dist_left <= seg_len:
            t = dist_left / seg_len
            pt = cur + t * seg
            hdg = float(np.arctan2(seg[1], seg[0]))
            return pt, i, hdg

        dist_left -= seg_len
        i = (i + 1) % nseg
        cur = path[i].astype(np.float64).copy()

    a = path[i]
    b = path[i + 1]
    hdg = float(np.arctan2(b[1] - a[1], b[0] - a[0]))
    return cur, i, hdg


def _dynamic_lookahead_px(speed_px_s):
    return float(clamp(88.0 + 0.78 * speed_px_s, 88.0, 170.0))


def _draw_centerline(img, path_cl):
    if path_cl is None or len(path_cl) < 2:
        return
    cv2.polylines(img, [path_cl[:, :2].astype(np.int32)], True, (0, 200, 60), 2, cv2.LINE_AA)


def _keep_largest(mask255):
    n, labels, stats, _ = cv2.connectedComponentsWithStats((mask255 > 0).astype(np.uint8), connectivity=8)
    if n <= 1:
        return mask255.copy()
    best = 1 + int(np.argmax(stats[1:, cv2.CC_STAT_AREA]))
    out = np.zeros_like(mask255)
    out[labels == best] = 255
    return out


def _fill_holes(mask255, max_hole):
    inv = cv2.bitwise_not(mask255)
    n, labels, stats, _ = cv2.connectedComponentsWithStats((inv > 0).astype(np.uint8), connectivity=8)
    out = mask255.copy()
    h, w = mask255.shape
    for lab in range(1, n):
        area = stats[lab, cv2.CC_STAT_AREA]
        x = stats[lab, cv2.CC_STAT_LEFT]
        y = stats[lab, cv2.CC_STAT_TOP]
        ww = stats[lab, cv2.CC_STAT_WIDTH]
        hh = stats[lab, cv2.CC_STAT_HEIGHT]
        if x == 0 or y == 0 or x + ww >= w or y + hh >= h:
            continue
        if area <= max_hole:
            out[labels == lab] = 255
    return out


def _build_free_white(bgr):
    h, w = bgr.shape[:2]
    hsv = cv2.cvtColor(bgr, cv2.COLOR_BGR2HSV)

    red1 = cv2.inRange(hsv, (0, 70, 50), (12, 255, 255))
    red2 = cv2.inRange(hsv, (168, 70, 50), (179, 255, 255))
    red = cv2.bitwise_or(red1, red2)

    ys, xs = np.where(red > 0)
    if len(xs) < 50:
        raise RuntimeError("Not enough red pixels — check lighting / track visible")

    pts = np.column_stack([xs, ys]).astype(np.float32)
    hull = cv2.convexHull(pts).reshape(-1, 1, 2).astype(np.int32)
    roi_mask = np.zeros((h, w), np.uint8)
    cv2.fillConvexPoly(roi_mask, hull, 255)

    white = cv2.inRange(hsv, (0, 0, 180), (179, 70, 255))
    white = cv2.bitwise_and(white, roi_mask)

    barriers = cv2.bitwise_or(red, white)
    barriers = cv2.bitwise_and(barriers, roi_mask)

    e5 = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (5, 5))
    e7 = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (7, 7))
    e9 = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (9, 9))
    barriers = cv2.morphologyEx(barriers, cv2.MORPH_OPEN, e5, iterations=1)
    barriers = cv2.morphologyEx(barriers, cv2.MORPH_CLOSE, e7, iterations=3)
    barriers = cv2.dilate(barriers, e9, iterations=1)

    walls = cv2.bitwise_or(barriers, cv2.bitwise_not(roi_mask))
    flood = walls.copy()
    ffmask = np.zeros((h + 2, w + 2), np.uint8)
    cv2.floodFill(flood, ffmask, (0, 0), 128)
    outside = ((flood == 128).astype(np.uint8) * 255)

    return cv2.bitwise_and(cv2.bitwise_not(outside), (walls == 0).astype(np.uint8) * 255)


def _clean_track_mask(free_white):
    _, m = cv2.threshold(free_white, 0, 255, cv2.THRESH_BINARY)
    m = _keep_largest(m)
    kC = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (MAP_CLEAN_CLOSE_K, MAP_CLEAN_CLOSE_K))
    m = cv2.morphologyEx(m, cv2.MORPH_CLOSE, kC, iterations=MAP_CLEAN_CLOSE_ITER)
    m = _fill_holes(m, MAP_CLEAN_MAX_HOLE)
    kO = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (MAP_CLEAN_OPEN_K, MAP_CLEAN_OPEN_K))
    m = cv2.morphologyEx(m, cv2.MORPH_OPEN, kO, iterations=MAP_CLEAN_OPEN_ITER)
    return _keep_largest(m)


def _resample(pts, N):
    closed = np.vstack([pts, pts[0]])
    diffs = np.diff(closed, axis=0)
    s = np.concatenate([[0.0], np.cumsum(np.hypot(diffs[:, 0], diffs[:, 1]))])
    L = s[-1]
    if L < 1e-6:
        return pts
    tgt = np.linspace(0, L, N, endpoint=False)
    return np.column_stack([
        np.interp(tgt, s, closed[:, 0]),
        np.interp(tgt, s, closed[:, 1]),
    ])


def _smooth_cyclic(pts, win):
    n = len(pts)
    if n < 3 or win < 3:
        return pts
    win = win | 1
    k = win // 2
    padded = np.vstack([pts[-k:], pts, pts[:k]])
    kern = np.ones(win) / win
    return np.column_stack([
        np.convolve(padded[:, 0], kern, "valid"),
        np.convolve(padded[:, 1], kern, "valid"),
    ])


def _build_centerline_points(free_clean):
    _, road = cv2.threshold(free_clean, 127, 255, cv2.THRESH_BINARY)
    k = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (MAP_CENTER_CLOSE_K, MAP_CENTER_CLOSE_K))
    road = cv2.morphologyEx(road, cv2.MORPH_CLOSE, k, iterations=MAP_CENTER_CLOSE_ITER)

    contours, hierarchy = cv2.findContours(road, cv2.RETR_CCOMP, cv2.CHAIN_APPROX_NONE)
    if not contours or hierarchy is None:
        raise RuntimeError("No contours in track mask")
    hierarchy = hierarchy[0]

    outer = max(
        (i for i, h in enumerate(hierarchy) if h[3] == -1),
        key=lambda i: abs(cv2.contourArea(contours[i])),
    )

    inner = -1
    best_a = -1
    child = hierarchy[outer][2]
    while child != -1:
        a = abs(cv2.contourArea(contours[child]))
        if a > best_a:
            best_a = a
            inner = child
        child = hierarchy[child][0]

    if inner < 0:
        by_area = sorted(range(len(contours)), key=lambda i: abs(cv2.contourArea(contours[i])), reverse=True)
        if len(by_area) < 2:
            raise RuntimeError("Cannot find inner contour")
        inner = by_area[1]

    outer_pts = contours[outer].reshape(-1, 2).astype(np.float64)
    inner_pts = contours[inner].reshape(-1, 2).astype(np.float64)
    outer_r = _resample(outer_pts, MAP_N_SAMPLES)
    _, idx = cKDTree(inner_pts).query(outer_r)
    center = _smooth_cyclic((outer_r + inner_pts[idx]) * 0.5, MAP_SMOOTH_WIN)
    return np.vstack([center, center[0]])


def _build_centerline_from_frame(frame):
    print("[MAP] Building...")
    try:
        work = _preprocess_lighting(frame)
        free_white = _build_free_white(work)
        free_clean = _clean_track_mask(free_white)
        pts = _build_centerline_points(free_clean)
        if state["flip_centerline"]:
            pts = _ensure_closed(pts[::-1].copy())
        _save_centerline(CENTERLINE_CSV, pts, frame.shape[1], frame.shape[0])
        print(f"[MAP] Done — {len(pts)} pts -> {CENTERLINE_CSV}")
        return pts.astype(np.float64)
    except Exception as e:
        print(f"[MAP ERROR] {e}")
        import traceback
        traceback.print_exc()
        return None


# =============================================================================
# IMM tracker
# =============================================================================

class _CVModel:
    DIM = 4

    def __init__(self):
        self.x = np.zeros((4, 1), np.float64)
        self.P = np.eye(4, dtype=np.float64) * 500.0

    @staticmethod
    def F(dt):
        return np.array([[1, 0, dt, 0], [0, 1, 0, dt], [0, 0, 1, 0], [0, 0, 0, 1]], np.float64)

    @staticmethod
    def Q(dt):
        dt2 = dt * dt
        dt3 = dt2 * dt
        dt4 = dt3 * dt
        q = CV_ACC_SIGMA ** 2
        return np.array([
            [dt4 / 4, 0, dt3 / 2, 0],
            [0, dt4 / 4, 0, dt3 / 2],
            [dt3 / 2, 0, dt2, 0],
            [0, dt3 / 2, 0, dt2],
        ], np.float64) * q


class _CTModel:
    DIM = 5

    def __init__(self):
        self.x = np.zeros((5, 1), np.float64)
        self.P = np.eye(5, dtype=np.float64) * 500.0
        self.P[4, 4] = CT_OMEGA_INIT_VAR

    @staticmethod
    def F(dt, omega):
        if abs(omega) < 1e-4:
            return np.array([
                [1, 0, dt, 0, 0],
                [0, 1, 0, dt, 0],
                [0, 0, 1, 0, 0],
                [0, 0, 0, 1, 0],
                [0, 0, 0, 0, 1],
            ], np.float64)
        s = math.sin(omega * dt)
        c = math.cos(omega * dt)
        o = omega
        return np.array([
            [1, 0, s / o, -(1 - c) / o, 0],
            [0, 1, (1 - c) / o, s / o, 0],
            [0, 0, c, -s, 0],
            [0, 0, s, c, 0],
            [0, 0, 0, 0, 1],
        ], np.float64)

    @staticmethod
    def Q(dt):
        dt2 = dt * dt
        dt3 = dt2 * dt
        dt4 = dt3 * dt
        qa = CT_ACC_SIGMA ** 2
        Q5 = np.zeros((5, 5), np.float64)
        Q5[:4, :4] = np.array([
            [dt4 / 4, 0, dt3 / 2, 0],
            [0, dt4 / 4, 0, dt3 / 2],
            [dt3 / 2, 0, dt2, 0],
            [0, dt3 / 2, 0, dt2],
        ]) * qa
        Q5[4, 4] = (CT_OMEGA_SIGMA * dt) ** 2
        return Q5


_H4 = np.array([[1, 0, 0, 0], [0, 1, 0, 0]], np.float64)
_H5 = np.array([[1, 0, 0, 0, 0], [0, 1, 0, 0, 0]], np.float64)


def _Hfor(dim):
    return _H4 if dim == 4 else _H5


class _IMMTracker:
    N = 2

    def __init__(self):
        self.models = [_CVModel(), _CTModel()]
        self.mu = np.array(IMM_MU0, dtype=np.float64)
        self.mu /= self.mu.sum()
        self.Pi = np.array([
            [IMM_P_STAY_CV, 1 - IMM_P_STAY_CV],
            [1 - IMM_P_STAY_CT, IMM_P_STAY_CT],
        ], np.float64)
        self.inited = False
        self._last_omega = 0.0

    def _to5(self, idx):
        m = self.models[idx]
        if idx == 0:
            x5 = np.vstack([m.x, [[self._last_omega]]])
            P5 = np.zeros((5, 5))
            P5[:4, :4] = m.P
            P5[4, 4] = CT_OMEGA_INIT_VAR
        else:
            x5, P5 = m.x.copy(), m.P.copy()
        return x5, P5

    def init_at(self, x, y):
        for m in self.models:
            m.x[:] = 0.0
            m.x[0, 0] = x
            m.x[1, 0] = y
            m.P = np.eye(m.DIM, dtype=np.float64) * 200.0
        self.models[1].P[4, 4] = CT_OMEGA_INIT_VAR
        self.inited = True

    def correct_always(self, zx, zy, dt):
        dt = float(np.clip(dt, 1e-3, 0.2))
        if not self.inited:
            self.init_at(zx, zy)

        z = np.array([[zx], [zy]], np.float64)
        R = np.eye(2, dtype=np.float64) * (MEAS_SIGMA_PX ** 2)
        c = self.Pi.T @ self.mu
        mu_ij = np.zeros((self.N, self.N))
        for i in range(self.N):
            for j in range(self.N):
                mu_ij[i, j] = self.Pi[i, j] * self.mu[i] / (c[j] + 1e-300)

        mx, mP = [], []
        for j in range(self.N):
            x5j = np.zeros((5, 1))
            for i in range(self.N):
                xi5, _ = self._to5(i)
                x5j += mu_ij[i, j] * xi5
            P5j = np.zeros((5, 5))
            for i in range(self.N):
                xi5, Pi5 = self._to5(i)
                d = xi5 - x5j
                P5j += mu_ij[i, j] * (Pi5 + d @ d.T)
            dj = self.models[j].DIM
            mx.append(x5j[:dj])
            mP.append(P5j[:dj, :dj])

        liks = np.zeros(self.N)
        for j in range(self.N):
            mj = self.models[j]
            mj.x = mx[j]
            mj.P = mP[j]
            if j == 0:
                Fj = _CVModel.F(dt)
                Qj = _CVModel.Q(dt)
            else:
                omega = float(np.clip(mj.x[4, 0], -CT_OMEGA_MAX, CT_OMEGA_MAX))
                mj.x[4, 0] = omega
                self._last_omega = omega
                Fj = _CTModel.F(dt, omega)
                Qj = _CTModel.Q(dt)

            xp = Fj @ mj.x
            Pp = Fj @ mj.P @ Fj.T + Qj
            Hj = _Hfor(mj.DIM)
            S = Hj @ Pp @ Hj.T + R
            K = Pp @ Hj.T @ np.linalg.inv(S)
            innov = z - Hj @ xp
            mj.x = xp + K @ innov
            IKH = np.eye(mj.DIM) - K @ Hj
            mj.P = IKH @ Pp @ IKH.T + K @ R @ K.T

            if j == 1:
                self._last_omega = float(mj.x[4, 0])

            det_S = max(np.linalg.det(S), 1e-300)
            mahal2 = (innov.T @ np.linalg.inv(S) @ innov).item()
            liks[j] = math.exp(max(-0.5 * mahal2, -500.0)) / math.sqrt((2 * math.pi) ** 2 * det_S)

        self.mu = c * liks
        d = self.mu.sum()
        self.mu = np.array(IMM_MU0, dtype=np.float64) if d < 1e-300 else self.mu / d
        return self._fuse()

    def hold(self):
        return self._fuse()

    def _fuse(self):
        x5 = np.zeros((5, 1))
        for j in range(self.N):
            xj5, _ = self._to5(j)
            x5 += self.mu[j] * xj5
        return float(x5[0, 0]), float(x5[1, 0]), float(x5[2, 0]), float(x5[3, 0])

    @property
    def mu_cv(self):
        return float(self.mu[0])

    @property
    def mu_ct(self):
        return float(self.mu[1])

    @property
    def omega_deg_s(self):
        return math.degrees(float(self.models[1].x[4, 0]))



# =============================================================================
# Autonomous controller
# =============================================================================

AUTO_MIN_SPEED_RAW = 3000.0
AUTO_MID_SPEED_RAW = 3600.0
AUTO_MAX_SPEED_RAW = 4200.0
AUTO_BRAKE_IF_NO_TRACK_MS = 600.0
AUTO_HARD_EHEAD_DEG = 140.0
AUTO_HARD_ELAT_PX = 220.0

# Pure-pursuit-like controller tuned for your drive-pod style car:
# - steer toward a forward lookahead target, not the nearest point
# - rely mainly on motion-direction error, not nearest-path heading error
# - add only a very small centerline bias to stop long-term drift
# - keep speed low and almost constant
AUTO_PP_K_ALPHA = 0.15
AUTO_PP_K_LAT = 0.014
AUTO_PP_ALPHA_DEADBAND = 4.5
AUTO_PP_ELAT_DEADBAND = 10.0
AUTO_PP_RATE_DEG = 1.10
AUTO_PP_FILTER = 0.42
AUTO_PP_NEAR_STRAIGHT_GAIN = 0.38
AUTO_PP_NEAR_ALPHA = 7.0
AUTO_PP_NEAR_ELAT = 9.0
AUTO_PP_DELAY_COMP_S = 0.20
AUTO_PP_D_ALPHA = 0.055
AUTO_PP_D_ELAT = 0.018
AUTO_PP_RATE_ALPHA = 0.30
AUTO_PP_RATE_ELAT = 0.22
AUTO_PP_D_MAX = 3.0
AUTO_PP_SOFTSAT_DEG = 12.5


def _speed_schedule(abs_delta_deg: float, abs_e_lat: float, abs_alpha_deg: float) -> float:
    if abs_alpha_deg > 22.0 or abs_e_lat > 44.0 or abs_delta_deg > 7.0:
        return AUTO_MIN_SPEED_RAW
    if abs_alpha_deg > 9.0 or abs_e_lat > 18.0 or abs_delta_deg > 3.0:
        return AUTO_MID_SPEED_RAW
    return AUTO_MAX_SPEED_RAW


def _update_speed_toward_target(target_speed_raw: float):
    step = state["speed_step"]
    cur = state["speed"]
    if cur < target_speed_raw:
        state["speed"] = min(target_speed_raw, cur + step)
    elif cur > target_speed_raw:
        state["speed"] = max(target_speed_raw, cur - step)


def _first_order_update(prev: float, target: float, alpha: float) -> float:
    return (1.0 - alpha) * prev + alpha * target


def _autonomy_step():
    with _vision_lock:
        v = dict(_vision)

    now = time.time()
    state["status"] = "AUTO"

    if math.isfinite(v["e_lat"]) and math.isfinite(v["e_head_deg"]):
        state["last_track_t"] = now
    elif (now - state["last_track_t"]) * 1000.0 > AUTO_BRAKE_IF_NO_TRACK_MS:
        state["status"] = "NO_TRACK"
        state["braking"] = True
        state["speed"] = 0.0
        state["target_deg"] = 0.0
        state["v_ref_raw"] = 0.0
        state["delta_cmd"] = 0.0
        state["pp_alpha_deg"] = 0.0
        return
    else:
        state["status"] = "TRACK_HOLD"
        state["target_deg"] = 0.0
        state["v_ref_raw"] = AUTO_MIN_SPEED_RAW
        state["pp_alpha_deg"] = 0.0
        _update_speed_toward_target(AUTO_MIN_SPEED_RAW)
        state["braking"] = False
        return

    e_lat = float(v["e_lat"])
    speed_px = max(10.0, float(v["speed_px_s"]))
    theta_deg = float(v["theta_deg"])
    pp_target_x = float(v.get("pp_target_x", float("nan")))
    pp_target_y = float(v.get("pp_target_y", float("nan")))
    pp_alpha_deg = float(v.get("pp_alpha_deg", float("nan")))
    pp_lookahead_px = float(v.get("pp_lookahead_px", 0.0))

    state["pp_target_x"] = pp_target_x
    state["pp_target_y"] = pp_target_y
    state["pp_lookahead_px"] = pp_lookahead_px

    if not math.isfinite(pp_alpha_deg):
        # Fallback: use the logged heading error but with a very small weight.
        pp_alpha_deg = clamp(float(v["e_head_deg"]), -45.0, 45.0)
        state["status"] = "PP_FALLBACK"

    e_lat_raw = e_lat
    pp_alpha_raw = pp_alpha_deg

    if abs(pp_alpha_deg) < AUTO_PP_ALPHA_DEADBAND:
        pp_alpha_deg = 0.0
    if abs(e_lat) < AUTO_PP_ELAT_DEADBAND:
        e_lat = 0.0

    state["e_lat_filt"] = _first_order_update(float(state["e_lat_filt"]), e_lat, 0.28)
    state["pp_alpha_filt"] = _first_order_update(float(state["pp_alpha_filt"]), pp_alpha_deg, 0.24)

    dt_ctrl = 1.0 / CONTROL_HZ
    e_lat_rate = (state["e_lat_filt"] - float(state["last_e_lat"])) / max(1e-3, dt_ctrl)
    pp_alpha_rate = (state["pp_alpha_filt"] - float(state["last_e_head_deg"])) / max(1e-3, dt_ctrl)
    state["e_lat_rate_filt"] = _first_order_update(float(state["e_lat_rate_filt"]), e_lat_rate, AUTO_PP_RATE_ELAT)
    state["pp_alpha_rate_filt"] = _first_order_update(float(state["pp_alpha_rate_filt"]), pp_alpha_rate, AUTO_PP_RATE_ALPHA)

    e_lat_pred = state["e_lat_filt"] + AUTO_PP_DELAY_COMP_S * state["e_lat_rate_filt"]
    pp_alpha_pred = state["pp_alpha_filt"] + AUTO_PP_DELAY_COMP_S * state["pp_alpha_rate_filt"]
    e_lat_pred = clamp(e_lat_pred, -95.0, 95.0)
    pp_alpha_pred = clamp(pp_alpha_pred, -35.0, 35.0)

    alpha_term = AUTO_PP_K_ALPHA * pp_alpha_pred
    lat_term = math.degrees(math.atan2(AUTO_PP_K_LAT * e_lat_pred, speed_px + 110.0))
    d_term = clamp(
        AUTO_PP_D_ALPHA * state["pp_alpha_rate_filt"] + AUTO_PP_D_ELAT * state["e_lat_rate_filt"],
        -AUTO_PP_D_MAX,
        AUTO_PP_D_MAX,
    )

    raw_delta = -(alpha_term + lat_term + d_term)
    raw_delta = AUTO_PP_SOFTSAT_DEG * math.tanh(raw_delta / AUTO_PP_SOFTSAT_DEG)
    raw_delta *= float(state["auto_steer_sign"])

    if abs(pp_alpha_pred) < AUTO_PP_NEAR_ALPHA and abs(e_lat_pred) < AUTO_PP_NEAR_ELAT:
        raw_delta *= AUTO_PP_NEAR_STRAIGHT_GAIN
        state["status"] = "STRAIGHT"
    else:
        state["status"] = "PP_TRACK"

    prev_cmd = float(state["delta_cmd"])
    filt_cmd = (1.0 - AUTO_PP_FILTER) * prev_cmd + AUTO_PP_FILTER * raw_delta
    delta = prev_cmd + clamp(filt_cmd - prev_cmd, -AUTO_PP_RATE_DEG, AUTO_PP_RATE_DEG)
    delta = clamp(delta, -state["max_deg"], state["max_deg"])

    target_speed = _speed_schedule(abs(delta), abs(e_lat_pred), abs(pp_alpha_pred))

    state["last_e_lat"] = state["e_lat_filt"]
    state["last_e_head_deg"] = state["pp_alpha_filt"]
    state["pp_alpha_deg"] = pp_alpha_pred
    state["delta_cmd"] = delta
    state["target_deg"] = delta
    state["v_ref_raw"] = target_speed
    state["last_delta_for_speed"] = delta
    _update_speed_toward_target(target_speed)
    state["braking"] = False
# =============================================================================
# BLE control loop
# =============================================================================

async def control_loop():
    global ble_client
    period = 1.0 / CONTROL_HZ
    loop = asyncio.get_event_loop()

    while not _cv_stop.is_set():
        t0 = loop.time()
        dt = period

        if ble_client and ble_client.is_connected:
            if state["auto_enabled"]:
                _autonomy_step()
            else:
                state["status"] = "AUTO_OFF"
                state["braking"] = True
                state["speed"] = 0.0
                state["target_deg"] = 0.0
                state["v_ref_raw"] = 0.0
                state["delta_cmd"] = 0.0

            if state["braking"]:
                try:
                    await ble_client.write_gatt_char(WRITE_UUID, BRAKE_PACKET, response=False)
                except Exception:
                    pass
                a_sent, b, c, drop = compute_abc()
                log_row(a_sent, b, c, drop)
            else:
                diff = state["target_deg"] - state["turn_deg"]
                step = state["steer_smooth"] * dt
                if abs(diff) <= step:
                    state["turn_deg"] = state["target_deg"]
                else:
                    state["turn_deg"] += math.copysign(step, diff)

                a_sent, b, c, drop = compute_abc()
                try:
                    await ble_client.write_gatt_char(
                        WRITE_UUID,
                        build_packet(a_sent, b, c),
                        response=False,
                    )
                except Exception:
                    pass
                log_row(a_sent, b, c, drop)

        elapsed = loop.time() - t0
        await asyncio.sleep(max(0.0, period - elapsed))


# =============================================================================
# Vision thread
# =============================================================================

def _cv_thread():
    cam_src = VIDEO_FILE if VIDEO_FILE else CAM_INDEX
    cam = _CamThread(cam_src, FRAME_W, FRAME_H).start()
    backSub = _build_mog2()
    imm = _IMMTracker()

    path_cl = _load_centerline(CENTERLINE_CSV, flip=state["flip_centerline"])
    last_seg_i = 0
    if path_cl is not None:
        print(f"[CV] Loaded centerline: {len(path_cl)} pts")
        if state["flip_centerline"]:
            print("[CV] Centerline direction flipped")

    show_mask = False
    show_debug = False
    last_est = None
    roi_miss = 0
    tick = 0
    no_meas = 0
    was_holding = False
    blend_buf = []
    prev_theta = None
    last_good_theta = None
    prev_t = time.time()

    print("[CV] Running -- M=centerline  R=log  A=auto  V=mask  L=lighting  D=debug  Q=quit")

    try:
        while not _cv_stop.is_set():
            frame = cam.read_latest(timeout=0.5)
            if frame is None:
                continue

            if _new_centerline[0] is not None:
                path_cl = _new_centerline[0]
                last_seg_i = 0
                _new_centerline[0] = None

            tick += 1
            now = time.time()
            dt = float(np.clip(now - prev_t, 1e-3, 0.2))
            prev_t = now
            dt_ms = dt * 1000.0
            t_ms = time.time() * 1000.0

            work = _preprocess_lighting(frame)
            fg = _clean_mask(backSub.apply(work))
            H, W = fg.shape[:2]

            use_global = (not imm.inited) or (roi_miss >= ROI_FAIL_TO_GLOBAL) or (tick % GLOBAL_SEARCH_EVERY == 0)
            roi = None
            mode = "GLOBAL" if use_global else "ROI"
            if not use_global and last_est is not None:
                roi = _make_roi(last_est[0], last_est[1], W, H)
                if roi is None:
                    mode = "GLOBAL"

            det = _pick_contour(fg, roi=roi)
            used_meas = False
            meas_cx = meas_cy = -1
            kf_x = kf_y = kf_vx = kf_vy = 0.0

            if det is not None and det["motion_px"] >= MIN_MOTION_PIXELS:
                meas_cx, meas_cy = det["cx"], det["cy"]
                if was_holding and RESET_VEL_AFTER_HOLD:
                    imm.init_at(meas_cx, meas_cy)
                kf_x, kf_y, kf_vx, kf_vy = imm.correct_always(meas_cx, meas_cy, dt)
                used_meas = True
                no_meas = 0
                roi_miss = 0
                was_holding = False
            else:
                roi_miss += 1
                no_meas += 1
                if imm.inited and no_meas >= HOLD_AFTER_NO_MEAS:
                    kf_x, kf_y, kf_vx, kf_vy = imm.hold()
                    kf_vx = kf_vy = 0.0
                    was_holding = True
                elif imm.inited:
                    kf_x, kf_y, kf_vx, kf_vy = imm.hold()
                    was_holding = True

            if imm.inited:
                last_est = (kf_x, kf_y)

            blend_x, blend_y = kf_x, kf_y
            blend_src = "KF_ONLY"
            a_used = 0.0
            speed_px_s = math.hypot(kf_vx, kf_vy)
            meas_alpha = _adaptive_meas_alpha(speed_px_s)

            if used_meas and imm.inited:
                dx = float(meas_cx - kf_x)
                dy = float(meas_cy - kf_y)
                d = math.hypot(dx, dy)
                if d <= max(1.0, MEAS_GATE_PX):
                    trust = max(0.0, 1.0 - d / max(1.0, MEAS_GATE_PX))
                    a = clamp(meas_alpha * trust + (1 - trust) * (0.4 * meas_alpha), 0, 1)
                    blend_x = a * float(meas_cx) + (1 - a) * float(kf_x)
                    blend_y = a * float(meas_cy) + (1 - a) * float(kf_y)
                    blend_src = "BLEND"
                    a_used = a
                else:
                    blend_src = "KF_GATED"

            theta_rad = float("nan")
            theta_src = "NONE"
            step_px = 0.0
            inst_theta = None
            if speed_px_s >= MIN_SPEED_FOR_VEL_HEADING:
                inst_theta = math.atan2(kf_vy, kf_vx)
                theta_src = "KF_VEL"
            else:
                blend_buf.append((float(blend_x), float(blend_y)))
                max_len = max(2, MOTION_WINDOW + 1)
                if len(blend_buf) > max_len:
                    blend_buf = blend_buf[-max_len:]
                if len(blend_buf) >= 2:
                    k0 = max(0, len(blend_buf) - 1 - MOTION_WINDOW)
                    x0_, y0_ = blend_buf[k0]
                    x1, y1 = blend_buf[-1]
                    step_px = math.hypot(x1 - x0_, y1 - y0_)
                    if step_px >= max(0.1, MIN_STEP_DIST):
                        inst_theta = math.atan2(y1 - y0_, x1 - x0_)
                        theta_src = "MOTION"

            if inst_theta is not None:
                theta_rad = _ema_angle(prev_theta, inst_theta, _adaptive_alpha(speed_px_s))
                prev_theta = theta_rad
                last_good_theta = theta_rad
            elif last_good_theta is not None:
                theta_rad = last_good_theta
                theta_src = "HOLD"

            theta_deg = float(np.rad2deg(theta_rad)) if np.isfinite(theta_rad) else float("nan")
            dt_p = float(np.clip(dt, PREDICT_DT_MIN, PREDICT_DT_MAX)) if speed_px_s >= MIN_SPEED_FOR_PREDICT else 0.0
            ctrl_x = blend_x + kf_vx * dt_p
            ctrl_y = blend_y + kf_vy * dt_p

            e_lateral = float("nan")
            e_heading_deg = float("nan")
            path_heading = None
            proj_pt = None
            pp_target = None
            pp_alpha_deg = float("nan")
            pp_lookahead_px = 0.0
            pp_path_heading = None

            if path_cl is not None and imm.inited:
                try:
                    p = np.array([ctrl_x, ctrl_y], dtype=float)
                    (seg_i, path_heading, e_lateral, proj_pt), last_seg_i = _nearest_on_polyline(
                        p, path_cl, last_seg_i, NEAREST_WIN
                    )
                    if np.isfinite(theta_rad):
                        e_heading_deg = math.degrees(wrap_pi(theta_rad - path_heading))

                    pp_lookahead_px = _dynamic_lookahead_px(speed_px_s)
                    pp_target, _, pp_path_heading = _advance_along_path(path_cl, seg_i, proj_pt, pp_lookahead_px)

                    if pp_target is not None:
                        desired_heading = math.atan2(pp_target[1] - ctrl_y, pp_target[0] - ctrl_x)
                        ref_heading = theta_rad if np.isfinite(theta_rad) else path_heading
                        if ref_heading is not None and np.isfinite(ref_heading):
                            pp_alpha_deg = math.degrees(wrap_pi(desired_heading - ref_heading))
                except RuntimeError:
                    pass

            with _vision_lock:
                _vision.update(
                    t_ms=t_ms,
                    dt_ms=dt_ms,
                    mode=mode,
                    used_meas=used_meas,
                    holding=was_holding,
                    meas_cx=meas_cx,
                    meas_cy=meas_cy,
                    kf_x=kf_x,
                    kf_y=kf_y,
                    kf_vx=kf_vx,
                    kf_vy=kf_vy,
                    blend_x=blend_x,
                    blend_y=blend_y,
                    ctrl_x=ctrl_x,
                    ctrl_y=ctrl_y,
                    blend_src=blend_src,
                    blend_a=a_used,
                    speed_px_s=speed_px_s,
                    theta_deg=theta_deg,
                    theta_src=theta_src,
                    step_px=step_px,
                    mu_cv=imm.mu_cv,
                    mu_ct=imm.mu_ct,
                    omega_deg_s=imm.omega_deg_s,
                    e_lat=e_lateral,
                    e_head_deg=e_heading_deg,
                    pp_target_x=float(pp_target[0]) if pp_target is not None else float("nan"),
                    pp_target_y=float(pp_target[1]) if pp_target is not None else float("nan"),
                    pp_alpha_deg=pp_alpha_deg,
                    pp_lookahead_px=pp_lookahead_px,
                )

            out = frame.copy()
            _draw_centerline(out, path_cl)

            if path_cl is None:
                cv2.putText(out, "No centerline -- press M", (10, H - 20), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 80, 255), 2)

            if roi is not None:
                x0r, y0r, x1r, y1r = roi
                cv2.rectangle(out, (x0r, y0r), (x1r, y1r), (255, 255, 0), 2)
            if det is not None:
                x, y, w_, h_ = det["bbox"]
                cv2.rectangle(out, (x, y), (x + w_, y + h_), (0, 0, 255), 2)
                cv2.circle(out, (det["cx"], det["cy"]), 5, (0, 0, 255), -1)
            if imm.inited:
                cv2.circle(out, (int(round(kf_x)), int(round(kf_y))), 7, (0, 255, 0), 2)
                cv2.circle(out, (int(round(blend_x)), int(round(blend_y))), 5, (255, 255, 0), -1)
                if np.isfinite(theta_rad):
                    _draw_arrow(out, blend_x, blend_y, theta_rad, HEADING_LEN, (255, 0, 0), 4)
            if proj_pt is not None and path_heading is not None:
                cv2.circle(out, (int(round(proj_pt[0])), int(round(proj_pt[1]))), 6, (0, 255, 255), 2)
                _draw_arrow(out, proj_pt[0], proj_pt[1], path_heading, 60, (0, 255, 255), 3)
            if pp_target is not None:
                cv2.circle(out, (int(round(pp_target[0])), int(round(pp_target[1]))), 6, (255, 0, 255), -1)
                cv2.line(out, (int(round(ctrl_x)), int(round(ctrl_y))), (int(round(pp_target[0])), int(round(pp_target[1]))), (255, 0, 255), 2, cv2.LINE_AA)
                if pp_path_heading is not None and np.isfinite(pp_path_heading):
                    _draw_arrow(out, pp_target[0], pp_target[1], pp_path_heading, 45, (255, 0, 255), 2)

            log_lbl = "LOG:ON" if state["logging"] else "LOG:OFF"
            auto_lbl = "AUTO:ON" if state["auto_enabled"] else "AUTO:OFF"
            cv2.putText(
                out,
                f"{auto_lbl} | {state['status']} | dt={dt_ms:.1f}ms | {log_lbl} | LIT:{_LightingMode.current()} | flip:{int(state['flip_centerline'])}",
                (10, 30),
                cv2.FONT_HERSHEY_SIMPLEX,
                0.58,
                (255, 255, 255),
                2,
            )
            if math.isfinite(e_lateral):
                cv2.putText(
                    out,
                    f"e_lat={e_lateral:.1f}px  e_head={e_heading_deg:.1f}deg",
                    (10, 60),
                    cv2.FONT_HERSHEY_SIMPLEX,
                    0.63,
                    (0, 255, 255),
                    2,
                )
            if np.isfinite(theta_deg):
                cv2.putText(
                    out,
                    f"theta={theta_deg:.1f}  speed={speed_px_s:.0f}px/s  pp_alpha={pp_alpha_deg:.1f}  LA={pp_lookahead_px:.0f}  cmd_speed={state['v_ref_raw']:.0f}  cmd_delta={state['delta_cmd']:.1f}",
                    (10, 90),
                    cv2.FONT_HERSHEY_SIMPLEX,
                    0.57,
                    (255, 255, 0),
                    2,
                )

            cv2.imshow("Autonomous Controller [M=centerline | R=log | A=auto | V=mask | L=lighting | D=debug | Q=quit]", out)
            if show_mask:
                cv2.imshow("fg_mask", fg)

            key = cv2.waitKey(1) & 0xFF
            if key in (ord("q"), ord("Q")):
                _cv_stop.set()
                break
            if key in (ord("v"), ord("V")):
                show_mask = not show_mask
                if not show_mask:
                    cv2.destroyWindow("fg_mask")
            if key in (ord("m"), ord("M")):
                new_path = _build_centerline_from_frame(frame)
                if new_path is not None:
                    _new_centerline[0] = new_path
            if key in (ord("r"), ord("R")):
                if state["logging"]:
                    stop_logging()
                    print("[LOG] Stopped")
                else:
                    start_logging()
                    print(f"[LOG] Started -> {state['log_path']}")
            if key in (ord("a"), ord("A")):
                state["auto_enabled"] = not state["auto_enabled"]
                state["braking"] = not state["auto_enabled"]
                if not state["auto_enabled"]:
                    state["speed"] = 0.0
                    state["target_deg"] = 0.0
                    state["v_ref_raw"] = 0.0
                    state["delta_cmd"] = 0.0
                print(f"[AUTO] {'ON' if state['auto_enabled'] else 'OFF'}")
            if key in (ord("l"), ord("L")):
                _LightingMode.next()
            if key in (ord("d"), ord("D")):
                show_debug = not show_debug
                print(f"[CV] Debug overlay: {'ON' if show_debug else 'OFF'}")

    finally:
        cam.stop()
        cv2.destroyAllWindows()
        print("[CV] Stopped")
        _cv_stop.set()


# =============================================================================
# Main
# =============================================================================

async def main_async(args):
    global ble_client

    target_address = None
    resolved_device = None

    if args.discover:
        await _resolve_ble_target(args)
        return

    if args.no_ble:
        print("[BLE] Disabled -- running in no-Bluetooth mode")
        ble_client = DummyBleClient()
        try:
            await control_loop()
        finally:
            stop_logging()
        return

    if BleakClient is None:
        raise RuntimeError("bleak is not installed. Install it (pip install bleak) or run with --no-ble")

    target_address, resolved_device = await _resolve_ble_target(args)
    if target_address is None:
        return

    label = resolved_device.name if resolved_device is not None and resolved_device.name else target_address
    print(f"Connecting to {label} ({target_address}) ...")
    try:
        async with BleakClient(target_address) as client:
            ble_client = client
            if not client.is_connected:
                raise RuntimeError("Could not connect")

            print("[BLE] Connected!")
            await client.write_gatt_char(WRITE_UUID, IDLE_PACKET, response=False)
            try:
                await control_loop()
            finally:
                stop_logging()
                try:
                    await client.write_gatt_char(WRITE_UUID, BRAKE_PACKET, response=False)
                    await asyncio.sleep(0.2)
                    await client.write_gatt_char(WRITE_UUID, IDLE_PACKET, response=False)
                except Exception:
                    pass
    except Exception as exc:
        if BleakDeviceNotFoundError is not None and isinstance(exc, BleakDeviceNotFoundError):
            if sys.platform == "darwin":
                raise RuntimeError(
                    "BLE device was not found by macOS. Run with --discover to list nearby devices, then reconnect using the macOS UUID from the scan output or use --name <device-name>."
                ) from exc
        raise


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--address", default=None, help="BLE address/identifier of the car (macOS uses UUID, not MAC)")
    p.add_argument("--name", default=None, help="BLE device name to connect to by scanning first")
    p.add_argument("--discover", action="store_true", help="Scan and list nearby BLE devices, then exit")
    p.add_argument("--scan-timeout", type=float, default=6.0, help="BLE scan duration in seconds for --discover/--name")
    p.add_argument("--no-ble", action="store_true", help="Run vision/controller without Bluetooth")
    p.add_argument(
        "--auto-steer-sign",
        type=float,
        default=1.0,
        help="Use 1 or -1. Flip if car steers opposite to the intended autonomous command",
    )
    p.add_argument("--flip-centerline", action="store_true", help="Reverse centerline direction when loading/building it")
    p.add_argument("--max-deg", type=float, default=9.0)
    p.add_argument("--steer-smooth", type=float, default=42.0)
    p.add_argument("--ratio", type=float, default=0.80)
    p.add_argument("--c-sign", type=float, default=-1.0)
    p.add_argument("--speed-k", type=float, default=0.03)
    args = p.parse_args()

    if not args.no_ble and not args.discover and not args.address and not args.name:
        p.error("Provide --address, --name, or --discover to use BLE, or use --no-ble.")

    state["auto_steer_sign"] = float(args.auto_steer_sign)
    state["flip_centerline"] = bool(args.flip_centerline)
    state["max_deg"] = float(args.max_deg)
    state["steer_smooth"] = float(args.steer_smooth)
    state["ratio"] = float(args.ratio)
    state["c_sign"] = float(args.c_sign)
    state["speed_k"] = float(args.speed_k)
    state["speed_step"] = 90.0
    state["last_track_t"] = time.time()

    if args.discover:
        try:
            asyncio.run(main_async(args))
        except KeyboardInterrupt:
            print("\n[STOP]")
        return

    def _run_async_main(exc_queue):
        try:
            asyncio.run(main_async(args))
        except BaseException as exc:
            exc_queue.put(exc)
            _cv_stop.set()

    try:
        if sys.platform == "darwin":
            exc_queue = queue.Queue()
            worker = threading.Thread(target=_run_async_main, args=(exc_queue,), daemon=True)
            worker.start()
            try:
                _cv_thread()
            finally:
                _cv_stop.set()
                worker.join(timeout=2.0)
            if not exc_queue.empty():
                raise exc_queue.get()
        else:
            threading.Thread(target=_cv_thread, daemon=True).start()
            asyncio.run(main_async(args))
    except KeyboardInterrupt:
        _cv_stop.set()
        stop_logging()
        print("\n[STOP]")


if __name__ == "__main__":
    main()
