# Embedded/Industrial Formats (Heathkit, Cromemco)
add_library(uft_embedded STATIC
    uft_heathkit.c
    uft_cromemco.c
)
target_include_directories(uft_embedded PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(uft_embedded uft_core)
