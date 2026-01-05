# UFT FDC Bitstream Module - VFO/PLL Emulation
# Part of UnifiedFloppyTool
# Note: This module is C++ based

set(FDC_BITSTREAM_SOURCES
    bit_array.cpp
    fdc_bitstream.cpp
    fdc_crc.cpp
    fdc_misc.cpp
    fdc_vfo_base.cpp
    mfm_codec.cpp
    vfo_experimental.cpp
    vfo_fixed.cpp
    vfo_pid.cpp
    vfo_pid2.cpp
    vfo_pid3.cpp
    vfo_simple.cpp
    vfo_simple2.cpp
)

# Create static library
add_library(uft_fdc_bitstream STATIC
    ${FDC_BITSTREAM_SOURCES}
)

target_include_directories(uft_fdc_bitstream PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/uft
    ${CMAKE_SOURCE_DIR}/include/uft/flux
)

# C++17 standard (needed for some features)
set_target_properties(uft_fdc_bitstream PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# Compiler warnings
if(MSVC)
    target_compile_options(uft_fdc_bitstream PRIVATE /W3 /EHsc)
else()
    target_compile_options(uft_fdc_bitstream PRIVATE $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall> $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wextra>)
endif()
