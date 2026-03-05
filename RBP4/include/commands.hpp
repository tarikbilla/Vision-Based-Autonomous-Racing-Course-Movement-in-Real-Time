#ifndef COMMANDS_HPP
#define COMMANDS_HPP

#include <cstdint>
#include <vector>
#include <algorithm>
#include <string>

struct ControlVector {
    bool light_on;
    int32_t speed;
    int32_t right_turn_value;
    int32_t left_turn_value;
    
    ControlVector() : light_on(false), speed(0), right_turn_value(0), left_turn_value(0) {}
    ControlVector(bool light, int32_t spd, int32_t right, int32_t left)
        : light_on(light), speed(spd), right_turn_value(right), left_turn_value(left) {}
};

class Commands {
public:
    // Build command bytes matching Python implementation:
    // buildCommand(device_identifier_hex, control)
    // device_identifier_hex: hex string like "bf0a00082800"
    static std::vector<uint8_t> buildCommand(const std::string& device_identifier_hex, const ControlVector& control);
    
    // Utility functions
    static int32_t clamp(int32_t value, int32_t min_val, int32_t max_val);
    static int32_t mapValue(int32_t value, int32_t in_min, int32_t in_max, 
                           int32_t out_min, int32_t out_max);
};

#endif // COMMANDS_HPP
