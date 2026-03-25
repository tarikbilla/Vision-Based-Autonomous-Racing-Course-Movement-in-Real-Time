# VisionRC\_FINAL — Complete Work Process

> **Project:** Vision-Based Autonomous RC Car Control System  
> **Language:** C++17  
> **Build System:** CMake 3.16+  
> **Platform:** macOS (Apple Silicon) / Linux  
> **Compiler Flags:** `-Wall -Wextra -O2`

---

## Phase 1: BLE Connection (Bluetooth Low Energy Communication)

**Technologies / Libraries / Protocols:**
- **SimpleBLE** — Cross-platform C++ BLE library (`simpleble::SimpleBLE`)
- **Bluetooth Low Energy (BLE) 4.0+** — Wireless communication protocol
- **GATT (Generic Attribute Profile)** — BLE data exchange protocol
- **Nordic UART Service (NUS)** — UUID `6e400002-b5a3-f393-e0a9-e50e24dcca9e` (Write Characteristic)
- **Custom Binary Packet Protocol** — 16-byte command packets with header, payload, and tail
- **C++ STL** — `std::unique_ptr`, `std::vector<uint8_t>`, `std::string`, `std::transform`
- **Abstract Factory Pattern** — `BleClient` interface with `SimpleBleClient` and `DummyBleClient` implementations
- **Conditional Compilation** — `#ifdef HAVE_SIMPLEBLE` for optional BLE support

**Source Files:** `ble_client.hpp`, `ble_client.cpp`, `types.hpp`

### Step 1: Bluetooth Adapter Discovery
- Query the system for available Bluetooth adapters using `SimpleBLE::Adapter::get_adapters()`
- Validate that at least one adapter exists; abort with error if none found
- Select the first available adapter for scanning

### Step 2: BLE Device Scanning
- Initiate a timed scan (`adapter.scan_for(timeout_ms)`) with configurable timeout (default 6 seconds)
- Retrieve all discovered peripherals via `adapter.scan_get_results()`
- Print first 5 discovered devices for debugging (name + MAC address)
- Support two scan modes: **by advertised name** (`--name`) or **by MAC address** (`--address`)

### Step 3: Device Matching
- **Name matching:** Case-insensitive substring search; also matches common DRiFT car name patterns (`DRiFT`, `DRIFT`, `DR!FT`)
- **Address matching:** Case-insensitive exact MAC comparison; also checks if the peripheral name contains the address (handles macOS UUID-style addresses)
- Return the first matching peripheral for connection

### Step 4: BLE Connection Establishment
- Call `peripheral.connect()` on the matched device
- Wait 1 second (`std::this_thread::sleep_for`) for the connection to stabilize
- Handle connection failures with exception catching

### Step 5: GATT Service & Characteristic Discovery
- Iterate through all services and characteristics on the connected peripheral
- Search for the target Write Characteristic UUID (`6e400002-b5a3-f393-e0a9-e50e24dcca9e`)
- Case-insensitive UUID comparison using `std::transform` to lowercase
- Fallback: if target UUID not found, use the first available characteristic

### Step 6: Command Packet Construction
- Build 16-byte binary packets using `build_packet(a, b, c)`:
  - **Header:** 6 bytes `{0xBF, 0x0A, 0x00, 0x08, 0x28, 0x00}`
  - **Payload:** 4 × signed 16-bit little-endian values (speed A, steering B, differential C, reserved 0)
  - **Tail:** 1 byte `0x4E`
- Pre-built special packets: `brake_packet()` (emergency stop) and `idle_packet()` (neutral)
- Steering conversion: `B_PER_DEG = 16000.0 / 90.0` (degrees → raw steering units)

### Step 7: GATT Write Transmission
- Primary method: `write_command()` — no-response write for lowest latency (~0.85 ms)
- Fallback method: `write_request()` — acknowledged write if the characteristic requires it
- Exception handling with automatic fallback between write modes

### Step 8: Disconnect & Cleanup
- Automatic disconnection in destructor (`~SimpleBleClient()`)
- On shutdown: send brake packet → wait 200 ms → send idle packet → disconnect
- Thread-safe via RAII (`std::unique_ptr` ownership)

---

## Phase 2: Camera Feed (Video Capture & Frame Acquisition)

**Technologies / Libraries / Algorithms:**
- **OpenCV 4.x** — `cv::VideoCapture`, `cv::CAP_PROP_*` settings
- **AVFoundation** — macOS camera backend (`cv::CAP_AVFOUNDATION`)
- **V4L2** — Linux camera backend (`cv::CAP_V4L2`)
- **DirectShow / MSMF** — Windows camera backends (`cv::CAP_DSHOW`, `cv::CAP_MSMF`)
- **MJPEG Codec** — Hardware-accelerated frame compression (`cv::VideoWriter::fourcc('M','J','P','G')`)
- **C++ Multithreading** — `std::thread`, `std::mutex`, `std::condition_variable`, `std::scoped_lock`
- **Producer–Consumer Pattern** — Dedicated grab thread feeds latest frame to processing thread

**Source Files:** `car_detector.hpp`, `car_detector.cpp`

### Step 1: Camera Backend Selection
- Detect operating system at compile time (`__APPLE__`, `_WIN32`, Linux default)
- Try platform-preferred backend first, then fall back to `cv::CAP_ANY`
- macOS: AVFoundation → ANY; Linux: V4L2 → ANY; Windows: DirectShow → MSMF → ANY

### Step 2: Camera Initialization & Configuration
- Open camera by index (default index 0) with `cv::VideoCapture::open()`
- Set frame resolution: **1280 × 720** (`FRAME_W`, `FRAME_H`)
- Set buffer size to 1 frame (`CAP_PROP_BUFFERSIZE = 1`) to minimize latency
- Set codec to MJPEG for hardware-accelerated capture
- Disable auto-exposure (`CAP_PROP_AUTO_EXPOSURE = 0.25`)
- Disable auto white-balance (`CAP_PROP_AUTO_WB = 0`)

### Step 3: Dedicated Camera Grab Thread (`CamThread`)
- Spawn a background thread (`reader()`) that continuously calls `cap_.read(frame)`
- Store only the **latest frame** (overwrite previous), protected by `std::mutex`
- Signal availability via `std::condition_variable::notify_one()`
- This "latest-frame" model ensures the processing thread always gets the most recent image, discarding stale frames

### Step 4: Frame Retrieval with Timeout
- Processing thread calls `read_latest(timeout_ms)` with a blocking wait
- Uses `std::condition_variable::wait_for()` with configurable timeout (500 ms default)
- Returns `std::optional<cv::Mat>` — `std::nullopt` if timeout or no frame available
- Frame is cloned (`latest_.clone()`) to decouple from the grab thread

### Step 5: Thread Lifecycle Management
- `start()`: Opens camera, spawns reader thread
- `stop()`: Sets `stopped_` flag, notifies all waiters, joins thread, releases camera
- Thread-safe shutdown via `std::scoped_lock` and atomic flags

---

## Phase 3: Boundary & Center Detection (Track Map Extraction)

**Technologies / Libraries / Algorithms:**
- **OpenCV 4.x** — `cv::cvtColor`, `cv::inRange`, `cv::findContours`, `cv::connectedComponentsWithStats`, `cv::convexHull`, `cv::fillConvexPoly`, `cv::floodFill`, `cv::morphologyEx`, `cv::bitwise_and/or/not`, `cv::threshold`
- **HSV Color Space Segmentation** — Red boundary detection (`H: 0–12, 168–179`; `S: 70–255`; `V: 50–255`), White surface detection (`H: 0–179`; `S: 0–70`; `V: 180–255`)
- **Convex Hull Algorithm** — `cv::convexHull()` for ROI extraction from red boundary pixels
- **Flood Fill Algorithm** — `cv::floodFill()` to separate inside vs. outside of the track
- **Morphological Operations** — Opening, closing, dilation with elliptical structuring elements (5×5, 7×7, 9×9)
- **Connected Components Analysis** — `cv::connectedComponentsWithStats()` for largest-region and hole-filling
- **Contour Hierarchy** — `cv::RETR_CCOMP` for outer/inner boundary detection
- **Arc-Length Resampling** — Uniform re-distribution of centerline points (1200 samples)
- **Cyclic Moving-Average Smoothing** — Window size 31 with wrap-around padding
- **CSV File I/O** — `std::ifstream`, `std::ofstream`, `std::filesystem` for centerline persistence

**Source Files:** `path_detector.hpp`, `path_detector.cpp`

### Step 1: Lighting Pre-processing
- Apply adaptive lighting correction before track extraction
- **CLAHE** (Contrast Limited Adaptive Histogram Equalization): `cv::createCLAHE(3.0, Size(8,8))` on L-channel of Lab color space (default mode: `AUTO_CLAHE`)
- Alternative modes: `EQUALIZE` (histogram equalization on L-channel), `GAMMA_BRIGHT` (γ=0.5), `GAMMA_DARK` (γ=1.8), `NONE`
- Convert BGR → Lab, process L-channel, convert back to BGR

### Step 2: Red Boundary Detection (HSV Segmentation)
- Convert the pre-processed frame to HSV color space
- Apply dual-range red detection:
  - Range 1: `H ∈ [0, 12], S ∈ [70, 255], V ∈ [50, 255]`
  - Range 2: `H ∈ [168, 179], S ∈ [70, 255], V ∈ [50, 255]`
- Combine both masks with `cv::bitwise_or` to handle red's hue wrap-around
- Validate minimum red pixel count (≥ 50 pixels) to ensure track is visible

### Step 3: ROI Extraction via Convex Hull
- Find all non-zero (red) pixel coordinates with `cv::findNonZero()`
- Compute the convex hull of red pixels with `cv::convexHull()`
- Create a binary ROI mask by filling the convex polygon (`cv::fillConvexPoly`)
- This constrains all subsequent processing to the track area only

### Step 4: White Surface Detection & Barrier Combination
- Detect white track surface: `H ∈ [0, 179], S ∈ [0, 70], V ∈ [180, 255]`
- Mask white pixels to the ROI region (`cv::bitwise_and(white, roi_mask)`)
- Combine red + white as barrier pixels: `cv::bitwise_or(red, white)`
- Clean barriers with morphological operations:
  - Opening (5×5 ellipse, 1 iteration) — remove small noise
  - Closing (7×7 ellipse, 3 iterations) — fill barrier gaps
  - Dilation (9×9 ellipse, 1 iteration) — thicken barriers

### Step 5: Free-Space Extraction via Flood Fill
- Construct wall mask = barriers ∪ inverse-ROI (everything outside track)
- Flood-fill from corner `(0,0)` with value 128 to find the outside region
- Compute: `free_space = NOT(outside) AND NOT(walls)`
- Result: binary mask where white = drivable track surface

### Step 6: Track Mask Cleaning
- Threshold to binary and keep only the **largest connected component** (`connectedComponentsWithStats`)
- Morphological closing (31×31 ellipse, 1 iteration) to merge fragmented track regions
- Fill interior holes up to 5000 px area using connected components on the inverted mask
- Morphological opening (7×7 ellipse, 1 iteration) to smooth edges
- Keep largest component again to ensure single contiguous track

### Step 7: Outer & Inner Contour Extraction
- Find contours with `cv::RETR_CCOMP` (2-level hierarchy: outer + inner)
- Identify the **outer contour**: largest contour with no parent (`hierarchy[i][3] == -1`)
- Identify the **inner contour**: largest child contour of the outer contour
- Fallback: if no proper child, use the second-largest contour overall

### Step 8: Centerline Computation (Midpoint Method)
- Resample the outer contour to exactly **1200 uniformly-spaced points** using arc-length parameterization
- For each outer point, find the **nearest point on the inner contour** (brute-force nearest-neighbor)
- Compute the midpoint: `center = (outer_point + nearest_inner_point) / 2`
- Result: 1200 centerline points representing the middle of the track

### Step 9: Centerline Smoothing & Closure
- Apply **cyclic moving-average smoothing** with window size 31:
  - Pad the array cyclically (wrap head/tail by half-window)
  - Compute windowed average for each point
- Close the loop: append the first point to the end (`ensure_closed()`)
- Result: smooth, closed-loop centerline polyline

### Step 10: Centerline Persistence
- Save to `centerline.csv` with header `# W=<width> H=<height>` and column names `x,y`
- Load on startup if file exists; support `--flip-centerline` to reverse traversal direction
- Runtime rebuild: press `M` key to re-extract centerline from the current camera frame

---

## Phase 4: Car Detection (Moving Object Detection & Tracking)

**Technologies / Libraries / Algorithms:**
- **MOG2 (Mixture of Gaussians v2)** — `cv::createBackgroundSubtractorMOG2()` for foreground/background separation
- **Binary Thresholding** — `cv::threshold()` at value 200 to clean the foreground mask
- **Morphological Operations** — Opening (5×5, 1 iter) + Closing (7×7, 2 iter) with elliptical kernels
- **Contour Detection** — `cv::findContours()` with `cv::RETR_EXTERNAL` and `cv::CHAIN_APPROX_SIMPLE`
- **Bounding Rectangle** — `cv::boundingRect()` for object bounding box
- **Contour Area Filtering** — Min area 250 px², max area 250,000 px²
- **Motion Pixel Counting** — `cv::countNonZero()` within bounding box (minimum 120 motion pixels required)
- **ROI (Region of Interest)** — Adaptive 360×360 px tracking window centered on last known position
- **Global/ROI Search Strategy** — Alternating between full-frame and local search

**Source Files:** `car_detector.hpp`, `car_detector.cpp`

### Step 1: MOG2 Background Model Initialization
- Create background subtractor: `cv::createBackgroundSubtractorMOG2(history=500, varThreshold=16.0, detectShadows=false)`
- History of 500 frames for robust background model adaptation
- Shadow detection disabled for performance (no gray shadow regions in mask)
- 2-second warmup period (discarded) to let MOG2 learn the static background

### Step 2: Foreground Mask Extraction
- Apply MOG2 to each lighting-preprocessed frame: `backSub->apply(work, fg_raw)`
- Produces a grayscale mask: 255 = definite foreground, 0 = background

### Step 3: Mask Cleaning (Morphological Post-processing)
- Binary threshold at 200: remove uncertain pixels (gray values from partial background)
- Morphological opening (5×5 ellipse, 1 iteration): remove small noise blobs
- Morphological closing (7×7 ellipse, 2 iterations): fill holes inside the car silhouette
- Result: clean binary mask of moving foreground objects

### Step 4: ROI Management (Search Area Selection)
- **Global search:** Full-frame contour search — used when:
  - Tracker not yet initialized
  - Too many consecutive ROI misses (≥ 10 frames)
  - Every 8th frame (`GLOBAL_EVERY`) as a periodic refresh
- **ROI search:** 360×360 px window centered on last tracked position (±180 px + 20 px margin)
- ROI is clamped to frame boundaries via `clamp_roi()`

### Step 5: Contour Detection & Filtering
- Find all external contours in the search region (`cv::findContours`, `RETR_EXTERNAL`)
- Filter by area: reject contours < 250 px² or > 250,000 px²
- Filter by bounding box dimensions: reject if width or height < 10 px or > 900 px
- Count motion pixels within bounding box; reject if < 120 (`MIN_MOTION_PX`)

### Step 6: Best Contour Selection
- Select the contour with the **largest area** that passes all filters
- Extract detection result: bounding box, centroid (cx, cy), area, and motion pixel count
- Return as `std::optional<Det>` — empty if no valid detection

---

## Phase 5: IMM Kalman Tracking (State Estimation & Motion Prediction)

**Technologies / Libraries / Algorithms:**
- **Interacting Multiple Model (IMM) Filter** — Two-model Bayesian state estimator
- **Kalman Filter (KF)** — Predict-correct cycle for each motion model
- **Constant Velocity (CV) Model** — 4-state linear Kalman: `[x, y, vx, vy]`
- **Coordinated Turn (CT) Model** — 5-state nonlinear Kalman: `[x, y, vx, vy, ω]` (angular velocity)
- **OpenCV Matrix Operations** — `cv::Mat` for matrix algebra (state, covariance, Kalman gain)
- **Mahalanobis Distance** — Innovation likelihood for model probability update
- **Markov Transition Matrix** — Mode switching probabilities (`P_stay_CV = 0.97`, `P_stay_CT = 0.92`)
- **Exponential Moving Average (EMA)** — Adaptive heading smoothing with speed-dependent alpha
- **Measurement Gating** — 70 px gate radius for outlier rejection
- **Adaptive Blending** — Speed-dependent trust between raw measurement and Kalman estimate

**Source Files:** `imm_tracker.hpp`, `imm_tracker.cpp`

### Step 1: IMM Tracker Initialization
- Initialize on first valid detection: `init_at(x, y)` sets position, zero velocity
- CV model: 4×4 state, initial covariance P = 200·I₄
- CT model: 5×5 state, initial covariance P = 200·I₅, ω variance = 0.01
- Initial mode probabilities: `μ_CV = 0.8, μ_CT = 0.2` (favor straight-line motion)

### Step 2: IMM Mixing (Mode Interaction)
- Compute mixing probabilities using Markov transition matrix `Π`:
  - `Π = [[0.97, 0.03], [0.08, 0.92]]`
  - `c_j = Σ_i π_ij · μ_i` (normalization constants)
  - `μ_{i|j} = π_ij · μ_i / c_j` (conditional mixing weights)
- Compute mixed state and covariance for each model:
  - `x̂₀_j = Σ_i μ_{i|j} · x̂_i` (mixed state)
  - `P̂₀_j = Σ_i μ_{i|j} · (P_i + (x̂_i - x̂₀_j)(x̂_i - x̂₀_j)ᵀ)` (mixed covariance)
- Promote CV state (4D) to 5D by appending last known ω for mixing

### Step 3: CV Model Prediction & Update
- **Prediction:**
  - State transition: `F_CV(dt) = [[1,0,dt,0], [0,1,0,dt], [0,0,1,0], [0,0,0,1]]`
  - Process noise: `Q_CV(dt)` with acceleration σ = 200 px/s²
  - `x⁻ = F·x̂, P⁻ = F·P̂·Fᵀ + Q`
- **Update (Kalman correction):**
  - Measurement matrix: `H = [[1,0,0,0], [0,1,0,0]]` (observe position only)
  - Innovation: `ỹ = z - H·x⁻`
  - Kalman gain: `K = P⁻·Hᵀ·(H·P⁻·Hᵀ + R)⁻¹`, R = 8² · I₂ (measurement noise σ = 8 px)
  - Joseph form covariance update: `P = (I-KH)·P⁻·(I-KH)ᵀ + K·R·Kᵀ`
- Compute model likelihood using Mahalanobis distance on innovation

### Step 4: CT Model Prediction & Update
- **Prediction:**
  - State transition `F_CT(dt, ω)` uses coordinated-turn kinematics:
    - If |ω| < 10⁻⁴: degenerates to linear (same as CV + ω state)
    - Otherwise: `sin(ωdt)/ω`, `(1-cos(ωdt))/ω` terms for circular motion
  - Clamp ω to `[-6, 6]` rad/s (`CT_OMEGA_MAX`)
  - Process noise: σ_acc = 30 px/s², σ_ω = 0.3 rad/s
- **Update:** Same Kalman correction as CV but with 5×5 matrices and `H = [[1,0,0,0,0], [0,1,0,0,0]]`
- Compute model likelihood via Mahalanobis distance

### Step 5: Mode Probability Update
- Multiply prior mode probabilities by model likelihoods: `μ_j ∝ c_j · Λ_j`
- Normalize: `μ_j = μ_j / Σ μ_j`
- If total probability collapses (< 10⁻³⁰⁰), reset to initial priors

### Step 6: State Fusion (Output Estimate)
- Weighted combination of both models:
  - `x̂_fused = μ_CV · x̂_CV + μ_CT · x̂_CT` (promote CV to 5D for mixing)
- Output: `[x, y, vx, vy]` — fused position and velocity estimate

### Step 7: Measurement Gating & Adaptive Blending
- Gate radius: 70 px — if detection is farther from Kalman estimate, ignore measurement (`KF_GATED`)
- Trust factor: `trust = max(0, 1 - distance/gate)` — linearly decays with distance
- Adaptive alpha (speed-dependent):
  - Slow (< 450 px/s): α = 0.65 (trust Kalman more)
  - Fast (≥ 450 px/s): α = 0.88 (trust measurement more)
- Blend: `position = α·measurement + (1-α)·Kalman`

### Step 8: Heading Estimation
- **Primary (speed ≥ 8 px/s):** Velocity-based heading from Kalman: `θ = atan2(vy, vx)` (`KF_VEL`)
- **Secondary (slow/stationary):** Motion-window heading from 2-frame displacement (`MOTION`)
- **Fallback:** Hold last known good heading (`HOLD`)
- Apply EMA angle smoothing (circular averaging via cos/sin decomposition) with adaptive alpha

### Step 9: Hold Mode (No Detection)
- When no valid detection: output current fused state without correction
- After 3 consecutive misses (`HOLD_NO_MEAS`): zero out velocity to prevent drift
- Optionally re-initialize tracker when detection resumes after hold (`RESET_VEL_HOLD`)

---

## Phase 6: Path Geometry & Pure Pursuit (Autonomous Steering Control)

**Technologies / Libraries / Algorithms:**
- **Pure Pursuit Path Tracking Algorithm** — Geometric path-following via lookahead point
- **Dynamic Lookahead Distance** — Speed-dependent: `L = clamp(88 + 0.78·speed, 88, 170)` px
- **Nearest-Point-on-Polyline** — Windowed search (±40 segments) with projection and cross-product lateral error
- **Arc-Length Advancement** — Walk along polyline by distance to find lookahead target
- **PD Controller (Proportional-Derivative)** — Alpha + lateral error + derivative terms
- **Soft Saturation (tanh)** — `δ = 12.5° · tanh(δ_raw / 12.5°)` for smooth steering limits
- **First-Order Low-Pass Filter** — `y[n] = (1-α)·y[n-1] + α·target` for smoothing (α = 0.42)
- **Rate Limiter** — Maximum steering change per tick: ±1.10°
- **Delay Compensation** — Predictive state: `x_pred = x + 0.20s · ẋ_rate`
- **3-Tier Speed Scheduling** — MIN/MID/MAX speed based on error thresholds
- **Deadband** — Alpha deadband: 4.5°; lateral error deadband: 10 px
- **Straight-Line Attenuation** — Reduce steering by 62% when near-zero errors (`NEAR_STRAIGHT = 0.38`)

**Source Files:** `controller.hpp`, `controller.cpp`, `path_detector.hpp`, `path_detector.cpp`

### Step 1: Nearest Point Projection
- Given car position `(ctrl_x, ctrl_y)`, search the centerline polyline for the nearest segment
- Use windowed search of ±40 segments around the last known segment index (`NEAREST_WIN`)
- For each segment `[A, B]`: project the car position onto the line, clamp `t ∈ [0, 1]`
- Compute **lateral error** (`e_lat`): signed perpendicular distance using cross-product
- Compute **path heading**: `atan2(B.y - A.y, B.x - A.x)` at the nearest segment

### Step 2: Heading Error Computation
- `e_head = wrap_pi(θ_car - θ_path)` — angular difference between car heading and path tangent
- `wrap_pi()` normalizes angle to `[-π, π]` range
- Convert to degrees for the controller

### Step 3: Dynamic Lookahead Point
- Compute lookahead distance: `L = clamp(88 + 0.78 · speed_px_s, 88, 170)` pixels
- Walk forward along the polyline from the projection point by distance `L` (`advance_along_path`)
- Handle cyclic wrap-around for closed-loop tracks
- Return lookahead point coordinates and path heading at that point

### Step 4: Alpha Angle Computation (Pure Pursuit)
- Compute desired heading: `desired = atan2(target.y - car.y, target.x - car.x)`
- Reference heading: car's velocity heading (or path heading as fallback)
- `α = wrap_pi(desired - reference)` — the pure-pursuit alpha angle (in degrees)

### Step 5: Delay Compensation (Predictive Filtering)
- Compute filtered error rates:
  - `e_lat_rate = (e_lat_filt[n] - e_lat_filt[n-1]) / dt`
  - `α_rate = (α_filt[n] - α_filt[n-1]) / dt`
- Apply first-order filter on rates: `rate_filt = (1-α)·prev + α·rate`
  - α_rate smoothing: 0.30; e_lat_rate smoothing: 0.22
- Predict forward by 200 ms: `e_lat_pred = e_lat_filt + 0.20·e_lat_rate_filt`

### Step 6: PD Steering Command Computation
- **P-terms:**
  - Alpha term: `AUTO_PP_K_ALPHA (0.15) × α_predicted`
  - Lateral term: `atan2(AUTO_PP_K_LAT (0.014) × e_lat_predicted, speed + 110)` in degrees
- **D-terms:**
  - `D = clamp(0.055·α_rate + 0.018·e_lat_rate, -3.0, 3.0)`
- Raw steering: `δ_raw = -(alpha_term + lat_term + D_term)`
- Apply steering sign: multiply by `auto_steer_sign` (configurable for track direction)

### Step 7: Soft Saturation & Straight-Line Attenuation
- Soft saturation: `δ = 12.5° · tanh(δ_raw / 12.5°)` — smooth limit without hard clipping
- Straight-line detection: if `|α_pred| < 7°` AND `|e_lat_pred| < 9 px`:
  - Reduce steering by 62%: `δ *= 0.38` (prevents oscillation on straights)
  - Status: `STRAIGHT`

### Step 8: Output Filtering & Rate Limiting
- First-order filter: `δ_filt = 0.58·δ_prev + 0.42·δ_raw` (`AUTO_PP_FILTER = 0.42`)
- Rate limiter: `Δδ ≤ ±1.10°` per control tick (`AUTO_PP_RATE_DEG`)
- Final clamp: `|δ| ≤ max_deg` (configurable, default 9°, typically set to 24°)

### Step 9: 3-Tier Speed Scheduling
- **MIN speed** (3000 raw): `|α| > 22°` OR `|e_lat| > 44 px` OR `|δ| > 7°`
- **MID speed** (3600 raw): `|α| > 9°` OR `|e_lat| > 18 px` OR `|δ| > 3°`
- **MAX speed** (8200 raw): all errors below thresholds
- Speed transitions: step limiter of 90 units per control tick at 20 Hz

### Step 10: Speed Drop for Steering
- When steering, reduce speed proportionally: `drop = speed_k × |B_steering|`
- If `|speed_raw| > drop`: `a_sent = sign(speed) × (|speed| - drop)`
- Configurable via `--speed-k` (default 0.03; higher = more speed reduction in turns)

---

## Phase 7: Control Loop & BLE Transmission (Real-Time Actuation)

**Technologies / Libraries / Algorithms:**
- **Fixed-Rate Control Loop** — 20 Hz (`CONTROL_HZ = 20.0`), period = 50 ms
- **C++ High-Resolution Clock** — `std::chrono::steady_clock` for precise timing
- **Thread Synchronization** — `std::scoped_lock`, `std::mutex` for shared state access
- **Exponential Steering Smoothing** — Gradual steering angle transition per tick
- **CSV Data Logging** — Real-time telemetry logging with 50+ fields per row
- **Signal Handling** — `SIGINT`/`SIGTERM` for clean shutdown (`std::signal`)
- **POSIX Time** — `localtime_r` for timestamp generation

**Source Files:** `controller.hpp`, `controller.cpp`, `main.cpp`

### Step 1: Control Loop Initialization
- Launch control loop on a dedicated thread: `std::thread(vrc::control_loop, g_ble.get())`
- Vision/detection thread runs on main thread (required for OpenCV `imshow` GUI)
- Both threads communicate via shared `g_state` and `g_vision` structs (mutex-protected)

### Step 2: Autonomy Step Execution (every 50 ms)
- Read latest `VisionData` from the vision thread
- Check track availability: if `e_lat` and `e_head_deg` are finite → track found
- If no track for > 600 ms (`AUTO_BRAKE_NO_TRACK_MS`): emergency brake
- If track recently lost (< 600 ms): hold mode with minimum speed
- If track found: run full PD pure-pursuit steering computation (Phase 6)

### Step 3: Steering Smoothing
- Gradual transition: `turn_deg += copysign(step, target - turn_deg)`
- Step size = `steer_smooth × period` (e.g., 70 × 0.05 = 3.5° per tick)
- If difference ≤ step: snap to target (prevents overshoot)

### Step 4: Packet Assembly & Transmission
- Compute A, B, C values: `compute_abc()` returns speed (A), steering (B), differential (C)
- B = `turn_deg × 16000/90` (degrees → raw units)
- C = `c_sign × ratio × B` (differential steering, typically `ratio = 0.9–1.0`)
- If braking: send pre-built `brake_packet()`
- Otherwise: build and send custom packet via `ble->write_gatt_char()`

### Step 5: Timing Precision
- Measure elapsed time per iteration with `std::chrono::steady_clock`
- Sleep for remaining time: `sleep_for(max(0, period - elapsed))`
- Achieves consistent 20 Hz rate with < 1 ms jitter

### Step 6: CSV Telemetry Logging
- Toggle with `R` key at runtime
- File: `auto_collect_YYYYMMDD_HHMMSS.csv`
- Logs 50+ fields per row: timestamp, A/B/C sent, speed, steering, all vision data, Kalman states, errors, mode
- Flush after every row for crash-safe logging

---

## Phase 8: Visualization & User Interface (Real-Time Debug Display)

**Technologies / Libraries / Algorithms:**
- **OpenCV HighGUI** — `cv::imshow()`, `cv::waitKey()`, `cv::destroyAllWindows()`
- **OpenCV Drawing** — `cv::circle()`, `cv::rectangle()`, `cv::arrowedLine()`, `cv::polylines()`, `cv::putText()`, `cv::line()`
- **Keyboard Event Handling** — `cv::waitKey(1)` for non-blocking key capture

**Source Files:** `controller.cpp`, `path_detector.cpp`

### Step 1: Main Display Window
- Show the raw camera frame with all overlays
- Window title includes keyboard shortcut reference
- Non-blocking display: `cv::waitKey(1)` returns immediately

### Step 2: Centerline Overlay
- Draw the closed-loop centerline polyline in green (`cv::polylines`, color `(0, 200, 60)`, thickness 2)
- If no centerline loaded: display "No centerline -- press M" warning text

### Step 3: Detection Overlays
- **ROI box:** Yellow rectangle (`cv::rectangle`, color `(255, 255, 0)`)
- **Detection bounding box:** Red rectangle (`cv::rectangle`, color `(0, 0, 255)`)
- **Detection centroid:** Red filled circle, radius 5 px
- **Kalman estimate:** Green circle, radius 7 px
- **Blended position:** Yellow filled circle, radius 5 px

### Step 4: Heading & Path Overlays
- **Car heading arrow:** Blue arrowed line, 90 px length (`HEADING_LEN`) from blend position
- **Path projection point:** Cyan circle, radius 6 px + cyan heading arrow (60 px)
- **Pure pursuit target:** Magenta filled circle + magenta line from car to target + magenta heading arrow (45 px)

### Step 5: Status Text (HUD)
- Line 1: Auto status, mode, frame dt, logging status, lighting mode, flip state
- Line 2: Lateral error (px) and heading error (deg)
- Line 3: Theta, speed (px/s), PP alpha, lookahead, commanded speed, commanded delta

### Step 6: Keyboard Controls
- `Q` — Quit (sets `g_stop = true`)
- `A` — Toggle autonomous mode ON/OFF (brakes when OFF)
- `M` — Rebuild centerline from current camera frame
- `R` — Start/stop CSV telemetry logging
- `V` — Show/hide foreground mask window (`fg_mask`)
- `L` — Cycle lighting preprocessing mode (AUTO_CLAHE → NONE → EQUALIZE → GAMMA_BRIGHT → GAMMA_DARK)

---

## Phase 9: System Architecture & Multi-Threading

**Technologies / Libraries / Algorithms:**
- **C++17 Standard Library** — `std::thread`, `std::mutex`, `std::scoped_lock`, `std::atomic`, `std::optional`, `std::condition_variable`, `std::chrono`
- **CMake 3.16+** — Cross-platform build system with optional dependency detection
- **POSIX Signals** — `SIGINT`, `SIGTERM` handlers for graceful shutdown
- **Shared Global State** — `g_state`, `g_vision`, `g_new_centerline` with mutex protection
- **Command-Line Argument Parsing** — Custom parser supporting 13+ flags

**Source Files:** `main.cpp`, `types.hpp`

### Step 1: Argument Parsing & Configuration
- Parse 13+ command-line arguments: `--name`, `--address`, `--discover`, `--no-ble`, `--scan-timeout`, `--max-deg`, `--steer-smooth`, `--ratio`, `--c-sign`, `--speed-k`, `--auto-steer-sign`, `--flip-centerline`, `--ble-backend`
- Validate: at least one of `--name`, `--address`, `--discover`, or `--no-ble` must be specified
- Apply tuning parameters to global `g_state` struct

### Step 2: Thread Architecture
- **Main thread:** Vision processing + OpenCV GUI (`cv_thread()`)
- **Control thread:** 20 Hz BLE control loop (`control_loop()`)
- **Camera thread:** Dedicated frame grabber (`CamThread::reader()`)
- Total: 3 threads running concurrently

### Step 3: Shared State Communication
- `g_state` (mutex-protected): speed, steering, autonomy status, tuning parameters
- `g_vision` (mutex-protected): detection data, Kalman estimates, errors, timestamps
- `g_new_centerline` (mutex-protected): hot-reload centerline from vision thread to control
- `g_stop` (`std::atomic<bool>`): shutdown signal across all threads

### Step 4: Graceful Shutdown Sequence
- Signal handler catches `SIGINT`/`SIGTERM` → sets `g_stop = true`
- All threads check `g_stop` and exit their loops
- Control thread: joined via `ctl_thread.join()`
- Camera thread: stopped via `cam.stop()` (joins reader thread)
- Final cleanup: send brake → 200 ms wait → send idle → print `[STOP]`

---

## Phase 10: Benchmarking & Performance Measurement

**Technologies / Libraries / Algorithms:**
- **High-Resolution Timing** — `std::chrono::high_resolution_clock` for microsecond-precision per-phase timing
- **Percentile Statistics** — p50, p95, p99 latency analysis
- **CSV Export** — Per-frame timing data with 12+ columns
- **OpenCV VideoCapture** — Camera benchmarking at native resolution (1920×1080)

**Source Files:** `benchmark.cpp`, `PERFORMANCE_REPORT.md`

### Step 1: Benchmark Setup
- Headless mode: no display window, no BLE connection
- Configurable: `--frames` (default 300), `--warmup` (default 2s), `--camera` (default 0)
- Load existing `centerline.csv` for path geometry benchmarking

### Step 2: MOG2 Warmup
- Run MOG2 on live frames for 2 seconds (discarded) to build a stable background model
- Prevents initial frames from having artificially high processing times

### Step 3: Per-Phase Timing
- Five timed stages per frame, measured with `high_resolution_clock`:
  1. **Camera grab** — USB transfer latency (~11.6 ms mean)
  2. **MOG2 background subtraction** — Dominant cost (~8.6 ms mean, 84.9% of processing)
  3. **Contour detection** — ~0.2 ms mean
  4. **IMM Kalman update** — ~0.008 ms mean
  5. **Path geometry** — < 0.001 ms mean

### Step 4: Results Output
- `benchmark_frames_<timestamp>.csv`: per-frame detailed timing
- `benchmark_summary_<timestamp>.txt`: aggregate statistics (min, max, mean, median, p95, p99, stddev)
- **Measured performance:** 45.46 fps at 1920×1080, mean processing latency 10.15 ms, estimated end-to-end latency ~60 ms

---

## Summary: Complete Technology Stack

| Category | Technologies |
|----------|-------------|
| **Language** | C++17 |
| **Build System** | CMake 3.16+, Bash (build.sh) |
| **Computer Vision** | OpenCV 4.x (MOG2, HSV segmentation, contours, morphology, CLAHE, connected components, convex hull, flood fill, drawing, HighGUI) |
| **BLE Communication** | SimpleBLE, GATT protocol, Nordic UART Service (NUS), custom binary packet protocol |
| **State Estimation** | IMM (Interacting Multiple Model) filter, Kalman Filter (CV + CT models), Mahalanobis gating |
| **Path Tracking** | Pure Pursuit algorithm, nearest-point-on-polyline, arc-length path advancement, dynamic lookahead |
| **Control** | PD controller, soft saturation (tanh), rate limiter, first-order low-pass filter, delay compensation, 3-tier speed scheduling |
| **Concurrency** | std::thread, std::mutex, std::atomic, std::condition_variable, producer-consumer pattern |
| **Signal Processing** | EMA (exponential moving average), adaptive alpha, deadband, cyclic smoothing |
| **Camera** | AVFoundation (macOS), V4L2 (Linux), MJPEG codec, dedicated grab thread |
| **Data Logging** | CSV telemetry (50+ fields), filesystem I/O, POSIX timestamps |
| **Platform** | macOS (Apple Silicon), Linux (Ubuntu/Debian), cross-platform via CMake |
