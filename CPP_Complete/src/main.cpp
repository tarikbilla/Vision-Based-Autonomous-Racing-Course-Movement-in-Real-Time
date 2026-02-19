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
        
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--config" && i + 1 < argc) {
                configPath = argv[++i];
            } else if (arg == "--simulate") {
                simulate = true;
            } else if (arg == "--device" && i + 1 < argc) {
                deviceMac = argv[++i];
            } else if (arg == "--help") {
                std::cout << "Usage: " << argv[0] << " [options]\n"
                         << "Options:\n"
                         << "  --config <path>      Path to config file (default: config/config.json)\n"
                         << "  --simulate          Run in simulation mode\n"
                         << "  --device <mac>      BLE device MAC address\n"
                         << "  --help              Show this help message\n";
                return 0;
            }
        }
        
        // Load configuration
        std::cout << "[*] Loading configuration from: " << configPath << "\n";
        SystemConfig config = ConfigManager::loadConfig(configPath);
        std::cout << "[✓] Configuration loaded successfully\n\n";
        
        // Setup signal handlers
        signal(SIGINT, signalHandler);
        signal(SIGTERM, signalHandler);
        
        // Initialize components
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
        
        std::cout << "[*] Initializing BLE...\n";
        BLETarget target{
            config.ble.device_mac,
            config.ble.characteristic_uuid,
            config.ble.device_identifier
        };
        auto ble = createBLEClient(target, simulate);
        
        if (!simulate) {
            if (!deviceMac.empty()) {
                std::cout << "[*] Attempting to connect to device: " << deviceMac << "\n";
            } else {
                std::cout << "[*] Attempting to connect to device: " << config.ble.device_mac << "\n";
            }
            
            bool connected = false;
            for (int attempt = 1; attempt <= config.ble.reconnect_attempts; ++attempt) {
                try {
                    if (ble->connect()) {
                        connected = true;
                        std::cout << "[✓] BLE connected\n";
                        break;
                    }
                } catch (const std::exception& e) {
                    std::cout << "[!] Connection attempt " << attempt << " failed: " << e.what() << "\n";
                    if (attempt < config.ble.reconnect_attempts) {
                        std::cout << "[*] Retrying...\n";
                        std::this_thread::sleep_for(std::chrono::seconds(1));
                    }
                }
            }
            
            if (!connected) {
                std::cerr << "[!] Failed to connect to BLE device after " 
                         << config.ble.reconnect_attempts << " attempts\n";
                return 1;
            }
        } else {
            ble->connect();
            std::cout << "[✓] Fake BLE connected (simulation mode)\n";
        }
        std::cout << "\n";
        
        // Create orchestrator
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
        
        std::cout << "[*] Starting autonomous control system...\n";
        std::cout << "[*] Press Ctrl+C or 'q' to stop\n\n";
        
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
