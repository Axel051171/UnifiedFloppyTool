# ═══════════════════════════════════════════════════════════════════════════════
# UFT Disk Image Format Parsers
# ═══════════════════════════════════════════════════════════════════════════════
# Modules: IMD, TD0, DMK, FDI, FDC, Altair
# Version: 4.0.0
# ═══════════════════════════════════════════════════════════════════════════════

cmake_minimum_required(VERSION 3.16)

project(uft_disk_formats
    VERSION 4.0.0
    DESCRIPTION "UFT Disk Image Format Parsers"
    LANGUAGES C
)

# ═══════════════════════════════════════════════════════════════════════════════
# Library Definition
# ═══════════════════════════════════════════════════════════════════════════════

set(DISK_FORMATS_SOURCES
    uft_imd.c
    uft_td0.c
    uft_dmk.c
    uft_fdi.c
    uft_fdc.c
    uft_altair_hd.c
)

add_library(uft_disk_formats STATIC ${DISK_FORMATS_SOURCES})

target_include_directories(uft_disk_formats PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/uft
)

# C99/C11 standard
set_target_properties(uft_disk_formats PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
)

# Compiler warnings
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(uft_disk_formats PRIVATE
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall> $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wextra> $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wpedantic>
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wno-unused-parameter>
    )
elseif(MSVC)
    target_compile_options(uft_disk_formats PRIVATE /W4)
endif()

# ═══════════════════════════════════════════════════════════════════════════════
# Tests
# ═══════════════════════════════════════════════════════════════════════════════

if(BUILD_TESTING)
    enable_testing()
    
    add_executable(test_imd ${CMAKE_SOURCE_DIR}/tests/test_imd.c)
    target_link_libraries(test_imd uft_disk_formats)
    add_test(NAME test_imd COMMAND test_imd)
    
    add_executable(test_td0 ${CMAKE_SOURCE_DIR}/tests/test_td0.c)
    target_link_libraries(test_td0 uft_disk_formats)
    add_test(NAME test_td0 COMMAND test_td0)
    
    add_executable(test_dmk ${CMAKE_SOURCE_DIR}/tests/test_dmk.c)
    target_link_libraries(test_dmk uft_disk_formats)
    add_test(NAME test_dmk COMMAND test_dmk)
endif()

message(STATUS "  Disk Formats: IMD, TD0, DMK, FDI, FDC, Altair")
