# Z80 Tests

add_executable(test_z80_disasm test_z80_disasm.c)
target_link_libraries(test_z80_disasm uft_z80)
target_include_directories(test_z80_disasm PRIVATE ${PROJECT_SOURCE_DIR}/include)
add_test(NAME z80_disasm COMMAND test_z80_disasm)
