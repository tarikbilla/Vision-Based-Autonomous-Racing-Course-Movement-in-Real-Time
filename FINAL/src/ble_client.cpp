/*──────────────────────────────────────────────────────────────────────
 *  ble_client.cpp  –  SimpleBLE-based BLE backend
 *
 *  Robust scan → connect → service discovery → GATT write.
 *  Uses write_command first (faster, no response), falls back to
 *  write_request if the characteristic requires it.
 *──────────────────────────────────────────────────────────────────────*/
#include "ble_client.hpp"
#include "types.hpp"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <thread>
#include <stdexcept>

#ifdef HAVE_SIMPLEBLE
#include <simpleble/SimpleBLE.h>
#endif

namespace vrc {

/* ====================================================================
 *  SimpleBLE backend
 * ==================================================================*/
#ifdef HAVE_SIMPLEBLE

class SimpleBleClient final : public BleClient {
public:
    SimpleBleClient() = default;
    ~SimpleBleClient() override { disconnect(); }

    /* ── scan + connect by advertised name ─────────────────────── */
    bool connect_name(const std::string& target_name, double timeout_s) {
        std::cout << "[BLE] Scanning for '" << target_name
                  << "' (" << static_cast<int>(timeout_s) << "s)...\n";

        auto adapters = SimpleBLE::Adapter::get_adapters();
        if (adapters.empty()) {
            std::cerr << "[BLE] No Bluetooth adapters found!\n";
            return false;
        }
        auto& adapter = adapters[0];

        int timeout_ms = static_cast<int>(timeout_s * 1000);
        adapter.scan_for(timeout_ms);
        auto peripherals = adapter.scan_get_results();

        std::cout << "[BLE] Found " << peripherals.size() << " devices\n";

        // print first 5 for debug
        for (size_t i = 0; i < std::min<size_t>(5, peripherals.size()); ++i) {
            std::cout << "[BLE]   " << peripherals[i].identifier()
                      << "  " << peripherals[i].address() << "\n";
        }

        // find matching peripheral
        std::string target_lower = target_name;
        std::transform(target_lower.begin(), target_lower.end(),
                       target_lower.begin(), ::tolower);

        for (auto& p : peripherals) {
            std::string id = p.identifier();
            std::string id_lower = id;
            std::transform(id_lower.begin(), id_lower.end(),
                           id_lower.begin(), ::tolower);

            bool match = false;
            // exact match
            if (id == target_name)                      match = true;
            // case-insensitive substring
            if (id_lower.find(target_lower) != std::string::npos) match = true;
            // common DRiFT patterns
            if (id.find("DRiFT") == 0 || id.find("DRIFT") == 0 ||
                id.find("DR!FT") == 0)                 match = true;

            if (match) {
                std::cout << "[BLE] Match: " << id << " @ " << p.address() << "\n";
                return do_connect(p);
            }
        }

        std::cerr << "[BLE] Device '" << target_name << "' not found.\n";
        return false;
    }

    /* ── scan + connect by MAC address ─────────────────────────── */
    bool connect_address(const std::string& address, double timeout_s) {
        std::cout << "[BLE] Scanning for address '" << address
                  << "' (" << static_cast<int>(timeout_s) << "s)...\n";

        auto adapters = SimpleBLE::Adapter::get_adapters();
        if (adapters.empty()) {
            std::cerr << "[BLE] No Bluetooth adapters found!\n";
            return false;
        }
        auto& adapter = adapters[0];

        int timeout_ms = static_cast<int>(timeout_s * 1000);
        adapter.scan_for(timeout_ms);
        auto peripherals = adapter.scan_get_results();

        std::string addr_lower = address;
        std::transform(addr_lower.begin(), addr_lower.end(),
                       addr_lower.begin(), ::tolower);

        for (auto& p : peripherals) {
            std::string paddr = p.address();
            std::string paddr_lower = paddr;
            std::transform(paddr_lower.begin(), paddr_lower.end(),
                           paddr_lower.begin(), ::tolower);

            // also check if the name contains the address (macOS UUIDs)
            std::string pname = p.identifier();
            std::string pname_upper = pname;
            std::transform(pname_upper.begin(), pname_upper.end(),
                           pname_upper.begin(), ::toupper);
            std::string addr_upper = address;
            std::transform(addr_upper.begin(), addr_upper.end(),
                           addr_upper.begin(), ::toupper);

            if (paddr_lower == addr_lower ||
                pname_upper.find(addr_upper) != std::string::npos) {
                std::cout << "[BLE] Match: " << pname << " @ " << paddr << "\n";
                return do_connect(p);
            }
        }

        std::cerr << "[BLE] Address '" << address << "' not found.\n";
        return false;
    }

    /* ── interface ──────────────────────────────────────────────── */
    bool is_connected() const override { return connected_; }

    bool write_gatt_char(const std::string& /* uuid */,
                         const std::vector<uint8_t>& data) override {
        if (!connected_) return false;
        SimpleBLE::ByteArray bytes(data.begin(), data.end());

        // Try write_command first (no-response, lower latency)
        try {
            peripheral_.write_command(service_uuid_, char_uuid_, bytes);
            return true;
        } catch (...) {}

        // Fallback to write_request
        try {
            peripheral_.write_request(service_uuid_, char_uuid_, bytes);
            return true;
        } catch (const std::exception& e) {
            std::cerr << "[BLE] write failed: " << e.what() << "\n";
            return false;
        }
    }

private:
    /* ── connect + service discovery ───────────────────────────── */
    bool do_connect(SimpleBLE::Peripheral& p) {
        std::cout << "[BLE] Connecting to " << p.identifier() << "...\n";
        try {
            p.connect();
        } catch (const std::exception& e) {
            std::cerr << "[BLE] connect() failed: " << e.what() << "\n";
            return false;
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));

        // discover write characteristic
        std::string target_uuid = WRITE_UUID;
        std::transform(target_uuid.begin(), target_uuid.end(),
                       target_uuid.begin(), ::tolower);

        for (auto& svc : p.services()) {
            for (auto& ch : svc.characteristics()) {
                std::string cuuid = ch.uuid();
                std::transform(cuuid.begin(), cuuid.end(),
                               cuuid.begin(), ::tolower);
                if (cuuid == target_uuid) {
                    service_uuid_ = svc.uuid();
                    char_uuid_    = ch.uuid();
                    peripheral_   = p;
                    connected_    = true;
                    std::cout << "[BLE] Connected ✓  char=" << char_uuid_ << "\n";
                    return true;
                }
            }
        }

        // fallback: first characteristic
        for (auto& svc : p.services()) {
            for (auto& ch : svc.characteristics()) {
                service_uuid_ = svc.uuid();
                char_uuid_    = ch.uuid();
                peripheral_   = p;
                connected_    = true;
                std::cout << "[BLE] Connected (fallback char=" << char_uuid_ << ")\n";
                return true;
            }
        }

        std::cerr << "[BLE] No writable characteristic found.\n";
        return false;
    }

    void disconnect() {
        if (connected_) {
            try { peripheral_.disconnect(); } catch (...) {}
            connected_ = false;
        }
    }

    SimpleBLE::Peripheral peripheral_;
    std::string service_uuid_;
    std::string char_uuid_;
    bool connected_ = false;
};

/* ── factory functions (SimpleBLE available) ───────────────────── */
std::unique_ptr<BleClient> connect_by_name(const std::string& name,
                                           double scan_timeout_s) {
    auto c = std::make_unique<SimpleBleClient>();
    if (c->connect_name(name, scan_timeout_s)) return c;
    return nullptr;
}

std::unique_ptr<BleClient> connect_by_address(const std::string& address,
                                              double scan_timeout_s) {
    auto c = std::make_unique<SimpleBleClient>();
    if (c->connect_address(address, scan_timeout_s)) return c;
    return nullptr;
}

void ble_discover(double scan_timeout_s) {
    std::cout << "[BLE] Scanning " << static_cast<int>(scan_timeout_s)
              << "s for BLE devices...\n";

    auto adapters = SimpleBLE::Adapter::get_adapters();
    if (adapters.empty()) {
        std::cerr << "[BLE] No adapters found.\n";
        return;
    }
    auto& adapter = adapters[0];
    adapter.scan_for(static_cast<int>(scan_timeout_s * 1000));
    auto peripherals = adapter.scan_get_results();

    std::cout << "[BLE] Devices found (" << peripherals.size() << "):\n";
    for (auto& p : peripherals) {
        std::cout << "  " << std::setw(40) << std::left << p.address()
                  << "  " << p.identifier() << "\n";
    }
}

#else  // !HAVE_SIMPLEBLE – stub implementations

std::unique_ptr<BleClient> connect_by_name(const std::string&, double) {
    std::cerr << "[BLE] SimpleBLE not available. Install it or use --no-ble.\n"
              << "[BLE]   macOS:  brew install simpleble\n"
              << "[BLE]   Linux:  build from https://github.com/OpenBluetoothToolbox/SimpleBLE\n";
    return nullptr;
}
std::unique_ptr<BleClient> connect_by_address(const std::string&, double) {
    std::cerr << "[BLE] SimpleBLE not available.\n";
    return nullptr;
}
void ble_discover(double) {
    std::cerr << "[BLE] SimpleBLE not available.\n";
}

#endif  // HAVE_SIMPLEBLE

}  // namespace vrc
