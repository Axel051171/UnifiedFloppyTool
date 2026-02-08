cmake_minimum_required(VERSION 3.10)
project(uft_denoise C)

set(CMAKE_C_STANDARD 99)

# Include paths
include_directories(${CMAKE_SOURCE_DIR}/../../../include/uft/analysis)

# ── Libraries ──────────────────────────────────────────────
add_library(phi_otdr_denoise_1d STATIC phi_otdr_denoise_1d.c)
target_link_libraries(phi_otdr_denoise_1d m)

add_library(uft_denoise_bridge STATIC uft_denoise_bridge.c)
target_link_libraries(uft_denoise_bridge phi_otdr_denoise_1d m)

# ── Tests ──────────────────────────────────────────────────
enable_testing()

add_executable(test_denoise_bridge test_denoise_bridge.c)
target_link_libraries(test_denoise_bridge uft_denoise_bridge phi_otdr_denoise_1d m)
add_test(NAME denoise_bridge COMMAND test_denoise_bridge)
