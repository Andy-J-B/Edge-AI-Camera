cmake_minimum_required(VERSION 3.12)
project(sobel_simulation LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(verilator REQUIRED)
find_package(OpenCV REQUIRED)

# Target 1: Single Frame Image Simulation (Vsobel_filter)
add_executable(Vsobel_filter sim/tb_sobel.cpp)

verilate(Vsobel_filter
    SOURCES
        rtl/sobel_filter.v
        rtl/line_buffer.v
    TOP_MODULE
        sobel_filter
    VERILATOR_ARGS
        -Wall
        -I${CMAKE_CURRENT_SOURCE_DIR}/rtl
)

# Target 2: Continuous Video Stream Simulation (Vvideo_filter)
add_executable(Vvideo_filter sim/tb_video.cpp)

verilate(Vvideo_filter
    SOURCES
        rtl/sobel_filter.v
        rtl/line_buffer.v
    TOP_MODULE
        sobel_filter
    VERILATOR_ARGS
        -Wall
        -I${CMAKE_CURRENT_SOURCE_DIR}/rtl
)

target_link_libraries(Vvideo_filter PRIVATE ${OpenCV_LIBS})
target_include_directories(Vvideo_filter PRIVATE ${OpenCV_INCLUDE_DIRS})
