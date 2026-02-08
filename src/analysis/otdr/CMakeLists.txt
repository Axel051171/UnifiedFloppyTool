# OTDR Signal Analysis Module
# OTDR-style floppy disk flux quality analysis + TDFC+ signal processing

set(OTDR_SOURCES
    floppy_otdr.c
    tdfc.c
    tdfc_plus.c
    uft_otdr_bridge.c
)

set(OTDR_HEADERS
    ${CMAKE_SOURCE_DIR}/include/uft/analysis/floppy_otdr.h
    ${CMAKE_SOURCE_DIR}/include/uft/analysis/tdfc.h
    ${CMAKE_SOURCE_DIR}/include/uft/analysis/uft_otdr_bridge.h
    FloppyOtdrWidget.h
)

# C library (no Qt dependency)
add_library(uft_otdr_core STATIC
    floppy_otdr.c
    tdfc.c
    tdfc_plus.c
    uft_otdr_bridge.c
)
target_include_directories(uft_otdr_core PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/uft/analysis
)
target_link_libraries(uft_otdr_core PRIVATE m)
set_target_properties(uft_otdr_core PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
)

# Qt widget (requires Qt6)
if(Qt6_FOUND)
    add_library(uft_otdr_widget STATIC
        FloppyOtdrWidget.h   # Header-only MOC source
    )
    target_link_libraries(uft_otdr_widget PUBLIC
        uft_otdr_core
        Qt6::Widgets
    )
    target_include_directories(uft_otdr_widget PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
    )
    set_target_properties(uft_otdr_widget PROPERTIES
        AUTOMOC ON
    )
endif()
