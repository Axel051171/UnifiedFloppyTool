# UFT XDF Module - CMakeLists.txt
# Universal Forensic Disk Container Family

set(XDF_SOURCES
    uft_xdf_core.c
    uft_xdf_api.c
    uft_xdf_api_impl.c
)

set(XDF_HEADERS
    ${CMAKE_SOURCE_DIR}/include/uft/xdf/uft_xdf_core.h
    ${CMAKE_SOURCE_DIR}/include/uft/xdf/uft_xdf_dxdf.h
    ${CMAKE_SOURCE_DIR}/include/uft/xdf/uft_xdf_pxdf.h
    ${CMAKE_SOURCE_DIR}/include/uft/xdf/uft_xdf_txdf.h
    ${CMAKE_SOURCE_DIR}/include/uft/xdf/uft_xdf_zxdf.h
    ${CMAKE_SOURCE_DIR}/include/uft/xdf/uft_xdf_mxdf.h
)

add_library(uft_xdf STATIC ${XDF_SOURCES} ${XDF_HEADERS})

target_include_directories(uft_xdf PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/uft
    ${CMAKE_SOURCE_DIR}/include/uft/xdf
)

uft_link_math(uft_xdf)

set_target_properties(uft_xdf PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
)

if(MSVC)
    target_compile_options(uft_xdf PRIVATE /W3)
else()
    target_compile_options(uft_xdf PRIVATE -Wall -Wextra -Wno-unused-parameter -Wno-unused-function)
endif()

# Example program
add_executable(xdf_example xdf_example.c)
target_link_libraries(xdf_example PRIVATE uft_xdf)
target_include_directories(xdf_example PRIVATE ${CMAKE_SOURCE_DIR}/include)
uft_link_math(xdf_example)
