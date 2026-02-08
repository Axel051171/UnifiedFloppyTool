# Switch/MIG Dumper Module
# Provides Nintendo Switch cartridge support via hactool and MIG hardware

set(SWITCH_SOURCES
    uft_mig_dumper.c
    uft_switch_types.h
    uft_mig_dumper.h
    uft_xci_parser.h
)

# hactool sources (ISC License)
set(HACTOOL_SOURCES
    hactool/xci.c
    hactool/nca.c
    hactool/pfs0.c
    hactool/hfs0.c
    hactool/romfs.c
    hactool/save.c
    hactool/npdm.c
    hactool/kip.c
    hactool/nso.c
    hactool/nax0.c
    hactool/packages.c
    hactool/pki.c
    hactool/extkeys.c
    hactool/aes.c
    hactool/sha.c
    hactool/rsa.c
    hactool/utils.c
    hactool/filepath.c
    hactool/lz4.c
    hactool/bktr.c
    hactool/ConvertUTF.c
    hactool/cJSON.c
)

# mbedtls sources
file(GLOB MBEDTLS_SOURCES hactool/mbedtls/library/*.c)

add_library(uft_switch STATIC
    ${SWITCH_SOURCES}
    ${HACTOOL_SOURCES}
    ${MBEDTLS_SOURCES}
)

target_include_directories(uft_switch PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/hactool
    ${CMAKE_CURRENT_SOURCE_DIR}/hactool/mbedtls/include
)

# Platform-specific settings
if(WIN32)
    target_compile_definitions(uft_switch PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# Suppress warnings in third-party code
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    set_source_files_properties(${HACTOOL_SOURCES} ${MBEDTLS_SOURCES}
        PROPERTIES COMPILE_FLAGS "-w"
    )
endif()
