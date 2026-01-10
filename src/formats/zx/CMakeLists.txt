# ZX Spectrum Format Module
# Includes: BASIC tokenizer, TAP/TZX support, Screen converter

add_library(uft_zx STATIC
    uft_zxbasic.c
    uft_zxscreen.c
)

target_include_directories(uft_zx PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

# Math library for ldexp
if(NOT MSVC)
    target_link_libraries(uft_zx PRIVATE m)
endif()

message(STATUS "UFT ZX Spectrum module configured (BASIC, Screen)")
