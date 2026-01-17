# Obscure/Niche Formats (PMC, Calcomp, Pyldin, RC759, Applix)
add_library(uft_obscure STATIC
    uft_pmc_micromate.c
    uft_calcomp.c
    uft_pyldin.c
    uft_rc759.c
    uft_applix.c
)
target_include_directories(uft_obscure PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(uft_obscure uft_core)
