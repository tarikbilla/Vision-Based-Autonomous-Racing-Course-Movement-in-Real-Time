cmake_minimum_required(VERSION 3.16)
project(VisionRC_FINAL LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── OpenCV (required) ──────────────────────────────────────────────
find_package(OpenCV REQUIRED)

# ── SimpleBLE (optional — for real BLE support) ────────────────────
find_package(simpleble QUIET)
if(simpleble_FOUND)
    message(STATUS "✓ SimpleBLE found — real BLE enabled")
    add_definitions(-DHAVE_SIMPLEBLE)
    set(SIMPLEBLE_LIBS simpleble::simpleble)
else()
    message(STATUS "✗ SimpleBLE not found — BLE will not work (use --no-ble)")
    message(STATUS "  macOS:  brew install simpleble")
    message(STATUS "  Linux:  build from https://github.com/OpenBluetoothToolbox/SimpleBLE")
    set(SIMPLEBLE_LIBS "")
endif()

# ── Sources ────────────────────────────────────────────────────────
set(SOURCES
    src/main.cpp
    src/ble_client.cpp
    src/path_detector.cpp
    src/car_detector.cpp
    src/imm_tracker.cpp
    src/controller.cpp
)

# ── Target ─────────────────────────────────────────────────────────
add_executable(VisionRC_FINAL ${SOURCES})

target_include_directories(VisionRC_FINAL PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(VisionRC_FINAL PRIVATE
    ${OpenCV_LIBS}
    ${SIMPLEBLE_LIBS}
)

# ── Compiler flags ─────────────────────────────────────────────────
if(MSVC)
    target_compile_options(VisionRC_FINAL PRIVATE /W4 /O2)
else()
    target_compile_options(VisionRC_FINAL PRIVATE -Wall -Wextra -O2)
endif()

# ── Summary ────────────────────────────────────────────────────────
message(STATUS "")
message(STATUS "═══ VisionRC_FINAL Configuration ═══")
message(STATUS "  OpenCV:     ${OpenCV_VERSION}")
message(STATUS "  SimpleBLE:  ${simpleble_FOUND}")
message(STATUS "  C++:        ${CMAKE_CXX_STANDARD}")
message(STATUS "")
