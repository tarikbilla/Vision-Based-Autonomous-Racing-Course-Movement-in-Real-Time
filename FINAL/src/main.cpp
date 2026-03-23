/*──────────────────────────────────────────────────────────────────────
 *  main.cpp  –  VisionRC_FINAL entry point
 *
 *  Combines:
 *    • Smooth pure-pursuit controller  (from MAC/main.mm)
 *    • Robust SimpleBLE scan/connect   (from root project)
 *  into a single cross-platform C++ binary.
 *──────────────────────────────────────────────────────────────────────*/
#include "types.hpp"
#include "ble_client.hpp"
#include "controller.hpp"

#include <csignal>
#include <iostream>
#include <memory>
#include <thread>

/* ================================================================
 *  Global singletons (declared extern in types.hpp)
 * ================================================================*/
namespace vrc {
    State                                g_state;
    std::mutex                           g_state_mtx;
    VisionData                           g_vision;
    std::mutex                           g_vision_mtx;
    std::atomic<bool>                    g_stop{false};
    std::mutex                           g_centerline_mtx;
    std::optional<std::vector<cv::Point2d>> g_new_centerline;
}

/* ── signal handler for clean shutdown ────────────────────────── */
static std::unique_ptr<vrc::BleClient> g_ble;

static void signal_handler(int) {
    std::cout << "\n[SIGNAL] Shutting down...\n";
    vrc::g_stop.store(true);
}

/* ================================================================
 *  Argument parser
 * ================================================================*/
static void print_usage(const char* argv0) {
    std::cout
        << "Usage:\n"
        << "  " << argv0 << " --name <ble-name>  [options]\n"
        << "  " << argv0 << " --address <mac>     [options]\n"
        << "  " << argv0 << " --discover          [--scan-timeout s]\n"
        << "  " << argv0 << " --no-ble            [options]\n"
        << "\nOptions:\n"
        << "  --scan-timeout <s>      BLE scan time   (default 6)\n"
        << "  --auto-steer-sign <v>   Steer sign      (default 1)\n"
        << "  --flip-centerline       Reverse path direction\n"
        << "  --max-deg <v>           Max steering deg (default 9)\n"
        << "  --steer-smooth <v>      Smoothing rate   (default 42)\n"
        << "  --ratio <v>             C/B ratio        (default 0.80)\n"
        << "  --c-sign <v>            C sign           (default -1)\n"
        << "  --speed-k <v>           Speed drop coeff (default 0.03)\n"
        << "  --ble-backend <v>       Force backend    (auto|simpleble)\n";
}

static std::optional<vrc::Args> parse_args(int argc, char** argv) {
    vrc::Args a;
    for (int i = 1; i < argc; ++i) {
        std::string s = argv[i];
        auto need = [&](const std::string& flag) -> std::optional<std::string> {
            if (i+1 >= argc) { std::cerr << "Missing value for " << flag << "\n"; return std::nullopt; }
            return std::string(argv[++i]);
        };
        if      (s=="--address")         { auto v=need(s); if(!v) return std::nullopt; a.address=*v; }
        else if (s=="--name")            { auto v=need(s); if(!v) return std::nullopt; a.name=*v; }
        else if (s=="--discover")        { a.discover=true; }
        else if (s=="--scan-timeout")    { auto v=need(s); if(!v) return std::nullopt; a.scan_timeout=std::stod(*v); }
        else if (s=="--no-ble")          { a.no_ble=true; }
        else if (s=="--auto-steer-sign") { auto v=need(s); if(!v) return std::nullopt; a.auto_steer_sign=std::stod(*v); }
        else if (s=="--flip-centerline") { a.flip_centerline=true; }
        else if (s=="--max-deg")         { auto v=need(s); if(!v) return std::nullopt; a.max_deg=std::stod(*v); }
        else if (s=="--steer-smooth")    { auto v=need(s); if(!v) return std::nullopt; a.steer_smooth=std::stod(*v); }
        else if (s=="--ratio")           { auto v=need(s); if(!v) return std::nullopt; a.ratio=std::stod(*v); }
        else if (s=="--c-sign")          { auto v=need(s); if(!v) return std::nullopt; a.c_sign=std::stod(*v); }
        else if (s=="--speed-k")         { auto v=need(s); if(!v) return std::nullopt; a.speed_k=std::stod(*v); }
        else if (s=="--ble-backend")     { auto v=need(s); if(!v) return std::nullopt; a.ble_backend=*v; }
        else if (s=="-h"||s=="--help")   { print_usage(argv[0]); return std::nullopt; }
        else { std::cerr << "Unknown arg: " << s << "\n"; print_usage(argv[0]); return std::nullopt; }
    }
    if (!a.no_ble && !a.discover && !a.address && !a.name) {
        std::cerr << "Provide --name, --address, --discover, or --no-ble.\n";
        return std::nullopt;
    }
    return a;
}

/* ================================================================
 *  main
 * ================================================================*/
int main(int argc, char** argv) {
    auto args_opt = parse_args(argc, argv);
    if (!args_opt) return 1;
    const auto args = *args_opt;

    /* ── apply tuning params ─────────────────────────────────── */
    {
        std::scoped_lock lk(vrc::g_state_mtx);
        vrc::g_state.auto_steer_sign = args.auto_steer_sign;
        vrc::g_state.flip_centerline = args.flip_centerline;
        vrc::g_state.max_deg         = args.max_deg;
        vrc::g_state.steer_smooth    = args.steer_smooth;
        vrc::g_state.ratio           = args.ratio;
        vrc::g_state.c_sign          = args.c_sign;
        vrc::g_state.speed_k         = args.speed_k;
        vrc::g_state.speed_step      = 90.0;
        vrc::g_state.last_track_t    = vrc::now_sec();
    }

    std::signal(SIGINT,  signal_handler);
    std::signal(SIGTERM, signal_handler);

    /* ── BLE setup ───────────────────────────────────────────── */
    if (args.no_ble) {
        std::cout << "[BLE] Disabled — running in no-Bluetooth mode\n";
        g_ble = std::make_unique<vrc::DummyBleClient>();
    } else if (args.discover) {
        vrc::ble_discover(args.scan_timeout);
        return 0;
    } else if (args.name) {
        g_ble = vrc::connect_by_name(*args.name, args.scan_timeout);
        if (!g_ble) { std::cerr << "[BLE] Connection failed.\n"; return 2; }
    } else if (args.address) {
        g_ble = vrc::connect_by_address(*args.address, args.scan_timeout);
        if (!g_ble) { std::cerr << "[BLE] Connection failed.\n"; return 2; }
    }

    /* ── launch control loop + vision thread ─────────────────── */
    std::thread ctl_thread(vrc::control_loop, g_ble.get());

    vrc::cv_thread();           // runs on main thread (needs UI / cv::imshow)
    vrc::g_stop.store(true);

    if (ctl_thread.joinable()) ctl_thread.join();

    /* ── clean shutdown ──────────────────────────────────────── */
    vrc::stop_logging();
    if (g_ble && g_ble->is_connected()) {
        g_ble->write_gatt_char(vrc::WRITE_UUID, vrc::brake_packet());
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        g_ble->write_gatt_char(vrc::WRITE_UUID, vrc::idle_packet());
    }

    std::cout << "\n[STOP]\n";
    return 0;
}
