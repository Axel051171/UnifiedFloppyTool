# UFT MSA Format Module
# Atari ST MSA (Magic Shadow Archiver) parser with RLE compression

add_library(uft_msa STATIC
    uft_msa.c
)

target_include_directories(uft_msa
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

# No external dependencies - pure C99
set_target_properties(uft_msa PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
)

# Link to core for common utilities (if needed)
# target_link_libraries(uft_msa PRIVATE uft_core)
