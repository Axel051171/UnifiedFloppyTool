# UFT Commodore Business Machines Module
# Handles CBM DOS filesystem operations

set(CBM_SOURCES
    uft_cbm_dos.c
)

add_library(uft_cbm OBJECT ${CBM_SOURCES})

target_include_directories(uft_cbm PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/uft
    ${CMAKE_SOURCE_DIR}/include/uft/compat
)
