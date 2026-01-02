cmake_minimum_required(VERSION 3.16)
project(UFT_Tests)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_testing()

# Include path to find headers
include_directories(${CMAKE_SOURCE_DIR}/../include)

# Simple endian test
add_executable(test_endian test_endian.c)
add_test(NAME EndianTest COMMAND test_endian)
