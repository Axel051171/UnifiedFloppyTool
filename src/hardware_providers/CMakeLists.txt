cmake_minimum_required(VERSION 3.16)

# C++ Hardware Provider Layer (Qt-friendly wrappers)
set(CMAKE_AUTOMOC ON)

file(GLOB HWPROVIDER_SOURCES *.cpp)
file(GLOB HWPROVIDER_HEADERS *.h)

add_library(hardware_providers STATIC 
    ${HWPROVIDER_SOURCES} 
    ${HWPROVIDER_HEADERS}
)

target_include_directories(hardware_providers PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
)

# Qt dependency
find_package(Qt6 COMPONENTS Core QUIET)
if(Qt6_FOUND)
    target_link_libraries(hardware_providers PUBLIC Qt6::Core)
else()
    find_package(Qt5 COMPONENTS Core REQUIRED)
    target_link_libraries(hardware_providers PUBLIC Qt5::Core)
endif()

# Link to C hardware backends
target_link_libraries(hardware_providers PUBLIC flux_hw)

# Compiler warnings
target_compile_options(hardware_providers PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)
