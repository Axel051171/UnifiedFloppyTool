cmake_minimum_required(VERSION 3.16)
project(UFT_Tests)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Try Qt6 first, fallback to Qt5
find_package(Qt6 COMPONENTS Core Test QUIET)
if(NOT Qt6_FOUND)
    find_package(Qt5 COMPONENTS Core Test REQUIRED)
endif()

enable_testing()

# Simple endian test (no Qt needed)
add_executable(test_endian test_endian.c)
add_test(NAME EndianTest COMMAND test_endian)
