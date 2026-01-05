# Examples CMakeLists.txt

add_executable(example_read_flux
    example_read_flux.c
)

target_link_libraries(example_read_flux
    flux_core
)

# v2.6.1 - Amiga/ADF Example
add_executable(example_amiga_adf
    example_amiga_adf.c
)

target_link_libraries(example_amiga_adf
    flux_core
    flux_format
)

# v2.6.2 - X-Copy Error System Example
add_executable(example_xcopy_errors
    example_xcopy_errors.c
)

target_link_libraries(example_xcopy_errors
    flux_core
)

# v2.6.3 - Bootblock Scanner Example
add_executable(example_bootblock_scanner
    example_bootblock_scanner.c
)

target_link_libraries(example_bootblock_scanner
    flux_core
    flux_format
)
