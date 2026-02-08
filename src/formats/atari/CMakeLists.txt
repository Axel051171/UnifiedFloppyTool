# UFT Atari Format Module
# Part of UnifiedFloppyTool
# Supports: Atari ST (ST/MSA/STX), Atari 8-bit (ATR/ATX/XFD)

file(GLOB ATARI_SOURCES "*.c")
file(GLOB ATARI_HEADERS "*.h")

add_library(uft_format_atari STATIC
    ${ATARI_SOURCES}
    ${ATARI_HEADERS}
)

# Disable Qt auto-processing for this C-only library
set_target_properties(uft_format_atari PROPERTIES
    AUTOMOC OFF
    AUTOUIC OFF
    AUTORCC OFF
)

target_include_directories(uft_format_atari PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/uft
    ${CMAKE_SOURCE_DIR}/include/uft/compat
)

target_link_libraries(uft_format_atari PRIVATE
    uft_encoding
)

set_target_properties(uft_format_atari PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
)

if(MSVC)
    target_compile_options(uft_format_atari PRIVATE /W3)
else()
    target_compile_options(uft_format_atari PRIVATE $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall> $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wextra> $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wno-unused-parameter>)
endif()
