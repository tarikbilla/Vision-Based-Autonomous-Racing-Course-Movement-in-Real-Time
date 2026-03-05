#ifndef CONFIG_MANAGER_HPP
#define CONFIG_MANAGER_HPP

#include <string>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

struct CameraConfig {
    int index;
    int width;
    int height;
    int fps;
};

struct BLEConfig {
    std::string device_mac;
    std::string characteristic_uuid;
    std::string device_identifier;
    int connection_timeout_s;
    int reconnect_attempts;
};

struct BoundaryConfig {
    int black_threshold;
    int ray_max_length;
    int evasive_distance;
    int default_speed;
    int steering_limit;
    bool light_on;
};

struct TrackerConfig {
    std::string tracker_type;
};

struct UIConfig {
    bool show_window;
    int command_rate_hz;
    bool color_tracking;
};

struct SystemConfig {
    CameraConfig camera;
    BLEConfig ble;
    BoundaryConfig boundary;
    TrackerConfig tracker;
    UIConfig ui;
};

class ConfigManager {
public:
    static SystemConfig loadConfig(const std::string& configPath);
    static json readJsonFile(const std::string& filePath);
    
private:
    static void validateConfig(const SystemConfig& config);
};

#endif // CONFIG_MANAGER_HPP
