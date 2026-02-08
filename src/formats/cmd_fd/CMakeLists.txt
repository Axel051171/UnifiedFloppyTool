# CMD FD Format Module (D1M/D2M/D4M)
add_library(uft_cmd_fd STATIC
    uft_cmd_fd.c
)

target_include_directories(uft_cmd_fd PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(uft_cmd_fd PUBLIC
    uft_core
)
