cmake_minimum_required(VERSION 3.16)
project(dtc_c_components C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
add_library(dtc_components STATIC
  src/bitbuffer.c src/crc.c src/encoding.c src/flux.c
  src/track.c src/detect.c src/protection.c src/match.c src/ctraw.c src/ipf.c)
target_include_directories(dtc_components PUBLIC include)
target_compile_options(dtc_components PRIVATE -Wall -Wextra -Wpedantic)
add_executable(dtc_tests tests/test_main.c)
target_link_libraries(dtc_tests PRIVATE dtc_components m)
enable_testing()
add_test(NAME dtc_tests COMMAND dtc_tests)

