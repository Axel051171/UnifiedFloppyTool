# Motorola Formats (VersaDOS)
add_library(uft_motorola STATIC
    uft_versados.c
)
target_include_directories(uft_motorola PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(uft_motorola uft_core)
