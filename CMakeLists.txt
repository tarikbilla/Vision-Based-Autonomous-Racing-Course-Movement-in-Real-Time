cmake_minimum_required(VERSION 3.10)
project(VisionBasedRCCarControl)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(OpenCV REQUIRED)

# Try to find SimpleBLE (optional - for real BLE support)
find_package(simpleble QUIET)
if(simpleble_FOUND)
    message(STATUS "SimpleBLE found - Real BLE support enabled")
    add_definitions(-DHAVE_SIMPLEBLE)
    set(SIMPLEBLE_LIBS simpleble::simpleble)
else()
    message(STATUS "SimpleBLE not found - BLE will not work in non-simulation mode")
    message(STATUS "To install SimpleBLE:")
    message(STATUS "  macOS: brew install simpleble")
    message(STATUS "  Linux: Build from https://github.com/OpenBluetoothToolbox/SimpleBLE")
    set(SIMPLEBLE_LIBS "")
endif()

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${OpenCV_INCLUDE_DIRS})

# Try to find nlohmann_json
find_package(nlohmann_json 3.2.0 QUIET)
if(NOT nlohmann_json_FOUND)
    # If not found via find_package, try to find header only version
    find_path(NLOHMANN_JSON_INCLUDE_DIR nlohmann/json.hpp)
    if(NOT NLOHMANN_JSON_INCLUDE_DIR)
        message(STATUS "nlohmann_json not found, using header-only mode")
        # Download or assume it's in a standard location
    else()
        include_directories(${NLOHMANN_JSON_INCLUDE_DIR})
    endif()
endif()

# Source files
set(SOURCES
    src/main.cpp
    src/config_manager.cpp
    src/camera_capture.cpp
    src/object_tracker.cpp
    src/boundary_detection.cpp
    src/ble_handler.cpp
    src/commands.cpp
    src/control_orchestrator.cpp
    src/car_detector.cpp
)

# Create executable
add_executable(VisionBasedRCCarControl ${SOURCES})

# Link libraries
target_link_libraries(VisionBasedRCCarControl PRIVATE ${OpenCV_LIBS} ${SIMPLEBLE_LIBS})

if(TARGET nlohmann_json::nlohmann_json)
    target_link_libraries(VisionBasedRCCarControl PRIVATE nlohmann_json::nlohmann_json)
endif()

# Compiler flags for optimization
if(MSVC)
    target_compile_options(VisionBasedRCCarControl PRIVATE /W4 /O2)
else()
    target_compile_options(VisionBasedRCCarControl PRIVATE -Wall -Wextra -Wpedantic -O2)
endif()

# Installation
install(TARGETS VisionBasedRCCarControl DESTINATION bin)
install(FILES config/config.json DESTINATION config)

# Print configuration
message(STATUS "")
message(STATUS "VisionBasedRCCarControl Configuration:")
message(STATUS "  OpenCV Version: ${OpenCV_VERSION}")
message(STATUS "  OpenCV Libraries: ${OpenCV_LIBS}")
message(STATUS "  SimpleBLE Support: ${simpleble_FOUND}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "")

