# Unified Decoders - CMakeLists.txt
# NOTE: Most v2/v3 decoders disabled - incomplete types or missing headers

set(DECODER_SOURCES
    # uft_mfm_decoder_v2.c       # DISABLED: incomplete uft_track_t
    # uft_mfm_encoder.c          # DISABLED: missing header
    # uft_fm_decoder_v2.c        # DISABLED: incomplete uft_track_t
    # uft_gcr_cbm_decoder_v2.c   # DISABLED: incomplete uft_track_t
    # uft_gcr_apple_decoder_v2.c # DISABLED: incomplete uft_track_t (assumed)
    # uft_amiga_mfm_decoder_v2.c # DISABLED: incomplete uft_track_t (assumed)
    # uft_amiga_mfm_decoder_v3.c # DISABLED: missing uft_amiga_mfm_decoder.h
    uft_amiga_checksum.c
    uft_decoder_init.c
)

add_library(uft_decoders_v2 STATIC ${DECODER_SOURCES})

target_include_directories(uft_decoders_v2 PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(uft_decoders_v2 PRIVATE
    uft_core
)

# Math library via zentrale Funktion
uft_link_math(uft_decoders_v2)
