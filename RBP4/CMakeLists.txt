cmake_minimum_required(VERSION 3.16)
project(autonomous_centerline_controller_pure_pursuit_delay_fixed_cpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenCV REQUIRED)

find_package(simpleble QUIET)
if(simpleble_FOUND)
    message(STATUS "SimpleBLE found - enabling SimpleBLE backend for BLE")
    add_definitions(-DHAVE_SIMPLEBLE)
    set(SIMPLEBLE_LIBS simpleble::simpleble)
else()
    message(STATUS "SimpleBLE not found - using bluetoothctl backend fallback")
    set(SIMPLEBLE_LIBS "")
endif()

add_executable(autonomous_centerline_controller_pure_pursuit_delay_fixed_cpp
    autonomous_centerline_controller_pure_pursuit_delay_fixed.cpp
)

add_executable(autonomous_centerline_controller_low_regulation_cpp
    autonomous_centerline_controller_low_regulation.cpp
)

target_link_libraries(autonomous_centerline_controller_pure_pursuit_delay_fixed_cpp PRIVATE ${OpenCV_LIBS} ${SIMPLEBLE_LIBS})
target_link_libraries(autonomous_centerline_controller_low_regulation_cpp PRIVATE ${OpenCV_LIBS} ${SIMPLEBLE_LIBS})
