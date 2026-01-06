cmake_minimum_required(VERSION 3.16)

# Core Recovery Library - PLL, Multi-Pass, Reed-Solomon, etc.
file(GLOB RECOVERY_SOURCES *.c)

add_library(flux_recovery STATIC ${RECOVERY_SOURCES})

target_include_directories(flux_recovery PUBLIC 
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/uft
)

# Link to core for shared types
target_link_libraries(flux_recovery PUBLIC flux_core)

# Math library for PLL calculations via zentrale Funktion
uft_link_math(flux_recovery)

# Compiler warnings
target_compile_options(flux_recovery PRIVATE
    $<$<C_COMPILER_ID:GNU,Clang>:$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall> $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wextra>>
    $<$<C_COMPILER_ID:MSVC>:/W4>
)
