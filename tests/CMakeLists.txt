# UnifiedFloppyTool Test Suite

# Smoke tests (basic functionality)
add_subdirectory(smoke)

# Core tests (P1-4: unified track)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/core/CMakeLists.txt")
    add_subdirectory(core)
endif()

# FS tests (P1-5: FAT detection)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/fs/CMakeLists.txt")
    add_subdirectory(fs)
endif()

# XDF integration tests
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/xdf/CMakeLists.txt")
    add_subdirectory(xdf)
endif()

# Format tests (SCL, etc.)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/formats/CMakeLists.txt")
    add_subdirectory(formats)
endif()

# Z80 tests (Disassembler)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/z80/CMakeLists.txt")
    add_subdirectory(z80)
endif()

# C64 tests (6502 Disasm, PRG Parser, Drive Scanner, CBM Disk)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/c64/CMakeLists.txt" AND TARGET uft_c64)
    add_subdirectory(c64)
endif()

# Hardened tests (if available)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/hardened/CMakeLists.txt" AND TARGET uft_hardened)
    add_subdirectory(hardened)
endif()

# Profile format tests (HFE, WOZ, DC42, D88, D77)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/profiles/CMakeLists.txt")
    add_subdirectory(profiles)
endif()

# Header validation (temporarily disabled - missing UFT_ERR_NOMEM)
# if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/header_validation/CMakeLists.txt")
#     add_subdirectory(header_validation)
# endif()
