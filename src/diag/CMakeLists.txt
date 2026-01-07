# UFT diag Module
set(DIAG_SOURCES
    uft_disc_diagnostics.c
)

add_library(uft_diag STATIC ${DIAG_SOURCES})

target_include_directories(uft_diag PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
)

set_target_properties(uft_diag PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
)
