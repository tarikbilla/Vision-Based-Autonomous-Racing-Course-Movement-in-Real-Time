#include "config_manager.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>

SystemConfig ConfigManager::loadConfig(const std::string& configPath) {
    json configJson;
    
    try {
        configJson = readJsonFile(configPath);
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("Failed to read config file: ") + e.what());
    }
    
    SystemConfig config;
    
    // Parse camera config
    if (configJson.contains("camera")) {
        auto& cam = configJson["camera"];
        config.camera.index = cam.value("index", 0);
        config.camera.width = cam.value("width", 1920);
        config.camera.height = cam.value("height", 1080);
        config.camera.fps = cam.value("fps", 30);
    }
    
    // Parse BLE config
    if (configJson.contains("ble")) {
        auto& ble = configJson["ble"];
        config.ble.device_mac = ble.value("device_mac", "");
        config.ble.characteristic_uuid = ble.value("characteristic_uuid", "");
        config.ble.device_identifier = ble.value("device_identifier", "");
        config.ble.connection_timeout_s = ble.value("connection_timeout_s", 10);
        config.ble.reconnect_attempts = ble.value("reconnect_attempts", 3);
    }
    
    // Parse boundary config
    if (configJson.contains("boundary")) {
        auto& boundary = configJson["boundary"];
        config.boundary.black_threshold = boundary.value("black_threshold", 50);
        config.boundary.ray_max_length = boundary.value("ray_max_length", 200);
        config.boundary.evasive_distance = boundary.value("evasive_distance", 100);
        config.boundary.default_speed = boundary.value("default_speed", 100);
        config.boundary.steering_limit = boundary.value("steering_limit", 50);
        config.boundary.light_on = boundary.value("light_on", true);
    }
    
    // Parse tracker config
    if (configJson.contains("tracker")) {
        auto& tracker = configJson["tracker"];
        config.tracker.tracker_type = tracker.value("tracker_type", "CSRT");
    }
    
    // Parse UI config
    if (configJson.contains("ui")) {
        auto& ui = configJson["ui"];
        config.ui.show_window = ui.value("show_window", true);
        config.ui.command_rate_hz = ui.value("command_rate_hz", 20);
        config.ui.color_tracking = ui.value("color_tracking", false);
    }
    
    validateConfig(config);
    return config;
}

json ConfigManager::readJsonFile(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file: " + filePath);
    }
    
    json data;
    file >> data;
    return data;
}

void ConfigManager::validateConfig(const SystemConfig& config) {
    if (config.camera.width <= 0 || config.camera.height <= 0) {
        throw std::runtime_error("Invalid camera dimensions");
    }
    if (config.camera.fps <= 0) {
        throw std::runtime_error("Invalid camera FPS");
    }
    if (config.boundary.black_threshold < 0 || config.boundary.black_threshold > 255) {
        throw std::runtime_error("Invalid black_threshold");
    }
}
