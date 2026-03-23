cmake_minimum_required(VERSION 3.16)
project(autonomous_centerline_controller_pure_pursuit_delay_fixed_cpp LANGUAGES CXX OBJCXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OBJCXX_STANDARD 17)
set(CMAKE_OBJCXX_STANDARD_REQUIRED ON)

find_package(OpenCV REQUIRED)

add_executable(autonomous_centerline_controller_pure_pursuit_delay_fixed_cpp
    main.mm
)

target_link_libraries(autonomous_centerline_controller_pure_pursuit_delay_fixed_cpp
    PRIVATE
    ${OpenCV_LIBS}
    "-framework CoreBluetooth"
    "-framework Foundation"
)
