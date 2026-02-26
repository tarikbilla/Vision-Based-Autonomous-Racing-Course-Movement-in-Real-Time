#ifndef BLE_HANDLER_HPP
#define BLE_HANDLER_HPP

#include "commands.hpp"
#include <string>
#include <memory>
#include <vector>
#include <optional>

struct BLETarget {
    std::string device_mac;
    std::string characteristic_uuid;
    std::string device_identifier;
};

class BLEClient {
public:
    virtual ~BLEClient() = default;
    virtual bool connect() = 0;
    virtual void disconnect() = 0;
    virtual bool sendControl(const ControlVector& control) = 0;
    virtual bool isConnected() const = 0;
};

class FakeBLEClient : public BLEClient {
public:
    FakeBLEClient(const BLETarget& target);
    ~FakeBLEClient() override;
    
    bool connect() override;
    void disconnect() override;
    bool sendControl(const ControlVector& control) override;
    bool isConnected() const override;
    
private:
    BLETarget target_;
    bool connected_;
};

class RealBLEClient : public BLEClient {
public:
    RealBLEClient(
        const BLETarget& target,
        const std::string& device_name_or_mac = "",
        int scan_timeout_ms = 10000,
        std::optional<int> device_index = std::nullopt
    );
    ~RealBLEClient() override;
    
    bool connect() override;
    void disconnect() override;
    bool sendControl(const ControlVector& control) override;
    bool isConnected() const override;
    
    // Device discovery
    std::vector<std::pair<std::string, std::string>> listDevices();
    
private:
    BLETarget target_;
    std::string device_name_or_mac_;
    int scan_timeout_ms_;
    std::optional<int> device_index_;
    bool connected_;
    void* platformData_; // Platform-specific BLE data (SimpleBLE adapter/peripheral)
    std::string service_uuid_;
    std::string char_uuid_;
    
    // Helper methods matching Python implementation
    std::vector<std::pair<std::string, std::string>> scanPeripherals();
    void* findCandidate(); // Returns SimpleBLE peripheral
    std::optional<std::pair<std::string, std::string>> selectCharacteristic(void* peripheral);
};

std::unique_ptr<BLEClient> createBLEClient(
    const BLETarget& target,
    bool simulate,
    const std::string& device_hint = "",
    std::optional<int> device_index = std::nullopt
);

#endif // BLE_HANDLER_HPP
