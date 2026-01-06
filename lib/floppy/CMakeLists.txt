# ===========================================================================
# UFT Floppy Library - Low-Level Disk I/O and Encoding
# ===========================================================================
#
# Part of UnifiedFloppyTool (UFT) v3.3.0
#
# Features:
# - GCR Encoding (C64 5-bit, Mac 6+2)
# - MFM Encoding (IBM, Amiga)
# - FAT12 Filesystem Support
# - Geometry Detection + CHS/LBA Conversion
# - D64, ADF Image Formats
# - Flux Analysis
# - Copy Protection Detection
#
# SPDX-License-Identifier: GPL-3.0-or-later

# ===========================================================================
# Source Files
# ===========================================================================

set(UFT_FLOPPY_SOURCES
    # Core
    ${CMAKE_CURRENT_SOURCE_DIR}/src/uft_floppy_io.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/uft_floppy_geometry.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/uft_fat12.c
    
    # Encoding
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoding/uft_gcr.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/encoding/uft_mfm.c
    
    # Formats
    ${CMAKE_CURRENT_SOURCE_DIR}/src/formats/uft_d64.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/formats/uft_adf.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/formats/uft_flux.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/formats/uft_image.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/formats/uft_protection.c
)

# ===========================================================================
# Library Target
# ===========================================================================

add_library(uft_floppy STATIC ${UFT_FLOPPY_SOURCES})

target_include_directories(uft_floppy
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/include/encoding
        ${CMAKE_CURRENT_SOURCE_DIR}/include/formats
)

# C11 Standard
target_compile_features(uft_floppy PUBLIC c_std_11)

# Platform defines
if(WIN32)
    target_compile_definitions(uft_floppy PRIVATE UFT_PLATFORM_WINDOWS=1)
elseif(UNIX AND NOT APPLE)
    target_compile_definitions(uft_floppy PRIVATE UFT_PLATFORM_LINUX=1)
elseif(APPLE)
    target_compile_definitions(uft_floppy PRIVATE UFT_PLATFORM_MACOS=1)
endif()

# Relaxed warnings for this library (external code)
if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    # GCC-specific flags
    target_compile_options(uft_floppy PRIVATE
        -Wall
        -Wno-unused-variable
        -Wno-unused-function
        -Wno-sign-conversion
        -Wno-implicit-fallthrough
        -Wno-stringop-truncation  # GCC only
        -Wno-unused-result
    )
elseif(CMAKE_C_COMPILER_ID STREQUAL "Clang" OR CMAKE_C_COMPILER_ID STREQUAL "AppleClang")
    # Clang/AppleClang flags (no -Wno-stringop-truncation)
    target_compile_options(uft_floppy PRIVATE
        -Wall
        -Wno-unused-variable
        -Wno-unused-function
        -Wno-sign-conversion
        -Wno-implicit-fallthrough
        -Wno-unused-result
    )
elseif(MSVC)
    target_compile_options(uft_floppy PRIVATE /W3 /wd4996)
endif()

message(STATUS "  UFT Floppy Library: GCR, MFM, FAT12, Flux, Protection")
