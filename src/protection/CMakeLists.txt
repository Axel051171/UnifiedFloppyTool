# Protection Detection Module
add_library(uft_protection STATIC
    uft_protection_api.c
    uft_protection_stubs.c
    uft_geos_protection.c
)

target_include_directories(uft_protection PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(uft_protection PUBLIC
    uft_core
)

# C64 Copy Protection Detection (Super-Kit 1541 based)
add_subdirectory(c64)
