cmake_minimum_required(VERSION 3.16)
project(uft_floppy VERSION 1.0.0 LANGUAGES C)

# ============================================================================
# Options
# ============================================================================

option(UFT_BUILD_EXAMPLES "Build example programs" ON)
option(UFT_BUILD_TESTS "Build unit tests" OFF)
option(UFT_ENABLE_SANITIZERS "Enable AddressSanitizer and UBSan" OFF)

# ============================================================================
# Compiler Settings
# ============================================================================

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Warning flags
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(
        -Wall -Wextra -Wpedantic
        -Wformat=2 -Wformat-security
        -Wnull-dereference -Wstack-protector
        -Wconversion -Wsign-conversion
        -Wpointer-arith -Wcast-qual
        -Wshadow -Wdouble-promotion
    )
    
    if(UFT_ENABLE_SANITIZERS)
        add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address,undefined)
    endif()
    
elseif(MSVC)
    add_compile_options(/W4 /WX /permissive-)
endif()

# ============================================================================
# Library
# ============================================================================

add_library(uft_floppy STATIC
    src/uft_floppy_io.c
    src/uft_floppy_geometry.c
    src/uft_fat12.c
    src/encoding/uft_gcr.c
    src/encoding/uft_mfm.c
    src/formats/uft_d64.c
    src/formats/uft_adf.c
)

target_include_directories(uft_floppy
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/encoding>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/formats>
        $<INSTALL_INTERFACE:include>
)

if(WIN32)
    target_compile_definitions(uft_floppy PRIVATE UFT_PLATFORM_WINDOWS=1)
elseif(UNIX AND NOT APPLE)
    target_compile_definitions(uft_floppy PRIVATE UFT_PLATFORM_LINUX=1)
elseif(APPLE)
    target_compile_definitions(uft_floppy PRIVATE UFT_PLATFORM_MACOS=1)
endif()

# ============================================================================
# Examples
# ============================================================================

if(UFT_BUILD_EXAMPLES)
    add_executable(floppy_info examples/floppy_info.c)
    target_link_libraries(floppy_info PRIVATE uft_floppy)
    
    add_executable(floppy_dir examples/floppy_dir.c)
    target_link_libraries(floppy_dir PRIVATE uft_floppy)
    
    add_executable(floppy_dump examples/floppy_dump.c)
    target_link_libraries(floppy_dump PRIVATE uft_floppy)
endif()

# ============================================================================
# Installation
# ============================================================================

include(GNUInstallDirs)

install(TARGETS uft_floppy
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
