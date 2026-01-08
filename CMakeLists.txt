# ═══════════════════════════════════════════════════════════════════════════════
# UnifiedFloppyTool v3.7.0
# ═══════════════════════════════════════════════════════════════════════════════
# Cross-platform floppy disk preservation and analysis tool
# Build: cmake -B build && cmake --build build
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

# Position Independent Code (required for Qt on Linux)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# ═══════════════════════════════════════════════════════════════════════════════
# Build Options
# ═══════════════════════════════════════════════════════════════════════════════
option(UFT_BUILD_GUI      "Build Qt6 GUI application"      ON)
option(UFT_BUILD_TOOLS    "Build command-line tools"       ON)
option(UFT_BUILD_TESTS    "Build test suite"               OFF)
option(UFT_ENABLE_SIMD    "Enable SIMD optimizations"      ON)

# ═══════════════════════════════════════════════════════════════════════════════
# Platform Helper Functions
# ═══════════════════════════════════════════════════════════════════════════════
function(uft_link_math target)
    if(UNIX)
        target_link_libraries(${target} PRIVATE m)
    endif()
endfunction()

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
    else()
        message(STATUS "Qt6 not found - GUI will not be built")
        set(UFT_BUILD_GUI OFF)
    endif()
endif()

# ═══════════════════════════════════════════════════════════════════════════════
# SIMD Detection
# ═══════════════════════════════════════════════════════════════════════════════
if(UFT_ENABLE_SIMD)
    include(CheckCCompilerFlag)
    check_c_compiler_flag("-msse4.2" HAVE_SSE42)
    check_c_compiler_flag("-mavx2" HAVE_AVX2)
    if(HAVE_AVX2)
        message(STATUS "  SIMD: AVX2")
    elseif(HAVE_SSE42)
        message(STATUS "  SIMD: SSE4.2")
    endif()
endif()

message(STATUS "════════════════════════════════════════════════════")
message(STATUS "UnifiedFloppyTool v${PROJECT_VERSION}")
message(STATUS "  Build GUI: ${UFT_BUILD_GUI}")
message(STATUS "  Build Tools: ${UFT_BUILD_TOOLS}")
message(STATUS "════════════════════════════════════════════════════")

# ═══════════════════════════════════════════════════════════════════════════════
# Core Libraries (always built)
# ═══════════════════════════════════════════════════════════════════════════════

# CRC Module
add_subdirectory(src/crc)

# Core Module
add_subdirectory(src/core)

# Encoding Module (FM/MFM/GCR)
add_subdirectory(src/encoding)

# FDC Bitstream (C++ VFO/PLL)
add_subdirectory(src/flux/fdc_bitstream)

# PLL Module
add_subdirectory(src/flux/pll)

# Tracks Module
add_subdirectory(src/tracks)

# Format Modules
add_subdirectory(src/formats/scp)
add_subdirectory(src/formats/amstrad)
add_subdirectory(src/formats/msa)
add_subdirectory(src/formats/flashfloppy)

# Loaders
add_subdirectory(src/loaders/disk_formats)

# Optional modules (if they have content)
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

# ═══════════════════════════════════════════════════════════════════════════════
# GUI Application (Qt6 required)
# ═══════════════════════════════════════════════════════════════════════════════
if(UFT_BUILD_GUI AND Qt6_FOUND)
    # Check if GUI sources exist
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/main.cpp" AND 
       EXISTS "${CMAKE_SOURCE_DIR}/src/mainwindow.cpp")
        
        file(GLOB GUI_SOURCES "src/*.cpp")
        file(GLOB GUI_HEADERS "src/*.h")
        file(GLOB GUI_FORMS "forms/*.ui")
        
        qt_add_executable(${PROJECT_NAME}
            ${GUI_SOURCES}
            ${GUI_HEADERS}
            ${GUI_FORMS}
        )
        
        target_link_libraries(${PROJECT_NAME} PRIVATE
            Qt6::Core
            Qt6::Widgets
            Qt6::Gui
            uft_core
            uft_crc
            uft_encoding
        )
        
        # Install
        install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)
    else()
        message(STATUS "GUI sources not found - skipping GUI build")
    endif()
endif()

# ═══════════════════════════════════════════════════════════════════════════════
# Command-Line Tools
# ═══════════════════════════════════════════════════════════════════════════════
if(UFT_BUILD_TOOLS)
    # Add tools subdirectory if it exists
    if(EXISTS "${CMAKE_SOURCE_DIR}/tools/CMakeLists.txt")
        add_subdirectory(tools)
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

# ═══════════════════════════════════════════════════════════════════════════════
# Summary
# ═══════════════════════════════════════════════════════════════════════════════
message(STATUS "════════════════════════════════════════════════════")
message(STATUS "Configuration complete")
message(STATUS "════════════════════════════════════════════════════")
