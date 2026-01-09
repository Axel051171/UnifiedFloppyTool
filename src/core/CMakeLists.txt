# UFT Core Module - Minimal Build
# Many core files have type/error code conflicts (P0-2 TODO)

# Only include files that compile cleanly
set(CORE_SOURCES
    uft_common.c
    uft_const_correct.c
    uft_crc_utils.c
    uft_endian.c
    uft_flux_utils.c
    uft_hal.c
    uft_operation_result.c
    uft_platform.c
    uft_safe_string.c
    uft_string_utils.c
    uft_timer.c
    uft_usb.c
    uft_version.c
)

# Filter to only existing files
set(CORE_SOURCES_EXIST)
foreach(src ${CORE_SOURCES})
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${src}")
        list(APPEND CORE_SOURCES_EXIST ${CMAKE_CURRENT_SOURCE_DIR}/${src})
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
    target_link_libraries(uft_core PUBLIC uft_crc)
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
