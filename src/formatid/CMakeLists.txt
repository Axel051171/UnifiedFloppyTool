# ==============================================================================
# UFT FormatID Module - Format Detection & Geometry Guessing
# Based on FAT BPB parsing and common geometry tables
# ==============================================================================

cmake_minimum_required(VERSION 3.16)

set(UFT_FORMATID_SOURCES
    guess.c
)

set(UFT_FORMATID_HEADERS
    ${CMAKE_SOURCE_DIR}/include/uft/formatid/guess.h
)

add_library(uft_formatid STATIC ${UFT_FORMATID_SOURCES} ${UFT_FORMATID_HEADERS})

target_include_directories(uft_formatid
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link with FAT BPB module (in src/fs)
if(TARGET uft_fat_bpb)
    target_link_libraries(uft_formatid PRIVATE uft_fat_bpb)
endif()

# C11 Standard
set_target_properties(uft_formatid PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF
)

# Strict warnings
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(uft_formatid PRIVATE -Wall -Wextra -Wpedantic)
elseif(MSVC)
    target_compile_options(uft_formatid PRIVATE /W4)
endif()

message(STATUS "UFT FormatID Module configured")
