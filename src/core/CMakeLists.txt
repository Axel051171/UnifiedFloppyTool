# UFT Core Module - Minimal Build
# Many core files have type/error code conflicts (P0-2 TODO)

# Get all existing .c files in this directory
file(GLOB CORE_ALL_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.c")

# List of files known to compile cleanly
set(CORE_WHITELIST
    uft_apd_format.c
    uft_decode_score.c
    uft_fat_editor.c
    uft_format_registry.c
    uft_format_verify.c
    uft_image_db.c
    uft_merge_engine.c
    uft_mfm_codec.c
    uft_operation_result.c
    uft_param_bridge.c
    uft_platform.c
    uft_pll_unified.c
    uft_process.c
    uft_protection_pipeline.c
    uft_safe_alloc.c
    uft_safe_io.c
    uft_safe_string.c
    uft_settings.c
    uft_sos_protection.c
    uft_track.c
    uft_write_verify_pipeline.c
)

# Filter to only whitelisted files
set(CORE_SOURCES_EXIST)
foreach(src ${CORE_WHITELIST})
    set(full_path "${CMAKE_CURRENT_SOURCE_DIR}/${src}")
    if(EXISTS "${full_path}")
        list(APPEND CORE_SOURCES_EXIST "${full_path}")
        message(STATUS "uft_core: Adding ${src}")
    else()
        message(STATUS "uft_core: Skipping ${src} (not found)")
    endif()
endforeach()

if(CORE_SOURCES_EXIST)
    add_library(uft_core STATIC ${CORE_SOURCES_EXIST})
    target_include_directories(uft_core PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/uft
        ${CMAKE_SOURCE_DIR}/include/uft/core
        ${CMAKE_SOURCE_DIR}/include/uft/compat
    )
    target_link_libraries(uft_core PUBLIC uft_crc z)
    set_target_properties(uft_core PROPERTIES C_STANDARD 11)
    if(NOT MSVC)
        target_compile_options(uft_core PRIVATE -Wno-unused-parameter)
    endif()
else()
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/uft_core_stub.c "/* stub */\n")
    add_library(uft_core STATIC ${CMAKE_CURRENT_BINARY_DIR}/uft_core_stub.c)
    target_link_libraries(uft_core PUBLIC uft_crc)
endif()
message(STATUS "uft_core: Minimal build - see P0-2 for full feature restoration")
