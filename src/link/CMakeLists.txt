# ==============================================================================
# UFT Link Module - Serial Stream (TransWarp Integration)
# Cross-platform serial I/O for Amiga disk capture and similar workflows
# ==============================================================================

cmake_minimum_required(VERSION 3.16)

set(UFT_LINK_SOURCES
    serial_stream.c
)

set(UFT_LINK_HEADERS
    ${CMAKE_SOURCE_DIR}/include/uft/link/serial_stream.h
)

add_library(uft_link STATIC ${UFT_LINK_SOURCES} ${UFT_LINK_HEADERS})

target_include_directories(uft_link
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# C11 Standard
set_target_properties(uft_link PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF
)

# Platform-specific linking
if(WIN32)
    # Windows: No extra libs needed for serial (uses Win32 API)
elseif(UNIX)
    # POSIX: termios is part of libc
endif()

# Strict warnings
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(uft_link PRIVATE -Wall -Wextra -Wpedantic)
elseif(MSVC)
    target_compile_options(uft_link PRIVATE /W4)
endif()

message(STATUS "UFT Link Module (Serial Stream) configured")
