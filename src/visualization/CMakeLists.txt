# UFT visualization Module
set(VISUALIZATION_SOURCES
    display_track.c
)

add_library(uft_visualization STATIC ${VISUALIZATION_SOURCES})

target_include_directories(uft_visualization PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
)

set_target_properties(uft_visualization PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
)
