cmake_minimum_required(VERSION 3.10)
project(VisionBasedRCCarControl_RBP4)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenCV REQUIRED)

find_package(simpleble QUIET)
if(simpleble_FOUND)
    message(STATUS "SimpleBLE found - Real BLE support enabled")
    add_definitions(-DHAVE_SIMPLEBLE)
    set(SIMPLEBLE_LIBS simpleble::simpleble)
else()
    message(STATUS "SimpleBLE not found - BLE will not work in non-simulation mode")
    set(SIMPLEBLE_LIBS "")
endif()

include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${OpenCV_INCLUDE_DIRS})

find_package(nlohmann_json 3.2.0 QUIET)
if(NOT nlohmann_json_FOUND)
    find_path(NLOHMANN_JSON_INCLUDE_DIR nlohmann/json.hpp)
    if(NLOHMANN_JSON_INCLUDE_DIR)
        include_directories(${NLOHMANN_JSON_INCLUDE_DIR})
    endif()
endif()

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

add_executable(VisionBasedRCCarControl_RBP4 ${SOURCES})

target_link_libraries(VisionBasedRCCarControl_RBP4 PRIVATE ${OpenCV_LIBS} ${SIMPLEBLE_LIBS})

if(TARGET nlohmann_json::nlohmann_json)
    target_link_libraries(VisionBasedRCCarControl_RBP4 PRIVATE nlohmann_json::nlohmann_json)
endif()

if(MSVC)
    target_compile_options(VisionBasedRCCarControl_RBP4 PRIVATE /W4 /O2)
else()
    target_compile_options(VisionBasedRCCarControl_RBP4 PRIVATE -Wall -Wextra -Wpedantic -O2)
endif()

install(TARGETS VisionBasedRCCarControl_RBP4 DESTINATION bin)
install(FILES config/config_rbp4_ultra_low.json DESTINATION config)

message(STATUS "")
message(STATUS "VisionBasedRCCarControl_RBP4 Configuration:")
message(STATUS "  OpenCV Version: ${OpenCV_VERSION}")
message(STATUS "  OpenCV Libraries: ${OpenCV_LIBS}")
message(STATUS "  SimpleBLE Support: ${simpleble_FOUND}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "")
