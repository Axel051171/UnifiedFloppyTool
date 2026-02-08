# ═══════════════════════════════════════════════════════════════════════════════
# UFT Commodore Formats Module
# ═══════════════════════════════════════════════════════════════════════════════
#
# Supports: D64, D67, D71, D80, D81, D82, CRT, PRG, P00
#
# ═══════════════════════════════════════════════════════════════════════════════

set(COMMODORE_SOURCES
    crt.c
    d64.c
    d67.c
    d71.c
    d80.c
    d81.c
    d82.c
    uft_d64_view.c
)

add_library(uft_commodore STATIC ${COMMODORE_SOURCES})

target_include_directories(uft_commodore PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
)

set_target_properties(uft_commodore PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
)
