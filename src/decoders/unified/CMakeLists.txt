# Unified Decoders - CMakeLists.txt

set(DECODER_SOURCES
    uft_mfm_decoder_v2.c
    uft_fm_decoder_v2.c
    uft_gcr_cbm_decoder_v2.c
    uft_gcr_apple_decoder_v2.c
    uft_amiga_mfm_decoder_v2.c
    uft_decoder_init.c
)

add_library(uft_decoders_v2 STATIC ${DECODER_SOURCES})

target_include_directories(uft_decoders_v2 PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(uft_decoders_v2
    uft_unified
    m
)
