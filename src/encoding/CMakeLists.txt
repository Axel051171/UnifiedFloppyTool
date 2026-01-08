# UFT Encoding Module - FM/MFM/GCR
# Part of UnifiedFloppyTool

# legacy format encoding files - now using uft_encoding_types.h compatibility layer
# Integration completed 2025-01-05

set(UFT_ENCODING_SOURCES
    fm_encoding.c
    mfm_encoding.c
    dec_m2fm_encoding.c
    luts.c
)

set(UFT_ENCODING_HEADERS
    uft_encoding_types.h
    fm_encoding.h
    mfm_encoding.h
    dec_m2fm_encoding.h
    luts.h
)

# GCR submodule - DISABLED: need HxC types.h
# These files are from legacy format and need additional headers
set(GCR_SOURCES
    # gcr/apple2_gcr_track.c    # DISABLED: needs types.h
    # gcr/apple_mac_gcr_track.c # DISABLED: needs types.h
    # gcr/c64_gcr_track.c       # DISABLED: needs types.h
    # gcr/victor9k_gcr_track.c  # DISABLED: needs types.h
)

set(GCR_HEADERS
    gcr/apple2_gcr_track.h
    gcr/apple_mac_gcr_track.h
    gcr/c64_gcr_track.h
    gcr/victor9k_gcr_track.h
    gcr/c64.h
    gcr/data_gcr.h
)

# Create static library with all encoding sources
add_library(uft_encoding STATIC
    ${UFT_ENCODING_SOURCES}
    ${UFT_ENCODING_HEADERS}
    ${GCR_SOURCES}
    ${GCR_HEADERS}
)

target_include_directories(uft_encoding PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/gcr
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/uft
    ${CMAKE_SOURCE_DIR}/include/uft/compat
)

# C99 standard
set_target_properties(uft_encoding PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
)

# Compiler warnings
if(MSVC)
    target_compile_options(uft_encoding PRIVATE /W3)
else()
    target_compile_options(uft_encoding PRIVATE $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall> $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wextra>)
endif()
