# ═══════════════════════════════════════════════════════════════════════════════
# UnifiedFloppyTool v3.7.0
# ═══════════════════════════════════════════════════════════════════════════════

cmake_minimum_required(VERSION 3.16)

project(UnifiedFloppyTool
    VERSION 3.7.0
    DESCRIPTION "Cross-platform floppy disk preservation and analysis tool"
    LANGUAGES CXX C
)

# ═══════════════════════════════════════════════════════════════════════════════
# Language Standards
# ═══════════════════════════════════════════════════════════════════════════════
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Position Independent Code
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# ═══════════════════════════════════════════════════════════════════════════════
# Global Include Directories
# ═══════════════════════════════════════════════════════════════════════════════
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/uft
    ${CMAKE_SOURCE_DIR}/include/uft/core
    ${CMAKE_SOURCE_DIR}/include/uft/crc
    ${CMAKE_SOURCE_DIR}/include/uft/compat
    ${CMAKE_SOURCE_DIR}/include/uft/flux
    ${CMAKE_SOURCE_DIR}/include/uft/decoders
    ${CMAKE_SOURCE_DIR}/include/uft/recovery
    ${CMAKE_SOURCE_DIR}/include/uft/PRIVATE
    ${CMAKE_SOURCE_DIR}/include/uft/PRIVATE/compat
    ${CMAKE_SOURCE_DIR}/include/tracks
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/encoding
    ${CMAKE_SOURCE_DIR}/src/tracks
    ${CMAKE_SOURCE_DIR}/src/tracks/fm
    ${CMAKE_SOURCE_DIR}/src/tracks/mfm
    ${CMAKE_SOURCE_DIR}/src/tracks/gcr
    ${CMAKE_SOURCE_DIR}/src/algorithms
    ${CMAKE_SOURCE_DIR}/src/algorithms/tracks
    ${CMAKE_SOURCE_DIR}/src/algorithms/encoding
)

# ═══════════════════════════════════════════════════════════════════════════════
# Build Options
# ═══════════════════════════════════════════════════════════════════════════════
option(UFT_BUILD_GUI      "Build Qt6 GUI application"      ON)
option(UFT_BUILD_TOOLS    "Build command-line tools"       OFF)
option(UFT_BUILD_TESTS    "Build test suite"               ON)
option(UFT_BUILD_EXAMPLES "Build example programs"         OFF)
option(UFT_ENABLE_SIMD    "Enable SIMD optimizations"      ON)
option(UFT_ENABLE_OPENMP  "Enable OpenMP parallelization"  ON)
option(UFT_ENABLE_LTO     "Enable Link-Time Optimization"  OFF)
option(UFT_ENABLE_ASAN    "Enable Address Sanitizer"       OFF)

# ═══════════════════════════════════════════════════════════════════════════════
# Platform Detection
# ═══════════════════════════════════════════════════════════════════════════════
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(UFT_PLATFORM_MACOS TRUE)
    message(STATUS "Platform: macOS")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(UFT_PLATFORM_WINDOWS TRUE)
    message(STATUS "Platform: Windows")
else()
    set(UFT_PLATFORM_LINUX TRUE)
    message(STATUS "Platform: Linux")
endif()

# ═══════════════════════════════════════════════════════════════════════════════
# OpenMP Configuration (optional, fallback on failure)
# ═══════════════════════════════════════════════════════════════════════════════
if(UFT_ENABLE_OPENMP)
    find_package(OpenMP QUIET)
    if(OpenMP_FOUND OR OpenMP_C_FOUND)
        message(STATUS "OpenMP: ENABLED (${OpenMP_C_VERSION})")
        set(UFT_HAS_OPENMP TRUE)
    else()
        message(STATUS "OpenMP: NOT FOUND - building without parallelization")
        set(UFT_HAS_OPENMP FALSE)
        add_compile_definitions(UFT_NO_OPENMP)
    endif()
else()
    message(STATUS "OpenMP: DISABLED by option")
    set(UFT_HAS_OPENMP FALSE)
    add_compile_definitions(UFT_NO_OPENMP)
endif()

# ═══════════════════════════════════════════════════════════════════════════════
# SIMD Configuration (platform-aware)
# ═══════════════════════════════════════════════════════════════════════════════
if(UFT_ENABLE_SIMD)
    include(CheckCCompilerFlag)
    if(MSVC)
        # MSVC auto-detects SIMD
        set(UFT_HAS_SSE2 TRUE)
        check_c_compiler_flag("/arch:AVX2" HAS_AVX2)
        if(HAS_AVX2)
            set(UFT_HAS_AVX2 TRUE)
        endif()
    else()
        check_c_compiler_flag("-msse2" HAS_SSE2)
        check_c_compiler_flag("-mavx2" HAS_AVX2)
        if(HAS_SSE2)
            set(UFT_HAS_SSE2 TRUE)
        endif()
        if(HAS_AVX2)
            set(UFT_HAS_AVX2 TRUE)
        endif()
    endif()
    message(STATUS "SIMD: SSE2=${UFT_HAS_SSE2} AVX2=${UFT_HAS_AVX2}")
else()
    message(STATUS "SIMD: DISABLED by option")
    add_compile_definitions(UFT_NO_SIMD)
endif()

# ═══════════════════════════════════════════════════════════════════════════════
# Platform Helper Functions
# ═══════════════════════════════════════════════════════════════════════════════
function(uft_link_math target)
    if(UNIX)
        target_link_libraries(${target} PRIVATE m)
    endif()
endfunction()

function(uft_add_openmp target)
    if(UFT_HAS_OPENMP)
        target_link_libraries(${target} PRIVATE OpenMP::OpenMP_C)
    endif()
endfunction()

# ═══════════════════════════════════════════════════════════════════════════════
# Compiler Flags
# ═══════════════════════════════════════════════════════════════════════════════
if(MSVC)
    # MSVC requires /std:c11 for proper C11 support (stdint.h, mixed declarations)
    add_compile_options(/W3 /wd4100 /wd4996 /std:c11)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
else()
    add_compile_options(-Wall -Wextra -Wno-unused-parameter)
    # Enable POSIX features
    add_compile_definitions(_POSIX_C_SOURCE=200809L)
    if(NOT APPLE)
        add_compile_definitions(_GNU_SOURCE)
    endif()
endif()

# ═══════════════════════════════════════════════════════════════════════════════
# Find Qt6 (Optional for GUI)
# ═══════════════════════════════════════════════════════════════════════════════
if(UFT_BUILD_GUI)
    find_package(Qt6 COMPONENTS Core Widgets Gui QUIET)
    if(Qt6_FOUND)
        message(STATUS "Qt6 found: ${Qt6_VERSION}")
        set(CMAKE_AUTOMOC ON)
        set(CMAKE_AUTORCC ON)
        set(CMAKE_AUTOUIC ON)
        set(CMAKE_AUTOUIC_SEARCH_PATHS
            ${CMAKE_SOURCE_DIR}/forms
            ${CMAKE_SOURCE_DIR}/gui/forms
        )
    else()
        message(STATUS "Qt6 not found - GUI will not be built")
        set(UFT_BUILD_GUI OFF)
    endif()
endif()

message(STATUS "════════════════════════════════════════════════════")
message(STATUS "UnifiedFloppyTool v${PROJECT_VERSION}")
message(STATUS "  Build GUI: ${UFT_BUILD_GUI}")
message(STATUS "  Build Tools: ${UFT_BUILD_TOOLS}")
message(STATUS "════════════════════════════════════════════════════")

# ═══════════════════════════════════════════════════════════════════════════════
# Core Libraries
# ═══════════════════════════════════════════════════════════════════════════════

add_subdirectory(src/crc)
add_subdirectory(src/core)
add_subdirectory(src/encoding)
add_subdirectory(src/flux/fdc_bitstream)
add_subdirectory(src/flux/pll)
add_subdirectory(src/tracks)
add_subdirectory(src/formats/scp)
add_subdirectory(src/formats/amstrad)
add_subdirectory(src/formats/msa)
add_subdirectory(src/formats/flashfloppy)
add_subdirectory(src/loaders/disk_formats)

# Optional modules
if(EXISTS "${CMAKE_SOURCE_DIR}/src/decoder/CMakeLists.txt")
    add_subdirectory(src/decoder)
endif()
if(EXISTS "${CMAKE_SOURCE_DIR}/src/decoders/CMakeLists.txt")
    add_subdirectory(src/decoders)
endif()
if(EXISTS "${CMAKE_SOURCE_DIR}/src/protection/CMakeLists.txt")
    add_subdirectory(src/protection)
endif()
if(EXISTS "${CMAKE_SOURCE_DIR}/src/recovery/CMakeLists.txt")
    add_subdirectory(src/recovery)
endif()
if(EXISTS "${CMAKE_SOURCE_DIR}/src/hal/CMakeLists.txt")
    add_subdirectory(src/hal)
endif()
if(EXISTS "${CMAKE_SOURCE_DIR}/src/sector/CMakeLists.txt")
    add_subdirectory(src/sector)
endif()

# Additional modules (P2-7)
if(EXISTS "${CMAKE_SOURCE_DIR}/src/batch/CMakeLists.txt")
    add_subdirectory(src/batch)
endif()
if(EXISTS "${CMAKE_SOURCE_DIR}/src/cbm/CMakeLists.txt")
    add_subdirectory(src/cbm)
endif()

# C64 Forensics Module (6502 Disasm, PRG Parser, Fastloader DB)
if(EXISTS "${CMAKE_SOURCE_DIR}/src/c64/CMakeLists.txt")
    add_subdirectory(src/c64)
endif()

# Z80 Module (Disassembler for ZX Spectrum)
if(EXISTS "${CMAKE_SOURCE_DIR}/src/z80/CMakeLists.txt")
    add_subdirectory(src/z80)
endif()

# Track Analysis Module (XCopy Pro algorithms)
if(EXISTS "${CMAKE_SOURCE_DIR}/src/analysis/CMakeLists.txt")
    add_subdirectory(src/analysis)
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/src/link/CMakeLists.txt")
    add_subdirectory(src/link)
endif()

# FormatID Module (FAT detection, geometry guessing)
if(EXISTS "${CMAKE_SOURCE_DIR}/src/formatid/CMakeLists.txt")
    add_subdirectory(src/formatid)
endif()

# Amiga Protection Detection
if(EXISTS "${CMAKE_SOURCE_DIR}/src/formats/amiga/CMakeLists.txt")
    add_subdirectory(src/formats/amiga)
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/src/cli/CMakeLists.txt")
    add_subdirectory(src/cli)
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/src/cloud/CMakeLists.txt")
    add_subdirectory(src/cloud)
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/src/whdload/CMakeLists.txt")
    add_subdirectory(src/whdload)
endif()

# XDF Container Format Family
if(EXISTS "${CMAKE_SOURCE_DIR}/src/xdf/CMakeLists.txt")
    add_subdirectory(src/xdf)
endif()

# ═══════════════════════════════════════════════════════════════════════════════
# GUI Application
# ═══════════════════════════════════════════════════════════════════════════════
if(UFT_BUILD_GUI AND Qt6_FOUND)
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/main.cpp")
        file(GLOB GUI_SOURCES "src/*.cpp")
        file(GLOB GUI_HEADERS "src/*.h")
        file(GLOB GUI_FORMS "forms/*.ui")
        
        qt_add_executable(${PROJECT_NAME}
            ${GUI_SOURCES}
            ${GUI_HEADERS}
            ${GUI_FORMS}
        )
        
        # Set AUTOUIC search paths as target property
        set_target_properties(${PROJECT_NAME} PROPERTIES
            AUTOUIC_SEARCH_PATHS "${CMAKE_SOURCE_DIR}/forms;${CMAKE_SOURCE_DIR}/gui/forms"
        )
        
        target_link_libraries(${PROJECT_NAME} PRIVATE
            Qt6::Core
            Qt6::Widgets
            Qt6::Gui
            uft_core
            uft_crc
            uft_encoding
            uft_tracks
            uft_protection
            uft_recovery
            uft_decoders
            uft_hal
            uft_sector
        )
        
        # Windows-specific libraries
        if(WIN32)
            target_link_libraries(${PROJECT_NAME} PRIVATE ws2_32)
        endif()
        
        # macOS-specific frameworks
        if(APPLE)
            target_link_libraries(${PROJECT_NAME} PRIVATE
                "-framework CoreFoundation"
                "-framework IOKit"
            )
        endif()
        
        install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)
    endif()
endif()

# ═══════════════════════════════════════════════════════════════════════════════
# Tests
# ═══════════════════════════════════════════════════════════════════════════════
if(UFT_BUILD_TESTS)
    enable_testing()
    if(EXISTS "${CMAKE_SOURCE_DIR}/tests/CMakeLists.txt")
        add_subdirectory(tests)
    endif()
endif()

message(STATUS "════════════════════════════════════════════════════")
message(STATUS "Configuration complete")
message(STATUS "════════════════════════════════════════════════════")
