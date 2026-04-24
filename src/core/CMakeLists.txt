# ============================================================================
# UFT Core Module
# ============================================================================
# Contains the 10 real core .c files. All must exist — if one is missing,
# the build fails fast (no more "/* stub */" fallback library).
# ============================================================================

find_package(ZLIB QUIET)

set(CORE_SOURCES
    uft_advanced_mode.c
    uft_core_stubs.c           # glue: real plugin-delegation for disk API
    uft_decode_pipeline.c
    uft_format_plugin.c        # plugin registry + probe helpers
    uft_format_verify.c        # generic/weak/flux verify
    uft_interleave.c           # TA2 M2: Atari sector interleave calculator
    uft_ir_format.c
    uft_smart_open.c
    uft_unified_types.c
    uft_write_precomp.c        # TA1 M2: Mac-800K peak-shift compensation
)

# Enforce: every source listed must actually exist (no dead whitelist)
foreach(src ${CORE_SOURCES})
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${src}")
        message(FATAL_ERROR "uft_core: required source ${src} missing")
    endif()
endforeach()

add_library(uft_core STATIC ${CORE_SOURCES})

target_include_directories(uft_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/uft
    ${CMAKE_SOURCE_DIR}/include/uft/core
    ${CMAKE_SOURCE_DIR}/include/uft/compat
)

if(ZLIB_FOUND)
    target_link_libraries(uft_core PUBLIC uft_crc ZLIB::ZLIB)
    target_compile_definitions(uft_core PRIVATE UFT_HAS_ZLIB=1)
    message(STATUS "uft_core: zlib found — APD extras enabled")
else()
    target_link_libraries(uft_core PUBLIC uft_crc)
    message(STATUS "uft_core: zlib not found — APD extras disabled")
endif()

# uft_interleave uses floorf from libm
if(UNIX)
    target_link_libraries(uft_core PUBLIC m)
endif()

set_target_properties(uft_core PROPERTIES C_STANDARD 11)

if(NOT MSVC)
    target_compile_options(uft_core PRIVATE -Wno-unused-parameter)
endif()

message(STATUS "uft_core: ${CORE_SOURCES}")
