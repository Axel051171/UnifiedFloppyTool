# UFT Encoding Module - FM/MFM/GCR
# Part of UnifiedFloppyTool

# Core encoding files
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

# GCR headers that exist
set(GCR_HEADERS
    gcr/c64.h
    gcr/data_gcr.h
)

# Create static library
add_library(uft_encoding STATIC
    ${UFT_ENCODING_SOURCES}
    ${UFT_ENCODING_HEADERS}
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
    target_compile_options(uft_encoding PRIVATE -Wall -Wextra)
endif()
