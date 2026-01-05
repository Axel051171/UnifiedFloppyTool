cmake_minimum_required(VERSION 3.16)

file(GLOB FLUX_CORE_SOURCES src/*.c src/c64/*.c src/filesystems/*.c)
file(GLOB FLUX_CORE_HEADERS include/*.h include/c64/*.h include/filesystems/*.h)

add_library(flux_core STATIC ${FLUX_CORE_SOURCES} ${FLUX_CORE_HEADERS})

target_include_directories(flux_core PUBLIC include)

if(UFT_ENABLE_SIMD)
    target_compile_definitions(flux_core PRIVATE UFT_ENABLE_SIMD)
endif()

target_compile_options(flux_core PRIVATE
    $<$<C_COMPILER_ID:GNU,Clang>:$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall> $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wextra> $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wpedantic>>
    $<$<C_COMPILER_ID:MSVC>:/W4>
)
