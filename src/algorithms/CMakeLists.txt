# CMakeLists.txt for UFT Algorithm Modules
# Phase 1 + Phase 2 Complete Implementation

cmake_minimum_required(VERSION 3.16)

# ============================================================================
# Phase 1: Core Algorithm Fixes
# ============================================================================

# 1. Adaptive PLL (Fix #1: Sync loss under jitter)
set(UFT_PLL_SOURCES
    pll/uft_adaptive_pll.c
    pll/uft_adaptive_pll.h
)

# 2. Sync Detector (Fix #2: False positive sync detection)
set(UFT_SYNC_SOURCES
    sync/uft_sync_detector.c
    sync/uft_sync_detector.h
)

# 4. Weak Bits (Fix #4: Weak bit handling)
set(UFT_WEAK_SOURCES
    weak_bits/uft_weak_bits.c
    weak_bits/uft_weak_bits.h
)

# ============================================================================
# Phase 2: Extended Algorithm Fixes
# ============================================================================

# 3. CRC Aligned (Fix #3: CRC bit-alignment errors)
set(UFT_CRC_SOURCES
    crc/uft_crc_aligned.c
    crc/uft_crc_aligned.h
)

# 5. Track Boundary (Fix #5: Track boundary detection)
set(UFT_TRACK_SOURCES
    track/uft_track_boundary.c
    track/uft_track_boundary.h
)

# 6. Encoding Detection (Fix #6: GCR/MFM confusion)
set(UFT_ENCODING_SOURCES
    encoding/uft_encoding_detect.c
    encoding/uft_encoding_detect.h
)

# 7. Interleave (Fix #7: Sector interleave handling)
set(UFT_INTERLEAVE_SOURCES
    interleave/uft_interleave.c
    interleave/uft_interleave.h
)

# 8. Recovery (Fix #8: Partial sector handling)
set(UFT_RECOVERY_SOURCES
    recovery/uft_partial_recovery.c
    recovery/uft_partial_recovery.h
)

# 9. Format Detection (Fix #9: Format signature confusion)
set(UFT_FORMAT_SOURCES
    format/uft_format_detect.c
    format/uft_format_detect.h
)

# 10. Flux Precision (Fix #10: Timing quantization errors)
set(UFT_FLUX_SOURCES
    flux/uft_flux_precise.c
    flux/uft_flux_precise.h
)

# ============================================================================
# Combined Library
# ============================================================================

set(UFT_ALGORITHM_SOURCES
    ${UFT_PLL_SOURCES}
    ${UFT_SYNC_SOURCES}
    ${UFT_WEAK_SOURCES}
    ${UFT_CRC_SOURCES}
    ${UFT_TRACK_SOURCES}
    ${UFT_ENCODING_SOURCES}
    ${UFT_INTERLEAVE_SOURCES}
    ${UFT_RECOVERY_SOURCES}
    ${UFT_FORMAT_SOURCES}
    ${UFT_FLUX_SOURCES}
)

add_library(uft_algorithms STATIC ${UFT_ALGORITHM_SOURCES})

target_include_directories(uft_algorithms PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/pll
    ${CMAKE_CURRENT_SOURCE_DIR}/sync
    ${CMAKE_CURRENT_SOURCE_DIR}/weak_bits
    ${CMAKE_CURRENT_SOURCE_DIR}/crc
    ${CMAKE_CURRENT_SOURCE_DIR}/track
    ${CMAKE_CURRENT_SOURCE_DIR}/encoding
    ${CMAKE_CURRENT_SOURCE_DIR}/interleave
    ${CMAKE_CURRENT_SOURCE_DIR}/recovery
    ${CMAKE_CURRENT_SOURCE_DIR}/format
    ${CMAKE_CURRENT_SOURCE_DIR}/flux
)

target_link_libraries(uft_algorithms PUBLIC m)

target_compile_options(uft_algorithms PRIVATE
    -Wall -Wextra -Wpedantic
    $<$<CONFIG:Debug>:-g -O0>
    $<$<CONFIG:Release>:-O3>
)

# C99 standard
set_target_properties(uft_algorithms PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
)

# ============================================================================
# Individual Module Libraries (optional)
# ============================================================================

# PLL module
add_library(uft_pll STATIC ${UFT_PLL_SOURCES})
target_include_directories(uft_pll PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/pll)
target_link_libraries(uft_pll PUBLIC m)

# Sync module
add_library(uft_sync STATIC ${UFT_SYNC_SOURCES})
target_include_directories(uft_sync PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/sync)
target_link_libraries(uft_sync PUBLIC m)

# CRC module
add_library(uft_crc STATIC ${UFT_CRC_SOURCES})
target_include_directories(uft_crc PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/crc)

# Weak bits module
add_library(uft_weak STATIC ${UFT_WEAK_SOURCES})
target_include_directories(uft_weak PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/weak_bits)

# Track boundary module
add_library(uft_track STATIC ${UFT_TRACK_SOURCES})
target_include_directories(uft_track PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/track)
target_link_libraries(uft_track PUBLIC m)

# Encoding detection module
add_library(uft_encoding STATIC ${UFT_ENCODING_SOURCES})
target_include_directories(uft_encoding PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/encoding)

# Interleave module
add_library(uft_interleave STATIC ${UFT_INTERLEAVE_SOURCES})
target_include_directories(uft_interleave PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/interleave)

# Recovery module
add_library(uft_recovery STATIC ${UFT_RECOVERY_SOURCES})
target_include_directories(uft_recovery PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/recovery)

# Format detection module
add_library(uft_format STATIC ${UFT_FORMAT_SOURCES})
target_include_directories(uft_format PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/format)

# Flux precision module
add_library(uft_flux STATIC ${UFT_FLUX_SOURCES})
target_include_directories(uft_flux PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/flux)
target_link_libraries(uft_flux PUBLIC m)

# ============================================================================
# Fuzz Targets (requires clang with libFuzzer)
# ============================================================================

option(BUILD_FUZZ_TARGETS "Build fuzz testing targets" OFF)

if(BUILD_FUZZ_TARGETS)
    # Phase 1 fuzz targets
    add_executable(fuzz_pll ../tests/fuzz/fuzz_pll.c)
    target_link_libraries(fuzz_pll uft_pll)
    target_compile_options(fuzz_pll PRIVATE -fsanitize=fuzzer,address)
    target_link_options(fuzz_pll PRIVATE -fsanitize=fuzzer,address)
    
    add_executable(fuzz_sync ../tests/fuzz/fuzz_sync.c)
    target_link_libraries(fuzz_sync uft_sync)
    target_compile_options(fuzz_sync PRIVATE -fsanitize=fuzzer,address)
    target_link_options(fuzz_sync PRIVATE -fsanitize=fuzzer,address)
    
    add_executable(fuzz_weak ../tests/fuzz/fuzz_weak_bits.c)
    target_link_libraries(fuzz_weak uft_weak)
    target_compile_options(fuzz_weak PRIVATE -fsanitize=fuzzer,address)
    target_link_options(fuzz_weak PRIVATE -fsanitize=fuzzer,address)
    
    # Phase 2 fuzz targets (build with specific defines)
    add_executable(fuzz_crc ../tests/fuzz/fuzz_phase2.c)
    target_compile_definitions(fuzz_crc PRIVATE FUZZ_TARGET_CRC)
    target_link_libraries(fuzz_crc uft_crc)
    target_compile_options(fuzz_crc PRIVATE -fsanitize=fuzzer,address)
    target_link_options(fuzz_crc PRIVATE -fsanitize=fuzzer,address)
    
    add_executable(fuzz_boundary ../tests/fuzz/fuzz_phase2.c)
    target_compile_definitions(fuzz_boundary PRIVATE FUZZ_TARGET_BOUNDARY)
    target_link_libraries(fuzz_boundary uft_track m)
    target_compile_options(fuzz_boundary PRIVATE -fsanitize=fuzzer,address)
    target_link_options(fuzz_boundary PRIVATE -fsanitize=fuzzer,address)
    
    add_executable(fuzz_encoding ../tests/fuzz/fuzz_phase2.c)
    target_compile_definitions(fuzz_encoding PRIVATE FUZZ_TARGET_ENCODING)
    target_link_libraries(fuzz_encoding uft_encoding)
    target_compile_options(fuzz_encoding PRIVATE -fsanitize=fuzzer,address)
    target_link_options(fuzz_encoding PRIVATE -fsanitize=fuzzer,address)
    
    add_executable(fuzz_recovery ../tests/fuzz/fuzz_phase2.c)
    target_compile_definitions(fuzz_recovery PRIVATE FUZZ_TARGET_RECOVERY)
    target_link_libraries(fuzz_recovery uft_recovery)
    target_compile_options(fuzz_recovery PRIVATE -fsanitize=fuzzer,address)
    target_link_options(fuzz_recovery PRIVATE -fsanitize=fuzzer,address)
    
    add_executable(fuzz_format ../tests/fuzz/fuzz_phase2.c)
    target_compile_definitions(fuzz_format PRIVATE FUZZ_TARGET_FORMAT)
    target_link_libraries(fuzz_format uft_format)
    target_compile_options(fuzz_format PRIVATE -fsanitize=fuzzer,address)
    target_link_options(fuzz_format PRIVATE -fsanitize=fuzzer,address)
endif()

# ============================================================================
# Installation
# ============================================================================

install(TARGETS uft_algorithms
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

install(FILES
    pll/uft_adaptive_pll.h
    sync/uft_sync_detector.h
    weak_bits/uft_weak_bits.h
    crc/uft_crc_aligned.h
    track/uft_track_boundary.h
    encoding/uft_encoding_detect.h
    interleave/uft_interleave.h
    recovery/uft_partial_recovery.h
    format/uft_format_detect.h
    flux/uft_flux_precise.h
    DESTINATION include/uft
)
