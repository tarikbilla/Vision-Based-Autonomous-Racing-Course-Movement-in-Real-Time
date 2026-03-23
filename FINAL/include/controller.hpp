/*──────────────────────────────────────────────────────────────────────
 *  controller.hpp  –  Pure-pursuit autonomy + control loop
 *──────────────────────────────────────────────────────────────────────*/
#ifndef VISIONRC_CONTROLLER_HPP
#define VISIONRC_CONTROLLER_HPP

#include "types.hpp"
#include "ble_client.hpp"

#include <memory>

namespace vrc {

/* ── autonomy tuning constants ─────────────────────────────────── */
constexpr double CONTROL_HZ              = 20.0;

constexpr double AUTO_MIN_SPEED_RAW      = 3000.0;
constexpr double AUTO_MID_SPEED_RAW      = 3600.0;
constexpr double AUTO_MAX_SPEED_RAW      = 8200.0;
constexpr double AUTO_BRAKE_NO_TRACK_MS  = 600.0;
constexpr double AUTO_HARD_EHEAD_DEG     = 140.0;
constexpr double AUTO_HARD_ELAT_PX       = 220.0;

constexpr double AUTO_PP_K_ALPHA         = 0.15;
constexpr double AUTO_PP_K_LAT           = 0.014;
constexpr double AUTO_PP_ALPHA_DEADBAND  = 4.5;
constexpr double AUTO_PP_ELAT_DEADBAND   = 10.0;
constexpr double AUTO_PP_RATE_DEG        = 1.10;
constexpr double AUTO_PP_FILTER          = 0.42;
constexpr double AUTO_PP_NEAR_STRAIGHT   = 0.38;
constexpr double AUTO_PP_NEAR_ALPHA      = 7.0;
constexpr double AUTO_PP_NEAR_ELAT       = 9.0;
constexpr double AUTO_PP_DELAY_COMP_S    = 0.20;
constexpr double AUTO_PP_D_ALPHA         = 0.055;
constexpr double AUTO_PP_D_ELAT          = 0.018;
constexpr double AUTO_PP_RATE_ALPHA      = 0.30;
constexpr double AUTO_PP_RATE_ELAT       = 0.22;
constexpr double AUTO_PP_D_MAX           = 3.0;
constexpr double AUTO_PP_SOFTSAT_DEG     = 12.5;

/* ── compute_abc (speed / steer → packet values) ──────────────── */
AbcResult compute_abc();

/* ── control loop (runs at CONTROL_HZ) ────────────────────────── */
void control_loop(BleClient* ble);

/* ── vision + detection thread (runs camera, feeds VisionData) ── */
void cv_thread();

/* ── logging ───────────────────────────────────────────────────── */
void start_logging();
void stop_logging();
void log_row(const AbcResult& r);

/* ── time utility ──────────────────────────────────────────────── */
double now_sec();

}  // namespace vrc

#endif  // VISIONRC_CONTROLLER_HPP
