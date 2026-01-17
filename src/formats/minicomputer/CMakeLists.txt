# Minicomputer Formats (Data General Nova, Prime)
add_library(uft_minicomputer STATIC
    uft_dg_nova.c
    uft_prime.c
)
target_include_directories(uft_minicomputer PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(uft_minicomputer uft_core)
