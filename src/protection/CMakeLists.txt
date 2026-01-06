# UFT Protection Module
# Part of UnifiedFloppyTool

# Explicitly list sources instead of glob to control which compile
# NOTE: Many files disabled due to struct/type mismatches
set(PROTECTION_SOURCES
    uft_amiga_caps.c
    uft_apple2_protection.c
    uft_atari8_protection.c
    # uft_atarist_copylock.c    # DISABLED: struct type mismatch
    # uft_atarist_dec0de.c      # DISABLED: result struct type mismatch
    # uft_atarist_macrodos.c    # DISABLED: struct type mismatch (assumed)
    # uft_atarist_protection.c  # DISABLED: struct type mismatch
    uft_c64_protection_enhanced.c
    uft_copylock.c
    uft_fuzzy_bits.c
    uft_longtrack.c
    uft_magnetic_state.c
    # uft_pc_cdrom_protection.c # DISABLED: unknown types uft_safedisc_info_t, etc
    # uft_pc_protection.c       # DISABLED: uft_error_t resolution issue
    # uft_protection.c          # DISABLED: unknown type uft_protection_ctx_t
    uft_protection_api.c
    uft_protection_classify.c
    uft_protection_detect.c
    uft_protection_ext.c
    uft_protection_params.c
    uft_rtc_decompress.c
    uft_speedlock.c
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

target_link_libraries(uft_protection PRIVATE m)

set_target_properties(uft_protection PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
)

if(MSVC)
    target_compile_options(uft_protection PRIVATE /W3)
else()
    target_compile_options(uft_protection PRIVATE $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall> $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wextra> $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wno-unused-parameter>)
endif()
