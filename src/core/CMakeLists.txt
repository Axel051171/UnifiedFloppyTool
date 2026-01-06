# UFT Core Module - CMakeLists.txt
# Contains: Core API, Cache, Error handling, Format detection, PLL, SIMD

set(CORE_SOURCES
    flux_core.c
    format_detection.c
    uft_adaptive_decoder.c
    uft_audit_trail.c
    uft_bit_confidence.c
    uft_bitstream_preserve.c
    uft_cache.c
    uft_core.c
    uft_decoder_plugin.c
    uft_dpll_wd1772.c
    uft_error.c
    uft_error_handling.c
    uft_flux_statistics.c
    uft_format_params.c
    uft_format_plugin.c
    uft_global_mutex.c
    uft_gui_bridge_v2.c
    uft_gui_params.c
    uft_ir_format.c
    uft_limits.c
    uft_memory.c
    uft_multi_decoder.c
    uft_multirev.c
    uft_params_universal.c
    uft_parser_v3_core.c
    uft_parser_v3_integration.c
    uft_pll.c
    # uft_protection_suite.c  # DISABLED: API mismatch with uft_protection.h - needs refactoring
    uft_streaming_hash.c
    uft_track_preset.c
    uft_types.c
    uft_writer_verify.c
)

# SIMD sources - Now using uft_compiler.h for macros
set(SIMD_SOURCES
    uft_simd.c
    uft_mfm_scalar.c
)

# Check for SSE2 support
include(CheckCCompilerFlag)
check_c_compiler_flag("-msse2" HAVE_SSE2)
if(HAVE_SSE2)
    list(APPEND SIMD_SOURCES uft_mfm_sse2.c)
    set_source_files_properties(uft_mfm_sse2.c PROPERTIES
        COMPILE_FLAGS "-msse2"
    )
endif()

# Check for AVX2 support
check_c_compiler_flag("-mavx2" HAVE_AVX2)
if(HAVE_AVX2)
    list(APPEND SIMD_SOURCES uft_mfm_avx2.c)
    set_source_files_properties(uft_mfm_avx2.c PROPERTIES
        COMPILE_FLAGS "-mavx2"
    )
endif()

add_library(uft_core STATIC ${CORE_SOURCES} ${SIMD_SOURCES})

target_include_directories(uft_core PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(uft_core
    m
)

# Thread support
find_package(Threads)
if(Threads_FOUND)
    target_link_libraries(uft_core Threads::Threads)
endif()
