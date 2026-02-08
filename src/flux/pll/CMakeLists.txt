# UFT PLL Module
# Part of UnifiedFloppyTool

set(PLL_SOURCES
    uft_pll_pi.c
    # uft_pll_params.c  # DISABLED: API mismatch with header
)

add_library(uft_pll STATIC ${PLL_SOURCES})

target_include_directories(uft_pll PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/uft
    ${CMAKE_SOURCE_DIR}/include/uft/flux
)

set_target_properties(uft_pll PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
)

if(MSVC)
    target_compile_options(uft_pll PRIVATE /W3)
else()
    target_compile_options(uft_pll PRIVATE $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall> $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wextra>)
endif()
