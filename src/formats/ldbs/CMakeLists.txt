# LDBS Format Module
add_library(uft_ldbs STATIC
    uft_ldbs.c
)

target_include_directories(uft_ldbs PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(uft_ldbs PUBLIC
    uft_core
)
