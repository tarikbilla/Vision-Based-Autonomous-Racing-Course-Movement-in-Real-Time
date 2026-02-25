cmake_minimum_required(VERSION 3.10)
project(VisionBasedRCCarControl VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build options
option(BUILD_WITH_UI "Build with UI support" ON)
option(BUILD_TESTS "Build unit tests" OFF)

# Find required packages
find_package(OpenCV REQUIRED)
find_package(Threads REQUIRED)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${OpenCV_INCLUDE_DIRS})

# Source files
set(SOURCES
    src/main.cpp
    src/camera_capture.cpp
    src/object_tracker.cpp
    src/boundary_detection.cpp
    src/ble_handler.cpp
    src/control_orchestrator.cpp
    src/config_manager.cpp
)

# Header files
set(HEADERS
    include/camera_capture.h
    include/object_tracker.h
    include/boundary_detection.h
    include/ble_handler.h
    include/control_orchestrator.h
    include/config_manager.h
    include/types.h
)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# Link libraries
target_link_libraries(${PROJECT_NAME} 
    ${OpenCV_LIBS}
    Threads::Threads
)

# Compiler-specific options
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -O3)
endif()

# Installation
install(TARGETS ${PROJECT_NAME} DESTINATION bin)
install(FILES config/config.json DESTINATION etc)

# Print configuration
message(STATUS "OpenCV version: ${OpenCV_VERSION}")
message(STATUS "OpenCV libraries: ${OpenCV_LIBS}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
