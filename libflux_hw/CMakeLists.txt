cmake_minimum_required(VERSION 3.16)

file(GLOB_RECURSE FLUX_HW_SOURCES src/*/*.c)
file(GLOB FLUX_HW_HEADERS include/*.h)

add_library(flux_hw STATIC ${FLUX_HW_SOURCES} ${FLUX_HW_HEADERS})

target_include_directories(flux_hw PUBLIC include)
target_link_libraries(flux_hw PUBLIC flux_core)

# USB library (platform-specific)
if(UNIX AND NOT APPLE)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBUSB REQUIRED libusb-1.0)
    target_link_libraries(flux_hw PRIVATE ${LIBUSB_LIBRARIES})
    target_include_directories(flux_hw PRIVATE ${LIBUSB_INCLUDE_DIRS})
endif()

target_compile_options(flux_hw PRIVATE
    $<$<C_COMPILER_ID:GNU,Clang>:$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall> $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wextra> $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wpedantic>>
    $<$<C_COMPILER_ID:MSVC>:/W4>
)
