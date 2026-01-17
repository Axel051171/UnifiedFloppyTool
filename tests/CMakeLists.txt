# UFT Test Suite
# Part of INDUSTRIAL_UPGRADE_PLAN

enable_testing()

# Find all test source files
file(GLOB TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/test_*.c")

# Exclude tests with missing dependencies (TODO: implement these modules)
set(EXCLUDED_TESTS
    "test_mega65"           # Missing uft_mega65.h
    "test_mega65_fat32"     # Missing uft_d81_probe, uft_mbr_* functions
    "test_platform"         # Missing platform test framework
    "test_pll"              # Missing PLL test dependencies  
    "test_recovery"         # Missing recovery test framework
    "test_roundtrip"        # Missing roundtrip dependencies
    "test_smoke"            # Missing smoke test framework
    "test_woz_writer"       # Missing WOZ writer test deps
    "test_writer_backend"   # Missing writer backend
    "test_libdsk_formats"   # Type conflicts with unified_types
    "test_ti99_formats"     # Type conflicts
    "test_new_formats"      # Type conflicts
    "test_fat_extensions"   # Missing uft_fs module (not built in CI)
)

# Create test executables
foreach(test_source ${TEST_SOURCES})
    get_filename_component(test_name ${test_source} NAME_WE)
    
    # Skip excluded tests
    list(FIND EXCLUDED_TESTS ${test_name} _idx)
    if(${_idx} GREATER -1)
        message(STATUS "UFT Test SKIPPED (TODO): ${test_name}")
        continue()
    endif()
    
    add_executable(${test_name} ${test_source})
    
    target_include_directories(${test_name} PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/uft
    )
    
    # Base libraries for all tests
    set(TEST_LIBS uft_core uft_crc uft_ride)
    
    # Add test-specific dependencies
    if(${test_name} STREQUAL "test_ti99_formats")
        list(APPEND TEST_LIBS uft_ti99)
    elseif(${test_name} STREQUAL "test_fat_extensions")
        if(TARGET uft_fs)
            list(APPEND TEST_LIBS uft_fs)
        endif()
    elseif(${test_name} STREQUAL "test_c64_protection")
        list(APPEND TEST_LIBS uft_c64_protection)
    elseif(${test_name} STREQUAL "test_track_align")
        # Include Track Alignment source directly
        target_sources(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/src/protection/c64/uft_track_align.c)
    elseif(${test_name} STREQUAL "test_nib_format")
        # Include NIB/NB2/NBZ format source directly
        target_sources(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/src/formats/c64/uft_nib_format.c)
    elseif(${test_name} STREQUAL "test_gcr_ops")
        # Include GCR Operations source directly
        target_sources(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/src/formats/c64/uft_gcr_ops.c)
    elseif(${test_name} STREQUAL "test_d64_g64")
        # Include D64/G64 conversion source directly
        target_sources(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/src/formats/c64/uft_d64_g64.c)
    elseif(${test_name} STREQUAL "test_track_writer")
        # Include Track Writer source directly
        target_sources(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/src/hardware/uft_track_writer.c)
    elseif(${test_name} STREQUAL "test_c64_protection_ext")
        # Include Extended Protection source directly
        target_sources(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/src/protection/c64/uft_c64_protection_ext.c)
    elseif(${test_name} STREQUAL "test_flux_analysis")
        # Include Flux Analysis source directly
        target_sources(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/src/flux/uft_flux_analysis.c)
        target_link_libraries(${test_name} PRIVATE m)
    elseif(${test_name} STREQUAL "test_c64_bam")
        # Include BAM Editor source directly
        target_sources(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/src/formats/c64/uft_c64_bam.c)
    elseif(${test_name} STREQUAL "test_bam_editor")
        # Include BAM Editor source directly
        target_sources(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/src/formats/c64/uft_bam_editor.c)
    elseif(${test_name} STREQUAL "test_d64_file")
        # Include D64 file and BAM sources
        target_sources(${test_name} PRIVATE 
            ${CMAKE_SOURCE_DIR}/src/formats/c64/uft_d64_file.c
            ${CMAKE_SOURCE_DIR}/src/formats/c64/uft_bam_editor.c)
    elseif(${test_name} STREQUAL "test_t64")
        # Include T64 source directly
        target_sources(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/src/formats/c64/uft_t64.c)
    elseif(${test_name} STREQUAL "test_tap")
        # Include TAP source directly
        target_sources(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/src/formats/c64/uft_tap.c)
    elseif(${test_name} STREQUAL "test_d71_d81")
        # Include D71/D81 source directly
        target_sources(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/src/formats/c64/uft_d71_d81.c)
    elseif(${test_name} STREQUAL "test_crt")
        # Include CRT source directly
        target_sources(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/src/formats/c64/uft_crt.c)
    elseif(${test_name} STREQUAL "test_reu")
        # Include REU source directly
        target_sources(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/src/formats/c64/uft_reu.c)
    elseif(${test_name} STREQUAL "test_sid")
        # Include SID source directly
        target_sources(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/src/formats/c64/uft_sid.c)
    elseif(${test_name} STREQUAL "test_geos")
        # Include GEOS source directly
        target_sources(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/src/formats/c64/uft_geos.c)
    elseif(${test_name} STREQUAL "test_p00")
        # Include P00 source directly
        target_sources(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/src/formats/c64/uft_p00.c)
    elseif(${test_name} STREQUAL "test_vsf")
        # Include VSF source directly
        target_sources(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/src/formats/c64/uft_vsf.c)
    elseif(${test_name} STREQUAL "test_cmd")
        # Include CMD source directly
        target_sources(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/src/formats/c64/uft_cmd.c)
    elseif(${test_name} STREQUAL "test_switch")
        # Include Switch source directly
        target_sources(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/src/formats/nintendo/uft_switch.c)
    elseif(${test_name} STREQUAL "test_frz")
        # Include FRZ source directly
        target_sources(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/src/formats/c64/uft_frz.c)
    elseif(${test_name} STREQUAL "test_c64rom")
        # Include C64ROM source directly
        target_sources(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/src/formats/c64/uft_c64rom.c)
    elseif(${test_name} STREQUAL "test_ps1")
        # Include PS1 source directly
        target_sources(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/src/formats/sony/uft_ps1.c)
    elseif(${test_name} STREQUAL "test_freezer")
        # Include Freezer source directly
        target_sources(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/src/formats/c64/uft_freezer.c)
    elseif(${test_name} STREQUAL "test_genesis")
        # Include Genesis source directly
        target_sources(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/src/formats/sega/uft_genesis.c)
    elseif(${test_name} STREQUAL "test_3ds")
        # Include 3DS source directly
        target_sources(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/src/formats/nintendo/uft_3ds.c)
    elseif(${test_name} STREQUAL "test_gameboy")
        # Include Game Boy source directly
        target_sources(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/src/formats/nintendo/uft_gameboy.c)
    elseif(${test_name} STREQUAL "test_n64")
        # Include N64 source directly
        target_sources(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/src/formats/nintendo/uft_n64.c)
    elseif(${test_name} STREQUAL "test_atari")
        # Include Atari source directly
        target_sources(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/src/formats/atari/uft_atari.c)
    elseif(${test_name} STREQUAL "test_pce")
        # Include PCE source directly
        target_sources(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/src/formats/nec/uft_pce.c)
    elseif(${test_name} STREQUAL "test_neogeo")
        # Include Neo Geo source directly
        target_sources(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/src/formats/snk/uft_neogeo.c)
    elseif(${test_name} STREQUAL "test_sms")
        # Include SMS source directly
        target_sources(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/src/formats/sega/uft_sms.c)
    elseif(${test_name} STREQUAL "test_nes_snes_nds")
        # Include NES, SNES, NDS sources
        target_sources(${test_name} PRIVATE 
            ${CMAKE_SOURCE_DIR}/src/formats/nintendo/uft_nes.c
            ${CMAKE_SOURCE_DIR}/src/formats/nintendo/uft_snes.c
            ${CMAKE_SOURCE_DIR}/src/formats/nintendo/uft_nds.c)
    elseif(${test_name} STREQUAL "test_floppy_formats")
        # Include floppy format sources
        target_sources(${test_name} PRIVATE 
            ${CMAKE_SOURCE_DIR}/src/formats/atari/uft_atari_st.c
            ${CMAKE_SOURCE_DIR}/src/formats/sinclair/uft_spectrum.c
            ${CMAKE_SOURCE_DIR}/src/formats/sega/uft_sega_cd.c)
    elseif(${test_name} STREQUAL "test_fat_bootsector")
        # Include FAT bootsector source directly
        target_sources(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/src/formats/fat/uft_fat_bootsector.c)
    elseif(${test_name} STREQUAL "test_diskcopy")
        # Include Disk Copy source directly (Apple format)
        target_sources(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/src/formats/apple/uft_diskcopy.c)
    endif()
    
    target_link_libraries(${test_name} PRIVATE ${TEST_LIBS})
    
    # Register as CTest
    add_test(NAME ${test_name} COMMAND ${test_name})
    
    set_target_properties(${test_name} PROPERTIES
        C_STANDARD 11
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests
    )
    
    message(STATUS "UFT Test ENABLED: ${test_name}")
endforeach()

message(STATUS "════════════════════════════════════════════════════")
message(STATUS "UFT Tests: Run 'ctest' after build to execute")
message(STATUS "════════════════════════════════════════════════════")
