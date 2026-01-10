# Z80 Module - Disassembler and utilities

add_library(uft_z80 STATIC
    uft_z80_disasm.c
)

target_include_directories(uft_z80 PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)
