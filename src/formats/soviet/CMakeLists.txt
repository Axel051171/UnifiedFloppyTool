# Soviet/Eastern European Formats (BK-0010)

add_library(uft_soviet STATIC
    uft_bk0010.c
)

target_include_directories(uft_soviet PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(uft_soviet
    uft_core
)
