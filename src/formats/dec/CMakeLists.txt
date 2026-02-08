# DEC RX50 Format Support

add_library(uft_dec STATIC
    uft_rx50.c
)

target_include_directories(uft_dec PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(uft_dec
    uft_core
)
