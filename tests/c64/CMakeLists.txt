# C64 Tests - Link to library tests

if(TARGET uft_c64)
    # 6502 Disassembler Test
    add_executable(test_6502_disasm test_6502_disasm.c)
    target_link_libraries(test_6502_disasm uft_c64)
    add_test(NAME c64_6502_disasm COMMAND test_6502_disasm)
    
    # Drive Scanner Test
    add_executable(test_drive_scan test_drive_scan.c)
    target_link_libraries(test_drive_scan uft_c64)
    add_test(NAME c64_drive_scan COMMAND test_drive_scan)
    
    # PRG Parser Test
    add_executable(test_prg_parser test_prg_parser.c)
    target_link_libraries(test_prg_parser uft_c64)
    add_test(NAME c64_prg_parser COMMAND test_prg_parser)
    
    # CBM Disk Test
    add_executable(test_cbm_disk test_cbm_disk.c)
    target_link_libraries(test_cbm_disk uft_c64)
    add_test(NAME c64_cbm_disk COMMAND test_cbm_disk)
endif()
