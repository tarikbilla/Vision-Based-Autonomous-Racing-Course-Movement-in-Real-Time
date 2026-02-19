#include "ble_handler.hpp"
#include <iostream>
#include <thread>
#include <algorithm>
#include <stdexcept>

// Try to include SimpleBLE if available
#ifdef HAVE_SIMPLEBLE
#include <simpleble/SimpleBLE.h>
using namespace SimpleBLE;
#endif

// ========================
// FakeBLEClient Implementation
// ========================

FakeBLEClient::FakeBLEClient(const BLETarget& target)
    : target_(target), connected_(false) {
}

FakeBLEClient::~FakeBLEClient() {
    disconnect();
}

bool FakeBLEClient::connect() {
    std::cout << "[*] Fake BLE mode - simulating connection...\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    connected_ = true;
    std::cout << "[✓] Fake BLE connection established\n";
    return true;
}

void FakeBLEClient::disconnect() {
    if (connected_) {
        std::cout << "[*] Fake BLE disconnected\n";
    }
    connected_ = false;
}

bool FakeBLEClient::sendControl(const ControlVector& control) {
    if (!connected_) {
        return false;
    }
    auto cmd = Commands::buildCommand(control);
    // Simulate sending
    return true;
}

bool FakeBLEClient::isConnected() const {
    return connected_;
}

// ========================
// RealBLEClient Implementation
// ========================

RealBLEClient::RealBLEClient(
    const BLETarget& target,
    const std::string& device_name_or_mac,
    int scan_timeout_ms,
    std::optional<int> device_index
)
    : target_(target),
      device_name_or_mac_(device_name_or_mac),
      scan_timeout_ms_(scan_timeout_ms),
      device_index_(device_index),
      connected_(false),
      platformData_(nullptr) {
}

RealBLEClient::~RealBLEClient() {
    disconnect();
#ifdef HAVE_SIMPLEBLE
    if (platformData_) {
        delete static_cast<Peripheral*>(platformData_);
        platformData_ = nullptr;
    }
#endif
}

#ifdef HAVE_SIMPLEBLE

std::vector<std::pair<std::string, std::string>> RealBLEClient::scanPeripherals() {
    std::cout << "[*] Scanning for BLE devices...\n";
    
    auto adapters = Adapter::get_adapters();
    if (adapters.empty()) {
        throw std::runtime_error("No BLE adapters found");
    }
    
    Adapter adapter = adapters[0];
    adapter.scan_for(scan_timeout_ms_);
    
    auto peripherals = adapter.scan_get_results();
    std::vector<std::pair<std::string, std::string>> devices;
    
    for (const auto& p : peripherals) {
        devices.push_back({p.identifier(), p.address()});
    }
    
    std::cout << "[✓] Found " << devices.size() << " BLE devices\n";
    return devices;
}

void* RealBLEClient::findCandidate() {
    std::cout << "[*] Scanning for BLE devices...\n";
    
    auto adapters = Adapter::get_adapters();
    if (adapters.empty()) {
        throw std::runtime_error("No BLE adapters found");
    }
    
    Adapter adapter = adapters[0];
    adapter.scan_for(scan_timeout_ms_);
    
    auto peripherals = adapter.scan_get_results();
    std::vector<std::pair<std::string, std::string>> scanned_devices;
    
    for (const auto& p : peripherals) {
        scanned_devices.push_back({p.identifier(), p.address()});
    }
    
    std::cout << "[✓] Found " << scanned_devices.size() << " BLE devices\n";
    
    // Use device_index if provided
    if (device_index_.has_value()) {
        int idx = device_index_.value();
        if (idx < 0 || idx >= static_cast<int>(peripherals.size())) {
            throw std::runtime_error(
                "Device index " + std::to_string(idx) + 
                " out of range (0-" + std::to_string(peripherals.size() - 1) + ")"
            );
        }
        std::cout << "[*] Selected device by index [" << idx << "]: " 
                  << peripherals[idx].identifier() << " - " << peripherals[idx].address() << "\n";
        return new Peripheral(peripherals[idx]);
    }
    
    // Find matching candidates
    std::vector<Peripheral> candidates;
    
    for (const auto& peripheral : peripherals) {
        std::string name = peripheral.identifier();
        std::string address = peripheral.address();
        std::string name_lower = name;
        std::string name_upper = name;
        std::string address_lower = address;
        std::string target_mac_lower = target_.device_mac;
        
        // Convert to lowercase
        std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(), ::tolower);
        std::transform(name_upper.begin(), name_upper.end(), name_upper.begin(), ::toupper);
        std::transform(address_lower.begin(), address_lower.end(), address_lower.begin(), ::tolower);
        std::transform(target_mac_lower.begin(), target_mac_lower.end(), target_mac_lower.begin(), ::tolower);
        
        bool match = false;
        
        // Check device_name_or_mac if provided
        if (!device_name_or_mac_.empty()) {
            std::string hint_lower = device_name_or_mac_;
            std::transform(hint_lower.begin(), hint_lower.end(), hint_lower.begin(), ::tolower);
            
            if (name_lower.find(hint_lower) != std::string::npos ||
                address_lower == hint_lower) {
                match = true;
            }
        } else {
            // Match drift car patterns (same as Python)
            if (name_lower.find("drift") != std::string::npos ||
                name_lower.find("dr!ft") != std::string::npos ||
                name.find("DRiFT") == 0 ||
                name.find("DRIFT") == 0 ||
                name.find("DR!FT") == 0 ||
                name_upper.find("ED5C2384488D") != std::string::npos ||
                name_upper.find("F9AF3CE2D2F5") != std::string::npos ||
                address_lower == target_mac_lower) {
                match = true;
            }
        }
        
        if (match) {
            std::cout << "[*] Found candidate: " << name << " - " << address << "\n";
            candidates.push_back(peripheral);
        }
    }
    
    if (candidates.empty()) {
        std::string preview;
        int count = std::min(10, static_cast<int>(scanned_devices.size()));
        for (int i = 0; i < count; ++i) {
            if (i > 0) preview += ", ";
            preview += scanned_devices[i].first + " (" + scanned_devices[i].second + ")";
        }
        throw std::runtime_error(
            "DRIFT car not found during scan. Found " + 
            std::to_string(scanned_devices.size()) + 
            " devices: " + preview
        );
    }
    
    std::cout << "[✓] Selected device: " << candidates[0].identifier() 
              << " - " << candidates[0].address() << "\n";
    return new Peripheral(candidates[0]);
}

std::optional<std::pair<std::string, std::string>> RealBLEClient::selectCharacteristic(void* peripheral_ptr) {
    Peripheral* peripheral = static_cast<Peripheral*>(peripheral_ptr);
    
    auto services = peripheral->services();
    
    // First try to find exact UUID match
    for (const auto& service : services) {
        for (const auto& characteristic : service.characteristics()) {
            std::string char_uuid = characteristic.uuid();
            std::string target_uuid = target_.characteristic_uuid;
            
            std::transform(char_uuid.begin(), char_uuid.end(), char_uuid.begin(), ::tolower);
            std::transform(target_uuid.begin(), target_uuid.end(), target_uuid.begin(), ::tolower);
            
            if (char_uuid == target_uuid) {
                std::cout << "[✓] Found target characteristic: " << characteristic.uuid() << "\n";
                return std::make_pair(service.uuid(), characteristic.uuid());
            }
        }
    }
    
    // Fallback to first writable characteristic
    for (const auto& service : services) {
        for (const auto& characteristic : service.characteristics()) {
            std::cout << "[*] Using fallback characteristic: " << characteristic.uuid() << "\n";
            return std::make_pair(service.uuid(), characteristic.uuid());
        }
    }
    
    return std::nullopt;
}

bool RealBLEClient::connect() {
    try {
        std::cout << "[*] Connecting to BLE device: " << target_.device_mac << "\n";
        
        // Find and connect to device
        Peripheral* peripheral = static_cast<Peripheral*>(findCandidate());
        platformData_ = peripheral;
        
        std::cout << "[*] Connecting...\n";
        peripheral->connect();
        
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        // Select characteristic
        auto characteristic = selectCharacteristic(peripheral);
        if (!characteristic.has_value()) {
            throw std::runtime_error("No writable characteristic found");
        }
        
        service_uuid_ = characteristic->first;
        char_uuid_ = characteristic->second;
        
        connected_ = true;
        std::cout << "[✓] BLE connected successfully\n";
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "[!] BLE connection failed: " << e.what() << "\n";
        connected_ = false;
        return false;
    }
}

void RealBLEClient::disconnect() {
    if (connected_ && platformData_) {
        Peripheral* peripheral = static_cast<Peripheral*>(platformData_);
        peripheral->disconnect();
        std::cout << "[*] BLE disconnected\n";
    }
    connected_ = false;
}

bool RealBLEClient::sendControl(const ControlVector& control) {
    if (!connected_ || !platformData_) {
        return false;
    }
    
    try {
        Peripheral* peripheral = static_cast<Peripheral*>(platformData_);
        
        std::string command_hex = Commands::buildCommand(control);
        
        // Convert hex string to bytes
        std::vector<uint8_t> command_bytes;
        for (size_t i = 0; i < command_hex.length(); i += 2) {
            std::string byte_str = command_hex.substr(i, 2);
            uint8_t byte = static_cast<uint8_t>(std::stoi(byte_str, nullptr, 16));
            command_bytes.push_back(byte);
        }
        
        SimpleBLE::ByteArray data(command_bytes.begin(), command_bytes.end());
        
        // Try write_request first, fallback to write_command
        try {
            peripheral->write_request(service_uuid_, char_uuid_, data);
            return true;
        } catch (...) {
            try {
                peripheral->write_command(service_uuid_, char_uuid_, data);
                return true;
            } catch (...) {
                return false;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[!] Failed to send control: " << e.what() << "\n";
        return false;
    }
}

std::vector<std::pair<std::string, std::string>> RealBLEClient::listDevices() {
    return scanPeripherals();
}

#else // !HAVE_SIMPLEBLE

// Fallback implementation without SimpleBLE
std::vector<std::pair<std::string, std::string>> RealBLEClient::scanPeripherals() {
    std::cerr << "[!] SimpleBLE library not available. Please install SimpleBLE.\n";
    std::cerr << "[!] On macOS: brew install simpleble\n";
    std::cerr << "[!] On Linux: Build from https://github.com/OpenBluetoothToolbox/SimpleBLE\n";
    return {};
}

void* RealBLEClient::findCandidate() {
    throw std::runtime_error("SimpleBLE library not available. Cannot scan for devices.");
}

std::optional<std::pair<std::string, std::string>> RealBLEClient::selectCharacteristic(void* peripheral) {
    return std::nullopt;
}

bool RealBLEClient::connect() {
    std::cerr << "[!] SimpleBLE library not available\n";
    std::cerr << "[!] Please install SimpleBLE to use real BLE functionality\n";
    std::cerr << "[!] On macOS: brew install simpleble\n";
    std::cerr << "[!] On Linux: Build from https://github.com/OpenBluetoothToolbox/SimpleBLE\n";
    std::cerr << "[!] Use --simulate flag for testing without hardware\n";
    return false;
}

void RealBLEClient::disconnect() {
    connected_ = false;
}

bool RealBLEClient::sendControl(const ControlVector& control) {
    return false;
}

std::vector<std::pair<std::string, std::string>> RealBLEClient::listDevices() {
    std::cerr << "[!] SimpleBLE library not available\n";
    return {};
}

#endif // HAVE_SIMPLEBLE

bool RealBLEClient::isConnected() const {
    return connected_;
}

// ========================
// Factory Function
// ========================

std::unique_ptr<BLEClient> createBLEClient(
    const BLETarget& target,
    bool simulate,
    const std::string& device_hint,
    std::optional<int> device_index
) {
    if (simulate) {
        return std::make_unique<FakeBLEClient>(target);
    } else {
        return std::make_unique<RealBLEClient>(target, device_hint, 10000, device_index);
    }
}
