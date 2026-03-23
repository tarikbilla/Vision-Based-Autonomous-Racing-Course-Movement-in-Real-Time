/*──────────────────────────────────────────────────────────────────────
 *  ble_client.hpp  –  BLE abstraction (SimpleBLE backend)
 *
 *  Provides robust scan / connect / GATT-write using the SimpleBLE
 *  library.  Falls back to a no-op DummyBleClient when --no-ble is
 *  used or SimpleBLE is not available.
 *──────────────────────────────────────────────────────────────────────*/
#ifndef VISIONRC_BLE_CLIENT_HPP
#define VISIONRC_BLE_CLIENT_HPP

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace vrc {

/* ── abstract interface ─────────────────────────────────────────── */
class BleClient {
public:
    virtual ~BleClient() = default;
    virtual bool is_connected() const = 0;
    virtual bool write_gatt_char(const std::string& uuid,
                                 const std::vector<uint8_t>& data) = 0;
};

/* ── no-op client (--no-ble mode) ───────────────────────────────── */
class DummyBleClient final : public BleClient {
public:
    bool is_connected() const override { return true; }
    bool write_gatt_char(const std::string&,
                         const std::vector<uint8_t>&) override { return true; }
};

/* ── factory ────────────────────────────────────────────────────── */

// Connect by advertised name (scan → connect → service discovery → char)
std::unique_ptr<BleClient> connect_by_name(const std::string& name,
                                           double scan_timeout_s);

// Connect by MAC / address string
std::unique_ptr<BleClient> connect_by_address(const std::string& address,
                                              double scan_timeout_s);

// Discover & list all BLE peripherals for --discover mode
void ble_discover(double scan_timeout_s);

}  // namespace vrc

#endif  // VISIONRC_BLE_CLIENT_HPP
