# G71 Format Module (1571 GCR)
add_library(uft_g71 STATIC
    uft_g71.c
)

target_include_directories(uft_g71 PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(uft_g71 PUBLIC
    uft_core
)
