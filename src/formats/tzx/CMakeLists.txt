# ═══════════════════════════════════════════════════════════════════════════════
# UFT TZX/CDT Module - ZX Spectrum & Amstrad CPC Tape Support
# ═══════════════════════════════════════════════════════════════════════════════
# 
# Features:
#   - TZX/CDT file parsing
#   - WAV audio generation for real hardware playback
#   - ZX Spectrum and Amstrad CPC timing support
#   - TAP <-> TZX bidirectional conversion
#
# Origin: 
#   - Parser: UFT Team
#   - WAV Generator: Ported from rtzx (Phil Stewart, MIT License)
#
# ═══════════════════════════════════════════════════════════════════════════════

cmake_minimum_required(VERSION 3.16)

set(TZX_SOURCES
    uft_tzx_parser_v3.c
    uft_tzx_wav.c
    uft_zxtap.c
)

set(TZX_HEADERS
    uft_tzx_wav.h
    uft_zxtap.h
)

add_library(uft_tzx STATIC
    ${TZX_SOURCES}
)

target_include_directories(uft_tzx PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/uft
)

set_target_properties(uft_tzx PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
)

if(MSVC)
    target_compile_options(uft_tzx PRIVATE /W3)
else()
    target_compile_options(uft_tzx PRIVATE -Wall -Wextra -Wno-unused-parameter)
    target_link_libraries(uft_tzx PRIVATE m)
endif()

# ─────────────────────────────────────────────────────────────────────────────
# Test executables
# ─────────────────────────────────────────────────────────────────────────────
option(UFT_BUILD_TZX_TESTS "Build TZX test executables" OFF)

if(UFT_BUILD_TZX_TESTS)
    # TZX WAV Test
    add_executable(uft_tzx_wav_test uft_tzx_wav.c)
    target_compile_definitions(uft_tzx_wav_test PRIVATE TZX_WAV_TEST)
    target_include_directories(uft_tzx_wav_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    if(NOT MSVC)
        target_link_libraries(uft_tzx_wav_test PRIVATE m)
    endif()
    
    # ZXTAP Test
    add_executable(uft_zxtap_test uft_zxtap.c)
    target_compile_definitions(uft_zxtap_test PRIVATE ZXTAP_TEST)
    target_include_directories(uft_zxtap_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
endif()

# ─────────────────────────────────────────────────────────────────────────────
# Installation
# ─────────────────────────────────────────────────────────────────────────────
install(TARGETS uft_tzx
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

install(FILES ${TZX_HEADERS} DESTINATION include/uft/formats/tzx)

message(STATUS "UFT TZX: ZX Spectrum (.tzx) + Amstrad CPC (.cdt) support")
message(STATUS "UFT TZX: WAV export enabled")
message(STATUS "UFT TZX: TAP <-> TZX conversion enabled")
