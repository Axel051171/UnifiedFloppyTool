# UFT CRC Module
# Part of UnifiedFloppyTool

# UFT CRC implementation files (clean, self-contained)
set(CRC_SOURCES
    uft_crc32_tables.c
    uft_crc_extended.c
)

set(CRC_HEADERS
    uft_crc_polys.h
)

add_library(uft_crc STATIC
    ${CRC_SOURCES}
    ${CRC_HEADERS}
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
    target_compile_options(uft_crc PRIVATE -Wall -Wextra)
endif()
