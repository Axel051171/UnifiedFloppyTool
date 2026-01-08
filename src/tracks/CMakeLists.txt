# UFT Tracks Module - FM/MFM/GCR Track Handling
file(GLOB_RECURSE TRACKS_SOURCES "*.c")
list(FILTER TRACKS_SOURCES EXCLUDE REGEX ".*test.*\\.c$")
if(TRACKS_SOURCES)
    add_library(uft_tracks STATIC ${TRACKS_SOURCES})
    target_include_directories(uft_tracks PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/fm
        ${CMAKE_CURRENT_SOURCE_DIR}/mfm
        ${CMAKE_CURRENT_SOURCE_DIR}/gcr
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/uft
        ${CMAKE_SOURCE_DIR}/include/uft/compat
    )
    set_target_properties(uft_tracks PROPERTIES C_STANDARD 11)
    if(NOT MSVC)
        target_compile_options(uft_tracks PRIVATE -Wno-unused-parameter)
    endif()
else()
    message(STATUS "uft_tracks: No source files found, skipping")
endif()
