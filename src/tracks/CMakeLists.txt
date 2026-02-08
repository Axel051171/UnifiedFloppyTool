# UFT Tracks Module - FM/MFM/GCR Track Handling
# 19 legacy HxC track format modules enabled
# track_generator.c and sector_extractor.c require deeper HxC porting (header conflicts)

file(GLOB_RECURSE TRACKS_SOURCES "*.c")
list(FILTER TRACKS_SOURCES EXCLUDE REGEX ".*test.*\\.c$")
list(FILTER TRACKS_SOURCES EXCLUDE REGEX ".*track_generator\\.c$")
list(FILTER TRACKS_SOURCES EXCLUDE REGEX ".*sector_extractor\\.c$")

message(STATUS "uft_tracks: 19 legacy HxC format modules enabled")

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
        ${CMAKE_SOURCE_DIR}/include/uft/PRIVATE/compat
        ${CMAKE_SOURCE_DIR}/include/tracks
        ${CMAKE_SOURCE_DIR}/src/encoding
        ${CMAKE_SOURCE_DIR}/src/algorithms/tracks
    )
    target_link_libraries(uft_tracks PUBLIC uft_encoding)
    set_target_properties(uft_tracks PROPERTIES C_STANDARD 11)
    if(NOT MSVC)
        target_compile_options(uft_tracks PRIVATE 
            -Wno-unused-parameter
            -Wno-unused-function
            -Wno-unused-variable
        )
    endif()
else()
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/uft_tracks_stub.c "/* stub */\n")
    add_library(uft_tracks STATIC ${CMAKE_CURRENT_BINARY_DIR}/uft_tracks_stub.c)
endif()
