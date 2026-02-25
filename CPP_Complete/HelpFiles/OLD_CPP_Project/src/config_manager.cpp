#include "config_manager.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <vector>

namespace rc_car {

ConfigManager::ConfigManager() {
    loadDefaults();
}

ConfigManager::ConfigManager(const std::string& config_file) {
    loadDefaults();
    load(config_file);
}

void ConfigManager::loadDefaults() {
    // Camera settings
    config_["camera.index"] = "0";
    config_["camera.width"] = "1920";
    config_["camera.height"] = "1080";
    config_["camera.fps"] = "30";
    
    // Tracking settings
    config_["tracker.type"] = "CSRT";  // CSRT, GOTURN, KCF, MOSSE
    config_["tracker.max_midpoints"] = "10";
    
    // Boundary detection
    config_["boundary.black_threshold"] = "50";
    config_["boundary.ray_max_length"] = "200";
    config_["boundary.evasive_threshold"] = "80";
    config_["boundary.ray_angles"] = "-60,0,60";  // Comma-separated
    config_["boundary.base_speed"] = "10";
    
    // BLE settings
    config_["ble.device_mac"] = "f9:af:3c:e2:d2:f5";
    config_["ble.characteristic_uuid"] = "6e400002-b5a3-f393-e0a9-e50e24dcca9e";
    config_["ble.device_identifier"] = "bf0a00082800";
    config_["ble.command_rate_hz"] = "200";
    config_["ble.connection_timeout"] = "5";
    config_["ble.reconnection_attempts"] = "3";
    
    // Control settings
    config_["control.speed_limit_forward"] = "100";
    config_["control.speed_limit_reverse"] = "100";
    config_["control.steering_limit"] = "30";
    config_["control.light_on_value"] = "0200";
    config_["control.light_off_value"] = "0000";
    
    // System settings
    config_["system.show_ui"] = "true";
    config_["system.autonomous_mode"] = "false";
}

void ConfigManager::load(const std::string& config_file) {
    config_file_path_ = config_file;
    parseConfigFile(config_file);
}

void ConfigManager::parseConfigFile(const std::string& filepath) {
    // Try the provided path first
    std::ifstream file(filepath);
    
    // If not found, try relative paths
    if (!file.is_open()) {
        // Try relative to current directory
        std::string alt_path1 = "../" + filepath;
        file.open(alt_path1);
    }
    
    if (!file.is_open()) {
        // Try from project root
        std::string alt_path2 = "../../" + filepath;
        file.open(alt_path2);
    }
    
    if (!file.is_open()) {
        std::cerr << "Warning: Could not open config file: " << filepath 
                  << " (also tried ../" << filepath << " and ../../" << filepath << ")" << std::endl;
        std::cerr << "Using defaults. You can specify config file with: -c /path/to/config.json" << std::endl;
        return;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        // Remove comments
        size_t comment_pos = line.find('#');
        if (comment_pos != std::string::npos) {
            line = line.substr(0, comment_pos);
        }
        
        // Trim whitespace
        line.erase(0, line.find_first_not_of(" \t"));
        line.erase(line.find_last_not_of(" \t") + 1);
        
        if (line.empty()) continue;
        
        // Parse key=value
        size_t eq_pos = line.find('=');
        if (eq_pos != std::string::npos) {
            std::string key = line.substr(0, eq_pos);
            std::string value = line.substr(eq_pos + 1);
            
            // Trim key and value
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);
            
            config_[key] = value;
        }
    }
    
    file.close();
}

void ConfigManager::save(const std::string& config_file) {
    std::string filepath = config_file.empty() ? config_file_path_ : config_file;
    if (filepath.empty()) {
        filepath = "config/config.json";
    }
    
    std::ofstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open config file for writing: " << filepath << std::endl;
        return;
    }
    
    file << "# Vision-Based RC Car Control System Configuration\n";
    file << "# Format: key=value\n\n";
    
    // Group by category
    std::vector<std::string> categories = {"camera", "tracker", "boundary", "ble", "control", "system"};
    
    for (const auto& category : categories) {
        file << "\n# " << category << " settings\n";
        for (const auto& pair : config_) {
            if (pair.first.substr(0, category.length() + 1) == category + ".") {
                file << pair.first << "=" << pair.second << "\n";
            }
        }
    }
    
    file.close();
}

std::string ConfigManager::getString(const std::string& key, const std::string& default_val) const {
    auto it = config_.find(key);
    return (it != config_.end()) ? it->second : default_val;
}

int ConfigManager::getInt(const std::string& key, int default_val) const {
    auto it = config_.find(key);
    if (it != config_.end()) {
        try {
            return std::stoi(it->second);
        } catch (...) {
            return default_val;
        }
    }
    return default_val;
}

double ConfigManager::getDouble(const std::string& key, double default_val) const {
    auto it = config_.find(key);
    if (it != config_.end()) {
        try {
            return std::stod(it->second);
        } catch (...) {
            return default_val;
        }
    }
    return default_val;
}

bool ConfigManager::getBool(const std::string& key, bool default_val) const {
    auto it = config_.find(key);
    if (it != config_.end()) {
        std::string value = it->second;
        std::transform(value.begin(), value.end(), value.begin(), ::tolower);
        return (value == "true" || value == "1" || value == "yes");
    }
    return default_val;
}

void ConfigManager::setString(const std::string& key, const std::string& value) {
    config_[key] = value;
}

void ConfigManager::setInt(const std::string& key, int value) {
    config_[key] = std::to_string(value);
}

void ConfigManager::setDouble(const std::string& key, double value) {
    config_[key] = std::to_string(value);
}

void ConfigManager::setBool(const std::string& key, bool value) {
    config_[key] = value ? "true" : "false";
}

} // namespace rc_car
