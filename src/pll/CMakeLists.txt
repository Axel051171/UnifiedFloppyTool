# UFT PLL Module
# Variable Frequency Oscillator implementations for MFM data separation

add_library(uft_pll STATIC
    uft_vfo_pid3.c
)

target_include_directories(uft_pll PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link math library
target_link_libraries(uft_pll PRIVATE m)
