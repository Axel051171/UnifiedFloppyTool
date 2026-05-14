# UFT Test Suite
# Part of INDUSTRIAL_UPGRADE_PLAN

enable_testing()

# Catch dangling library references at configure time, not at link time.
# Without this, target_link_libraries(... uft_ride) where uft_ride is
# neither a target nor a system library is passed straight to the linker
# as -luft_ride, only failing during link with a confusing error.
set(CMAKE_LINK_LIBRARIES_ONLY_TARGETS ON)

# System libs (libm) need to be resolved as imported paths so the strict
# ONLY_TARGETS check above accepts them.
if(UNIX)
    find_library(M_LIBRARY m)
    if(NOT M_LIBRARY)
        # On glibc, libm is part of libc — link-line just gets nothing.
        set(M_LIBRARY "")
    endif()
endif()

# Find all test source files. The C-loop below handles .c + a curated set
# of header-only C++ tests (no Qt deps). The Qt-loop further down handles
# all remaining .cpp tests (which need Qt6::Test / SerialPort / providers).
file(GLOB TEST_SOURCES_C    "${CMAKE_CURRENT_SOURCE_DIR}/test_*.c")
file(GLOB TEST_SOURCES_CPP  "${CMAKE_CURRENT_SOURCE_DIR}/test_*.cpp")

# Header-only C++ tests that join the main C-loop (no Qt linking).
# Add new entries here when introducing a header-only foundation test.
set(_HEADER_ONLY_CPP_TESTS
    "${CMAKE_CURRENT_SOURCE_DIR}/test_hal_foundation.cpp"     # MF-150
    "${CMAKE_CURRENT_SOURCE_DIR}/test_greaseweazle_v2.cpp"    # MF-154 P1.1
    "${CMAKE_CURRENT_SOURCE_DIR}/test_hal_conformance.cpp"    # MF-158 P1.5
    "${CMAKE_CURRENT_SOURCE_DIR}/test_mock_hardware.cpp"      # MF-159 P1.6
    "${CMAKE_CURRENT_SOURCE_DIR}/test_mock_provider_v2.cpp"   # MF-160 P1.7
    "${CMAKE_CURRENT_SOURCE_DIR}/test_scp_provider_v2.cpp"         # MF-161 P1.8
    "${CMAKE_CURRENT_SOURCE_DIR}/test_kryoflux_provider_v2.cpp"   # MF-162 P1.9
    "${CMAKE_CURRENT_SOURCE_DIR}/test_fluxengine_provider_v2.cpp"  # MF-163 P1.10
    "${CMAKE_CURRENT_SOURCE_DIR}/test_fc5025_provider_v2.cpp"      # MF-164 P1.11
    "${CMAKE_CURRENT_SOURCE_DIR}/test_xum1541_provider_v2.cpp"        # MF-165 P1.12
    "${CMAKE_CURRENT_SOURCE_DIR}/test_applesauce_provider_v2.cpp"   # MF-166 P1.13
    "${CMAKE_CURRENT_SOURCE_DIR}/test_adfcopy_provider_v2.cpp"     # MF-167 P1.14
    "${CMAKE_CURRENT_SOURCE_DIR}/test_usbfloppy_provider_v2.cpp") # MF-168 P1.15

set(TEST_SOURCES ${TEST_SOURCES_C} ${_HEADER_ONLY_CPP_TESTS})

# Exclude tests with missing dependencies (TODO: implement these modules)
set(EXCLUDED_TESTS
    "test_mega65"           # Needs uft_mega65 module (header is stub-only)
    # "test_mega65_fat32"   # Re-enabled: compiles standalone
    # "test_libdsk_formats" # Re-enabled: apridisk, nanowasp, qrst, cfi now in registry
    "test_track_writer"     # src/hardware/uft_track_writer.c removed in cleanup
    "test_flux_analysis"    # src/flux/uft_flux_analysis.c removed in cleanup
    "test_c64_bam"          # src/formats/c64/uft_c64_bam.c removed in cleanup
    "test_write_verify_pipeline"  # src/core/uft_write_verify_pipeline.c removed in cleanup
    # "test_ti99_formats"   # Re-enabled: TIFILES/FIAD modules exist
    # "test_new_formats"    # Re-enabled: compiles standalone
    # "test_fat_extensions"  # Re-enabled: compiles standalone
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
    
    # Base libraries for all tests.
    # uft_ride was removed in MF-011 (commit 386f3f6) — src/ride/ deleted.
    set(TEST_LIBS uft_core uft_crc)

    # Self-contained tests: no UFT library dependencies at all
    set(_SELF_CONTAINED_TESTS
        test_pll test_recovery test_roundtrip
        test_writer_backend test_woz_writer test_platform
        test_hal_foundation   # MF-150: type-driven HAL foundation, header-only
        test_greaseweazle_v2) # MF-154 P1.1: GW V2 provider (provider .cpp linked below)
    list(FIND _SELF_CONTAINED_TESTS ${test_name} _sc_idx)
    if(${_sc_idx} GREATER -1)
        set(TEST_LIBS "")  # No UFT libs needed
        if(${test_name} STREQUAL "test_pll")
            if(UNIX AND M_LIBRARY)
                set(TEST_LIBS ${M_LIBRARY})  # math library for pow(), fabs()
            endif()
        endif()
    endif()

    # Add test-specific dependencies (guarded — library subdirs may not
    # be added to the root CMakeLists.txt, in which case the test still
    # builds against TEST_LIBS-only and any uft_* symbols it really needs
    # surface as honest link errors instead of "is not a target").
    if(${test_name} STREQUAL "test_ti99_formats")
        if(TARGET uft_ti99)
            list(APPEND TEST_LIBS uft_ti99)
        endif()
    elseif(${test_name} STREQUAL "test_fat_extensions")
        if(TARGET uft_fs)
            list(APPEND TEST_LIBS uft_fs)
        endif()
    elseif(${test_name} STREQUAL "test_c64_protection")
        if(TARGET uft_c64_protection)
            list(APPEND TEST_LIBS uft_c64_protection)
        endif()
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
        if(UNIX AND M_LIBRARY)
            target_link_libraries(${test_name} PRIVATE ${M_LIBRARY})
        endif()
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
    elseif(${test_name} STREQUAL "test_adf_bam")
        # Include ADF BAM source directly
        target_sources(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/src/fs/uft_adf_bam.c)
    elseif(${test_name} STREQUAL "test_amiga_extract")
        # Include AmigaDOS chain walker source directly
        target_sources(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/src/fs/uft_amigados.c)
    elseif(${test_name} STREQUAL "test_align_fuse_bridge")
        # Include align-fuse bridge source directly
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/analysis/events/uft_align_fuse_bridge.c)
    elseif(${test_name} STREQUAL "test_write_gate")
        # Include write-gate policy + snapshot sources
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/policy/uft_write_gate.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_snapshot.c)
    elseif(${test_name} STREQUAL "test_xdf_api")
        # Include XDF API source directly
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/formats/xdf/uft_xdf_api.c)
    elseif(${test_name} STREQUAL "test_amiga_virus_db")
        # Include Amiga virus signature DB source directly
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/fs/uft_amiga_virus_db.c)
    elseif(${test_name} STREQUAL "test_ipf_air_accessors")
        # Include IPF AIR parser source directly (TUs-private types stay
        # encapsulated; the test exercises the opaque-handle API).
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/formats/ipf/uft_ipf_air.c)
    elseif(${test_name} STREQUAL "test_greaseweazle_v2")
        # MF-154 P1.1: GreaseweazleProviderV2 mixin composition test.
        # Needs: V2 provider .cpp + GW C backend (for uft_gw_strerror etc.).
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/hardware_providers/greaseweazle_provider_v2.cpp
            ${CMAKE_SOURCE_DIR}/src/hal/uft_greaseweazle_full.c)
        target_include_directories(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src)
        set(TEST_LIBS "")
    elseif(${test_name} STREQUAL "test_scp_provider_v2")
        # MF-161 P1.8: SCPProviderV2 mixin composition test.
        # Needs: V2 provider .cpp + SCP direct C backend (M3.1 scaffold).
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/hardware_providers/scp_provider_v2.cpp
            ${CMAKE_SOURCE_DIR}/src/hal/uft_scp_direct.c)
        target_include_directories(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src)
        set(TEST_LIBS "")
    elseif(${test_name} STREQUAL "test_kryoflux_provider_v2")
        # MF-162 P1.9: KryoFluxProviderV2 mixin composition test.
        # Needs: V2 provider .cpp; no C-HAL backend (KF uses DTC subprocess).
        # SubprocessMock is header-only; tests/ must be on the include path.
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/hardware_providers/kryoflux_provider_v2.cpp)
        target_include_directories(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src
            ${CMAKE_SOURCE_DIR}/tests)
        set(TEST_LIBS "")
    elseif(${test_name} STREQUAL "test_fluxengine_provider_v2")
        # MF-163 P1.10: FluxEngineProviderV2 mixin composition test.
        # Needs: V2 provider .cpp; no C-HAL backend (FE uses fluxengine subprocess).
        # SubprocessMock is header-only; tests/ must be on the include path.
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/hardware_providers/fluxengine_provider_v2.cpp)
        target_include_directories(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src
            ${CMAKE_SOURCE_DIR}/tests)
        set(TEST_LIBS "")
    elseif(${test_name} STREQUAL "test_fc5025_provider_v2")
        # MF-164 P1.11: FC5025ProviderV2 mixin composition test.
        # Needs: V2 provider .cpp; no C-HAL backend (FC5025 uses injectable runner).
        # SubprocessMock is header-only; tests/ must be on the include path.
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/hardware_providers/fc5025_provider_v2.cpp)
        target_include_directories(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src
            ${CMAKE_SOURCE_DIR}/tests)
        set(TEST_LIBS "")
    elseif(${test_name} STREQUAL "test_xum1541_provider_v2")
        # MF-165 P1.12: XUM1541ProviderV2 mixin composition test.
        # Needs: V2 provider .cpp; no C-HAL backend (XUM1541 uses injectable runners).
        # No SubprocessMock needed — XUM1541 uses scripted lambda runners directly.
        # tests/ on the include path for mock_hardware/ header access if needed.
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/hardware_providers/xum1541_provider_v2.cpp)
        target_include_directories(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src
            ${CMAKE_SOURCE_DIR}/tests)
        set(TEST_LIBS "")
    elseif(${test_name} STREQUAL "test_applesauce_provider_v2")
        # MF-166 P1.13: ApplesauceProviderV2 mixin composition test.
        # Needs: V2 provider .cpp; no C-HAL backend (Applesauce uses injectable
        # runners over IApplesauceTransport). No Qt dependency — transport is
        # abstracted away; tests inject scripted lambdas directly.
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/hardware_providers/applesauce_provider_v2.cpp)
        target_include_directories(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src
            ${CMAKE_SOURCE_DIR}/tests)
        set(TEST_LIBS "")
    elseif(${test_name} STREQUAL "test_adfcopy_provider_v2")
        # MF-167 P1.14: ADFCopyProviderV2 mixin composition test.
        # Needs: V2 provider .cpp; no C-HAL backend (ADFCopy is Qt-only, no
        # uft_adfcopy.c exists). No Qt dependency — transport is abstracted away;
        # tests inject scripted lambda runners directly. No SubprocessMock needed.
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/hardware_providers/adfcopy_provider_v2.cpp)
        target_include_directories(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src
            ${CMAKE_SOURCE_DIR}/tests)
        set(TEST_LIBS "")
    elseif(${test_name} STREQUAL "test_usbfloppy_provider_v2")
        # MF-168 P1.15: USBFloppyProviderV2 mixin composition test.
        # Needs: V2 provider .cpp + UFI C-HAL (ufi.c) for the type definitions
        # and UFT_E_GENERIC alias. No Qt dependency — transport is abstracted
        # via injectable runners; tests inject scripted lambda runners directly.
        # ufi.c is included so the linker resolves uft_ufi_* symbols that the
        # production runner template references (even though tests never call
        # them — the .cpp links cleanly without an OS backend registered).
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/hardware_providers/usbfloppy_provider_v2.cpp
            ${CMAKE_SOURCE_DIR}/src/hal/ufi.c)
        target_include_directories(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src
            ${CMAKE_SOURCE_DIR}/tests)
        set(TEST_LIBS "")
    elseif(${test_name} STREQUAL "test_hal_conformance")
        # MF-158 P1.5 + MF-160 P1.7 + MF-161 P1.8 + MF-162 P1.9 + MF-163 P1.10
        # + MF-164 P1.11 + MF-165 P1.12 + MF-166 P1.13 + MF-167 P1.14
        # + MF-168 P1.15:
        # HAL conformance harness — runs every concept's SECTION against
        # every V2 provider in the typelist.
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/hardware_providers/greaseweazle_provider_v2.cpp
            ${CMAKE_SOURCE_DIR}/src/hal/uft_greaseweazle_full.c
            ${CMAKE_SOURCE_DIR}/src/hardware_providers/scp_provider_v2.cpp
            ${CMAKE_SOURCE_DIR}/src/hal/uft_scp_direct.c
            ${CMAKE_SOURCE_DIR}/src/hardware_providers/kryoflux_provider_v2.cpp
            ${CMAKE_SOURCE_DIR}/src/hardware_providers/fluxengine_provider_v2.cpp
            ${CMAKE_SOURCE_DIR}/src/hardware_providers/fc5025_provider_v2.cpp
            ${CMAKE_SOURCE_DIR}/src/hardware_providers/xum1541_provider_v2.cpp
            ${CMAKE_SOURCE_DIR}/src/hardware_providers/applesauce_provider_v2.cpp
            ${CMAKE_SOURCE_DIR}/src/hardware_providers/adfcopy_provider_v2.cpp
            ${CMAKE_SOURCE_DIR}/src/hardware_providers/usbfloppy_provider_v2.cpp
            ${CMAKE_SOURCE_DIR}/src/hal/ufi.c
            ${CMAKE_SOURCE_DIR}/tests/mock_provider_v2.cpp)
        target_include_directories(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src
            ${CMAKE_SOURCE_DIR}/tests)
        set(TEST_LIBS "")
    elseif(${test_name} STREQUAL "test_mock_provider_v2")
        # MF-160 P1.7: per-variant exhaustive coverage for MockProviderV2.
        # Self-contained — only needs the mock TU + tests/ on the
        # include path so #include "mock_provider_v2.h" resolves.
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/tests/mock_provider_v2.cpp)
        target_include_directories(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/tests)
        set(TEST_LIBS "")
    elseif(${test_name} STREQUAL "test_gw_protocol")
        # MF-135: GW protocol regression tests need uft_gw_strerror() +
        # the inline tick conversion helpers from the full HAL impl.
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/hal/uft_greaseweazle_full.c)
        # No UFT lib dependencies — the test only touches HAL constants.
        set(TEST_LIBS "")
    elseif(${test_name} STREQUAL "test_gw_encoder")
        # GW-F2 (task #113): byte-exact regression test for the GW flux
        # encoder. Links the REAL uft_gw_encode_flux_stream() from the
        # full HAL impl — same pattern as test_gw_protocol — so the test
        # cannot drift from a copy. No UFT lib dependencies.
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/hal/uft_greaseweazle_full.c)
        set(TEST_LIBS "")
    elseif(${test_name} STREQUAL "test_mfm_sector_parser")
        # MF-141 / AUD-002: standalone MFM sector parser. Test
        # synthesizes its own bitstream + uses an inlined CRC, so the
        # parser TU is the only production source needed.
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/flux/uft_mfm_sector_parser.c)
        set(TEST_LIBS "")
    endif()
    
    if(TEST_LIBS)
        target_link_libraries(${test_name} PRIVATE ${TEST_LIBS})
    endif()
    
    # Register as CTest
    add_test(NAME ${test_name} COMMAND ${test_name})
    
    # Header-only C++ foundation tests (MF-150) need C++20.
    list(FIND _HEADER_ONLY_CPP_TESTS ${test_source} _ho_idx)
    if(${_ho_idx} GREATER -1)
        set_target_properties(${test_name} PROPERTIES
            CXX_STANDARD 20
            CXX_STANDARD_REQUIRED ON
            CXX_EXTENSIONS OFF
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests
        )
    else()
        set_target_properties(${test_name} PROPERTIES
            C_STANDARD 11
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests
        )
    endif()

    message(STATUS "UFT Test ENABLED: ${test_name}")
endforeach()

# ═══════════════════════════════════════════════════════════════════════════════
# Qt-based Tests (C++ / QtTest framework)
# ═══════════════════════════════════════════════════════════════════════════════
find_package(Qt6 COMPONENTS Core Test Widgets QUIET)
if(TARGET Qt6::Test)
    set(CMAKE_AUTOMOC ON)

    # Discover Qt C++ test sources, excluding the header-only set already
    # handled by the main C-loop (MF-150).
    set(QT_TEST_SOURCES ${TEST_SOURCES_CPP})
    foreach(_skip ${_HEADER_ONLY_CPP_TESTS})
        list(REMOVE_ITEM QT_TEST_SOURCES ${_skip})
    endforeach()

    foreach(qt_test_source ${QT_TEST_SOURCES})
        get_filename_component(qt_test_name ${qt_test_source} NAME_WE)

        add_executable(${qt_test_name} ${qt_test_source})
        target_include_directories(${qt_test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src
            ${CMAKE_SOURCE_DIR}/include
        )
        target_link_libraries(${qt_test_name} PRIVATE
            Qt6::Core Qt6::Test
        )

        # MF-169 (P1.17): the V1 `hardware_providers` cmake target was
        # deleted with the V1 provider hierarchy. Qt-side tests now
        # link only against Qt6::Core+Test+Widgets+SerialPort. The V2
        # provider tests live in `_HEADER_ONLY_CPP_TESTS` and pull
        # their own per-test target_sources for the specific V2 cpp.
        if(TARGET Qt6::SerialPort AND NOT ${qt_test_name} STREQUAL "test_wiring_runtime")
            target_link_libraries(${qt_test_name} PRIVATE Qt6::SerialPort)
        endif()
        # MF-155 (P1.3): test_wiring_runtime exercises QPushButton + click,
        # so it needs Qt6::Widgets. Link unconditionally if available — costs
        # nothing for the few existing Qt-Test targets that ignore Widgets.
        if(TARGET Qt6::Widgets)
            target_link_libraries(${qt_test_name} PRIVATE Qt6::Widgets)
        endif()

        # CXX_STANDARD: most existing Qt-tests are C++17. test_wiring_runtime
        # uses C++20 generic-lambda template-parameter syntax (`[]<class P>`),
        # so it must explicitly opt in to C++20.
        if(${qt_test_name} STREQUAL "test_wiring_runtime")
            set_target_properties(${qt_test_name} PROPERTIES
                CXX_STANDARD 20
                CXX_STANDARD_REQUIRED ON
                CXX_EXTENSIONS OFF
                RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests
            )
        else()
            set_target_properties(${qt_test_name} PROPERTIES
                CXX_STANDARD 17
                RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests
            )
        endif()

        add_test(NAME ${qt_test_name} COMMAND ${qt_test_name})
        message(STATUS "UFT Qt Test ENABLED: ${qt_test_name}")
    endforeach()
else()
    message(STATUS "UFT Qt Tests SKIPPED: Qt6::Test not found")
endif()

# ═══════════════════════════════════════════════════════════════════════════════
# Principle-7 / §M.1 compliance audit (source-scan, no build needed)
# ═══════════════════════════════════════════════════════════════════════════════
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    add_test(
        NAME audit_plugin_compliance
        COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_SOURCE_DIR}/scripts/audit_plugin_compliance.py
                --root ${CMAKE_SOURCE_DIR}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    set_tests_properties(audit_plugin_compliance PROPERTIES
        LABELS "principle-compliance"
    )
    message(STATUS "UFT Audit ENABLED: audit_plugin_compliance (Python ${Python3_VERSION})")
else()
    message(STATUS "UFT Audit SKIPPED: Python3 interpreter not found")
endif()

# ═══════════════════════════════════════════════════════════════════════════════
# SSOT error-code compliance (post-4b-4 cutover — hard fail on drift)
#
# Runs scripts/verify_errors_ssot.sh which regenerates uft_error.h,
# uft_error_strings.c, and uft_error_compat_gen.h into a temp dir from
# data/errors.tsv + data/errors_legacy_aliases.tsv and diffs them
# against the committed copies. Any mismatch = generator-forgotten or
# hand-edit-of-generated-file drift.
# ═══════════════════════════════════════════════════════════════════════════════
add_test(
    NAME ssot_errors_compliance
    COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/verify_errors_ssot.sh
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
set_tests_properties(ssot_errors_compliance PROPERTIES
    LABELS "ssot-compliance"
)
message(STATUS "UFT SSOT Check ENABLED: ssot_errors_compliance")

# ═══════════════════════════════════════════════════════════════════════════════
# Build-system parity check (MF-006 regression guard)
#
# Compares the file set CMake's GLOB_RECURSE picks up against the explicit
# SOURCES block in UnifiedFloppyTool.pro. Fails on NEW divergence since the
# committed baseline in data/build_system_baseline.tsv. The baseline
# captures the historical gap (samdisk vendor code, _parser_v3.c stubs,
# mbedtls examples) which is shrunk by separate cleanup work.
# ═══════════════════════════════════════════════════════════════════════════════
if(Python3_Interpreter_FOUND)
    add_test(
        NAME build_system_parity
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/verify_build_sources.py
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    set_tests_properties(build_system_parity PROPERTIES
        LABELS "ssot-compliance"
    )
    message(STATUS "UFT SSOT Check ENABLED: build_system_parity")
else()
    message(STATUS "UFT SSOT Check SKIPPED: build_system_parity (Python3 not found)")
endif()

message(STATUS "════════════════════════════════════════════════════")
message(STATUS "UFT Tests: Run 'ctest' after build to execute")
message(STATUS "════════════════════════════════════════════════════")
