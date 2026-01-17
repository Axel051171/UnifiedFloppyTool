# FLEX/UniFLEX Format Support

add_library(uft_flex STATIC
    uft_flex.c
)

target_include_directories(uft_flex PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(uft_flex
    uft_core
)
