# ===========================================================================
# UFT FlashFloppy Format Algorithms
# ===========================================================================
#
# Extracted from FlashFloppy by Keir Fraser (Public Domain/Unlicense)
#
# Supported formats:
# - TI-99/4A (VIB detection)
# - PC-98 FDI/HDM
# - MSX (BPB detection)
# - MGT (SAM Coupé/+D)
# - UKNC (Soviet PDP-11 clone)
#

set(UFT_FF_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/uft_ff_formats.c
)

set(UFT_FF_HEADERS
    ${CMAKE_SOURCE_DIR}/include/uft/formats/flashfloppy/uft_ff_formats.h
)

add_library(uft_ff_formats STATIC
    ${UFT_FF_SOURCES}
    ${UFT_FF_HEADERS}
)

target_include_directories(uft_ff_formats PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

set_target_properties(uft_ff_formats PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
)

# Relaxed warnings for external code
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(uft_ff_formats PRIVATE
        -Wno-unused-parameter
    )
endif()

message(STATUS "  UFT FlashFloppy Formats: TI-99, PC-98, MSX, MGT, UKNC")
