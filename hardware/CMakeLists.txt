cmake_minimum_required(VERSION 3.12)
project(sobel_simulation LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 1. Find Verilator
find_package(verilator REQUIRED)

# 2. Add C++ testbench executable
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
