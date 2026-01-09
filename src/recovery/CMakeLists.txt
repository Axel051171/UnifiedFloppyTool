# UFT Recovery Module - Minimal Build
# Many recovery files have type definition conflicts (P0-2 TODO)

file(GLOB RECOVERY_SOURCES "*.c")

# Keep only basic recovery files without type conflicts
list(FILTER RECOVERY_SOURCES EXCLUDE REGEX ".*uft_forensic.*\\.c$")
list(FILTER RECOVERY_SOURCES EXCLUDE REGEX ".*uft_recovery_params\\.c$")
list(FILTER RECOVERY_SOURCES EXCLUDE REGEX ".*uft_multiread.*\\.c$")

if(RECOVERY_SOURCES)
    add_library(uft_recovery STATIC ${RECOVERY_SOURCES})
    target_include_directories(uft_recovery PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/uft
        ${CMAKE_SOURCE_DIR}/include/uft/recovery
    )
    set_target_properties(uft_recovery PROPERTIES C_STANDARD 11)
    if(NOT MSVC)
        target_compile_options(uft_recovery PRIVATE -Wno-unused-parameter)
    endif()
else()
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/uft_recovery_stub.c "/* stub */\n")
    add_library(uft_recovery STATIC ${CMAKE_CURRENT_BINARY_DIR}/uft_recovery_stub.c)
    message(STATUS "uft_recovery: Minimal build - forensic features disabled (P0-2)")
endif()
