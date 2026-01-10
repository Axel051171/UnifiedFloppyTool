# UFT Format Adapters - CMakeLists.txt
# Connects format parsers to XDF-API

set(ADAPTER_SOURCES
    uft_adf_adapter.c
    uft_d64_adapter.c
    uft_img_adapter.c
    uft_trd_adapter.c
)

set(ADAPTER_HEADERS
    ${CMAKE_SOURCE_DIR}/include/uft/adapters/uft_adf_adapter.h
    ${CMAKE_SOURCE_DIR}/include/uft/adapters/uft_d64_adapter.h
    ${CMAKE_SOURCE_DIR}/include/uft/adapters/uft_img_adapter.h
    ${CMAKE_SOURCE_DIR}/include/uft/adapters/uft_trd_adapter.h
)

add_library(uft_adapters STATIC ${ADAPTER_SOURCES} ${ADAPTER_HEADERS})

target_include_directories(uft_adapters PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(uft_adapters PUBLIC
    uft_xdf
)

set_target_properties(uft_adapters PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
)

if(MSVC)
    target_compile_options(uft_adapters PRIVATE /W3)
else()
    target_compile_options(uft_adapters PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()
