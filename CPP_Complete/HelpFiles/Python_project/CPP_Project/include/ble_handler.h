#ifndef BLE_HANDLER_H
#define BLE_HANDLER_H

#include <string>
#include <atomic>
#include <thread>
#include <mutex>
#include "types.h"

namespace rc_car {

// BLE Handler interface
// Note: Actual BLE implementation depends on available library (BlueZ, simpleble-cpp, etc.)
// This is a placeholder interface that can be implemented with the chosen BLE library

class BLEHandler {
private:
    std::string device_mac_;
    std::string device_characteristic_uuid_;
    std::string device_identifier_;
    
    bool connected_;
    std::atomic<bool> running_;
    std::thread send_thread_;
    mutable std::mutex control_mutex_;  // Mutable to allow locking in const member functions
    
    ControlVector current_control_;
    int command_send_rate_hz_;  // Target rate (e.g., 200 Hz)
    
    void sendLoop();
    std::string generateCommand(const ControlVector& control) const;
    std::string intToHex(int value, int digits = 4) const;
    
    // BLE connection methods (to be implemented based on chosen library)
    bool connectToDevice();
    void disconnectFromDevice();
    bool sendCommand(const std::string& command);
    
public:
    BLEHandler();
    BLEHandler(const std::string& mac_address, const std::string& characteristic_uuid);
    ~BLEHandler();
    
    bool initialize(const std::string& mac_address, const std::string& characteristic_uuid);
    bool connect();
    void disconnect();
    
    void setControl(const ControlVector& control);
    ControlVector getControl() const;
    
    void startSending();
    void stopSending();
    
    bool isConnected() const { return connected_; }
    
    void setCommandRate(int hz) { command_send_rate_hz_ = hz; }
    int getCommandRate() const { return command_send_rate_hz_; }
    
    // Light control helpers
    void setLight(bool on);
    void setSpeed(int speed);
    void setReverseSpeed(int speed);
    void setSteering(int left_value, int right_value);
    
    // Emergency stop
    void emergencyStop();
};

} // namespace rc_car

#endif // BLE_HANDLER_H
