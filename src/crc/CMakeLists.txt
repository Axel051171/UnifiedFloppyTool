# UFT CRC Module - CRC RevEng Tools
# Part of UnifiedFloppyTool
# Polynomial database and CRC calculation

set(CRC_SOURCES
    model.c
    poly.c
    preset.c
)

add_library(uft_crc STATIC
    ${CRC_SOURCES}
)

target_include_directories(uft_crc PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/uft
    ${CMAKE_SOURCE_DIR}/include/uft/crc
)

set_target_properties(uft_crc PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
)

if(MSVC)
    target_compile_options(uft_crc PRIVATE /W3)
else()
    target_compile_options(uft_crc PRIVATE $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall> $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wextra>)
endif()
