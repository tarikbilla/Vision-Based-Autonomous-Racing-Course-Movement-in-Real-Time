# API Reference

## Config Manager

### SystemConfig Structure
```cpp
struct SystemConfig {
    CameraConfig camera;      // Camera settings
    BLEConfig ble;           // BLE settings
    BoundaryConfig boundary; // Boundary detection settings
    TrackerConfig tracker;   // Tracker settings
    UIConfig ui;             // UI settings
};
```

### Usage
```cpp
#include "config_manager.hpp"

// Load configuration
SystemConfig config = ConfigManager::loadConfig("config/config.json");

// Access settings
int camera_width = config.camera.width;
std::string ble_mac = config.ble.device_mac;
```

## Camera Capture

### CameraCapture Class
```cpp
#include "camera_capture.hpp"

// Create camera
CameraCapture camera(0, 1920, 1080, 30);

// Open camera
camera.open();

// Get frame
Frame frame;
if (camera.getFrame(frame)) {
    cv::Mat image = frame.image;
    double timestamp = frame.timestamp;
}

// Close when done
camera.close();
```

### SimulatedCamera Class
```cpp
// For testing without hardware
SimulatedCamera sim_camera(1920, 1080, 30);
sim_camera.open();

Frame frame;
sim_camera.getFrame(frame);
```

## Object Tracker

### Tracker Interface
```cpp
#include "object_tracker.hpp"

// Create tracker
auto tracker = createTracker("CSRT", false);

// Initialize with ROI
cv::Rect roi(100, 100, 50, 50);
tracker->initialize(frame, &roi);

// Update tracking
TrackedObject tracked;
tracker->update(frame, tracked);

// Access results
cv::Point center = tracked.center;
cv::Point movement = tracked.movement;
cv::Rect bbox = tracked.bbox;
```

### Tracker Types
- `"CSRT"`: Discriminative correlation filters (recommended)
- `"KCF"`: Kernelized correlation filter
- `"GOTURN"`: Deep learning based (may require models)

## Boundary Detection

### BoundaryDetector Class
```cpp
#include "boundary_detection.hpp"

// Create detector
BoundaryDetector detector(
    50,                              // black_threshold
    {-45, 0, 45},                   // ray angles
    200,                             // ray_max_length
    100,                             // evasive_distance
    100,                             // default_speed
    50,                              // steering_limit
    true                             // light_on
);

// Detect road edges
auto [left, center, right, mask] = detector.detectRoadEdges(frame);

// Detect car in frame
cv::Rect car_bbox = detector.detectCarInFrame(frame, mask);

// Analyze and generate control
auto [rays, control] = detector.analyze(frame, car_center, car_movement);
```

### Ray Result
```cpp
struct RayResult {
    int angle_deg;   // Ray angle in degrees
    int distance;    // Distance to obstacle
};
```

## BLE Communication

### BLE Client Interface
```cpp
#include "ble_handler.hpp"

// Create client
BLETarget target{"f9:af:3c:e2:d2:f5", "uuid", "DRIFT"};
auto ble = createBLEClient(target, false);  // false = real BLE

// Connect
if (ble->connect()) {
    // Send commands
    ControlVector control(true, 100, 0, 20);
    ble->sendControl(control);
    
    // Disconnect when done
    ble->disconnect();
}
```

### Fake BLE (for testing)
```cpp
auto ble_fake = createBLEClient(target, true);  // true = simulation
ble_fake->connect();
// Works without hardware
```

## Commands

### ControlVector Structure
```cpp
struct ControlVector {
    bool light_on;              // Light on/off
    int32_t speed;              // Speed (0-255)
    int32_t right_turn_value;   // Right turn (0-255)
    int32_t left_turn_value;    // Left turn (0-255)
    
    // Constructor
    ControlVector(bool light, int32_t spd, int32_t right, int32_t left);
};
```

### Command Building
```cpp
#include "commands.hpp"

ControlVector control(true, 100, 20, 0);
std::vector<uint8_t> cmd = Commands::buildCommand(control);

// Utility functions
int32_t clamped = Commands::clamp(200, 0, 255);  // Returns 255
int32_t mapped = Commands::mapValue(5, 0, 10, 0, 100);  // Returns 50
```

## Control Orchestrator

### Orchestrator Usage
```cpp
#include "control_orchestrator.hpp"

// Create components
auto camera = std::make_unique<CameraCapture>(0, 1920, 1080, 30);
auto tracker = createTracker("CSRT", false);
auto boundary = std::make_unique<BoundaryDetector>(...);
auto ble = createBLEClient(target, false);

// Create orchestrator
OrchestratorOptions opts{true, 20, false};
auto orchestrator = std::make_unique<ControlOrchestrator>(
    std::move(camera),
    std::move(tracker),
    std::move(boundary),
    std::move(ble),
    opts
);

// Run
orchestrator->start();

// Later...
orchestrator->stop();
```

### Orchestrator Options
```cpp
struct OrchestratorOptions {
    bool showWindow;         // Show UI windows
    int commandRateHz;       // BLE command frequency
    bool colorTracking;      // Use color-based fallback
};
```

## Main Application

### Basic Template
```cpp
#include <memory>
#include "config_manager.hpp"
#include "camera_capture.hpp"
#include "object_tracker.hpp"
#include "boundary_detection.hpp"
#include "ble_handler.hpp"
#include "control_orchestrator.hpp"

int main() {
    try {
        // Load config
        SystemConfig config = ConfigManager::loadConfig("config/config.json");
        
        // Create components
        auto camera = std::make_unique<CameraCapture>(
            config.camera.index,
            config.camera.width,
            config.camera.height,
            config.camera.fps
        );
        
        auto tracker = createTracker(config.tracker.tracker_type, false);
        
        auto boundary = std::make_unique<BoundaryDetector>(
            config.boundary.black_threshold,
            {-45, 0, 45},
            config.boundary.ray_max_length,
            config.boundary.evasive_distance,
            config.boundary.default_speed,
            config.boundary.steering_limit,
            config.boundary.light_on
        );
        
        BLETarget ble_target{
            config.ble.device_mac,
            config.ble.characteristic_uuid,
            config.ble.device_identifier
        };
        auto ble = createBLEClient(ble_target, false);
        ble->connect();
        
        // Create orchestrator
        OrchestratorOptions opts{
            config.ui.show_window,
            config.ui.command_rate_hz,
            config.ui.color_tracking
        };
        auto orchestrator = std::make_unique<ControlOrchestrator>(
            std::move(camera),
            std::move(tracker),
            std::move(boundary),
            std::move(ble),
            opts
        );
        
        // Run
        orchestrator->start();
        
        // Keep running...
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
```

## Error Handling

### Exception Types
```cpp
try {
    camera.open();
} catch (const std::runtime_error& e) {
    std::cerr << "Runtime error: " << e.what() << "\n";
} catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << "\n";
}
```

### Common Errors
- `"Camera capture failed to open"`: Camera not found
- `"Invalid ROI selection"`: Bad ROI bounds
- `"Tracker initialization failed"`: Tracker creation issue
- `"Tracking lost"`: Object left frame or poor tracking

## Configuration JSON Schema

```json
{
  "camera": {
    "index": 0,
    "width": 1920,
    "height": 1080,
    "fps": 30
  },
  "ble": {
    "device_mac": "f9:af:3c:e2:d2:f5",
    "characteristic_uuid": "6e400002-b5a3-f393-e0a9-e50e24dcca9e",
    "device_identifier": "DRIFT",
    "connection_timeout_s": 10,
    "reconnect_attempts": 3
  },
  "boundary": {
    "black_threshold": 50,
    "ray_max_length": 200,
    "evasive_distance": 100,
    "default_speed": 100,
    "steering_limit": 50,
    "light_on": true
  },
  "tracker": {
    "tracker_type": "CSRT"
  },
  "ui": {
    "show_window": true,
    "command_rate_hz": 20,
    "color_tracking": false
  }
}
```

## Threading

### Thread Safety Guarantees
- Frame queue: Thread-safe with mutex + condition variable
- Control vector: Protected by mutex
- Latest frame: Protected by lock

### Example: Safe Access
```cpp
{
    std::lock_guard<std::mutex> lock(controlMutex_);
    ble_->sendControl(latestControl_);
}
```

## Performance Considerations

### Typical Performance
- Camera capture: 30 FPS
- Tracking analysis: 6-7 FPS (with 150ms interval)
- BLE command rate: 20 Hz
- End-to-end latency: <200ms

### Optimization Tips
1. Use Release build: `-DCMAKE_BUILD_TYPE=Release`
2. Lower camera resolution for Pi 4
3. Increase tracking analysis interval
4. Use -O3 optimization flag

## Building Custom Applications

### Template Structure
```cpp
#include "config_manager.hpp"
#include "camera_capture.hpp"
#include "object_tracker.hpp"
#include "boundary_detection.hpp"
#include "ble_handler.hpp"
#include "control_orchestrator.hpp"

// Your custom code here
```

### Key Integration Points
1. Configuration loading
2. Component creation
3. Orchestrator initialization
4. Run loop management
5. Cleanup on exit
