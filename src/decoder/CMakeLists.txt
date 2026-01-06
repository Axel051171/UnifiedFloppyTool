# UFT Decoder Module - CMakeLists.txt
# Contains: PLL, GCR, Sync, Fusion, Recovery
# NOTE: Some modules temporarily disabled pending header updates

set(DECODER_SOURCES
    uft_crc.c
    # uft_forensic_flux_decoder.c  # TODO: Header mismatch
    # uft_fusion.c                 # TODO: Header mismatch
    uft_gcr.c
    # uft_multi_rev_fusion_v2.c    # TODO: Header mismatch
    uft_pll.c
    # uft_pll_v2.c                 # TODO: Header mismatch
    uft_recovery.c
    uft_sync.c
)

add_library(uft_decoder STATIC ${DECODER_SOURCES})

target_include_directories(uft_decoder PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/uft
)

# Math library
if(NOT MSVC)
    target_link_libraries(uft_decoder PRIVATE m)
endif()
