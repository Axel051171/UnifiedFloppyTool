# UFT Decoder Module - CMakeLists.txt
# Contains: PLL, GCR, Sync, Fusion, Recovery

set(DECODER_SOURCES
    uft_crc.c
    uft_forensic_flux_decoder.c
    uft_fusion.c
    uft_gcr.c
    uft_pll.c
    uft_recovery.c
    uft_sync.c
    # uft_multi_rev_fusion_v2.c    # Merged into uft_fusion.c
    # uft_pll_v2.c                 # Merged into uft_pll.c
)

add_library(uft_decoder STATIC ${DECODER_SOURCES})

target_include_directories(uft_decoder PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/uft
)

# Math library via zentrale Funktion
uft_link_math(uft_decoder)
