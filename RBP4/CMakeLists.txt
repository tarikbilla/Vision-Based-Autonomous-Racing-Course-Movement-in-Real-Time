cmake_minimum_required(VERSION 3.16)
project(autonomous_centerline_controller_pure_pursuit_delay_fixed_cpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenCV REQUIRED)
find_package(simpleble QUIET)

add_executable(autonomous_centerline_controller_pure_pursuit_delay_fixed_cpp
    main.cpp
)

target_link_libraries(autonomous_centerline_controller_pure_pursuit_delay_fixed_cpp PRIVATE ${OpenCV_LIBS})

if(simpleble_FOUND)
    target_compile_definitions(autonomous_centerline_controller_pure_pursuit_delay_fixed_cpp PRIVATE HAVE_SIMPLEBLE=1)
    target_link_libraries(autonomous_centerline_controller_pure_pursuit_delay_fixed_cpp PRIVATE simpleble::simpleble)
    message(STATUS "SimpleBLE found: enabling C++ SimpleBLE backend")
else()
    message(STATUS "SimpleBLE not found: using bluetoothctl backend")
endif()
