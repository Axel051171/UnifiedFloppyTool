# UFT Recovery Module
# Part of UnifiedFloppyTool

set(RECOVERY_SOURCES
    uft_bitstream_recovery.c
    uft_cross_track.c
    uft_flux_recovery.c
    # uft_forensic_recovery.c  # DISABLED: missing header uft/recovery/uft_forensic_recovery.h
    # uft_forensic_track.c     # DISABLED: missing header uft/recovery/uft_forensic_recovery.h
    uft_multiread_pipeline.c
    uft_protection.c
    uft_recovery_advanced.c
    uft_recovery_meta.c
    uft_recovery_params.c
    uft_sector_recovery.c
)

file(GLOB RECOVERY_HEADERS "*.h")

add_library(uft_recovery STATIC
    ${RECOVERY_SOURCES}
    ${RECOVERY_HEADERS}
)

target_include_directories(uft_recovery PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/uft
    ${CMAKE_SOURCE_DIR}/include/uft/recovery
)

set_target_properties(uft_recovery PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
)

if(MSVC)
    target_compile_options(uft_recovery PRIVATE /W3)
else()
    target_compile_options(uft_recovery PRIVATE $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall> $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wextra> $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wno-unused-parameter>)
endif()
