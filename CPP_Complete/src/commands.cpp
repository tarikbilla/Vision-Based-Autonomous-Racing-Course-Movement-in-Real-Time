#include "commands.hpp"

std::vector<uint8_t> Commands::buildCommand(const ControlVector& control) {
    // Build command bytes for RC car BLE transmission
    // Format: [light_flag, speed, left_turn, right_turn]
    std::vector<uint8_t> command;
    
    command.push_back(control.light_on ? 0x01 : 0x00);
    command.push_back(static_cast<uint8_t>(clamp(control.speed, 0, 255)));
    command.push_back(static_cast<uint8_t>(clamp(control.left_turn_value, 0, 255)));
    command.push_back(static_cast<uint8_t>(clamp(control.right_turn_value, 0, 255)));
    
    return command;
}

int32_t Commands::clamp(int32_t value, int32_t min_val, int32_t max_val) {
    if (value < min_val) return min_val;
    if (value > max_val) return max_val;
    return value;
}

int32_t Commands::mapValue(int32_t value, int32_t in_min, int32_t in_max,
                          int32_t out_min, int32_t out_max) {
    if (in_max == in_min) return out_min;
    return out_min + (value - in_min) * (out_max - out_min) / (in_max - in_min);
}
