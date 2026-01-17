# Japanese Extended Formats (Sharp X1, Sanyo MBC, Hitachi S1)
add_library(uft_japanese_ext STATIC
    uft_sharp_x1.c
    uft_sanyo_mbc.c
    uft_hitachi_s1.c
)
target_include_directories(uft_japanese_ext PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(uft_japanese_ext uft_core)
