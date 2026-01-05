cmake_minimum_required(VERSION 3.16)

file(GLOB FLUX_FORMAT_SOURCES src/*.c)
file(GLOB FLUX_FORMAT_HEADERS include/*.h)

add_library(flux_format STATIC ${FLUX_FORMAT_SOURCES} ${FLUX_FORMAT_HEADERS})

target_include_directories(flux_format PUBLIC include)
target_link_libraries(flux_format PUBLIC flux_core)

target_compile_options(flux_format PRIVATE
    $<$<C_COMPILER_ID:GNU,Clang>:$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall> $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wextra> $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wpedantic>>
    $<$<C_COMPILER_ID:MSVC>:/W4>
)
