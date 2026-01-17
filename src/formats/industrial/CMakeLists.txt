# Industrial/Embedded Formats (Heathkit, Cromemco)
add_library(uft_industrial STATIC
    uft_heathkit.c
    uft_cromemco.c
)
target_include_directories(uft_industrial PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(uft_industrial uft_core)
