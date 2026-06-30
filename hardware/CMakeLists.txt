cmake_minimum_required(VERSION 3.12)
project(sobel_simulation LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find Verilator framework
find_package(verilator REQUIRED)

add_executable(Vsobel_filter sim/tb_sobel.cpp)

# Generate C++ classes from nested RTL sources
verilator_generate_csources(Vsobel_filter
    SOURCES
        rtl/sobel_filter.v
        rtl/line_buffer.v
    TOP_MODULE
        sobel_filter
    VERILATOR_ARGS
        -Wall
        -I${CMAKE_CURRENT_SOURCE_DIR}/rtl 
)

target_link_libraries(Vsobel_filter PRIVATE verilator)
