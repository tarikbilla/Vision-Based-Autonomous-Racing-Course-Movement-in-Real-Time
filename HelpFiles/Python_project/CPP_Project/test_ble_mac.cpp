#include <iostream>
#include <string>
#include <iomanip>
#include <sstream>
#include <thread>
#include <chrono>
#include <atomic>
#include <cstring>

// Simple BLE test program for Mac
// This uses system calls to bluetoothctl/gatttool or can be adapted for simpleble-cpp

class BLETest {
private:
    std::string device_mac_;
    std::string device_characteristic_uuid_;
    std::string device_identifier_;
    bool connected_;
    
    std::string intToHex(int value, int digits = 4) const {
        std::stringstream ss;
        ss << std::hex << std::setfill('0') << std::setw(digits) << value;
        return ss.str();
    }
    
    std::string generateCommand(int light_on, int speed, int left_turn, int right_turn) const {
        int speed_value = speed;
        int drift_value = 0;
        int steering_value = 0;
        
        // Calculate steering value (as per Python prototype)
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
    
    // Convert hex string to bytes for BLE write
    std::vector<uint8_t> hexStringToBytes(const std::string& hex) const {
        std::vector<uint8_t> bytes;
        for (size_t i = 0; i < hex.length(); i += 2) {
            std::string byteString = hex.substr(i, 2);
            uint8_t byte = static_cast<uint8_t>(strtoul(byteString.c_str(), nullptr, 16));
            bytes.push_back(byte);
        }
        return bytes;
    }
    
public:
    BLETest(const std::string& mac, const std::string& uuid)
        : device_mac_(mac), device_characteristic_uuid_(uuid),
          device_identifier_("bf0a00082800"), connected_(false) {
    }
    
    void printCommand(const std::string& command, const std::string& description) {
        std::cout << "\n[" << description << "]" << std::endl;
        std::cout << "Command (hex): " << command << std::endl;
        
        // Show bytes
        auto bytes = hexStringToBytes(command);
        std::cout << "Bytes: ";
        for (size_t i = 0; i < bytes.size(); ++i) {
            std::cout << std::hex << std::setfill('0') << std::setw(2) 
                     << static_cast<int>(bytes[i]);
            if (i < bytes.size() - 1) std::cout << " ";
        }
        std::cout << std::dec << std::endl;
        
        // Parse and display
        std::cout << "  Device ID: " << command.substr(0, 12) << std::endl;
        std::cout << "  Speed: " << std::stoi(command.substr(12, 4), nullptr, 16) << std::endl;
        std::cout << "  Drift: " << std::stoi(command.substr(16, 4), nullptr, 16) << std::endl;
        std::cout << "  Steering: " << std::stoi(command.substr(20, 4), nullptr, 16) << std::endl;
        std::cout << "  Lights: " << command.substr(24, 4) << std::endl;
        std::cout << "  Checksum: " << command.substr(28, 2) << std::endl;
    }
    
    void testCommands() {
        std::cout << "========================================" << std::endl;
        std::cout << "BLE Command Test for RC Car" << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "Device MAC: " << device_mac_ << std::endl;
        std::cout << "Characteristic UUID: " << device_characteristic_uuid_ << std::endl;
        std::cout << "========================================" << std::endl;
        
        // Test various commands
        std::cout << "\n--- Test Commands ---\n" << std::endl;
        
        // Stop command
        std::string stop_cmd = generateCommand(0, 0, 0, 0);
        printCommand(stop_cmd, "STOP");
        
        // Start with lights on, slow forward
        std::string start_cmd = generateCommand(1, 20, 0, 0);
        printCommand(start_cmd, "START (Forward, Speed=20, Lights ON)");
        
        // Forward with right turn
        std::string right_cmd = generateCommand(1, 30, 0, 20);
        printCommand(right_cmd, "FORWARD RIGHT (Speed=30, Right Turn=20)");
        
        // Forward with left turn
        std::string left_cmd = generateCommand(1, 30, 20, 0);
        printCommand(left_cmd, "FORWARD LEFT (Speed=30, Left Turn=20)");
        
        // Reverse
        std::string reverse_cmd = generateCommand(1, 255 - 20, 0, 0);
        printCommand(reverse_cmd, "REVERSE (Reverse Speed=20)");
        
        std::cout << "\n========================================" << std::endl;
        std::cout << "Command generation test complete!" << std::endl;
        std::cout << "========================================" << std::endl;
    }
    
    void interactiveMode() {
        std::cout << "\n========================================" << std::endl;
        std::cout << "Interactive BLE Control Mode" << std::endl;
        std::cout << "========================================" << std::endl;
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
        std::cout << "========================================" << std::endl;
        
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
                    std::cout << "START: Speed=" << speed << ", Lights ON" << std::endl;
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
                    speed = 255 - 20;  // Reverse
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
                    // Send stop command before quitting
                    {
                        std::string stop = generateCommand(0, 0, 0, 0);
                        printCommand(stop, "FINAL STOP");
                    }
                    return;
                    
                default:
                    std::cout << "Unknown command. Type 'q' to quit." << std::endl;
                    continue;
            }
            
            // Generate and display command
            std::string command = generateCommand(light, speed, left, right);
            printCommand(command, "SENDING");
            
            // TODO: Actually send via BLE here
            std::cout << "\n[NOTE: BLE sending not implemented yet - command shown above]" << std::endl;
            std::cout << "[To actually send, implement BLE library integration]" << std::endl;
        }
    }
};

int main(int argc, char* argv[]) {
    // Default values (can be overridden via command line)
    std::string mac = "f9:af:3c:e2:d2:f5";  // Gray car
    std::string uuid = "6e400002-b5a3-f393-e0a9-e50e24dcca9e";
    
    if (argc > 1) {
        mac = argv[1];
    }
    if (argc > 2) {
        uuid = argv[2];
    }
    
    BLETest ble_test(mac, uuid);
    
    std::cout << "\nChoose mode:" << std::endl;
    std::cout << "1. Test command generation (no BLE connection)" << std::endl;
    std::cout << "2. Interactive mode (test commands)" << std::endl;
    std::cout << "\nEnter choice (1 or 2): ";
    
    std::string choice;
    std::getline(std::cin, choice);
    
    if (choice == "1") {
        ble_test.testCommands();
    } else if (choice == "2") {
        ble_test.testCommands();  // Show commands first
        ble_test.interactiveMode();
    } else {
        std::cout << "Invalid choice. Running test mode..." << std::endl;
        ble_test.testCommands();
    }
    
    return 0;
}
