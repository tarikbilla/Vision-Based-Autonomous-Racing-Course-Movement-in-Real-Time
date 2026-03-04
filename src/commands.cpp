#include "commands.hpp"
#include <sstream>
#include <iomanip>
#include <string>
#include <vector>

std::vector<uint8_t> Commands::buildCommand(const std::string& device_identifier_hex, const ControlVector& control) {
    // Build command bytes matching Python implementation:
    // hex string: device_identifier + speed(4) + drift(4) + steer(4) + light(4) + checksum(2)
    // Example device_identifier: "bf0a00082800"
    auto int_to_hex = [](int value, int digits) -> std::string {
        std::ostringstream oss;
        oss << std::hex;
        oss << std::setw(digits) << std::setfill('0') << std::nouppercase << (value & ((1 << (4*digits)) - 1));
        return oss.str();
    };

    auto steering_value = [](int left_turn, int right_turn) -> int {
        if (right_turn > 0) return right_turn;
        if (left_turn > 0) return 255 - left_turn;
        return 0;
    };

    int drift_value = 0;
    std::string light_value = control.light_on ? std::string("0200") : std::string("0000");
    int steer = steering_value(control.left_turn_value, control.right_turn_value);

    std::string hexstr = device_identifier_hex;
    // speed, drift, steer are encoded as 4-hex-digit fields (2 bytes each)
    // Python uses 4-digit hex for each of these
    std::ostringstream tmp;
    tmp << std::hex << std::nouppercase;
    // speed
    tmp.str(""); tmp.clear();
    tmp << std::setw(4) << std::setfill('0') << (control.speed & 0xffff);
    hexstr += tmp.str();
    // drift
    tmp.str(""); tmp.clear();
    tmp << std::setw(4) << std::setfill('0') << (drift_value & 0xffff);
    hexstr += tmp.str();
    // steer
    tmp.str(""); tmp.clear();
    tmp << std::setw(4) << std::setfill('0') << (steer & 0xffff);
    hexstr += tmp.str();
    // light
    hexstr += light_value;
    // checksum (fixed 00)
    hexstr += "00";

    // Convert hex string to byte vector
    std::vector<uint8_t> command;
    if (hexstr.size() % 2 != 0) {
        // malformed
        return command;
    }
    for (size_t i = 0; i < hexstr.size(); i += 2) {
        std::string byte_hex = hexstr.substr(i, 2);
        uint8_t byte = static_cast<uint8_t>(std::stoul(byte_hex, nullptr, 16));
        command.push_back(byte);
    }

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
