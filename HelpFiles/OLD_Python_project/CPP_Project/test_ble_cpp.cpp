/**
 * @file test_ble_cpp.cpp
 * @brief BLE Scanner and Connector for DRIFT RC Car - C++ version
 * 
 * This program:
 * 1. Scans and lists ALL Bluetooth devices
 * 2. Lets you select which device to connect to
 * 3. Connects and discovers services/characteristics
 * 4. Sends control commands to the car
 * 
 * Compilation:
 *   g++ -std=c++17 -o test_ble_cpp test_ble_cpp.cpp -lsimpleble
 * 
 * Or with CMake (if simpleble-cpp is installed):
 *   cmake -DTEST_BLE=ON ..
 *   make test_ble_cpp
 */

#include <iostream>
#include <string>
#include <vector>
#include <iomanip>
#include <sstream>
#include <thread>
#include <chrono>
#include <algorithm>
#include <cctype>

// Uncomment when simpleble-cpp is installed
// #define USE_SIMPLEBLE
#ifdef USE_SIMPLEBLE
#include <simpleble/SimpleBLE.h>
#endif

class BLETest {
private:
    struct DeviceInfo {
        std::string name;
        std::string address;
        bool is_drift;
        #ifdef USE_SIMPLEBLE
        SimpleBLE::Peripheral peripheral;
        #endif
    };
    
    std::string device_identifier_;
    std::string default_characteristic_uuid_;
    
    std::string intToHex(int value, int digits = 4) const {
        std::stringstream ss;
        ss << std::hex << std::setfill('0') << std::setw(digits) << value;
        return ss.str();
    }
    
    std::string generateCommand(int light_on, int speed, int left_turn, int right_turn) const {
        int speed_value = speed;
        int drift_value = 0;
        int steering_value = 0;
        
        // Calculate steering value (as per DRIFT protocol)
        if (right_turn > 0) {
            steering_value = right_turn;
        } else if (left_turn > 0) {
            steering_value = 255 - left_turn;
        }
        
        std::string light_value = light_on ? "0200" : "0000";
        std::string checksum = "00";
        
        std::string command = device_identifier_ +
                             intToHex(speed_value, 4) +
                             intToHex(drift_value, 4) +
                             intToHex(steering_value, 4) +
                             light_value +
                             checksum;
        
        return command;
    }
    
    std::vector<uint8_t> hexStringToBytes(const std::string& hex) const {
        std::vector<uint8_t> bytes;
        for (size_t i = 0; i < hex.length(); i += 2) {
            std::string byte_str = hex.substr(i, 2);
            uint8_t byte = static_cast<uint8_t>(std::stoul(byte_str, nullptr, 16));
            bytes.push_back(byte);
        }
        return bytes;
    }
    
    bool isDriftCar(const std::string& name) const {
        std::string name_lower = name;
        std::string name_upper = name;
        std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(), ::tolower);
        std::transform(name_upper.begin(), name_upper.end(), name_upper.begin(), ::toupper);
        
        return (name_lower.find("drift") != std::string::npos ||
                name_lower.find("dr!ft") != std::string::npos ||
                name.find("DRiFT") == 0 ||
                name.find("DRIFT") == 0 ||
                name.find("DR!FT") == 0 ||
                name_upper.find("ED5C2384488D") != std::string::npos ||
                name_upper.find("F9AF3CE2D2F5") != std::string::npos);
    }
    
public:
    BLETest() 
        : device_identifier_("bf0a00082800"),
          default_characteristic_uuid_("6e400002-b5a3-f393-e0a9-e50e24dcca9e") {
    }
    
    void printCommand(const std::string& command, const std::string& description) {
        std::cout << "\n[" << description << "]" << std::endl;
        std::cout << "Command (hex): " << command << std::endl;
        
        auto bytes = hexStringToBytes(command);
        std::cout << "Bytes: ";
        for (size_t i = 0; i < bytes.size(); ++i) {
            std::cout << std::hex << std::setfill('0') << std::setw(2) 
                     << static_cast<int>(bytes[i]);
            if (i < bytes.size() - 1) std::cout << " ";
        }
        std::cout << std::dec << std::endl;
        
        std::cout << "  Device ID: " << command.substr(0, 12) << std::endl;
        std::cout << "  Speed: " << std::stoi(command.substr(12, 4), nullptr, 16) << std::endl;
        std::cout << "  Drift: " << std::stoi(command.substr(16, 4), nullptr, 16) << std::endl;
        std::cout << "  Steering: " << std::stoi(command.substr(20, 4), nullptr, 16) << std::endl;
        std::cout << "  Lights: " << command.substr(24, 4) << std::endl;
        std::cout << "  Checksum: " << command.substr(28, 2) << std::endl;
    }
    
    #ifdef USE_SIMPLEBLE
    std::vector<DeviceInfo> scanAllDevices() {
        std::vector<DeviceInfo> devices;
        
        std::cout << "======================================================================" << std::endl;
        std::cout << "Scanning for ALL Bluetooth Devices..." << std::endl;
        std::cout << "======================================================================" << std::endl;
        
        try {
            auto adapters = SimpleBLE::Adapter::get_adapters();
            if (adapters.empty()) {
                std::cerr << "Error: No BLE adapters found!" << std::endl;
                return devices;
            }
            
            auto adapter = adapters[0];
            std::cout << "Using adapter: " << adapter.identifier() << std::endl;
            std::cout << "Scanning for 10 seconds..." << std::endl;
            std::cout << "   (Make sure your car is ON and nearby)" << std::endl;
            std::cout << std::endl;
            
            adapter.scan_for(10000);
            auto peripherals = adapter.scan_get_results();
            
            std::cout << "======================================================================" << std::endl;
            std::cout << "Found " << peripherals.size() << " Bluetooth device(s):" << std::endl;
            std::cout << "======================================================================" << std::endl;
            std::cout << std::endl;
            
            for (size_t i = 0; i < peripherals.size(); ++i) {
                DeviceInfo info;
                info.name = peripherals[i].identifier();
                if (info.name.empty()) {
                    info.name = "(Unknown)";
                }
                info.address = peripherals[i].address();
                info.is_drift = isDriftCar(info.name);
                #ifdef USE_SIMPLEBLE
                info.peripheral = peripherals[i];
                #endif
                
                std::string marker = info.is_drift ? "CAR" : "DEV";
                std::string note = info.is_drift ? " [DRIFT CAR?]" : "";
                
                std::cout << marker << " " << std::setw(2) << (i + 1) << ". Name: '" << info.name << "'" << std::endl;
                std::cout << "      MAC: " << info.address << note << std::endl;
                std::cout << std::endl;
                
                devices.push_back(info);
            }
            
            std::cout << "======================================================================" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Error during scan: " << e.what() << std::endl;
        }
        
        return devices;
    }
    
    bool connectToDevice(DeviceInfo& device, std::string& service_uuid, std::string& char_uuid) {
        std::cout << "\n======================================================================" << std::endl;
        std::cout << "Connecting to Device..." << std::endl;
        std::cout << "======================================================================" << std::endl;
        
        try {
            std::cout << "Connecting..." << std::endl;
            device.peripheral.connect();
            std::cout << "Connected successfully!" << std::endl;
            
            std::this_thread::sleep_for(std::chrono::seconds(1));
            
            auto services = device.peripheral.services();
            std::cout << "\nFound " << services.size() << " service(s):" << std::endl;
            std::cout << "----------------------------------------------------------------------" << std::endl;
            
            for (const auto& service : services) {
                std::cout << "\nService UUID: " << service.uuid() << std::endl;
                auto characteristics = service.characteristics();
                std::cout << "  Characteristics (" << characteristics.size() << "):" << std::endl;
                
                for (const auto& char_obj : characteristics) {
                    std::cout << "    - " << char_obj.uuid() << std::endl;
                    
                    if (char_obj.uuid() == default_characteristic_uuid_) {
                        service_uuid = service.uuid();
                        char_uuid = char_obj.uuid();
                        std::cout << "      MATCH! This is the control characteristic" << std::endl;
                    }
                }
            }
            
            std::cout << "----------------------------------------------------------------------" << std::endl;
            
            if (char_uuid.empty()) {
                std::cout << "\nWarning: Target characteristic not found!" << std::endl;
                return false;
            }
            
            std::cout << "\nUsing characteristic: " << char_uuid << std::endl;
            return true;
            
        } catch (const std::exception& e) {
            std::cerr << "Connection failed: " << e.what() << std::endl;
            return false;
        }
    }
    
    bool sendCommand(SimpleBLE::Peripheral& peripheral, 
                    const std::string& service_uuid,
                    const std::string& char_uuid,
                    const std::string& command_hex) {
        try {
            auto bytes = hexStringToBytes(command_hex);
            peripheral.write_request(service_uuid, char_uuid, bytes);
            return true;
        } catch (const std::exception& e) {
            std::cerr << "Error sending command: " << e.what() << std::endl;
            return false;
        }
    }
    #else
    std::vector<DeviceInfo> scanAllDevices() {
        std::cout << "======================================================================" << std::endl;
        std::cout << "BLE Scanning (Placeholder - simpleble-cpp not available)" << std::endl;
        std::cout << "======================================================================" << std::endl;
        std::cout << "\nTo use actual BLE scanning:" << std::endl;
        std::cout << "1. Install simpleble-cpp:" << std::endl;
        std::cout << "   git clone https://github.com/OpenBluetoothToolkit/simpleble.git" << std::endl;
        std::cout << "   cd simpleble && mkdir build && cd build" << std::endl;
        std::cout << "   cmake .. && make && sudo make install" << std::endl;
        std::cout << "2. Compile with: g++ -std=c++17 -DUSE_SIMPLEBLE -o test_ble_cpp test_ble_cpp.cpp -lsimpleble" << std::endl;
        return std::vector<DeviceInfo>();
    }
    #endif
    
    void interactiveMode() {
        std::cout << "\n======================================================================" << std::endl;
        std::cout << "Interactive BLE Control Mode" << std::endl;
        std::cout << "======================================================================" << std::endl;
        std::cout << "Commands:" << std::endl;
        std::cout << "  s - Start (forward, speed 20)" << std::endl;
        std::cout << "  t - Stop" << std::endl;
        std::cout << "  l - Left turn" << std::endl;
        std::cout << "  r - Right turn" << std::endl;
        std::cout << "  f - Forward straight" << std::endl;
        std::cout << "  b - Reverse" << std::endl;
        std::cout << "  + - Increase speed" << std::endl;
        std::cout << "  - - Decrease speed" << std::endl;
        std::cout << "  q - Quit" << std::endl;
        std::cout << "======================================================================" << std::endl;
        
        int speed = 0;
        int left = 0;
        int right = 0;
        int light = 0;
        
        std::string input;
        while (true) {
            std::cout << "\n> ";
            std::getline(std::cin, input);
            
            if (input.empty()) continue;
            
            char cmd = input[0];
            
            switch (cmd) {
                case 's':
                case 'S':
                    speed = 20;
                    light = 1;
                    left = 0;
                    right = 0;
                    std::cout << "START: Speed=20, Lights ON" << std::endl;
                    break;
                case 't':
                case 'T':
                    speed = 0;
                    light = 0;
                    left = 0;
                    right = 0;
                    std::cout << "STOP" << std::endl;
                    break;
                case 'l':
                case 'L':
                    left = 20;
                    right = 0;
                    if (speed == 0) speed = 20;
                    std::cout << "LEFT TURN: Left=" << left << std::endl;
                    break;
                case 'r':
                case 'R':
                    right = 20;
                    left = 0;
                    if (speed == 0) speed = 20;
                    std::cout << "RIGHT TURN: Right=" << right << std::endl;
                    break;
                case 'f':
                case 'F':
                    left = 0;
                    right = 0;
                    if (speed == 0) speed = 20;
                    std::cout << "FORWARD STRAIGHT" << std::endl;
                    break;
                case 'b':
                case 'B':
                    speed = 255 - 20;
                    left = 0;
                    right = 0;
                    std::cout << "REVERSE: Speed=" << (255 - speed) << std::endl;
                    break;
                case '+':
                    if (speed > 0 && speed < 255) {
                        speed = std::min(255, speed + 10);
                        std::cout << "Speed increased to: " << speed << std::endl;
                    }
                    break;
                case '-':
                    if (speed > 0) {
                        speed = std::max(0, speed - 10);
                        std::cout << "Speed decreased to: " << speed << std::endl;
                    }
                    break;
                case 'q':
                case 'Q':
                    std::cout << "Quitting..." << std::endl;
                    {
                        std::string stop = generateCommand(0, 0, 0, 0);
                        printCommand(stop, "FINAL STOP");
                    }
                    return;
                default:
                    std::cout << "Unknown command. Type 'q' to quit." << std::endl;
                    continue;
            }
            
            std::string command = generateCommand(light, speed, left, right);
            printCommand(command, "SENDING");
            
            #ifdef USE_SIMPLEBLE
            // TODO: Actually send via BLE
            std::cout << "[NOTE: BLE sending needs peripheral reference]" << std::endl;
            #else
            std::cout << "[NOTE: BLE library not available - command shown above]" << std::endl;
            #endif
        }
    }
    
    void testCommands() {
        std::cout << "======================================================================" << std::endl;
        std::cout << "BLE Command Test for RC Car" << std::endl;
        std::cout << "======================================================================" << std::endl;
        std::cout << "Device Identifier: " << device_identifier_ << std::endl;
        std::cout << "Characteristic UUID: " << default_characteristic_uuid_ << std::endl;
        std::cout << "======================================================================" << std::endl;
        
        std::cout << "\n--- Test Commands ---\n" << std::endl;
        
        std::string stop_cmd = generateCommand(0, 0, 0, 0);
        printCommand(stop_cmd, "STOP");
        
        std::string start_cmd = generateCommand(1, 20, 0, 0);
        printCommand(start_cmd, "START (Forward, Speed=20, Lights ON)");
        
        std::string right_cmd = generateCommand(1, 30, 0, 20);
        printCommand(right_cmd, "FORWARD RIGHT (Speed=30, Right Turn=20)");
        
        std::string left_cmd = generateCommand(1, 30, 20, 0);
        printCommand(left_cmd, "FORWARD LEFT (Speed=30, Left Turn=20)");
        
        std::string reverse_cmd = generateCommand(1, 255 - 20, 0, 0);
        printCommand(reverse_cmd, "REVERSE (Reverse Speed=20)");
        
        std::cout << "\n======================================================================" << std::endl;
        std::cout << "Command generation test complete!" << std::endl;
        std::cout << "======================================================================" << std::endl;
    }
};

int main(int argc, char* argv[]) {
    BLETest ble_test;
    
    std::cout << "\nChoose mode:" << std::endl;
    std::cout << "1. Test command generation (no BLE connection)" << std::endl;
    std::cout << "2. Scan and connect to BLE device (requires simpleble-cpp)" << std::endl;
    std::cout << "3. Interactive mode (test commands)" << std::endl;
    std::cout << "\nEnter choice (1, 2, or 3): ";
    
    std::string choice;
    std::getline(std::cin, choice);
    
    if (choice == "1") {
        ble_test.testCommands();
    } else if (choice == "2") {
        #ifdef USE_SIMPLEBLE
        auto devices = ble_test.scanAllDevices();
        if (devices.empty()) {
            std::cout << "No devices found or BLE not available." << std::endl;
            return 1;
        }
        
        std::cout << "\nEnter device number to connect (1-" << devices.size() << "): ";
        std::string device_choice;
        std::getline(std::cin, device_choice);
        
        try {
            int device_num = std::stoi(device_choice);
            if (device_num < 1 || device_num > static_cast<int>(devices.size())) {
                std::cerr << "Invalid device number" << std::endl;
                return 1;
            }
            
            DeviceInfo& selected = devices[device_num - 1];
            std::string service_uuid, char_uuid;
            
            if (ble_test.connectToDevice(selected, service_uuid, char_uuid)) {
                std::cout << "\nTesting commands..." << std::endl;
                
                // Test commands
                std::string stop_cmd = ble_test.generateCommand(0, 0, 0, 0);
                std::cout << "Sending STOP..." << std::endl;
                ble_test.sendCommand(selected.peripheral, service_uuid, char_uuid, stop_cmd);
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                
                std::string start_cmd = ble_test.generateCommand(1, 20, 0, 0);
                std::cout << "Sending START..." << std::endl;
                ble_test.sendCommand(selected.peripheral, service_uuid, char_uuid, start_cmd);
                std::this_thread::sleep_for(std::chrono::milliseconds(2000));
                
                std::cout << "Sending STOP..." << std::endl;
                ble_test.sendCommand(selected.peripheral, service_uuid, char_uuid, stop_cmd);
                
                selected.peripheral.disconnect();
            }
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
            return 1;
        }
        #else
        std::cout << "BLE scanning requires simpleble-cpp library." << std::endl;
        std::cout << "Compile with: g++ -std=c++17 -DUSE_SIMPLEBLE -o test_ble_cpp test_ble_cpp.cpp -lsimpleble" << std::endl;
        #endif
    } else if (choice == "3") {
        ble_test.testCommands();
        ble_test.interactiveMode();
    } else {
        std::cout << "Invalid choice. Running test mode..." << std::endl;
        ble_test.testCommands();
    }
    
    return 0;
}
