# UFT Generic Track Analysis Framework
# Based on XCopy Pro (1989-2011) algorithms

set(TRACK_ANALYSIS_SOURCES
    uft_track_analysis.c
    uft_xcopy_algo.c
)

set(TRACK_ANALYSIS_HEADERS
    uft_track_analysis.h
)

add_library(uft_track_analysis STATIC ${TRACK_ANALYSIS_SOURCES})

target_include_directories(uft_track_analysis PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
)

# Math library for sqrtf etc.
if(NOT MSVC)
    target_link_libraries(uft_track_analysis PRIVATE m)
endif()

# Link with Amiga-specific module if available
if(TARGET uft_amiga_protection)
    target_link_libraries(uft_track_analysis PRIVATE uft_amiga_protection)
endif()

# Add platform profiles subdirectory
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/profiles/CMakeLists.txt")
    add_subdirectory(profiles)
endif()

message(STATUS "UFT Track Analysis Module configured")
