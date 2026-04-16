# UFT CRC Module
# Part of UnifiedFloppyTool
#
# Note: uft_crc32_tables.c and uft_crc_extended.c were removed during cleanup.
# CRC functions are now in the main build via qmake (.pro).
# This CMakeLists.txt provides a header-only interface target for tests.

add_library(uft_crc INTERFACE)

target_include_directories(uft_crc INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/uft
    ${CMAKE_SOURCE_DIR}/include/uft/crc
)
