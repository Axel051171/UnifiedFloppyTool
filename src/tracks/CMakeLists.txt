# ═══════════════════════════════════════════════════════════════════════════════
# UFT Tracks Library - Consolidated Track Handlers
# ═══════════════════════════════════════════════════════════════════════════════
# Part of UnifiedFloppyTool
# 
# This is the SINGLE SOURCE OF TRUTH for all track handlers.
# Do NOT duplicate these files elsewhere in the project!
#
# Supported encodings:
#   - GCR: Apple II, Apple Mac, Commodore 64, Victor 9000
#   - MFM: Amiga, IBM PC, Northstar, Membrain, Centurion
#   - FM:  IBM PC, Heathkit, DEC RX02, Emu, Thomson MO5, AED, Arburg, Tycom
#
# Origin: HxCFloppyEmulator (Jean-François DEL NERO)
# License: GPL-2.0
# ═══════════════════════════════════════════════════════════════════════════════

cmake_minimum_required(VERSION 3.16)

# ─────────────────────────────────────────────────────────────────────────────
# GCR Track Handlers (Apple, Commodore, Victor)
# ─────────────────────────────────────────────────────────────────────────────
set(GCR_SOURCES
    gcr/apple2_gcr_track.c
    gcr/apple_mac_gcr_track.c
    gcr/c64_gcr_track.c
    gcr/victor9k_gcr_track.c
)

set(GCR_HEADERS
    gcr/apple2_gcr_track.h
    gcr/apple_mac_gcr_track.h
    gcr/c64_gcr_track.h
    gcr/victor9k_gcr_track.h
)

# ─────────────────────────────────────────────────────────────────────────────
# MFM Track Handlers (Amiga, IBM, etc.)
# ─────────────────────────────────────────────────────────────────────────────
set(MFM_SOURCES
    mfm/amiga_mfm_track.c
    mfm/iso_ibm_mfm_track.c
    mfm/northstar_mfm_track.c
    mfm/membrain_mfm_track.c
    mfm/centurion_mfm_track.c
)

set(MFM_HEADERS
    mfm/amiga_mfm_track.h
    mfm/iso_ibm_mfm_track.h
    mfm/northstar_mfm_track.h
    mfm/membrain_mfm_track.h
    mfm/centurion_mfm_track.h
)

# ─────────────────────────────────────────────────────────────────────────────
# FM Track Handlers (Single Density)
# ─────────────────────────────────────────────────────────────────────────────
set(FM_SOURCES
    fm/iso_ibm_fm_track.c
    fm/heathkit_fm_track.c
    fm/emu_emulator_fm_track.c
    fm/micraln_fm_track.c
    fm/dec_rx02_track.c
    fm/qd_mo5_track.c
    fm/aed6200p_track.c
    fm/arburg_track.c
    fm/tycom_fm_track.c
)

set(FM_HEADERS
    fm/iso_ibm_fm_track.h
    fm/heathkit_fm_track.h
    fm/emu_emulator_fm_track.h
    fm/micraln_fm_track.h
    fm/dec_rx02_track.h
    fm/qd_mo5_track.h
    fm/aed6200p_track.h
    fm/arburg_track.h
    fm/tycom_fm_track.h
)

# ─────────────────────────────────────────────────────────────────────────────
# Shared Track Utilities
# ─────────────────────────────────────────────────────────────────────────────
set(SHARED_SOURCES
    track_generator.c
    sector_extractor.c
    crc.c
)

set(SHARED_HEADERS
    track_generator.h
    sector_extractor.h
    crc.h
)

# ─────────────────────────────────────────────────────────────────────────────
# Create Static Library
# ─────────────────────────────────────────────────────────────────────────────
add_library(uft_tracks STATIC
    ${SHARED_SOURCES}
    ${GCR_SOURCES}
    ${MFM_SOURCES}
    ${FM_SOURCES}
)

# ─────────────────────────────────────────────────────────────────────────────
# Include Directories
# ─────────────────────────────────────────────────────────────────────────────
target_include_directories(uft_tracks PUBLIC
    # This directory (for local headers)
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/gcr
    ${CMAKE_CURRENT_SOURCE_DIR}/mfm
    ${CMAKE_CURRENT_SOURCE_DIR}/fm
    
    # Project-wide includes
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/uft
    ${CMAKE_SOURCE_DIR}/include/uft/compat
    
    # HxCFloppyEmulator compatibility (types.h, libhxcfe.h, etc.)
    ${CMAKE_SOURCE_DIR}/src/algorithms/tracks
)

# ─────────────────────────────────────────────────────────────────────────────
# Compiler Settings
# ─────────────────────────────────────────────────────────────────────────────
set_target_properties(uft_tracks PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF
)

# Platform-specific compile options
if(MSVC)
    target_compile_options(uft_tracks PRIVATE
        /W3
        /wd4100  # unreferenced formal parameter
        /wd4244  # conversion warnings
        /wd4267  # size_t to int
    )
else()
    target_compile_options(uft_tracks PRIVATE
        -Wall
        -Wextra
        -Wno-unused-parameter
        -Wno-sign-compare
    )
endif()

# ─────────────────────────────────────────────────────────────────────────────
# Dependencies
# ─────────────────────────────────────────────────────────────────────────────
# Link math library on Unix
if(UNIX)
    target_link_libraries(uft_tracks PRIVATE m)
endif()

# ─────────────────────────────────────────────────────────────────────────────
# Installation
# ─────────────────────────────────────────────────────────────────────────────
install(TARGETS uft_tracks
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

install(FILES ${SHARED_HEADERS} DESTINATION include/uft/tracks)
install(FILES ${GCR_HEADERS} DESTINATION include/uft/tracks/gcr)
install(FILES ${MFM_HEADERS} DESTINATION include/uft/tracks/mfm)
install(FILES ${FM_HEADERS}  DESTINATION include/uft/tracks/fm)

# ─────────────────────────────────────────────────────────────────────────────
# Status Message
# ─────────────────────────────────────────────────────────────────────────────
message(STATUS "UFT Tracks: Shared utilities (track_generator, sector_extractor, crc)")
message(STATUS "UFT Tracks: GCR=${CMAKE_CURRENT_SOURCE_DIR}/gcr (4 handlers)")
message(STATUS "UFT Tracks: MFM=${CMAKE_CURRENT_SOURCE_DIR}/mfm (5 handlers)")
message(STATUS "UFT Tracks: FM=${CMAKE_CURRENT_SOURCE_DIR}/fm (9 handlers)")
