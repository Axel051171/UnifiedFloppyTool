# UFT Decoders Module - CMakeLists.txt
# Contains: FM, MFM, GCR decoder plugins

set(DECODERS_SOURCES
    uft_fm_decoder.c
    uft_gcr_decoder.c
    uft_mfm_decoder.c
)

add_library(uft_decoders STATIC ${DECODERS_SOURCES})

target_include_directories(uft_decoders PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(uft_decoders PRIVATE
    uft_core
)

# Math library via zentrale Funktion
uft_link_math(uft_decoders)

# Add unified decoders subdirectory
add_subdirectory(unified)
