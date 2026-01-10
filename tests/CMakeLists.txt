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

# Policy tests (Write Safety Gate)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/policy/CMakeLists.txt" AND TARGET uft_policy)
    add_subdirectory(policy)
endif()

# Analysis tests (XCopy Algorithms)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/analysis/CMakeLists.txt" AND TARGET uft_track_analysis)
    add_subdirectory(analysis)
endif()

# Hardened tests (if available)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/hardened/CMakeLists.txt" AND TARGET uft_hardened)
    add_subdirectory(hardened)
endif()

# Header validation (temporarily disabled - missing UFT_ERR_NOMEM)
# if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/header_validation/CMakeLists.txt")
#     add_subdirectory(header_validation)
# endif()

# Adapter tests (Format Adapters)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/adapters/CMakeLists.txt" AND TARGET uft_adapters)
    add_subdirectory(adapters)
endif()

# ZX Spectrum tests (Screen converter)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/zx/CMakeLists.txt" AND TARGET uft_zx)
    add_subdirectory(zx)
endif()
