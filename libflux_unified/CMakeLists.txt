cmake_minimum_required(VERSION 3.16)

file(GLOB FLUX_UNIFIED_SOURCES src/*.c)
file(GLOB FLUX_UNIFIED_HEADERS include/*.h)

add_library(flux_unified STATIC ${FLUX_UNIFIED_SOURCES} ${FLUX_UNIFIED_HEADERS})

target_include_directories(flux_unified PUBLIC include)
target_link_libraries(flux_unified PUBLIC flux_core flux_format flux_hw)

target_compile_options(flux_unified PRIVATE
    $<$<C_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
    $<$<C_COMPILER_ID:MSVC>:/W4>
)
