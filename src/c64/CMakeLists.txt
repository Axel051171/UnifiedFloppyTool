# ==============================================================================
# UFT C64 Module - CMakeLists.txt
# 
# Components:
#   - 6502 Disassembler
#   - PRG Parser + BASIC Decoder
#   - CBM Drive/Tool Scanner
# ==============================================================================

cmake_minimum_required(VERSION 3.16)

# ------------------------------------------------------------------------------
# Library: uft_c64
# ------------------------------------------------------------------------------

set(UFT_C64_SOURCES
    uft_6502_disasm.c
    uft_c64_prg.c
    uft_cbm_drive_scan.c
    uft_cbm_disk.c
    uft_fastloader_db.c
)

set(UFT_C64_HEADERS
    ${CMAKE_SOURCE_DIR}/include/uft/c64/uft_6502_disasm.h
    ${CMAKE_SOURCE_DIR}/include/uft/c64/uft_c64_prg.h
    ${CMAKE_SOURCE_DIR}/include/uft/c64/uft_cbm_drive_scan.h
    ${CMAKE_SOURCE_DIR}/include/uft/c64/uft_cbm_disk.h
    ${CMAKE_SOURCE_DIR}/include/uft/c64/uft_fastloader_db.h
)

add_library(uft_c64 STATIC ${UFT_C64_SOURCES} ${UFT_C64_HEADERS})

target_include_directories(uft_c64
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# C11 Standard
set_target_properties(uft_c64 PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF
)

# Strict warnings
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(uft_c64 PRIVATE
        -Wall -Wextra -Wpedantic
        -Wconversion -Wsign-conversion
        -Wstrict-prototypes
    )
elseif(MSVC)
    target_compile_options(uft_c64 PRIVATE
        /W4 /WX-
    )
endif()

# ------------------------------------------------------------------------------
# Installation
# ------------------------------------------------------------------------------

install(TARGETS uft_c64
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

install(FILES ${UFT_C64_HEADERS}
    DESTINATION include/uft/c64
)

# ------------------------------------------------------------------------------
# Tests (if enabled)
# ------------------------------------------------------------------------------

# ------------------------------------------------------------------------------
# Tests (registered in tests/c64/CMakeLists.txt)
# ------------------------------------------------------------------------------

# ------------------------------------------------------------------------------
# Summary
# ------------------------------------------------------------------------------

message(STATUS "UFT C64 Module configured:")
message(STATUS "  - 6502 Disassembler")
message(STATUS "  - PRG Parser + BASIC v2 Decoder")
message(STATUS "  - CBM Drive/Tool Scanner")
