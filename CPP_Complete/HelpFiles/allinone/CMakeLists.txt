cmake_minimum_required(VERSION 3.16)
project(allinone LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(external/SimpleBLE/simpleble)

find_package(OpenCV REQUIRED)

add_executable(controller controller.cpp)

target_link_libraries(controller
    PRIVATE
        ${OpenCV_LIBS}
        simpleble
)