# UFT Protection Module
# Part of UnifiedFloppyTool

set(PROTECTION_SOURCES
    uft_amiga_caps.c
    uft_apple2_protection.c
    uft_atari8_protection.c
    uft_c64_protection_enhanced.c
    uft_copylock.c
    uft_fuzzy_bits.c
    uft_longtrack.c
    uft_magnetic_state.c
    uft_protection.c
    uft_protection_api.c
    uft_protection_classify.c
    uft_protection_detect.c
    uft_protection_ext.c
    uft_protection_params.c
    uft_rtc_decompress.c
    uft_speedlock.c
    # Atari ST modules (fixed 2025-01-08):
    uft_atarist_copylock.c
    uft_atarist_dec0de.c
    uft_atarist_macrodos.c
    uft_atarist_protection.c
    # PC modules (fixed 2025-01-08):
    uft_pc_cdrom_protection.c
    uft_pc_protection.c
)

file(GLOB PROTECTION_HEADERS "*.h")

add_library(uft_protection STATIC
    ${PROTECTION_SOURCES}
    ${PROTECTION_HEADERS}
)

target_include_directories(uft_protection PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/uft
    ${CMAKE_SOURCE_DIR}/include/uft/protection
)

uft_link_math(uft_protection)

set_target_properties(uft_protection PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
)

if(MSVC)
    target_compile_options(uft_protection PRIVATE /W3)
else()
    target_compile_options(uft_protection PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()
