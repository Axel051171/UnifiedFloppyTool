# RIDE Integration Module
# Part of UnifiedFloppyTool
#
# Note: All RIDE source files were removed during cleanup.
# Flux decoding is now in the main build via qmake (.pro).
# This CMakeLists.txt provides a header-only interface target for tests.

add_library(uft_ride INTERFACE)

target_include_directories(uft_ride INTERFACE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/uft
)
