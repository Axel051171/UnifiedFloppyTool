# ═══════════════════════════════════════════════════════════════════════════════
# UFT Sector Parser Module
# ═══════════════════════════════════════════════════════════════════════════════
# IBM FM/MFM Sector Parser - parses demodulated track byte streams
# ═══════════════════════════════════════════════════════════════════════════════

add_library(uft_sector STATIC
    uft_sector_parser.c
)

target_include_directories(uft_sector PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(uft_sector PRIVATE
    uft_core
)

# C11 Standard
set_target_properties(uft_sector PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
)

# ═══════════════════════════════════════════════════════════════════════════════
# Tests
# ═══════════════════════════════════════════════════════════════════════════════
if(BUILD_TESTING)
    add_executable(test_sector_parser test_sector_parser.c)
    target_link_libraries(test_sector_parser PRIVATE uft_sector)
    add_test(NAME test_sector_parser COMMAND test_sector_parser)
endif()
