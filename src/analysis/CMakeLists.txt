# UFT analysis Module
set(ANALYSIS_SOURCES
    uft_fdc_analyzer.c
    uft_metrics.c
    uft_surface_analysis.c
    uft_surface_analyzer.c
)

add_library(uft_analysis STATIC ${ANALYSIS_SOURCES})

target_include_directories(uft_analysis PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
)

set_target_properties(uft_analysis PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
)
