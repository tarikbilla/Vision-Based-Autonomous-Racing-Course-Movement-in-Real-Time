#include "ble_handler.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <chrono>
#include <thread>

namespace rc_car {

BLEHandler::BLEHandler()
    : device_mac_("f9:af:3c:e2:d2:f5"),
      device_characteristic_uuid_("6e400002-b5a3-f393-e0a9-e50e24dcca9e"),
      device_identifier_("bf0a00082800"),
      connected_(false), running_(false), command_send_rate_hz_(200) {
}

BLEHandler::BLEHandler(const std::string& mac_address, const std::string& characteristic_uuid)
    : device_mac_(mac_address),
      device_characteristic_uuid_(characteristic_uuid),
      device_identifier_("bf0a00082800"),
      connected_(false), running_(false), command_send_rate_hz_(200) {
}

BLEHandler::~BLEHandler() {
    stopSending();
    disconnect();
}

bool BLEHandler::initialize(const std::string& mac_address, const std::string& characteristic_uuid) {
    device_mac_ = mac_address;
    device_characteristic_uuid_ = characteristic_uuid;
    return true;
}

bool BLEHandler::connect() {
    if (connected_) {
        return true;
    }
    
    std::cout << "Connecting to BLE device: " << device_mac_ << std::endl;
    
    // TODO: Implement actual BLE connection using chosen library
    // This is a placeholder implementation
    // For BlueZ, you would use D-Bus API or bluetoothctl/gatttool
    // For simpleble-cpp, you would use SimpleBLE library
    
    bool success = connectToDevice();
    
    if (success) {
        connected_ = true;
        std::cout << "Successfully connected to BLE device" << std::endl;
    } else {
        std::cerr << "Failed to connect to BLE device" << std::endl;
    }
    
    return success;
}

void BLEHandler::disconnect() {
    if (!connected_) {
        return;
    }
    
    stopSending();
    disconnectFromDevice();
    connected_ = false;
    std::cout << "Disconnected from BLE device" << std::endl;
}

void BLEHandler::setControl(const ControlVector& control) {
    std::lock_guard<std::mutex> lock(control_mutex_);
    current_control_ = control;
}

ControlVector BLEHandler::getControl() const {
    std::lock_guard<std::mutex> lock(control_mutex_);
    return current_control_;
}

void BLEHandler::startSending() {
    if (running_) {
        return;
    }
    
    if (!connected_) {
        std::cerr << "Error: Not connected to BLE device" << std::endl;
        return;
    }
    
    running_ = true;
    send_thread_ = std::thread(&BLEHandler::sendLoop, this);
}

void BLEHandler::stopSending() {
    if (!running_) {
        return;
    }
    
    running_ = false;
    if (send_thread_.joinable()) {
        send_thread_.join();
    }
}

void BLEHandler::sendLoop() {
    auto sleep_duration = std::chrono::microseconds(1000000 / command_send_rate_hz_);
    
    while (running_) {
        ControlVector control;
        {
            std::lock_guard<std::mutex> lock(control_mutex_);
            control = current_control_;
        }
        
        std::string command = generateCommand(control);
        
        if (!sendCommand(command)) {
            std::cerr << "Warning: Failed to send BLE command" << std::endl;
        }
        
        std::this_thread::sleep_for(sleep_duration);
    }
}

std::string BLEHandler::intToHex(int value, int digits) const {
    std::stringstream ss;
    ss << std::hex << std::setfill('0') << std::setw(digits) << value;
    return ss.str();
}

std::string BLEHandler::generateCommand(const ControlVector& control) const {
    // Command format: DEVICE_IDENTIFIER + SPEED_HEX + DRIFT_HEX + STEERING_HEX + LIGHT_VALUE + CHECKSUM
    // Based on Python prototype: Command = "".join([DEVICE_IDENTIFIER, twoDigitHex(speed), twoDigitHex(drift), twoDigitHex(steering), LIGHT_VALUE, CHECKSUM])
    
    int speed_value = control.speed;
    int drift_value = 0;  // Typically 0
    int steering_value = 0;
    
    // Calculate steering value (as per Python prototype)
    if (control.right_turn > 0) {
        steering_value = control.right_turn;
    } else if (control.left_turn > 0) {
        steering_value = 255 - control.left_turn;
    }
    
    std::string light_value = control.light_on ? "0200" : "0000";
    std::string checksum = "00";  // Can be calculated if needed
    
    std::string command = device_identifier_ +
                         intToHex(speed_value, 4) +
                         intToHex(drift_value, 4) +
                         intToHex(steering_value, 4) +
                         light_value +
                         checksum;
    
    return command;
}

// Placeholder BLE implementation methods
// These need to be replaced with actual BLE library calls

bool BLEHandler::connectToDevice() {
    // TODO: Implement actual BLE connection
    // Example for BlueZ via D-Bus or simpleble-cpp:
    // 
    // For simpleble-cpp:
    //   SimpleBLE::Adapter adapter = SimpleBLE::Adapter::get_adapters()[0];
    //   adapter.scan_for(5000);
    //   auto peripherals = adapter.scan_get_results();
    //   for (auto& peripheral : peripherals) {
    //       if (peripheral.address() == device_mac_) {
    //           peripheral.connect();
    //           return true;
    //       }
    //   }
    //
    // For BlueZ D-Bus:
    //   Use org.bluez API via D-Bus
    
    std::cout << "NOTE: BLE connection is a placeholder. Implement with actual BLE library." << std::endl;
    // For now, simulate connection success for testing
    return true;  // Change to false to test connection failure
}

void BLEHandler::disconnectFromDevice() {
    // TODO: Implement actual BLE disconnection
    // Example:
    //   peripheral.disconnect();
}

bool BLEHandler::sendCommand(const std::string& command) {
    // TODO: Implement actual BLE command sending
    // Example for simpleble-cpp:
    //   peripheral.write_request(service_uuid, characteristic_uuid, bytes.fromhex(command));
    //
    // Example for BlueZ:
    //   Use gatttool or D-Bus API
    
    // For now, just print the command for debugging
    static int count = 0;
    if (++count % 200 == 0) {  // Print every 200 commands (once per second at 200Hz)
        std::cout << "BLE Command: " << command << std::endl;
    }
    
    return true;  // Simulate success
}

void BLEHandler::setLight(bool on) {
    std::lock_guard<std::mutex> lock(control_mutex_);
    current_control_.light_on = on ? 1 : 0;
}

void BLEHandler::setSpeed(int speed) {
    std::lock_guard<std::mutex> lock(control_mutex_);
    if (speed >= 0 && speed < 255) {
        current_control_.speed = speed;
        current_control_.light_on = 1;  // Lights on when moving
    }
}

void BLEHandler::setReverseSpeed(int speed) {
    std::lock_guard<std::mutex> lock(control_mutex_);
    if (speed >= 0 && speed < 255) {
        current_control_.speed = 255 - speed;  // Reverse mapping
        current_control_.light_on = 1;
    }
}

void BLEHandler::setSteering(int left_value, int right_value) {
    std::lock_guard<std::mutex> lock(control_mutex_);
    current_control_.left_turn = std::min(left_value, 255);
    current_control_.right_turn = std::min(right_value, 255);
}

void BLEHandler::emergencyStop() {
    std::lock_guard<std::mutex> lock(control_mutex_);
    current_control_.speed = 0;
    current_control_.left_turn = 0;
    current_control_.right_turn = 0;
    current_control_.light_on = 0;
    
    // Send stop command immediately
    std::string stop_command = generateCommand(current_control_);
    sendCommand(stop_command);
}

} // namespace rc_car
