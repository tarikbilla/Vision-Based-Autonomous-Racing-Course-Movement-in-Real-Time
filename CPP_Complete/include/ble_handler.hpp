#ifndef BLE_HANDLER_HPP
#define BLE_HANDLER_HPP

#include "commands.hpp"
#include <string>
#include <memory>
#include <vector>

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
    RealBLEClient(const BLETarget& target);
    ~RealBLEClient() override;
    
    bool connect() override;
    void disconnect() override;
    bool sendControl(const ControlVector& control) override;
    bool isConnected() const override;
    
    // Platform-specific implementations
    std::vector<std::pair<std::string, std::string>> listDevices();
    
private:
    BLETarget target_;
    bool connected_;
    void* platformData_; // Platform-specific BLE data
    
    // Platform-specific methods
    bool platformConnect();
    bool platformSendControl(const ControlVector& control);
};

std::unique_ptr<BLEClient> createBLEClient(const BLETarget& target, bool simulate);

#endif // BLE_HANDLER_HPP
