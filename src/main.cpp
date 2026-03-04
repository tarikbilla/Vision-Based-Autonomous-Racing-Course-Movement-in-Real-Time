#include <iostream>
#include <memory>
#include <string>
#include <csignal>

#include "config_manager.hpp"
#include "camera_capture.hpp"
#include "object_tracker.hpp"
#include "boundary_detection.hpp"
#include "ble_handler.hpp"
#include "control_orchestrator.hpp"

// Global orchestrator for signal handling
std::unique_ptr<ControlOrchestrator> g_orchestrator;

void signalHandler(int signal) {
    std::cout << "\n[*] Received signal " << signal << ". Shutting down...\n";
    if (g_orchestrator) {
        // Send STOP command before disconnecting BLE
        try {
            // Access BLE client from orchestrator
            // This assumes orchestrator exposes a method to send STOP
            ControlVector stop_control(false, 0, 0, 0);
            // Try to send STOP via BLE
            // If orchestrator exposes ble_ as public or via getter, use it
            // Otherwise, add a method to orchestrator to send STOP
            g_orchestrator->sendStopAndDisconnect();
        } catch (...) {
            std::cerr << "[!] Failed to send STOP command during shutdown\n";
        }
        g_orchestrator->stop();
    }
    exit(0);
}

int main(int argc, char* argv[]) {
    try {
        std::cout << "========================================\n"
                 << "Vision-Based Autonomous RC Car Control\n"
                 << "========================================\n\n";
        
        // Parse command line arguments
        std::string configPath = "config/config.json";
        bool simulate = false;
        std::string deviceMac;
        bool lowResource = false;
        
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--config" && i + 1 < argc) {
                configPath = argv[++i];
            } else if (arg == "--simulate") {
                simulate = true;
            } else if (arg == "--device" && i + 1 < argc) {
                deviceMac = argv[++i];
            } else if (arg == "--rpi" || arg == "--low-resource") {
                lowResource = true;
                configPath = "config/config_rpi4.json";
            } else if (arg == "--help") {
                std::cout << "Usage: " << argv[0] << " [options]\n"
                         << "Options:\n"
                         << "  --config <path>      Path to config file (default: config/config.json)\n"
                         << "  --simulate           Run in simulation mode\n"
                         << "  --device <mac>       BLE device MAC address\n"
                         << "  --rpi                Raspberry Pi mode (low resolution, optimized)\n"
                         << "  --low-resource       Low resource mode (same as --rpi)\n"
                         << "  --help               Show this help message\n";
                return 0;
            }
        }
        
        if (lowResource) {
            std::cout << "[*] Low-resource mode enabled (Raspberry Pi optimized)\n";
        }
        
        // Load configuration
        std::cout << "[*] Loading configuration from: " << configPath << "\n";
        SystemConfig config = ConfigManager::loadConfig(configPath);
        std::cout << "[✓] Configuration loaded successfully\n\n";
        
        // Setup signal handlers
        signal(SIGINT, signalHandler);
        signal(SIGTERM, signalHandler);
        
        // Initialize BLE client first (match Python ordering: connect before camera)
        std::cout << "[*] Initializing BLE...\n";
        BLETarget target{
            config.ble.device_mac,
            config.ble.characteristic_uuid,
            config.ble.device_identifier
        };
        auto ble = createBLEClient(target, simulate, deviceMac);
        
        // Stop command for emergency/shutdown
        ControlVector stop_control(false, 0, 0, 0);
        
        // Attempt to connect to BLE device (before opening the camera)
        if (!simulate) {
            std::cout << "[1/3] Connecting to BLE car...\n";

            bool connected = false;
            for (int attempt = 1; attempt <= config.ble.reconnect_attempts; ++attempt) {
                try {
                    if (ble->connect()) {
                        std::cout << "[✓] BLE car connected!\n";

                        // Send connection pulse like Python
                        std::cout << "[*] Sending short START pulse to confirm connection...\n";
                        ControlVector pulse_control(
                            true,  // light_on
                            std::max(5, config.boundary.default_speed),  // speed
                            0,  // right_turn_value
                            0   // left_turn_value
                        );
                        ble->sendControl(pulse_control);
                        std::this_thread::sleep_for(std::chrono::seconds(1));
                        ble->sendControl(stop_control);
                        std::cout << "[✓] Connection pulse complete.\n";

                        // After connecting, run the car forward for 1 second to verify motion
                        std::cout << "[*] Running car forward for 1 second to verify motion...\n";
                        ControlVector run_control(
                            true, // light_on (turn lights on during run)
                            std::max(5, config.boundary.default_speed), // speed (match START pulse)
                            0, // right_turn
                            0  // left_turn
                        );
                        if (!ble->sendControl(run_control)) {
                            std::cerr << "[!] Failed to send run command to BLE device\n";
                        }
                        std::this_thread::sleep_for(std::chrono::seconds(1));
                        if (!ble->sendControl(stop_control)) {
                            std::cerr << "[!] Failed to send stop command to BLE device\n";
                        }
                        std::cout << "[✓] Ran for 1 seconds and stopped.\n";

                        connected = true;
                        break;
                    }
                } catch (const std::exception& e) {
                    if (attempt == config.ble.reconnect_attempts) {
                        std::cerr << "[✗] BLE connection failed: " << e.what() << "\n";
                        return 1;
                    }
                    std::cout << "[*] Retry " << attempt << "/" << config.ble.reconnect_attempts << "...\n";
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                }
            }

            if (!connected) {
                std::cerr << "[✗] Failed to connect to BLE device after " 
                         << config.ble.reconnect_attempts << " attempts\n";
                return 1;
            }
        } else {
            // In simulation mode we still call connect to keep interfaces consistent
            ble->connect();

            // Mirror the real-device behavior in simulation: send a short START
            // pulse and run the simulated car for 2 seconds so users can observe.
            std::cout << "[*] (simulate) Sending short START pulse...\n";
            ControlVector pulse_control(
                true,
                std::max(5, config.boundary.default_speed),
                0,
                0
            );
            ble->sendControl(pulse_control);
            std::this_thread::sleep_for(std::chrono::seconds(1));
            ble->sendControl(stop_control);

            std::cout << "[*] (simulate) Running simulated car for 2 seconds...\n";
            ControlVector run_control(false, config.boundary.default_speed, 0, 0);
            ble->sendControl(run_control);
            std::this_thread::sleep_for(std::chrono::seconds(2));
            ble->sendControl(stop_control);
            std::cout << "[✓] (simulate) Run complete.\n";
        }

        std::cout << "\n";

        // Initialize camera and other components after BLE is ready
        std::cout << "[*] Initializing camera...\n";
        std::unique_ptr<CameraCapture> camera;
        if (simulate) {
            camera = std::make_unique<SimulatedCamera>(
                config.camera.width,
                config.camera.height,
                config.camera.fps
            );
        } else {
            camera = std::make_unique<CameraCapture>(
                config.camera.index,
                config.camera.width,
                config.camera.height,
                config.camera.fps
            );
        }
        std::cout << "[✓] Camera initialized: " << config.camera.width << "x"
                 << config.camera.height << " @ " << config.camera.fps << " FPS\n\n";

        std::cout << "[*] Initializing tracker: " << config.tracker.tracker_type << "\n";
        auto tracker = createTracker(config.tracker.tracker_type, simulate);
        std::cout << "[✓] Tracker initialized\n\n";

        std::cout << "[*] Initializing boundary detector...\n";
        std::vector<int> rayAngles = {-45, 0, 45};
        auto boundary = std::make_unique<BoundaryDetector>(
            config.boundary.black_threshold,
            rayAngles,
            config.boundary.ray_max_length,
            config.boundary.evasive_distance,
            config.boundary.default_speed,
            config.boundary.steering_limit,
            config.boundary.light_on
        );
        std::cout << "[✓] Boundary detector initialized\n\n";
        std::cout << "\n";
        
        // Create orchestrator
        std::cout << "[2/3] Starting camera + tracking pipeline...\n";
        OrchestratorOptions orchOptions{
            config.ui.show_window,
            config.ui.command_rate_hz,
            config.ui.color_tracking
        };
        
        g_orchestrator = std::make_unique<ControlOrchestrator>(
            std::move(camera),
            std::move(tracker),
            std::move(boundary),
            std::move(ble),
            orchOptions
        );
        
        std::cout << "[3/3] Autonomy running (press Ctrl+C to stop, or 'q' in window to quit)\n\n";
        
        g_orchestrator->start();

        // Match Python: run UI loop on main thread if enabled
        if (orchOptions.showWindow) {
            g_orchestrator->runUILoop();
        } else {
            // Keep main thread alive
            while (true) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }
        
    } catch (const std::exception& e) {
        std::cerr << "[!] Fatal error: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}
