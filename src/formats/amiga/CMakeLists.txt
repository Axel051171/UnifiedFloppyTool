# ==============================================================================
# UFT Amiga Protection Module - CMakeLists.txt
#
# Protection detection based on XCopy Pro (1989-2011) algorithms
# Supports: Copylock, Rob Northen, V-Max, and more
# ==============================================================================

cmake_minimum_required(VERSION 3.16)

# ------------------------------------------------------------------------------
# Library: uft_amiga_protection
# ------------------------------------------------------------------------------

set(UFT_AMIGA_PROTECTION_SOURCES
    uft_amiga_protection.c
)

set(UFT_AMIGA_PROTECTION_HEADERS
    uft_amiga_protection.h
)

add_library(uft_amiga_protection STATIC 
    ${UFT_AMIGA_PROTECTION_SOURCES} 
    ${UFT_AMIGA_PROTECTION_HEADERS}
)

target_include_directories(uft_amiga_protection
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# C11 Standard
set_target_properties(uft_amiga_protection PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF
)

# Strict warnings
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(uft_amiga_protection PRIVATE
        -Wall -Wextra -Wpedantic
        -Wconversion -Wsign-conversion
    )
elseif(MSVC)
    target_compile_options(uft_amiga_protection PRIVATE
        /W4 /WX-
    )
endif()

# ------------------------------------------------------------------------------
# Installation
# ------------------------------------------------------------------------------

install(TARGETS uft_amiga_protection
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

install(FILES ${UFT_AMIGA_PROTECTION_HEADERS}
    DESTINATION include/uft/formats/amiga
)

# ------------------------------------------------------------------------------
# Summary
# ------------------------------------------------------------------------------

message(STATUS "UFT Amiga Protection Module configured")
