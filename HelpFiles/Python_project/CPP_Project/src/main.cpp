/**
 * @file main.cpp
 * @brief Main entry point for Vision-Based Autonomous RC Car Control System
 * @author Vision-Based Autonomous RC Car Control System
 * @date 2024
 */

#include <iostream>
#include <signal.h>
#include <memory>
#include <thread>
#include <chrono>
#include <cstdlib>
#include <opencv2/highgui.hpp>
#include "control_orchestrator.h"

using namespace rc_car;

// Global orchestrator for signal handling
std::unique_ptr<ControlOrchestrator> g_orchestrator = nullptr;

void signalHandler(int signal) {
    std::cout << "\nReceived signal " << signal << ". Shutting down..." << std::endl;
    if (g_orchestrator) {
        g_orchestrator->stop();
    }
    exit(0);
}

void printUsage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [options]" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -c, --config <file>    Configuration file path (default: config/config.json)" << std::endl;
    std::cout << "  -h, --help            Show this help message" << std::endl;
    std::cout << "  -m, --manual          Start in manual mode (no autonomous control)" << std::endl;
    std::cout << "  --no-ui               Disable UI display" << std::endl;
}

int main(int argc, char* argv[]) {
    // Try multiple config file paths (relative to executable or project root)
    std::string config_file = "config/config.json";
    bool manual_mode = false;
    bool show_ui = true;
    
    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        } else if (arg == "-c" || arg == "--config") {
            if (i + 1 < argc) {
                config_file = argv[++i];
            } else {
                std::cerr << "Error: --config requires a file path" << std::endl;
                return 1;
            }
        } else if (arg == "-m" || arg == "--manual") {
            manual_mode = true;
        } else if (arg == "--no-ui") {
            show_ui = false;
        } else {
            std::cerr << "Unknown option: " << arg << std::endl;
            printUsage(argv[0]);
            return 1;
        }
    }
    
    // Register signal handlers
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    std::cout << "========================================" << std::endl;
    std::cout << "Vision-Based RC Car Control System" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Config file: " << config_file << std::endl;
    std::cout << "Mode: " << (manual_mode ? "Manual" : "Autonomous") << std::endl;
    std::cout << "UI: " << (show_ui ? "Enabled" : "Disabled") << std::endl;
    std::cout << "========================================" << std::endl;
    
    // Create orchestrator
    g_orchestrator = std::make_unique<ControlOrchestrator>();
    
    // Initialize
    if (!g_orchestrator->initialize(config_file)) {
        std::cerr << "Error: Failed to initialize system" << std::endl;
        return 1;
    }
    
    // Set mode
    g_orchestrator->setAutonomousMode(!manual_mode);
    
    // Start system
    if (!g_orchestrator->start()) {
        std::cerr << "Error: Failed to start system" << std::endl;
        return 1;
    }
    
    std::cout << "\nSystem running. Press Ctrl+C to stop." << std::endl;
    if (!manual_mode) {
        std::cout << "Autonomous mode: ACTIVE" << std::endl;
    }
    
    // Main loop (wait for shutdown)
    while (g_orchestrator->isRunning()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Check for 'q' key press if UI is enabled
        if (show_ui) {
            int key = cv::waitKey(1) & 0xFF;
            if (key == 'q' || key == 27) {  // 'q' or ESC
                std::cout << "\nQuit key pressed. Shutting down..." << std::endl;
                break;
            }
        }
    }
    
    // Stop system
    g_orchestrator->stop();
    g_orchestrator.reset();
    
    std::cout << "System shutdown complete." << std::endl;
    return 0;
}
