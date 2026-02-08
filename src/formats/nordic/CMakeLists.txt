# Nordic Formats (ABC 80/800)

add_library(uft_nordic STATIC
    uft_abc800.c
)

target_include_directories(uft_nordic PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(uft_nordic
    uft_core
)
