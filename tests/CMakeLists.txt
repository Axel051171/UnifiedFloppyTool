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
file(GLOB TEST_SOURCES_C    "${CMAKE_CURRENT_SOURCE_DIR}/test_*.c"
                            "${CMAKE_CURRENT_SOURCE_DIR}/unit/test_*.c")
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
    "${CMAKE_CURRENT_SOURCE_DIR}/test_usbfloppy_provider_v2.cpp"   # MF-168 P1.15
    "${CMAKE_CURRENT_SOURCE_DIR}/test_teensy_probe.cpp"           # MF-213 ARCH-7-C
    "${CMAKE_CURRENT_SOURCE_DIR}/test_concurrency.cpp"            # MF-226 P3.3 #110a
    "${CMAKE_CURRENT_SOURCE_DIR}/test_applesauce_runners_protocol.cpp" # MF-251
    "${CMAKE_CURRENT_SOURCE_DIR}/test_adfcopy_runners_protocol.cpp")   # MF-252

set(TEST_SOURCES ${TEST_SOURCES_C} ${_HEADER_ONLY_CPP_TESTS})

# Exclude tests with missing dependencies (TODO: implement these modules)
set(EXCLUDED_TESTS
    # Only genuinely blocked tests belong here. Each entry names WHAT is
    # missing, so the list stays auditable — scripts/update_inventory.py
    # checks the count against docs/MASTER_PLAN.md (MF-410).
    "test_libdsk_formats"      # libdsk-style format glue absent
    "test_fat_extensions"      # uft_fs lib not registered in root CMake
)

# Previously excluded, now running again — kept as a record of what the
# reasons were, because several of them turned out to be wrong rather than
# merely stale:
#   test_scp_direct_hal, test_xum1541_hal  MF-254/255 libusb wiring landed
#   test_applesauce_hal                    pure-utility part is real
#   test_fnmatch_shim, test_whdload_resload sources re-included
#   test_libdsk_formats(dup), test_ti99_formats
#   test_mfm_detect, test_cpm_fs, test_mfm_bridge   MF-411 — these three were
#       excluded for "API drift: the expected types exist in NO header". They
#       do exist, in include/uft/detect/; the tests were including phantom
#       twins under include/uft/analysis/ that declared an unimplemented API.
#       The twins are deleted, the includes point at the real headers.

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
    # (dead uft_ti99/uft_fs guards removed in MF-369 wave 2: their defining
    #  CMakeLists were never add_subdirectory'd, so the targets never existed
    #  at configure time — the tests always built TEST_LIBS-only.)
    if(${test_name} STREQUAL "test_c64_protection")
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
        target_sources(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/src/formats/c64/uft_d64_g64.c
            ${CMAKE_SOURCE_DIR}/src/formats/cbm/uft_cbm_geometry.c)

    elseif(${test_name} STREQUAL "test_c64_protection_ext")
        # Include Extended Protection source directly
        target_sources(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/src/protection/c64/uft_c64_protection_ext.c)


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
    elseif(${test_name} STREQUAL "test_st_plugin")
        # Real end-to-end test: link the Atari ST module directly (MF-312)
        target_sources(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/src/formats/atari/st.c)
    elseif(${test_name} STREQUAL "test_d67_plugin")
        # Real end-to-end test + FMT-1 regression: Commodore 2040 module (MF-314)
        target_sources(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/src/formats/commodore/d67.c)
    elseif(${test_name} STREQUAL "test_d80_plugin")
        # Real end-to-end test: link the Commodore 8050 module (MF-313)
        target_sources(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/src/formats/commodore/d80.c)
    elseif(${test_name} STREQUAL "test_d82_plugin")
        # Real end-to-end test: link the Commodore 8250 module (MF-313)
        target_sources(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/src/formats/commodore/d82.c)
    elseif(${test_name} STREQUAL "test_woz_roundtrip")
        # WOZ2 writer read->write byte-identity test (MF-317)
        target_sources(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/src/formats/apple/uft_woz.c)
    elseif(${test_name} STREQUAL "test_scp_writer_roundtrip")
        # SCP writer write->read flux round-trip; caught the Track-Length bug (MF-318)
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/formats/scp/uft_scp_writer.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_scp_parser.c)
    elseif(${test_name} STREQUAL "test_scp_footer_roundtrip")
        # SCP extension-footer write/read round-trip + reader spec fix (MF-351)
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/formats/scp/uft_scp_writer.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_scp_parser.c)
    elseif(${test_name} STREQUAL "test_scp_layout")
        # Punkt 2.3: SCP-Ablage (Spurzahl / Seiten / Startspur)
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/formats/scp/uft_scp_plugin.c
            ${CMAKE_SOURCE_DIR}/src/formats/scp/uft_scp_writer.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_decoder_plugin_stub.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_log.c)
    elseif(${test_name} STREQUAL "test_scp_weakbit_multirev")
        # SCP Klasse-1 forensic guard: multi-rev weak-bit pass-through (MF-327)
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/formats/scp/uft_scp_writer.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_scp_parser.c)
    elseif(${test_name} STREQUAL "test_protection_probe")
        # Klasse-3 detection: content-based protection-anomaly scan (MF-328/331)
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/analysis/uft_protection_probe.c
            ${CMAKE_SOURCE_DIR}/src/formats/scp/uft_scp_writer.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_scp_parser.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_kryoflux_stream.c)
    elseif(${test_name} STREQUAL "test_moof_roundtrip")
        # MOOF writer save->read semantic round-trip (MF-319)
        target_sources(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/src/formats/apple/uft_moof_parser.c)
    elseif(${test_name} STREQUAL "test_imd_write_roundtrip")
        # IMD write_track round-trip; caught the data_len/data_size write no-op bug (MF-320)
        target_sources(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/src/formats/imd/uft_imd_plugin.c)
    elseif(${test_name} STREQUAL "test_imd_error_marks")
        # IMD disk-error marking read/represent/preserve (Phase-4 WP5, MF-329)
        target_sources(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/src/formats/imd/uft_imd_plugin.c)
    elseif(${test_name} STREQUAL "test_edsk_error_marks")
        # EDSK uPD765 ST1/ST2 disk-error marking read/represent/preserve (Phase-4 WP5, MF-332)
        target_sources(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/src/formats/dsk_cpc/uft_dsk_cpc.c)
    elseif(${test_name} STREQUAL "test_d64_errormap")
        # D64 error-info block read/represent — surfaces 1541 error codes (Phase-4 WP5, MF-333)
        target_sources(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/src/formats/d64/uft_d64_plugin.c)
    elseif(${test_name} STREQUAL "test_d64_geometry_zones")
        # Phase-0 geometry: D64 GCR zoned per-track sector counts (MF-337)
        target_sources(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/src/formats/d64/uft_d64_plugin.c)
    elseif(${test_name} STREQUAL "test_d64_42track")
        # D64 extended 41/42-track variant support (MF-350)
        target_sources(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/src/formats/d64/uft_d64_plugin.c)
    elseif(${test_name} STREQUAL "test_atr_512")
        # ATR 512-byte-sector (SpartaDOS X) read, not clamped to 128 (MF-340)
        target_sources(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/src/formats/atr/uft_atr.c)
    elseif(${test_name} STREQUAL "test_cqm_layout")
        # CopyQM header offsets + RLE polarity vs. the published layout (MF-461)
        # The plugin reports integrity findings through UFT_WARN, hence uft_log.c
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/formats/cqm/uft_cqm.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_log.c)
    elseif(${test_name} STREQUAL "test_adf_write_roundtrip")
        # Phase-6 round-trip: ADF write_track -> read_track (MF-341)
        target_sources(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/src/formats/adf/uft_adf_plugin.c)
    elseif(${test_name} STREQUAL "test_atr_write_roundtrip")
        # Phase-6 round-trip: ATR write_track -> read_track (MF-341)
        target_sources(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/src/formats/atr/uft_atr.c)
    elseif(${test_name} STREQUAL "test_apple_do_po_bounds")
        # DO/PO: kein Fuellbyte als Sektor, Zylinder begrenzt (MF-463)
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/formats/do/uft_do.c
            ${CMAKE_SOURCE_DIR}/src/formats/po/uft_po.c)
    elseif(${test_name} STREQUAL "test_st_geometry")
        # ST geometry: BPB before size scan, extended 80..84 cyl range (MF-462)
        target_sources(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/src/formats/st/uft_st.c)
    elseif(${test_name} STREQUAL "test_st_write_roundtrip")
        # Phase-6 round-trip: Atari ST write_track -> read_track (MF-341)
        target_sources(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/src/formats/st/uft_st.c)
    elseif(${test_name} STREQUAL "test_d71_write_roundtrip")
        # Phase-6 round-trip: Commodore D71 write_track -> read_track (MF-342)
        target_sources(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/src/formats/d71/uft_d71.c)
    elseif(${test_name} STREQUAL "test_d81_write_roundtrip")
        # Phase-6 round-trip: Commodore D81 write_track -> read_track (MF-342)
        target_sources(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/src/formats/d81/uft_d81.c)
    elseif(${test_name} STREQUAL "test_img_write_roundtrip")
        # Phase-6 round-trip: IBM PC IMG write_track -> read_track (MF-342)
        # img read_metadata pulls uft_meta_free (uft_metadata.c) via core stubs
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/formats/img/uft_img.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_metadata.c
            # img warns honestly when it pads a short track (MF-465)
            ${CMAKE_SOURCE_DIR}/src/core/uft_log.c)
    elseif(${test_name} STREQUAL "test_do_write_roundtrip")
        # Phase-6 round-trip: Apple DOS-order DO write_track -> read_track (MF-343)
        target_sources(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/src/formats/do/uft_do.c)
    elseif(${test_name} STREQUAL "test_d80_write_roundtrip")
        # Phase-6 round-trip: Commodore D80 write_track -> read_track (MF-343)
        target_sources(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/src/formats/d80/uft_d80.c)
    elseif(${test_name} STREQUAL "test_d82_write_roundtrip")
        # Phase-6 round-trip: Commodore D82 write_track -> read_track (MF-343)
        target_sources(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/src/formats/d82/uft_d82.c)
    elseif(${test_name} STREQUAL "test_po_write_roundtrip")
        # Phase-6 round-trip: Apple ProDOS-order PO write_track -> read_track (MF-344)
        target_sources(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/src/formats/po/uft_po.c)
    elseif(${test_name} STREQUAL "test_korg_dss1_plugin")
        # Phase-3 new format: Korg DSS-1 sampler (819200, 5x1024) (MF-347)
        target_sources(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/src/formats/korg/uft_korg_dss1.c)
    elseif(${test_name} STREQUAL "test_adf_ext_plugin")
        # Phase-3 new format: Extended ADF (UAE-1ADF) reader (MF-352)
        target_sources(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/src/formats/adf_ext/uft_adf_ext.c)
    elseif(${test_name} STREQUAL "test_dmk_crc")
        # DMK WD177x ID/data CRC checking (MF-353)
        target_sources(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/src/formats/dmk/uft_dmk.c)
    elseif(${test_name} STREQUAL "test_hfe_v3_weak")
        # HFE v3 weak/fuzzy-bit RAND-opcode detection (MF-354)
        target_sources(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/src/formats/hfe/uft_hfe.c)
    elseif(${test_name} STREQUAL "test_g71_read")
        # G71 1571 GCR per-track raw read (MF-355)
        target_sources(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/src/formats/g71/uft_g71.c)
    elseif(${test_name} STREQUAL "test_mega65_fat32")
        # Re-enabled in MF-372: was excluded with the WRONG reason ("uft_mega65
        # stub-only") — it actually tests the real 589-line uft_fat32_mbr.c.
        target_sources(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/src/formats/fat32/uft_fat32_mbr.c)
    elseif(${test_name} STREQUAL "test_amigados_validate")
        # Reactivated in MF-374 (exclusion reason no longer holds)
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/fs/uft_amigados_extended.c
            ${CMAKE_SOURCE_DIR}/src/fs/uft_bootblock_scanner.c
            ${CMAKE_SOURCE_DIR}/src/fs/uft_amiga_virus_db.c)
    elseif(${test_name} STREQUAL "test_confidence_v10")
        # Reactivated in MF-374 (exclusion reason no longer holds)
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/analysis/events/uft_confidence_bridge.c            ${CMAKE_SOURCE_DIR}/src/analysis/events/otdr_event_core_v10.c
)
        target_include_directories(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/include/uft/analysis)
    elseif(${test_name} STREQUAL "test_crc16")
        # Reactivated in MF-374 (exclusion reason no longer holds)
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/whdload/whd_crc16.c)
    elseif(${test_name} STREQUAL "test_event_v8")
        # Reactivated in MF-374 (exclusion reason no longer holds)
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/analysis/events/uft_event_v8_bridge.c            ${CMAKE_SOURCE_DIR}/src/analysis/events/otdr_event_core_v8.c
)
        target_include_directories(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/include/uft/analysis)
    elseif(${test_name} STREQUAL "test_export_v12")
        # Reactivated in MF-374 (exclusion reason no longer holds)
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/analysis/events/uft_export_bridge.c            ${CMAKE_SOURCE_DIR}/src/analysis/events/otdr_event_core_v12.c
            ${CMAKE_SOURCE_DIR}/src/analysis/events/otdr_event_core_v9.c
            ${CMAKE_SOURCE_DIR}/src/analysis/events/otdr_event_core_v10.c
            ${CMAKE_SOURCE_DIR}/src/analysis/events/otdr_event_core_v11.c
)
        target_include_directories(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/include/uft/analysis)
    elseif(${test_name} STREQUAL "test_integrity_v9")
        # Reactivated in MF-374 (exclusion reason no longer holds)
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/analysis/events/uft_integrity_bridge.c            ${CMAKE_SOURCE_DIR}/src/analysis/events/otdr_event_core_v9.c
)
        target_include_directories(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/include/uft/analysis)
    elseif(${test_name} STREQUAL "test_mfm_detect")
        # MF-411: re-enabled — the test now includes uft/detect/, not the
        # phantom uft/analysis/ twin.
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/detect/mfm/mfm_detect.c)
        target_include_directories(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/include/uft/detect)
    elseif(${test_name} STREQUAL "test_mfm_bridge")
        # MF-411: the bridge calls into both detectors, so both TUs are needed.
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/detect/mfm/uft_mfm_detect_bridge.c
            ${CMAKE_SOURCE_DIR}/src/detect/mfm/mfm_detect.c
            ${CMAKE_SOURCE_DIR}/src/detect/mfm/cpm_fs.c)
        target_include_directories(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/include/uft/detect)
    elseif(${test_name} STREQUAL "test_otdr_bridge")
        # Reactivated in MF-374 (exclusion reason no longer holds)
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/analysis/otdr/uft_otdr_bridge.c
            ${CMAKE_SOURCE_DIR}/src/analysis/otdr/floppy_otdr.c)
    elseif(${test_name} STREQUAL "test_pipeline_v11")
        # Reactivated in MF-374 (exclusion reason no longer holds)
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/analysis/events/uft_pipeline_bridge.c            ${CMAKE_SOURCE_DIR}/src/analysis/events/otdr_event_core_v11.c
            ${CMAKE_SOURCE_DIR}/src/analysis/events/otdr_event_core_v9.c
            ${CMAKE_SOURCE_DIR}/src/analysis/events/otdr_event_core_v10.c
)
        target_include_directories(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/include/uft/analysis)
    elseif(${test_name} STREQUAL "test_ti99_formats")
        # Reactivated in MF-374 (exclusion reason no longer holds)
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/formats/ti99/uft_fiad.c
            ${CMAKE_SOURCE_DIR}/src/formats/ti99/uft_tifiles.c)
    elseif(${test_name} STREQUAL "test_cpm_fs")
        # Reactivated in MF-374 (exclusion reason no longer holds)
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/detect/mfm/cpm_fs.c
            ${CMAKE_SOURCE_DIR}/src/formats/cpm/uft_cpm_diskdef.c)
        target_include_directories(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/include/uft/detect)
    elseif(${test_name} STREQUAL "test_denoise_bridge")
        # Reactivated in MF-374 (exclusion reason no longer holds)
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/analysis/denoise/uft_denoise_bridge.c            ${CMAKE_SOURCE_DIR}/src/analysis/denoise/phi_otdr_denoise_1d.c
)
        target_include_directories(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/include/uft/analysis)
    elseif(${test_name} STREQUAL "test_event_bridge")
        # Reactivated in MF-374 (exclusion reason no longer holds)
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/analysis/events/otdr_event_core_v2.c
            ${CMAKE_SOURCE_DIR}/src/analysis/events/uft_event_bridge.c)
        target_include_directories(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/include/uft/analysis)
    elseif(${test_name} STREQUAL "test_savedskf_read")
        # IBM SaveDskF uncompressed reader rewrite (MF-356)
        target_sources(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/src/formats/pc/uft_savedskf.c)
    elseif(${test_name} STREQUAL "test_nfd_r0")
        # NFD r0 (T98-Next PC-98) reader/writer rewrite (MF-358)
        target_sources(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/src/formats/nfd/uft_nfd_plugin.c)
    elseif(${test_name} STREQUAL "test_fdi_spectrum")
        # ZX Spectrum FDI reader/writer rewrite (MF-359)
        target_sources(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/src/formats/fdi/uft_fdi_plugin.c)
    elseif(${test_name} STREQUAL "test_corpus_d71")
        # T1b cross-tool corpus: D71 by VICE c1541 (MF-376)
        target_sources(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/src/formats/d71/uft_d71.c)
        target_compile_definitions(${test_name} PRIVATE
            UFT_CORPUS_DIR="${CMAKE_SOURCE_DIR}/tests/corpus_free")
    elseif(${test_name} STREQUAL "test_corpus_d81")
        # T1b cross-tool corpus: D81 by VICE c1541 (MF-376)
        target_sources(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/src/formats/d81/uft_d81.c)
        target_compile_definitions(${test_name} PRIVATE
            UFT_CORPUS_DIR="${CMAKE_SOURCE_DIR}/tests/corpus_free")
    elseif(${test_name} STREQUAL "test_corpus_hfe")
        # T1b cross-tool corpus: HFE by greaseweazle (MF-376)
        target_sources(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/src/formats/hfe/uft_hfe.c)
        target_compile_definitions(${test_name} PRIVATE
            UFT_CORPUS_DIR="${CMAKE_SOURCE_DIR}/tests/corpus_free")
    elseif(${test_name} STREQUAL "test_corpus_scp")
        # T1b cross-tool corpus: SCP flux by greaseweazle (MF-376, local-only image)
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/formats/scp/uft_scp_plugin.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_decoder_plugin_stub.c)
        target_compile_definitions(${test_name} PRIVATE
            UFT_CORPUS_RESTRICTED_DIR="${CMAKE_SOURCE_DIR}/tests/corpus")
    elseif(${test_name} STREQUAL "test_corpus_d64")
        # T1b cross-tool corpus: D64 by VICE c1541 (MF-365)
        target_sources(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/src/formats/d64/uft_d64_plugin.c)
        target_compile_definitions(${test_name} PRIVATE
            UFT_CORPUS_DIR="${CMAKE_SOURCE_DIR}/tests/corpus_free")
    elseif(${test_name} STREQUAL "test_corpus_adf")
        # T1b cross-tool corpus: ADF by amitools xdftool (MF-365)
        target_sources(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/src/formats/adf/uft_adf_plugin.c)
        target_compile_definitions(${test_name} PRIVATE
            UFT_CORPUS_DIR="${CMAKE_SOURCE_DIR}/tests/corpus_free")
    elseif(${test_name} STREQUAL "test_cbm_geometry")
        # MF-434: one CBM geometry against the 24 tables it replaces
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/formats/cbm/uft_cbm_geometry.c)
        target_compile_definitions(${test_name} PRIVATE
            UFT_CORPUS_DIR="${CMAKE_SOURCE_DIR}/tests/corpus_free")
    elseif(${test_name} STREQUAL "test_convert_file_detection")
        # MF-450: uft_convert_file() had no test at all. Needs the whole format
        # layer, same recipe as test_register_all_formats.
        file(GLOB_RECURSE CFD_FORMAT_SOURCES
             ${CMAKE_SOURCE_DIR}/src/formats/*.c)
        target_sources(${test_name} PRIVATE ${CFD_FORMAT_SOURCES}
            # MF-473: SCP->ADF stimmt jetzt ueber die Umdrehungen ab
            ${CMAKE_SOURCE_DIR}/src/recovery/uft_multiread_pipeline.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_flux_decoder.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_flux_histogram.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_flux_sync_search.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_dewarp.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_media_profile.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_mfm_sector_parser.c
            ${CMAKE_SOURCE_DIR}/src/compat/uft_fnmatch.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_preflight.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_decoder_plugin_stub.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_metadata.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_log.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_loss_report.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_mfm_encoder.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_probe_format_impl.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_roundtrip.c
            ${CMAKE_SOURCE_DIR}/src/analysis/uft_protection_probe.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_kryoflux_stream.c
            # MF-489: der Wandler sieht sein eigenes Ergebnis nach
            ${CMAKE_SOURCE_DIR}/src/detect/mfm/uft_mfm_detect_bridge.c
            ${CMAKE_SOURCE_DIR}/src/detect/mfm/mfm_detect.c
            ${CMAKE_SOURCE_DIR}/src/detect/mfm/cpm_fs.c)
        target_include_directories(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/include/uft/detect
            ${CMAKE_SOURCE_DIR}/include/uft/formats
            ${CMAKE_SOURCE_DIR}/include/uft/formats/cbm
            ${CMAKE_SOURCE_DIR}/include/uft/formats/c64
            ${CMAKE_SOURCE_DIR}/include/uft/flux
            ${CMAKE_SOURCE_DIR}/include/uft/fs)
        target_compile_definitions(${test_name} PRIVATE
            UFT_CORPUS_DIR="${CMAKE_SOURCE_DIR}/tests/corpus_free")
    elseif(${test_name} STREQUAL "test_convert_scp_adf_multirev")
        # MF-473: SCP->ADF ueber ALLE Umdrehungen + multiread-Klassifikation.
        # Braucht denselben Unterbau wie test_convert_file_detection (der
        # Wandler laeuft ueber die Format-Registry), zusaetzlich die
        # Abstimmungs-Pipeline und den SCP-Parser fuer die Umdrehungen.
        file(GLOB_RECURSE MREV_FORMAT_SOURCES
             ${CMAKE_SOURCE_DIR}/src/formats/*.c)
        target_sources(${test_name} PRIVATE ${MREV_FORMAT_SOURCES}
            # uft_scp_parser.c kommt ueber uft_core (siehe src/core/CMakeLists.txt)
            ${CMAKE_SOURCE_DIR}/tests/flux_gen/amigados/flux_gen.c
            ${CMAKE_SOURCE_DIR}/src/recovery/uft_multiread_pipeline.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_flux_decoder.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_flux_histogram.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_flux_sync_search.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_dewarp.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_media_profile.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_mfm_sector_parser.c
            ${CMAKE_SOURCE_DIR}/src/compat/uft_fnmatch.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_preflight.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_decoder_plugin_stub.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_metadata.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_log.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_loss_report.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_mfm_encoder.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_probe_format_impl.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_roundtrip.c
            ${CMAKE_SOURCE_DIR}/src/analysis/uft_protection_probe.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_kryoflux_stream.c
            # MF-489: der Wandler sieht sein eigenes Ergebnis nach
            ${CMAKE_SOURCE_DIR}/src/detect/mfm/uft_mfm_detect_bridge.c
            ${CMAKE_SOURCE_DIR}/src/detect/mfm/mfm_detect.c
            ${CMAKE_SOURCE_DIR}/src/detect/mfm/cpm_fs.c)
        target_include_directories(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/tests/flux_gen/amigados
            ${CMAKE_SOURCE_DIR}/include/uft/detect
            ${CMAKE_SOURCE_DIR}/include/uft/formats
            ${CMAKE_SOURCE_DIR}/include/uft/formats/cbm
            ${CMAKE_SOURCE_DIR}/include/uft/formats/c64
            ${CMAKE_SOURCE_DIR}/include/uft/flux
            ${CMAKE_SOURCE_DIR}/include/uft/fs)
    elseif(${test_name} STREQUAL "test_flux_histogram")
        # MF-488: Zellendauer aus dem Abstandshistogramm
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/tests/flux_gen/amigados/flux_gen.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_flux_histogram.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_flux_sync_search.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_dewarp.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_flux_decoder.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_media_profile.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_mfm_sector_parser.c
            ${CMAKE_SOURCE_DIR}/src/formats/amiga/uft_amiga_syncs.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_log.c)
        target_include_directories(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/tests/flux_gen/amigados
            ${CMAKE_SOURCE_DIR}/include/uft/flux)
    elseif(${test_name} STREQUAL "test_pll_regression")
        # MF-487: Fangbereich der PLL, isoliert gegen synthetische Vektoren
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/tests/flux_gen/amigados/flux_gen.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_flux_decoder.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_flux_histogram.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_flux_sync_search.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_dewarp.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_media_profile.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_mfm_sector_parser.c
            ${CMAKE_SOURCE_DIR}/src/formats/amiga/uft_amiga_syncs.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_log.c)
        target_include_directories(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/tests/flux_gen/amigados
            ${CMAKE_SOURCE_DIR}/include/uft/flux)
    elseif(${test_name} STREQUAL "test_flux_dewarp")
        # MF-495: Gleichlauffehler herausrechnen
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/tests/flux_gen/amigados/flux_gen.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_flux_decoder.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_flux_histogram.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_flux_sync_search.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_dewarp.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_media_profile.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_mfm_sector_parser.c
            ${CMAKE_SOURCE_DIR}/src/formats/amiga/uft_amiga_syncs.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_log.c)
        target_include_directories(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/tests/flux_gen/amigados
            ${CMAKE_SOURCE_DIR}/include/uft/flux)
    elseif(${test_name} STREQUAL "test_flux_sync_search")
        # MF-492: Sync-Marken ohne PLL finden
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/tests/flux_gen/amigados/flux_gen.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_flux_decoder.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_flux_histogram.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_flux_sync_search.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_dewarp.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_media_profile.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_mfm_sector_parser.c
            ${CMAKE_SOURCE_DIR}/src/formats/amiga/uft_amiga_syncs.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_log.c)
        target_include_directories(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/tests/flux_gen/amigados
            ${CMAKE_SOURCE_DIR}/include/uft/flux)
    elseif(${test_name} STREQUAL "test_flux_jitter_vectors")
        # MF-486: synthetische Flux-Vektoren mit Zeitzittern
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/tests/flux_gen/amigados/flux_gen.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_flux_decoder.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_flux_histogram.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_flux_sync_search.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_dewarp.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_media_profile.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_mfm_sector_parser.c
            ${CMAKE_SOURCE_DIR}/src/formats/amiga/uft_amiga_syncs.c
            ${CMAKE_SOURCE_DIR}/src/formats/scp/uft_scp_writer.c
            ${CMAKE_SOURCE_DIR}/src/recovery/uft_multiread_pipeline.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_log.c)
        target_include_directories(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/tests/flux_gen/amigados
            ${CMAKE_SOURCE_DIR}/include/uft/flux)
    elseif(${test_name} STREQUAL "test_convert_output_verify")
        # MF-489: der Wandler sieht sein eigenes Ergebnis nach
        file(GLOB_RECURSE OUTV_FORMAT_SOURCES
             ${CMAKE_SOURCE_DIR}/src/formats/*.c)
        target_sources(${test_name} PRIVATE ${OUTV_FORMAT_SOURCES}
            ${CMAKE_SOURCE_DIR}/tests/flux_gen/amigados/flux_gen.c
            ${CMAKE_SOURCE_DIR}/src/recovery/uft_multiread_pipeline.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_flux_decoder.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_flux_histogram.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_flux_sync_search.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_dewarp.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_media_profile.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_mfm_sector_parser.c
            ${CMAKE_SOURCE_DIR}/src/compat/uft_fnmatch.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_preflight.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_decoder_plugin_stub.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_metadata.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_log.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_loss_report.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_mfm_encoder.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_probe_format_impl.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_roundtrip.c
            ${CMAKE_SOURCE_DIR}/src/analysis/uft_protection_probe.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_kryoflux_stream.c
            # MF-489: der Wandler sieht sein eigenes Ergebnis nach
            ${CMAKE_SOURCE_DIR}/src/detect/mfm/uft_mfm_detect_bridge.c
            ${CMAKE_SOURCE_DIR}/src/detect/mfm/mfm_detect.c
            ${CMAKE_SOURCE_DIR}/src/detect/mfm/cpm_fs.c)
        target_include_directories(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/tests/flux_gen/amigados
            ${CMAKE_SOURCE_DIR}/include/uft/detect
            ${CMAKE_SOURCE_DIR}/include/uft/formats
            ${CMAKE_SOURCE_DIR}/include/uft/formats/cbm
            ${CMAKE_SOURCE_DIR}/include/uft/formats/c64
            ${CMAKE_SOURCE_DIR}/include/uft/flux
            ${CMAKE_SOURCE_DIR}/include/uft/fs)
    elseif(${test_name} STREQUAL "test_convert_flippy_reverse")
        # MF-484: Flippy-Rueckseite, Zeitachse gespiegelt
        file(GLOB_RECURSE FLIP_FORMAT_SOURCES
             ${CMAKE_SOURCE_DIR}/src/formats/*.c)
        target_sources(${test_name} PRIVATE ${FLIP_FORMAT_SOURCES}
            ${CMAKE_SOURCE_DIR}/tests/flux_gen/amigados/flux_gen.c
            ${CMAKE_SOURCE_DIR}/src/recovery/uft_multiread_pipeline.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_flux_decoder.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_flux_histogram.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_flux_sync_search.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_dewarp.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_media_profile.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_mfm_sector_parser.c
            ${CMAKE_SOURCE_DIR}/src/compat/uft_fnmatch.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_preflight.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_decoder_plugin_stub.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_metadata.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_log.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_loss_report.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_mfm_encoder.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_probe_format_impl.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_roundtrip.c
            ${CMAKE_SOURCE_DIR}/src/analysis/uft_protection_probe.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_kryoflux_stream.c
            # MF-489: der Wandler sieht sein eigenes Ergebnis nach
            ${CMAKE_SOURCE_DIR}/src/detect/mfm/uft_mfm_detect_bridge.c
            ${CMAKE_SOURCE_DIR}/src/detect/mfm/mfm_detect.c
            ${CMAKE_SOURCE_DIR}/src/detect/mfm/cpm_fs.c)
        target_include_directories(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/tests/flux_gen/amigados
            ${CMAKE_SOURCE_DIR}/include/uft/detect
            ${CMAKE_SOURCE_DIR}/include/uft/formats
            ${CMAKE_SOURCE_DIR}/include/uft/formats/cbm
            ${CMAKE_SOURCE_DIR}/include/uft/formats/c64
            ${CMAKE_SOURCE_DIR}/include/uft/flux
            ${CMAKE_SOURCE_DIR}/include/uft/fs)
    elseif(${test_name} STREQUAL "test_convert_geometry_override")
        # MF-482: Geometrie-Vorgabe und Ueberhang beim SCP->ADF-Wandeln
        file(GLOB_RECURSE GEO_FORMAT_SOURCES
             ${CMAKE_SOURCE_DIR}/src/formats/*.c)
        target_sources(${test_name} PRIVATE ${GEO_FORMAT_SOURCES}
            ${CMAKE_SOURCE_DIR}/src/recovery/uft_multiread_pipeline.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_flux_decoder.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_flux_histogram.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_flux_sync_search.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_dewarp.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_media_profile.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_mfm_sector_parser.c
            ${CMAKE_SOURCE_DIR}/src/compat/uft_fnmatch.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_preflight.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_decoder_plugin_stub.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_metadata.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_log.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_loss_report.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_mfm_encoder.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_probe_format_impl.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_roundtrip.c
            ${CMAKE_SOURCE_DIR}/src/analysis/uft_protection_probe.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_kryoflux_stream.c
            # MF-489: der Wandler sieht sein eigenes Ergebnis nach
            ${CMAKE_SOURCE_DIR}/src/detect/mfm/uft_mfm_detect_bridge.c
            ${CMAKE_SOURCE_DIR}/src/detect/mfm/mfm_detect.c
            ${CMAKE_SOURCE_DIR}/src/detect/mfm/cpm_fs.c)
        target_include_directories(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/include/uft/detect
            ${CMAKE_SOURCE_DIR}/include/uft/formats
            ${CMAKE_SOURCE_DIR}/include/uft/formats/cbm
            ${CMAKE_SOURCE_DIR}/include/uft/formats/c64
            ${CMAKE_SOURCE_DIR}/include/uft/flux
            ${CMAKE_SOURCE_DIR}/include/uft/fs)
    elseif(${test_name} STREQUAL "test_convert_cell_adjust")
        # MF-480: Prozent-Feineinsteller fuer die Zellendauer. Braucht denselben
        # Unterbau wie test_convert_scp_adf_multirev (der Wandler laeuft ueber
        # die Format-Registry) plus den AmigaDOS-Fluxgenerator.
        file(GLOB_RECURSE CADJ_FORMAT_SOURCES
             ${CMAKE_SOURCE_DIR}/src/formats/*.c)
        target_sources(${test_name} PRIVATE ${CADJ_FORMAT_SOURCES}
            ${CMAKE_SOURCE_DIR}/tests/flux_gen/amigados/flux_gen.c
            ${CMAKE_SOURCE_DIR}/src/recovery/uft_multiread_pipeline.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_flux_decoder.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_flux_histogram.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_flux_sync_search.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_dewarp.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_media_profile.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_mfm_sector_parser.c
            ${CMAKE_SOURCE_DIR}/src/compat/uft_fnmatch.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_preflight.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_decoder_plugin_stub.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_metadata.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_log.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_loss_report.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_mfm_encoder.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_probe_format_impl.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_roundtrip.c
            ${CMAKE_SOURCE_DIR}/src/analysis/uft_protection_probe.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_kryoflux_stream.c
            # MF-489: der Wandler sieht sein eigenes Ergebnis nach
            ${CMAKE_SOURCE_DIR}/src/detect/mfm/uft_mfm_detect_bridge.c
            ${CMAKE_SOURCE_DIR}/src/detect/mfm/mfm_detect.c
            ${CMAKE_SOURCE_DIR}/src/detect/mfm/cpm_fs.c)
        target_include_directories(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/tests/flux_gen/amigados
            ${CMAKE_SOURCE_DIR}/include/uft/detect
            ${CMAKE_SOURCE_DIR}/include/uft/formats
            ${CMAKE_SOURCE_DIR}/include/uft/formats/cbm
            ${CMAKE_SOURCE_DIR}/include/uft/formats/c64
            ${CMAKE_SOURCE_DIR}/include/uft/flux
            ${CMAKE_SOURCE_DIR}/include/uft/fs)
    elseif(${test_name} STREQUAL "test_register_all_formats")
        # MF-446: the only test that links the entire format layer, because it
        # is the only one that can answer "does uft_register_all_formats()
        # actually register all of them". Anything less proves nothing.
        file(GLOB_RECURSE ALL_FORMAT_SOURCES
             ${CMAKE_SOURCE_DIR}/src/formats/*.c)
        target_sources(${test_name} PRIVATE ${ALL_FORMAT_SOURCES}
            # what the format layer calls out to
            # MF-473: SCP->ADF stimmt jetzt ueber die Umdrehungen ab
            ${CMAKE_SOURCE_DIR}/src/recovery/uft_multiread_pipeline.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_flux_decoder.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_flux_histogram.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_flux_sync_search.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_dewarp.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_media_profile.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_mfm_sector_parser.c
            ${CMAKE_SOURCE_DIR}/src/compat/uft_fnmatch.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_preflight.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_decoder_plugin_stub.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_metadata.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_log.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_loss_report.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_mfm_encoder.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_probe_format_impl.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_roundtrip.c
            ${CMAKE_SOURCE_DIR}/src/analysis/uft_protection_probe.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_kryoflux_stream.c
            # MF-489: der Wandler sieht sein eigenes Ergebnis nach
            ${CMAKE_SOURCE_DIR}/src/detect/mfm/uft_mfm_detect_bridge.c
            ${CMAKE_SOURCE_DIR}/src/detect/mfm/mfm_detect.c
            ${CMAKE_SOURCE_DIR}/src/detect/mfm/cpm_fs.c)
        # the plugins include their headers unqualified ("uft_dms.h")
        target_include_directories(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/include/uft/detect
            ${CMAKE_SOURCE_DIR}/include/uft/formats
            ${CMAKE_SOURCE_DIR}/include/uft/formats/cbm
            ${CMAKE_SOURCE_DIR}/include/uft/formats/c64
            ${CMAKE_SOURCE_DIR}/include/uft/flux
            ${CMAKE_SOURCE_DIR}/include/uft/fs)
        target_compile_definitions(${test_name} PRIVATE
            UFT_CORPUS_DIR="${CMAKE_SOURCE_DIR}/tests/corpus_free")
    elseif(${test_name} STREQUAL "test_amiga_decoder_limits")
        # MF-452: drei Befunde aus dem X-Copy-Quellenvergleich
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/flux/uft_flux_decoder.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_flux_histogram.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_flux_sync_search.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_dewarp.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_media_profile.c
            ${CMAKE_SOURCE_DIR}/src/formats/amiga/uft_amiga_protection.c
            ${CMAKE_SOURCE_DIR}/src/formats/amiga/uft_amiga_syncs.c)
        target_include_directories(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/formats/amiga)
    elseif(${test_name} STREQUAL "test_plugin_identity")
        # MF-445: registration order is the trap, so the same four plugins as
        # test_smart_open_quality — registered D64-first on purpose.
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/formats/d64/uft_d64_plugin.c
            ${CMAKE_SOURCE_DIR}/src/formats/d67/uft_d67.c
            ${CMAKE_SOURCE_DIR}/src/formats/d81/uft_d81.c
            ${CMAKE_SOURCE_DIR}/src/formats/xfd/uft_xfd.c
            ${CMAKE_SOURCE_DIR}/src/formats/dsk_generic/uft_dsk_generic.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_disk_convert.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_disk_stream.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_log.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_metadata.c)
        target_compile_definitions(${test_name} PRIVATE
            UFT_CORPUS_DIR="${CMAKE_SOURCE_DIR}/tests/corpus_free")
    elseif(${test_name} STREQUAL "test_smart_open_quality")
        # MF-443/444: first caller of uft_smart_open(); the numbers must be
        # counted. Four plugins, registered by the test itself — the registry
        # is the single detection source, so an empty one must be visible.
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/formats/d64/uft_d64_plugin.c
            ${CMAKE_SOURCE_DIR}/src/formats/d67/uft_d67.c
            ${CMAKE_SOURCE_DIR}/src/formats/d81/uft_d81.c
            ${CMAKE_SOURCE_DIR}/src/formats/xfd/uft_xfd.c)
        target_compile_definitions(${test_name} PRIVATE
            UFT_CORPUS_DIR="${CMAKE_SOURCE_DIR}/tests/corpus_free")
    elseif(${test_name} STREQUAL "test_scp_readers_agree")
        # MF-439: the five SCP readers must return the same flux
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/formats/scp/uft_scp_plugin.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_decoder_plugin_stub.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_scp_parser.c)
        target_compile_definitions(${test_name} PRIVATE
            UFT_CORPUS_RESTRICTED_DIR="${CMAKE_SOURCE_DIR}/tests/corpus")
    elseif(${test_name} STREQUAL "test_convert_scp_adf")
        # MF-438: Amiga flux decoded as Amiga; SKIPS (77) without the 32 MB SCP
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/formats/scp/uft_scp_plugin.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_decoder_plugin_stub.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_flux_decoder.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_flux_histogram.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_flux_sync_search.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_dewarp.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_media_profile.c)
        target_compile_definitions(${test_name} PRIVATE
            UFT_CORPUS_DIR="${CMAKE_SOURCE_DIR}/tests/corpus_free"
            UFT_CORPUS_RESTRICTED_DIR="${CMAKE_SOURCE_DIR}/tests/corpus")
    elseif(${test_name} STREQUAL "test_convert_hfe_adf")
        # MF-437: Amiga HFE decoded as Amiga, byte-identical to the source ADF
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/formats/hfe/uft_hfe.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_flux_decoder.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_flux_histogram.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_flux_sync_search.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_dewarp.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_media_profile.c)
        target_compile_definitions(${test_name} PRIVATE
            UFT_CORPUS_DIR="${CMAKE_SOURCE_DIR}/tests/corpus_free")
    elseif(${test_name} STREQUAL "test_media_profile")
        # MF-471: Medienprofile + Drehzahl-Adaption gegen das a8rawconv-Orakel
        # Linkt auch den Decoder, weil MF-471 die Verdrahtung dort prueft
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/flux/uft_media_profile.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_flux_decoder.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_flux_histogram.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_flux_sync_search.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_dewarp.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_mfm_sector_parser.c
            ${CMAKE_SOURCE_DIR}/src/formats/amiga/uft_amiga_syncs.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_log.c)
    elseif(${test_name} STREQUAL "test_flux_index_wiring")
        # MF-475: die gemessene Umdrehung erreicht alle fuenf Decoder
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/flux/uft_media_profile.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_flux_decoder.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_flux_histogram.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_flux_sync_search.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_dewarp.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_mfm_sector_parser.c
            ${CMAKE_SOURCE_DIR}/src/formats/amiga/uft_amiga_syncs.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_log.c)
    elseif(${test_name} STREQUAL "test_atx_roundtrip")
        # ATX write-read-Rundlauf (MF-474)
        # uft_interleave.c: Ersatz-Winkelpositionen im Schreiber (MF-479)
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/formats/atx/uft_atx.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_interleave.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_log.c)
    elseif(${test_name} STREQUAL "test_atx_interleave_positions")
        # MF-479: ohne gemessene Positionen schreibt ATX das Atari-Layout
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/formats/atx/uft_atx.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_interleave.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_log.c)
    elseif(${test_name} STREQUAL "test_atx_layout")
        # ATX track/chunk layout vs. the a8rawconv oracle (MF-467)
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/formats/atx/uft_atx.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_interleave.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_log.c)
    elseif(${test_name} STREQUAL "test_sector_id_on_disk")
        # ARCH-20/MF-465: id.sector is the number on the medium. Proof is the
        # D64/G64 corpus pair — same disk, same tool, two representations.
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/formats/d64/uft_d64_plugin.c
            ${CMAKE_SOURCE_DIR}/src/formats/g64/uft_g64.c
            ${CMAKE_SOURCE_DIR}/src/formats/adf/uft_adf_plugin.c
            ${CMAKE_SOURCE_DIR}/src/formats/do/uft_do.c
            ${CMAKE_SOURCE_DIR}/src/formats/img/uft_img.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_metadata.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_log.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_unified_types.c)
        target_compile_definitions(${test_name} PRIVATE
            UFT_CORPUS_DIR="${CMAKE_SOURCE_DIR}/tests/corpus_free")
    elseif(${test_name} STREQUAL "test_convert_via_plugin")
        # MF-433: converter seam — blob loader vs plugin, byte-identical
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/formats/c64/uft_d64_g64.c
            ${CMAKE_SOURCE_DIR}/src/formats/d64/uft_d64_plugin.c
            ${CMAKE_SOURCE_DIR}/src/formats/d67/uft_d67.c
            ${CMAKE_SOURCE_DIR}/src/formats/g64/uft_g64.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_unified_types.c)
        target_compile_definitions(${test_name} PRIVATE
            UFT_CORPUS_DIR="${CMAKE_SOURCE_DIR}/tests/corpus_free")
    elseif(${test_name} STREQUAL "test_imd_track_record")
        # IMD track-record layout regression vectors (MF-430)
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/formats/imd/uft_imd_adapter.c
            ${CMAKE_SOURCE_DIR}/src/formats/xdf/uft_xdf_adapter.c)
    elseif(${test_name} STREQUAL "test_corpus_cbm_vice")
        # T1b cross-tool corpus: D67/D80/D82/G71 from VICE c1541 (MF-427)
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/formats/d67/uft_d67.c
            ${CMAKE_SOURCE_DIR}/src/formats/d80/uft_d80.c
            ${CMAKE_SOURCE_DIR}/src/formats/d82/uft_d82.c
            ${CMAKE_SOURCE_DIR}/src/formats/g71/uft_g71.c)
        target_compile_definitions(${test_name} PRIVATE
            UFT_CORPUS_DIR="${CMAKE_SOURCE_DIR}/tests/corpus_free")
    elseif(${test_name} STREQUAL "test_corpus_xfd")
        # T1b cross-tool corpus: XFD derived from the atrcopy ATR (MF-426).
        # Both readers are linked so the two containers can be compared.
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/formats/xfd/uft_xfd.c
            ${CMAKE_SOURCE_DIR}/src/formats/atr/uft_atr.c)
        target_compile_definitions(${test_name} PRIVATE
            UFT_CORPUS_DIR="${CMAKE_SOURCE_DIR}/tests/corpus_free")
    elseif(${test_name} STREQUAL "test_corpus_atr")
        # T1b cross-tool corpus: ATR from the atrcopy project (MF-365)
        target_sources(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/src/formats/atr/uft_atr.c)
        target_compile_definitions(${test_name} PRIVATE
            UFT_CORPUS_DIR="${CMAKE_SOURCE_DIR}/tests/corpus_free")
    elseif(${test_name} STREQUAL "test_corpus_fdi")
        # T1 real corpus: ZX Spectrum FDI (Spectrofon #01 diskmag, local-only) (MF-367)
        target_sources(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/src/formats/fdi/uft_fdi_plugin.c)
        target_compile_definitions(${test_name} PRIVATE
            UFT_CORPUS_RESTRICTED_DIR="${CMAKE_SOURCE_DIR}/tests/corpus")
    elseif(${test_name} STREQUAL "test_external_integration")
        # MF-399: real KryoFlux stream decoder instead of local constants
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/flux/uft_kryoflux_stream.c)
    elseif(${test_name} STREQUAL "test_recovery")
        # MF-397: real CRC-16 + single-bit correction instead of local copies
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/algorithms/advanced/uft_crc_correction_v2.c
            ${CMAKE_SOURCE_DIR}/src/algorithms/advanced/uft_god_mode_api.c)
    elseif(${test_name} STREQUAL "test_fatfs")
        # MF-396: real FAT detection instead of a local copy
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/fs/uft_fat12.c)
    elseif(${test_name} STREQUAL "test_woz_writer")
        # MF-396: real WOZ CRC-32 instead of a local copy
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/formats/apple/uft_woz.c)
    elseif(${test_name} STREQUAL "test_adf")
        # MF-396: real AmigaDOS name hashing instead of a local copy
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/fs/uft_amigados.c)
    elseif(${test_name} STREQUAL "test_d64_writer")
        # MF-395: real GCR ops + cross-check of the two sync counters
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/formats/c64/uft_gcr_ops.c
            ${CMAKE_SOURCE_DIR}/src/protection/ufm_c64_metrics.c
            ${CMAKE_SOURCE_DIR}/src/formats/g64/uft_g64.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_metadata.c)
        target_compile_definitions(${test_name} PRIVATE
            UFT_CORPUS_DIR="${CMAKE_SOURCE_DIR}/tests/corpus_free")
    elseif(${test_name} STREQUAL "test_c64_metrics_corpus")
        # C64 GCR metrics vs a real VICE-produced G64 (MF-381, closes PROT-2)
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/protection/ufm_c64_metrics.c
            ${CMAKE_SOURCE_DIR}/src/protection/ufm_c64_scheme_detect.c
            ${CMAKE_SOURCE_DIR}/src/formats/g64/uft_g64.c)
        target_compile_definitions(${test_name} PRIVATE
            UFT_CORPUS_DIR="${CMAKE_SOURCE_DIR}/tests/corpus_free")
    elseif(${test_name} STREQUAL "test_format_probe_fuzz")
        # MF-392: adversarial input for every linked probe; runs under the
        # ASan/UBSan CI jobs. Same plugin set as test_plugin_probe_real.
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/formats/d64/uft_d64_plugin.c
            ${CMAKE_SOURCE_DIR}/src/formats/d71/uft_d71.c
            ${CMAKE_SOURCE_DIR}/src/formats/d81/uft_d81.c
            ${CMAKE_SOURCE_DIR}/src/formats/g64/uft_g64.c
            ${CMAKE_SOURCE_DIR}/src/formats/hfe/uft_hfe.c
            ${CMAKE_SOURCE_DIR}/src/formats/atr/uft_atr.c
            ${CMAKE_SOURCE_DIR}/src/formats/d88/uft_d88.c
            ${CMAKE_SOURCE_DIR}/src/formats/dc42/uft_dc42.c
            ${CMAKE_SOURCE_DIR}/src/formats/imd/uft_imd_plugin.c
            ${CMAKE_SOURCE_DIR}/src/formats/atx/uft_atx.c
            ${CMAKE_SOURCE_DIR}/src/formats/cqm/uft_cqm.c
            ${CMAKE_SOURCE_DIR}/src/formats/jv1/uft_jv1.c
            ${CMAKE_SOURCE_DIR}/src/formats/xfd/uft_xfd.c
            ${CMAKE_SOURCE_DIR}/src/formats/amstrad/uft_edsk.c
            ${CMAKE_SOURCE_DIR}/src/formats/amstrad/uft_edsk_parser.c
            ${CMAKE_SOURCE_DIR}/src/formats/msa/uft_msa_plugin.c
            ${CMAKE_SOURCE_DIR}/src/formats/nib/uft_nib.c
            ${CMAKE_SOURCE_DIR}/src/formats/apple/uft_woz_plugin.c
            ${CMAKE_SOURCE_DIR}/src/formats/apple/uft_woz.c
            ${CMAKE_SOURCE_DIR}/src/formats/do/uft_do.c
            ${CMAKE_SOURCE_DIR}/src/formats/po/uft_po.c
            ${CMAKE_SOURCE_DIR}/src/formats/img/uft_img.c
            ${CMAKE_SOURCE_DIR}/src/formats/td0/uft_td0.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_metadata.c
            ${CMAKE_SOURCE_DIR}/src/formats/fdi/uft_fdi_plugin.c
            ${CMAKE_SOURCE_DIR}/src/formats/ipf/uft_ipf_plugin.c
            ${CMAKE_SOURCE_DIR}/src/formats/ipf/uft_caps_ipf.c
            ${CMAKE_SOURCE_DIR}/src/formats/ipf/uft_ipf_air.c
            ${CMAKE_SOURCE_DIR}/src/formats/scp/uft_scp_plugin.c
            ${CMAKE_SOURCE_DIR}/src/formats/stx/uft_stx_plugin.c
            ${CMAKE_SOURCE_DIR}/src/formats/2img/uft_2img.c
            ${CMAKE_SOURCE_DIR}/src/formats/dsk_cpc/uft_dsk_cpc.c
            ${CMAKE_SOURCE_DIR}/src/formats/adf/uft_adf_plugin.c
            # cqm reports header-checksum / data-CRC findings via UFT_WARN (MF-461)
            ${CMAKE_SOURCE_DIR}/src/core/uft_log.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_decoder_plugin_stub.c)
    elseif(${test_name} STREQUAL "test_plugin_probe_real")
        # MF-385: probes the SHIPPED plugins instead of a copy in the test
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/formats/d64/uft_d64_plugin.c
            ${CMAKE_SOURCE_DIR}/src/formats/d71/uft_d71.c
            ${CMAKE_SOURCE_DIR}/src/formats/d81/uft_d81.c
            ${CMAKE_SOURCE_DIR}/src/formats/g64/uft_g64.c
            ${CMAKE_SOURCE_DIR}/src/formats/hfe/uft_hfe.c
            ${CMAKE_SOURCE_DIR}/src/formats/atr/uft_atr.c
            ${CMAKE_SOURCE_DIR}/src/formats/d88/uft_d88.c
            ${CMAKE_SOURCE_DIR}/src/formats/dc42/uft_dc42.c
            ${CMAKE_SOURCE_DIR}/src/formats/imd/uft_imd_plugin.c
            ${CMAKE_SOURCE_DIR}/src/formats/atx/uft_atx.c
            ${CMAKE_SOURCE_DIR}/src/formats/cqm/uft_cqm.c
            ${CMAKE_SOURCE_DIR}/src/formats/jv1/uft_jv1.c
            ${CMAKE_SOURCE_DIR}/src/formats/xfd/uft_xfd.c
            ${CMAKE_SOURCE_DIR}/src/formats/amstrad/uft_edsk.c
            ${CMAKE_SOURCE_DIR}/src/formats/amstrad/uft_edsk_parser.c
            ${CMAKE_SOURCE_DIR}/src/formats/msa/uft_msa_plugin.c
            ${CMAKE_SOURCE_DIR}/src/formats/nib/uft_nib.c
            ${CMAKE_SOURCE_DIR}/src/formats/apple/uft_woz_plugin.c
            ${CMAKE_SOURCE_DIR}/src/formats/apple/uft_woz.c
            ${CMAKE_SOURCE_DIR}/src/formats/do/uft_do.c
            ${CMAKE_SOURCE_DIR}/src/formats/po/uft_po.c
            ${CMAKE_SOURCE_DIR}/src/formats/img/uft_img.c
            ${CMAKE_SOURCE_DIR}/src/formats/td0/uft_td0.c
            # img read_metadata pulls uft_meta_free (uft_metadata.c) via core stubs
            ${CMAKE_SOURCE_DIR}/src/core/uft_metadata.c
            ${CMAKE_SOURCE_DIR}/src/formats/fdi/uft_fdi_plugin.c
            ${CMAKE_SOURCE_DIR}/src/formats/ipf/uft_ipf_plugin.c
            ${CMAKE_SOURCE_DIR}/src/formats/ipf/uft_caps_ipf.c
            ${CMAKE_SOURCE_DIR}/src/formats/ipf/uft_ipf_air.c
            ${CMAKE_SOURCE_DIR}/src/formats/scp/uft_scp_plugin.c
            ${CMAKE_SOURCE_DIR}/src/formats/stx/uft_stx_plugin.c
            ${CMAKE_SOURCE_DIR}/src/formats/2img/uft_2img.c
            ${CMAKE_SOURCE_DIR}/src/formats/dsk_cpc/uft_dsk_cpc.c
            ${CMAKE_SOURCE_DIR}/src/formats/adf/uft_adf_plugin.c
            # cqm reports header-checksum / data-CRC findings via UFT_WARN (MF-461)
            ${CMAKE_SOURCE_DIR}/src/core/uft_log.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_decoder_plugin_stub.c)
    elseif(${test_name} STREQUAL "test_protection_honest_failure")
        # MF-384: pins that unimplemented detectors fail instead of inventing
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/protection/uft_c64_protection_enhanced.c
            ${CMAKE_SOURCE_DIR}/src/protection/uft_protection_extended.c
            ${CMAKE_SOURCE_DIR}/src/protection/uft_speedlock.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_ir_format.c)
    elseif(${test_name} STREQUAL "test_scp_legacy_adapter")
        # MF-418: legacy uft_scp_* adapters over the shipped parser, checked
        # against the local-only cross-tool SCP capture.
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/core/uft_core_stubs.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_metadata.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_scp_parser.c)
        target_compile_definitions(${test_name} PRIVATE
            UFT_CORPUS_RESTRICTED_DIR="${CMAKE_SOURCE_DIR}/tests/corpus")
    elseif(${test_name} STREQUAL "test_c64_protection_real_corpus")
        # Positive control: real protected C64 disks, local-only images (MF-383)
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/protection/ufm_c64_metrics.c
            ${CMAKE_SOURCE_DIR}/src/protection/ufm_c64_scheme_detect.c
            ${CMAKE_SOURCE_DIR}/src/formats/g64/uft_g64.c
            # MF-405: the GUI's own G64 reader, so its data path is testable
            ${CMAKE_SOURCE_DIR}/src/formats/c64/uft_d64_g64.c
            ${CMAKE_SOURCE_DIR}/src/formats/cbm/uft_cbm_geometry.c)
        target_compile_definitions(${test_name} PRIVATE
            UFT_CORPUS_RESTRICTED_DIR="${CMAKE_SOURCE_DIR}/tests/corpus"
            # MF-404: the half-track test needs the rights-free VICE G64 as
            # its negative control (a disk that has none).
            UFT_CORPUS_DIR="${CMAKE_SOURCE_DIR}/tests/corpus_free")
    elseif(${test_name} STREQUAL "test_corpus_protection_copylock")
        # Real-corpus protection: CopyLock ST vs dec0de game samples, local-only (MF-377)
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/protection/uft_atarist_copylock.c)
        target_compile_definitions(${test_name} PRIVATE
            UFT_CORPUS_RESTRICTED_DIR="${CMAKE_SOURCE_DIR}/tests/corpus")
    elseif(${test_name} STREQUAL "test_akai_s900_plugin")
        # Phase-3 new format: Akai S900/S950 sampler (DD 5x1024 / HD 10x1024) (MF-348)
        target_sources(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/src/formats/akai/uft_akai_s900.c)
    elseif(${test_name} STREQUAL "test_lisa_twiggy_plugin")
        # Phase-3 new format: Apple Lisa Twiggy zoned GCR (871424, decoded sectors) (MF-349)
        target_sources(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/src/formats/lisa/uft_lisa_twiggy.c)
    elseif(${test_name} STREQUAL "test_td0_error_marks")
        # TD0 per-sector CRC/deleted flags + open-scan off-by-one (Phase-4 WP5, MF-334)
        target_sources(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/src/formats/td0/uft_td0.c)
    elseif(${test_name} STREQUAL "test_stx_error_marks")
        # STX (Pasti) FDC-status disk-error marking + descriptor offset fix (Phase-4 WP5, MF-335)
        target_sources(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/src/formats/stx/uft_stx_plugin.c)
    elseif(${test_name} STREQUAL "test_d88_error_marks")
        # D88 DDAM/FDC-status disk-error marking + offset fix (Phase-4 WP5, MF-336)
        target_sources(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/src/formats/d88/uft_d88.c)
    elseif(${test_name} STREQUAL "test_d64_write_roundtrip")
        # D64 write_track round-trip; confirms the systemic data_len fix on the CBM family (MF-323)
        target_sources(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/src/formats/d64/uft_d64_plugin.c)
    elseif(${test_name} STREQUAL "test_dc42_checksum_roundtrip")
        # DC42 write must keep the data checksum valid; caught stale-checksum bug (MF-324)
        target_sources(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/src/formats/dc42/uft_dc42.c)
    elseif(${test_name} STREQUAL "test_hdf_rdb_bounds")
        # RDB parser heap-overflow guard (tiny SummedLongs) — MF-326
        target_sources(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/src/formats/uft_hdf_parser.c)
        target_include_directories(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/src/formats)
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
            ${CMAKE_SOURCE_DIR}/src/analysis/events/uft_align_fuse_bridge.c            ${CMAKE_SOURCE_DIR}/src/analysis/events/otdr_align_fuse_v7.c
)
        target_include_directories(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/include/uft/analysis)
    elseif(${test_name} STREQUAL "test_write_gate")
        # Include write-gate policy + snapshot sources
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/policy/uft_write_gate.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_snapshot.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_sha256.c)
    elseif(${test_name} STREQUAL "test_xdf_api")
        # Include XDF API source directly
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/formats/xdf/uft_xdf_api.c
            ${CMAKE_SOURCE_DIR}/src/formats/xdf/uft_xdf_core.c
            ${CMAKE_SOURCE_DIR}/src/formats/xdf/uft_xdf_api_impl.c
            ${CMAKE_SOURCE_DIR}/src/compat/uft_fnmatch.c)
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
        # Needs: V2 provider .cpp + SCP direct C backend (now libusb-wired
        # in MF-254 — link against libusb when found).
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/hardware_providers/scp_provider_v2.cpp
            ${CMAKE_SOURCE_DIR}/src/hal/uft_scp_direct.c)
        target_include_directories(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src)
        if(LIBUSB_FOUND)
            target_link_libraries(${test_name} PRIVATE uft::libusb)
        endif()
        set(TEST_LIBS "")
    elseif(${test_name} STREQUAL "test_kryoflux_provider_v2")
        # MF-162 P1.9: KryoFluxProviderV2 mixin composition test.
        # Needs: V2 provider .cpp; no C-HAL backend (KF uses DTC subprocess).
        # MF-208 P1.24: + the KryoFlux stream decoder — do_read_raw_flux
        # now calls uft_kf_decode().
        # SubprocessMock is header-only; tests/ must be on the include path.
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/hardware_providers/kryoflux_provider_v2.cpp
            ${CMAKE_SOURCE_DIR}/src/flux/uft_kryoflux_stream.c)
        target_include_directories(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src
            ${CMAKE_SOURCE_DIR}/tests)
        set(TEST_LIBS "")
    elseif(${test_name} STREQUAL "test_kryoflux_stream")
        # MF-208 P1.24: KryoFlux stream-format decoder unit test.
        # Synthetic vectors pinning uft_kf_decode() to the documented
        # stream protocol. Needs only the decoder .c — no provider, no Qt.
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/flux/uft_kryoflux_stream.c)
        set(TEST_LIBS "")
    elseif(${test_name} STREQUAL "test_scp_read_memory")
        # MF-209 P1.24: uft_scp_read_track_memory() unit test — the
        # memory-buffer SCP decoder the FluxEngine SCP-pivot relies on.
        # Synthetic SCP vectors; needs only the parser .c — no Qt.
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/flux/uft_scp_parser.c)
        set(TEST_LIBS "")
    elseif(${test_name} STREQUAL "test_teensy_probe")
        # MF-213 ARCH-7-C: classify_teensy_probe() unit test — the pure
        # ADF-Copy/Applesauce probe decision core. teensy_probe.h is
        # header-inline + Qt-free (QString is only forward-declared), so
        # no sources and no Qt — just src/ on the include path.
        target_include_directories(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src)
        set(TEST_LIBS "")
    elseif(${test_name} STREQUAL "test_fluxengine_provider_v2")
        # MF-163 P1.10: FluxEngineProviderV2 mixin composition test.
        # Needs: V2 provider .cpp; no C-HAL backend (FE uses fluxengine subprocess).
        # MF-209 P1.24: + the SCP parser — do_read_raw_flux asks fluxengine
        # for SCP output and decodes it via uft_scp_read_track_memory().
        # SubprocessMock is header-only; tests/ must be on the include path.
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/hardware_providers/fluxengine_provider_v2.cpp
            ${CMAKE_SOURCE_DIR}/src/flux/uft_scp_parser.c)
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
    elseif(${test_name} STREQUAL "test_applesauce_runners_protocol")
        # MF-251 (v4.1.5-hardening): scripted-mock protocol test for the
        # 7 Applesauce runner-factories (applesauce_serial_runners.cpp).
        # Pulls only the runner .cpp + the provider .cpp (for the Result
        # types) — no Qt6::SerialPort dependency because the
        # ScriptedApplesauceTransport is a direct IApplesauceTransport
        # implementation, not a QSerialPort one. Validates protocol
        # parsing standalone so a future hardware bench-test (M3
        # §5) only needs to verify the QSerialPort I/O layer.
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/hardware_providers/applesauce_serial_runners.cpp
            ${CMAKE_SOURCE_DIR}/src/hardware_providers/applesauce_provider_v2.cpp)
        target_include_directories(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src
            ${CMAKE_SOURCE_DIR}/tests)
        set(TEST_LIBS "")
    elseif(${test_name} STREQUAL "test_adfcopy_runners_protocol")
        # MF-252 (v4.1.5-hardening): mirror of MF-251 for ADFCopy
        # (binary protocol over QSerialPort). Same standalone pattern —
        # ScriptedADFCopyTransport implements IADFCopyTransport directly.
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/hardware_providers/adfcopy_serial_runners.cpp
            ${CMAKE_SOURCE_DIR}/src/hardware_providers/adfcopy_provider_v2.cpp)
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
        # MF-210: kryoflux_provider_v2.cpp now links uft_kf_decode()
        # (src/flux/uft_kryoflux_stream.c, MF-208) and
        # fluxengine_provider_v2.cpp links the SCP parser
        # (src/flux/uft_scp_parser.c, MF-209). The conformance harness
        # compiles both provider .cpp files, so it needs both backends.
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/hardware_providers/greaseweazle_provider_v2.cpp
            ${CMAKE_SOURCE_DIR}/src/hal/uft_greaseweazle_full.c
            ${CMAKE_SOURCE_DIR}/src/hardware_providers/scp_provider_v2.cpp
            ${CMAKE_SOURCE_DIR}/src/hal/uft_scp_direct.c
            ${CMAKE_SOURCE_DIR}/src/hardware_providers/kryoflux_provider_v2.cpp
            ${CMAKE_SOURCE_DIR}/src/flux/uft_kryoflux_stream.c
            ${CMAKE_SOURCE_DIR}/src/hardware_providers/fluxengine_provider_v2.cpp
            ${CMAKE_SOURCE_DIR}/src/flux/uft_scp_parser.c
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
        if(LIBUSB_FOUND)
            target_link_libraries(${test_name} PRIVATE uft::libusb)
        endif()
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
    elseif(${test_name} STREQUAL "test_concurrency")
        # MF-226 / P3.3 #110a: improvement test — concurrent multi-device
        # sessions. Drives several MockProviderV2 instances at once + one
        # through a long session, proving the type-driven HAL's
        # no-shared-state guarantee. Same self-contained shape as
        # test_mock_provider_v2: just the mock TU + tests/ include path.
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/tests/mock_provider_v2.cpp)
        target_include_directories(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/tests)
        set(TEST_LIBS "")
    elseif(${test_name} STREQUAL "test_transitions_ns_contract")
        # v4.1.5 hardening / audit Lücke #2 (ARCH-2): enforces the
        # FluxCaptured::transitions_ns nanosecond-interval contract on
        # MockProviderV2 plus UFT-005 contract shields for KryoFlux +
        # FluxEngine providers (binary-not-found path must yield an
        # honest non-Captured outcome, never fabricated flux).
        #
        # Pure-C test (.c) per stop-hook spec. FluxCaptured is a C++
        # sum-type variant — pure C cannot parse outcomes.h directly.
        # Bridged via the tests/unit/transitions_ns_ffi.cpp wrapper
        # which exposes extern-"C" entry points. The .c test stays
        # forensically minimal; all C++ details live in the wrapper
        # next to it. Link language is forced to CXX so the C++ ABI
        # (std::string + std::vector in FluxCaptured) resolves at
        # link time without depending on the C-test compilation mode.
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/tests/mock_provider_v2.cpp
            ${CMAKE_SOURCE_DIR}/tests/unit/transitions_ns_ffi.cpp
            ${CMAKE_SOURCE_DIR}/src/hardware_providers/kryoflux_provider_v2.cpp
            ${CMAKE_SOURCE_DIR}/src/hardware_providers/fluxengine_provider_v2.cpp
            ${CMAKE_SOURCE_DIR}/src/flux/uft_kryoflux_stream.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_scp_parser.c)
        target_include_directories(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/tests
            ${CMAKE_SOURCE_DIR}/src)
        set_target_properties(${test_name} PROPERTIES LINKER_LANGUAGE CXX)
        set(TEST_LIBS uft_core uft_crc)
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
    elseif(${test_name} STREQUAL "test_provenance_chain")
        # MF-214 / P3.3 #110: forensic provenance chain-of-custody
        # improvement test. uft_provenance.c is self-contained (it ships
        # its own SHA-256), so the one TU is the only source needed.
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/forensic/uft_provenance.c)
        set(TEST_LIBS "")
    elseif(${test_name} STREQUAL "test_marginal_data_preserved")
        # MF-215 / P3.3 #110: "Kein Bit verloren" improvement test —
        # divergent multi-reads are preserved, never collapsed. The
        # multiread pipeline TU is self-contained (links libm for the
        # math it uses); no other UFT source needed.
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/recovery/uft_multiread_pipeline.c)
        if(UNIX)
            set(TEST_LIBS ${M_LIBRARY})
        else()
            set(TEST_LIBS "")
        endif()
    elseif(${test_name} STREQUAL "test_destructive_op_consent")
        # MF-216 / P3.3 #110: lossy/destructive-op consent gate
        # improvement test. uft_preflight.c drives uft_roundtrip.c (the
        # round-trip matrix) and uft_loss_report.c (the sidecar writer);
        # all three are self-contained C TUs.
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/core/uft_preflight.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_roundtrip.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_loss_report.c)
        set(TEST_LIBS "")
    elseif(${test_name} STREQUAL "test_flux_mfm_sync")
        # MF-218 / #109 sub-task: regression test for the IBM-MFM
        # A1-sync-run decode bug. Synthesises its own MFM track, so the
        # flux decoder TU is the only production source needed (libm
        # for the PLL math).
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/flux/uft_flux_decoder.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_flux_histogram.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_flux_sync_search.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_dewarp.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_media_profile.c)
        if(UNIX)
            set(TEST_LIBS ${M_LIBRARY})
        else()
            set(TEST_LIBS "")
        endif()
    elseif(${test_name} STREQUAL "test_flux_gcr_c64_sync")
        # v4.1.5 hardening / audit Lücke #1: hardware-independent C64
        # GCR decoder regression test. Same self-contained synthesis
        # pattern as test_flux_mfm_sync; only the flux decoder TU is
        # needed.
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/flux/uft_flux_decoder.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_flux_histogram.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_flux_sync_search.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_dewarp.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_media_profile.c
            # MF-407 (PROT-11): the flux -> GCR -> metrics chain
            ${CMAKE_SOURCE_DIR}/src/protection/ufm_c64_metrics.c)
        if(UNIX)
            set(TEST_LIBS ${M_LIBRARY})
        else()
            set(TEST_LIBS "")
        endif()
    elseif(${test_name} STREQUAL "test_flux_gcr_apple_sync")
        # v4.1.5 hardening / audit Lücke #1: hardware-independent
        # Apple ][ 6-and-2 GCR decoder regression test.
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/flux/uft_flux_decoder.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_flux_histogram.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_flux_sync_search.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_dewarp.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_media_profile.c)
        if(UNIX)
            set(TEST_LIBS ${M_LIBRARY})
        else()
            set(TEST_LIBS "")
        endif()
    elseif(${test_name} STREQUAL "test_flux_amiga_sync")
        # v4.1.5 hardening / audit Lücke #1: hardware-independent Amiga
        # MFM decoder regression test (odd/even-split sector layout).
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/flux/uft_flux_decoder.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_flux_histogram.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_flux_sync_search.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_dewarp.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_media_profile.c)
        if(UNIX)
            set(TEST_LIBS ${M_LIBRARY})
        else()
            set(TEST_LIBS "")
        endif()
    elseif(${test_name} STREQUAL "test_format_extension")
        # MF-228 / P3.3 #110c: format-extension improvement test —
        # decodes a synthetic Amiga HDF/RDB image, a format entirely
        # outside Greaseweazle's set. uft_hdf_parser.c is self-contained
        # (its header lives next to it in src/formats/).
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/formats/uft_hdf_parser.c)
        target_include_directories(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/formats)
        set(TEST_LIBS "")
    elseif(${test_name} STREQUAL "test_protection_detection")
        # MF-227 / P3.3 #110b: copy-protection detection improvement test
        # — synthesises tracks with known longtrack + Dungeon Master
        # fuzzy-bit signatures and asserts UFT names the scheme. Links
        # the longtrack + fuzzy-bit detectors; uft_fuzzy_bits.c pulls in
        # the flux decoder for its ctx-based functions (not exercised by
        # this test, but needed at link time). libm for the PLL/timing
        # math.
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/protection/uft_longtrack.c
            ${CMAKE_SOURCE_DIR}/src/protection/uft_fuzzy_bits.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_flux_decoder.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_flux_histogram.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_flux_sync_search.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_dewarp.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_media_profile.c)
        if(UNIX)
            set(TEST_LIBS ${M_LIBRARY})
        else()
            set(TEST_LIBS "")
        endif()
    elseif(${test_name} STREQUAL "test_loss_report")
        # v4.1.5-hardening: loss-report sidecar writer is a self-contained
        # core TU (KNOWN_ISSUES §1.1). The test was registered but linked
        # only against uft_core which doesn't include uft_loss_report.c
        # (verified: src/core/CMakeLists.txt CORE_SOURCES list).
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/core/uft_loss_report.c)
        set(TEST_LIBS "")
    elseif(${test_name} STREQUAL "test_roundtrip_matrix")
        # v4.1.5-hardening: round-trip matrix registry is a self-contained
        # core TU (KNOWN_ISSUES §5.1). Same pattern as test_loss_report —
        # source exists but isn't part of uft_core.
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/core/uft_roundtrip.c)
        set(TEST_LIBS "")
    elseif(${test_name} STREQUAL "test_preflight")
        # v4.1.5-hardening: preflight combines loss-report writer +
        # round-trip status (KNOWN_ISSUES §1.2). All three core TUs are
        # self-contained. Mirrors test_destructive_op_consent above.
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/core/uft_preflight.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_roundtrip.c
            ${CMAKE_SOURCE_DIR}/src/core/uft_loss_report.c)
        set(TEST_LIBS "")
    elseif(${test_name} STREQUAL "test_xum1541_hal")
        # MF-255 (v4.1.5-hardening): XUM1541 HAL test now exercises the
        # real libusb wiring. Compile the .c file with UFT_HAS_LIBUSB so
        # the test's #ifdef branches see the production path. Link libusb
        # when available.
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/hal/uft_xum1541.c)
        target_include_directories(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src)
        if(LIBUSB_FOUND)
            target_link_libraries(${test_name} PRIVATE uft::libusb)
        endif()
        set(TEST_LIBS "")
    elseif(${test_name} STREQUAL "test_scp_direct_hal")
        # v4.1.5-hardening: SCP-Direct HAL test exercises libusb wiring
        # (MF-254). Same pattern as test_xum1541_hal: link uft_scp_direct.c
        # + libusb when available.
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/hal/uft_scp_direct.c)
        target_include_directories(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src)
        if(LIBUSB_FOUND)
            target_link_libraries(${test_name} PRIVATE uft::libusb)
        endif()
        set(TEST_LIBS "")
    elseif(${test_name} STREQUAL "test_scp_direct_usb_mock")
        # MF-270 — Tier-2.5 USB-mock integration test. Compile
        # uft_scp_direct.c (via a wrapper TU that has the mock header
        # in scope) against the scripted libusb shim. No real libusb
        # link, no real USB. Other tests linking uft_scp_direct.c are
        # unaffected because the wrapper is per-target.
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/tests/usb_mock/scp_direct_mocked.c
            ${CMAKE_SOURCE_DIR}/tests/usb_mock/libusb_mock.c)
        target_include_directories(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src
            ${CMAKE_SOURCE_DIR}/tests
            ${CMAKE_SOURCE_DIR}/tests/usb_mock)
        target_compile_definitions(${test_name} PRIVATE
            UFT_HAS_LIBUSB=1
            UFT_USB_MOCK=1)
        set(TEST_LIBS "")
    elseif(${test_name} STREQUAL "test_xum1541_usb_mock")
        # MF-270 — same pattern as test_scp_direct_usb_mock for XUM1541.
        # XUM1541 additionally uses libusb_control_transfer (INIT +
        # SHUTDOWN), which the mock implements.
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/tests/usb_mock/xum1541_mocked.c
            ${CMAKE_SOURCE_DIR}/tests/usb_mock/libusb_mock.c)
        target_include_directories(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src
            ${CMAKE_SOURCE_DIR}/tests
            ${CMAKE_SOURCE_DIR}/tests/usb_mock)
        target_compile_definitions(${test_name} PRIVATE
            UFT_HAS_LIBUSB=1
            UFT_USB_MOCK=1)
        set(TEST_LIBS "")
    elseif(${test_name} STREQUAL "test_applesauce_hal")
        # v4.1.5-hardening: Applesauce HAL test — pure-utility +
        # lifecycle + setter validation. Serial-touching stubs return
        # honest -1 (no QSerialPort wiring needed in the C HAL layer;
        # that lives in src/hardware_providers/qserial_applesauce_*).
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/hal/uft_applesauce.c)
        target_include_directories(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src)
        set(TEST_LIBS "")
    elseif(${test_name} STREQUAL "test_fnmatch_shim")
        # v4.1.5-hardening: portable fnmatch (Windows fallback). Source
        # lives in src/compat/, not in uft_core.
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/compat/uft_fnmatch.c)
        set(TEST_LIBS "")
    elseif(${test_name} STREQUAL "test_whdload_resload")
        # v4.1.5-hardening: WHDLoad resload_* API catalog (restored
        # from v3.7.0). Source in src/whdload/, not uft_core.
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/whdload/whdload_resload_api.c)
        set(TEST_LIBS "")
    elseif(${test_name} STREQUAL "test_salvage_fs")
        # v4.1.5-hardening: salvage-fs strategies API (T7 DiskSalv-scaffold,
        # see docs/MASTER_PLAN.md §M2 Amiga-Block). Source is in
        # src/recovery/ (not uft_core), so add it explicitly.
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/recovery/uft_salvage_fs.c)
        set(TEST_LIBS "")
    elseif(${test_name} STREQUAL "test_msa")
        # v4.1.5-hardening: Atari MSA format (RLE-compressed ST). Source
        # in src/formats/msa/, not in uft_core.
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/formats/msa/uft_msa.c)
        set(TEST_LIBS "")
    elseif(${test_name} STREQUAL "test_bootblock_scanner")
        # v4.1.5-hardening: Amiga bootblock scanner (M2 T2).
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/fs/uft_bootblock_scanner.c
            ${CMAKE_SOURCE_DIR}/src/fs/uft_amiga_virus_db.c)
        set(TEST_LIBS "")
    elseif(${test_name} STREQUAL "test_disk_quickscan")
        # v4.1.5-hardening: CHECKDISK pre-capture scan (M2 T6).
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/analysis/uft_disk_quickscan.c)
        set(TEST_LIBS "")
    elseif(${test_name} STREQUAL "test_disc_diagnostics")
        # v4.1.5-hardening: disc diagnostics (TDFC).
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/diag/uft_disc_diagnostics.c)
        set(TEST_LIBS "")
    elseif(${test_name} STREQUAL "test_display_track")
        # v4.1.5-hardening: track display utility.
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/display/uft_display_track.c)
        set(TEST_LIBS "")
    elseif(${test_name} STREQUAL "test_whd_crc16")
        # v4.1.5-hardening: WHDLoad CRC16.
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/whdload/whd_crc16.c)
        set(TEST_LIBS "")
    elseif(${test_name} STREQUAL "test_uft_dms")
        # v4.1.5-hardening: DMS (DiskMasher) format. The .c uses an
        # unqualified `#include "uft_dms.h"` so the include dir of the
        # actual header (include/uft/formats/) must be added.
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/formats/dms/uft_dms.c)
        target_include_directories(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/include/uft/detect
            ${CMAKE_SOURCE_DIR}/include/uft/formats)
        set(TEST_LIBS "")
    elseif(${test_name} STREQUAL "test_tdfc_plus")
        # v4.1.5-hardening: TDFC+ OTDR module. tdfc.h lives in
        # include/uft/analysis/ but the .c uses unqualified `#include "tdfc.h"`.
        target_sources(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/src/analysis/otdr/tdfc_plus.c
            ${CMAKE_SOURCE_DIR}/src/analysis/otdr/tdfc.c)
        target_include_directories(${test_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/include/uft/analysis)
        if(UNIX AND M_LIBRARY)
            set(TEST_LIBS ${M_LIBRARY})
        else()
            set(TEST_LIBS "")
        endif()
    endif()
    
    if(TEST_LIBS)
        target_link_libraries(${test_name} PRIVATE ${TEST_LIBS})
    endif()
    
    # Register as CTest
    add_test(NAME ${test_name} COMMAND ${test_name})

    # Restricted-corpus tests (image is local-only, gitignored): exit code 77
    # means "corpus image absent, skipped" — not a failure (MF-367).
    if(${test_name} STREQUAL "test_corpus_fdi" OR
       ${test_name} STREQUAL "test_corpus_scp" OR
       ${test_name} STREQUAL "test_convert_scp_adf" OR
       ${test_name} STREQUAL "test_scp_readers_agree" OR
       ${test_name} STREQUAL "test_corpus_protection_copylock" OR
       ${test_name} STREQUAL "test_c64_protection_real_corpus" OR
       ${test_name} STREQUAL "test_scp_legacy_adapter")
        set_tests_properties(${test_name} PROPERTIES SKIP_RETURN_CODE 77)
    endif()

    # MF-254 / MF-255: tests that link uft_scp_direct.c or uft_xum1541.c
    # need libusb-1.0.dll on PATH at runtime (Windows). Use the same \;
    # escape pattern as the Qt block below so CMake doesn't split the
    # ENVIRONMENT value on the embedded PATH-separator semicolon.
    if(WIN32 AND DEFINED UFT_LIBUSB_DLL_DIR)
        if(${test_name} STREQUAL "test_hal_conformance" OR
           ${test_name} STREQUAL "test_scp_provider_v2" OR
           ${test_name} STREQUAL "test_xum1541_hal" OR
           ${test_name} STREQUAL "test_scp_direct_hal")
            set_tests_properties(${test_name} PROPERTIES
                ENVIRONMENT "PATH=${UFT_LIBUSB_DLL_DIR}")
        endif()
    endif()

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

# ─────────────────────────────────────────────────────────────────────────────
# Hardware-emulator tests (post-v4.1.5, see .claude/agents/
# hardware-emulation-author.md). Each controller's emulator lives under
# tests/emulators/<controller>/ and tests/flux_gen/<controller>/, outside
# the main TEST_SOURCES_C glob. The test executables are added explicitly
# here so the per-emulator file layout (state-machine + flux-gen +
# defects/) stays the controller-team's local concern.
#
# These tests are pure C, use NO production hardware paths (no libusb,
# no serial port). They model firmware sequencing semantics + synthetic
# flux determinism so the byte-mock tests can stay short and focused.
# ─────────────────────────────────────────────────────────────────────────────

# SCP-Direct firmware-realistic emulator (first invocation of the
# hardware-emulation-author agent).
add_executable(test_scp_emulator
    ${CMAKE_SOURCE_DIR}/tests/emulators/scp/test_scp_emulator.c
    ${CMAKE_SOURCE_DIR}/tests/emulators/scp/firmware_state_machine.c
    ${CMAKE_SOURCE_DIR}/tests/flux_gen/scp/flux_gen.c
    ${CMAKE_SOURCE_DIR}/tests/flux_gen/scp/defects/weak_bits.c
    ${CMAKE_SOURCE_DIR}/tests/flux_gen/scp/defects/crc_errors.c
    ${CMAKE_SOURCE_DIR}/tests/flux_gen/scp/defects/copy_protection/vmax.c)
target_include_directories(test_scp_emulator PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/uft)
set_target_properties(test_scp_emulator PROPERTIES
    C_STANDARD 11
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests)
add_test(NAME test_scp_emulator COMMAND test_scp_emulator)
message(STATUS "UFT Test ENABLED: test_scp_emulator (firmware-realistic SCP sim)")

# Greaseweazle firmware-realistic emulator (second invocation of the
# hardware-emulation-author agent — calibrated against the only PRODUCTION-
# tested UFT HAL at src/hal/uft_greaseweazle_full.c).
add_executable(test_greaseweazle_emulator
    ${CMAKE_SOURCE_DIR}/tests/emulators/greaseweazle/test_greaseweazle_emulator.c
    ${CMAKE_SOURCE_DIR}/tests/emulators/greaseweazle/firmware_state_machine.c
    ${CMAKE_SOURCE_DIR}/tests/flux_gen/greaseweazle/flux_gen.c)
target_include_directories(test_greaseweazle_emulator PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/uft)
set_target_properties(test_greaseweazle_emulator PROPERTIES
    C_STANDARD 11
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests)
add_test(NAME test_greaseweazle_emulator COMMAND test_greaseweazle_emulator)
message(STATUS "UFT Test ENABLED: test_greaseweazle_emulator (firmware-realistic GW sim)")

# XUM1541/ZoomFloppy firmware-realistic emulator (third invocation of the
# hardware-emulation-author agent). Links src/hal/uft_xum1541.c so the GCR
# generator's Commodore zone tables are cross-checked against the HAL SSOT
# uft_xum_sectors_for_track(). Only pure-utility HAL functions are called;
# libusb is linked solely because the root CMakeLists compiles the HAL with
# UFT_HAS_LIBUSB globally when libusb is found (no USB I/O at test runtime).
add_executable(test_xum1541_emulator
    ${CMAKE_SOURCE_DIR}/tests/emulators/xum1541/test_xum1541_emulator.c
    ${CMAKE_SOURCE_DIR}/tests/emulators/xum1541/firmware_state_machine.c
    ${CMAKE_SOURCE_DIR}/tests/flux_gen/xum1541/flux_gen.c
    ${CMAKE_SOURCE_DIR}/src/hal/uft_xum1541.c)
target_include_directories(test_xum1541_emulator PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/uft)
if(LIBUSB_FOUND)
    target_link_libraries(test_xum1541_emulator PRIVATE uft::libusb)
endif()
set_target_properties(test_xum1541_emulator PROPERTIES
    C_STANDARD 11
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests)
add_test(NAME test_xum1541_emulator COMMAND test_xum1541_emulator)
if(WIN32 AND DEFINED UFT_LIBUSB_DLL_DIR)
    # Same PATH-escape pattern as the MF-254/MF-255 HAL tests: the linked
    # libusb-1.0.dll must be findable at runtime on Windows.
    set_tests_properties(test_xum1541_emulator PROPERTIES
        ENVIRONMENT "PATH=${UFT_LIBUSB_DLL_DIR}")
endif()
message(STATUS "UFT Test ENABLED: test_xum1541_emulator (firmware-realistic XUM1541 sim)")

# Applesauce firmware-realistic emulator (fourth invocation of the
# hardware-emulation-author agent). Models the ASCII line protocol over
# USB-CDC serial + an Apple-domain GCR flux generator. Links
# src/hal/uft_applesauce.c so the generator's 8 MHz tick assumption and
# the emulator are cross-checked against the HAL SSOT
# (uft_as_get_sample_clock / uft_as_ticks_to_ns / uft_as_format_name).
# The Applesauce HAL is serial-only (no libusb); -lm covers fabs() in
# the SSOT tick assertions. Only pure-utility HAL functions are called.
add_executable(test_applesauce_emulator
    ${CMAKE_SOURCE_DIR}/tests/emulators/applesauce/test_applesauce_emulator.c
    ${CMAKE_SOURCE_DIR}/tests/emulators/applesauce/firmware_state_machine.c
    ${CMAKE_SOURCE_DIR}/tests/flux_gen/applesauce/flux_gen.c
    ${CMAKE_SOURCE_DIR}/src/hal/uft_applesauce.c)
target_include_directories(test_applesauce_emulator PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/uft
    ${CMAKE_SOURCE_DIR}/tests/emulators/applesauce
    ${CMAKE_SOURCE_DIR}/tests/flux_gen/applesauce)
# No libm link: the test avoids fabs() (see approx() in the source) so it
# builds under CI's LINK_LIBRARIES_ONLY_TARGETS, which rejects the bare
# "m" name (MF-305).
set_target_properties(test_applesauce_emulator PROPERTIES
    C_STANDARD 11
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests)
add_test(NAME test_applesauce_emulator COMMAND test_applesauce_emulator)
message(STATUS "UFT Test ENABLED: test_applesauce_emulator (firmware-realistic Applesauce sim)")

# KryoFlux firmware-realistic emulator (fifth invocation of the
# hardware-emulation-author agent). KryoFlux has no wire protocol in UFT —
# the HAL runs the DTC tool which writes a RAW stream file. So this
# emulator models the DTC+device pair and its flux generator emits REAL
# KryoFlux RAW streams that are decoded by the PRODUCTION parser
# uft_kf_decode() (src/flux/uft_kryoflux_stream.c) — every container
# defect class asserts the exact uft_kf_status_t the parser returns.
# Links the DTC HAL for the SSOT sample-clock cross-check (24.027 MHz);
# no subprocess is spawned at test runtime.
add_executable(test_kryoflux_emulator
    ${CMAKE_SOURCE_DIR}/tests/emulators/kryoflux/test_kryoflux_emulator.c
    ${CMAKE_SOURCE_DIR}/tests/emulators/kryoflux/firmware_state_machine.c
    ${CMAKE_SOURCE_DIR}/tests/flux_gen/kryoflux/flux_gen.c
    ${CMAKE_SOURCE_DIR}/src/flux/uft_kryoflux_stream.c
    ${CMAKE_SOURCE_DIR}/src/hal/uft_kryoflux_dtc.c)
target_include_directories(test_kryoflux_emulator PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/uft
    ${CMAKE_SOURCE_DIR}/tests/emulators/kryoflux
    ${CMAKE_SOURCE_DIR}/tests/flux_gen/kryoflux)
# No libm link (see the applesauce block above; MF-305).
set_target_properties(test_kryoflux_emulator PROPERTIES
    C_STANDARD 11
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests)
add_test(NAME test_kryoflux_emulator COMMAND test_kryoflux_emulator)
message(STATUS "UFT Test ENABLED: test_kryoflux_emulator (firmware-realistic KryoFlux sim)")

# FluxEngine firmware-realistic emulator (sixth invocation of the
# hardware-emulation-author agent). FluxEngine has no C HAL — the C++
# provider runs the `fluxengine` CLI, which on read writes an SCP file
# that UFT decodes with its own SCP parser. So this emulator models the
# fluxengine CLI + device and its flux generator emits a minimal REAL
# SCP file decoded by the PRODUCTION parser uft_scp_read_track_memory()
# (src/flux/uft_scp_parser.c) — every defect class asserts the exact
# uft_scp error code. No libm link (the test avoids fabs; MF-305).
add_executable(test_fluxengine_emulator
    ${CMAKE_SOURCE_DIR}/tests/emulators/fluxengine/test_fluxengine_emulator.c
    ${CMAKE_SOURCE_DIR}/tests/emulators/fluxengine/firmware_state_machine.c
    ${CMAKE_SOURCE_DIR}/tests/flux_gen/fluxengine/flux_gen.c
    ${CMAKE_SOURCE_DIR}/src/flux/uft_scp_parser.c)
target_include_directories(test_fluxengine_emulator PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/uft
    ${CMAKE_SOURCE_DIR}/tests/emulators/fluxengine
    ${CMAKE_SOURCE_DIR}/tests/flux_gen/fluxengine)
set_target_properties(test_fluxengine_emulator PROPERTIES
    C_STANDARD 11
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests)
add_test(NAME test_fluxengine_emulator COMMAND test_fluxengine_emulator)
message(STATUS "UFT Test ENABLED: test_fluxengine_emulator (firmware-realistic FluxEngine sim)")

# FC5025 firmware-realistic emulator (seventh invocation of the
# hardware-emulation-author agent). FC5025 is a SECTOR-only, read-only USB
# device (Device Side Data, VID:PID 0x16C0:0x06D6) — CBW/CSW protocol per
# the v1309 command-set spec. The generator produces decoded sector data
# with per-sector CRC status; the forensic core is Rule F-3 (a CRC-error
# sector preserves >= 2 divergent re-reads, never collapsed). Self-
# contained (no HAL/parser link needed); no libm (MF-305).
add_executable(test_fc5025_emulator
    ${CMAKE_SOURCE_DIR}/tests/emulators/fc5025/test_fc5025_emulator.c
    ${CMAKE_SOURCE_DIR}/tests/emulators/fc5025/firmware_state_machine.c
    ${CMAKE_SOURCE_DIR}/tests/flux_gen/fc5025/flux_gen.c)
target_include_directories(test_fc5025_emulator PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/uft
    ${CMAKE_SOURCE_DIR}/tests/emulators/fc5025
    ${CMAKE_SOURCE_DIR}/tests/flux_gen/fc5025)
set_target_properties(test_fc5025_emulator PROPERTIES
    C_STANDARD 11
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests)
add_test(NAME test_fc5025_emulator COMMAND test_fc5025_emulator)
message(STATUS "UFT Test ENABLED: test_fc5025_emulator (firmware-realistic FC5025 sim)")

# ADFCopy firmware-realistic emulator (eighth invocation of the
# hardware-emulation-author agent). ADFCopy is a Teensy-based (PJRC
# VID:PID 0x16C0:0x0483) USB-CDC serial controller for Amiga 3.5" drives.
# Unlike Applesauce's ASCII protocol it uses BINARY framing (1-byte cmd +
# payload, 'O'/'E'/'D' responses, status bitmask, CMD_READ_FLUX 3-byte
# header + flux). The generator emits Amiga-MFM flux at 40 MHz / 25 ns.
# Self-contained; no libm (MF-305).
add_executable(test_adfcopy_emulator
    ${CMAKE_SOURCE_DIR}/tests/emulators/adfcopy/test_adfcopy_emulator.c
    ${CMAKE_SOURCE_DIR}/tests/emulators/adfcopy/firmware_state_machine.c
    ${CMAKE_SOURCE_DIR}/tests/flux_gen/adfcopy/flux_gen.c)
target_include_directories(test_adfcopy_emulator PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/uft
    ${CMAKE_SOURCE_DIR}/tests/emulators/adfcopy
    ${CMAKE_SOURCE_DIR}/tests/flux_gen/adfcopy)
set_target_properties(test_adfcopy_emulator PROPERTIES
    C_STANDARD 11
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests)
add_test(NAME test_adfcopy_emulator COMMAND test_adfcopy_emulator)
message(STATUS "UFT Test ENABLED: test_adfcopy_emulator (firmware-realistic ADFCopy sim)")

# UFI / USBFloppy firmware-realistic emulator (ninth invocation of the
# hardware-emulation-author agent — the finale). UFI is the one
# controller whose C HAL (src/hal/ufi.c) exposes an INJECTABLE backend
# vtable (uft_ufi_ops_t). So the emulator installs a mock backend that
# answers SCSI CDBs from a synthetic disk and drives the PRODUCTION HAL
# functions (uft_ufi_inquiry / read_capacity / read_sectors /
# request_sense) end-to-end — the strongest coupling to production code
# of the nine. Links src/hal/ufi.c. No libm (MF-305).
add_executable(test_ufi_emulator
    ${CMAKE_SOURCE_DIR}/tests/emulators/ufi/test_ufi_emulator.c
    ${CMAKE_SOURCE_DIR}/tests/emulators/ufi/firmware_state_machine.c
    ${CMAKE_SOURCE_DIR}/tests/flux_gen/ufi/flux_gen.c
    ${CMAKE_SOURCE_DIR}/src/hal/ufi.c)
target_include_directories(test_ufi_emulator PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/uft
    ${CMAKE_SOURCE_DIR}/tests/emulators/ufi
    ${CMAKE_SOURCE_DIR}/tests/flux_gen/ufi)
set_target_properties(test_ufi_emulator PROPERTIES
    C_STANDARD 11
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests)
add_test(NAME test_ufi_emulator COMMAND test_ufi_emulator)
message(STATUS "UFT Test ENABLED: test_ufi_emulator (firmware-realistic UFI sim — 9/9 finale)")

# ═══════════════════════════════════════════════════════════════════════════════
# Qt-based Tests (C++ / QtTest framework)
# ═══════════════════════════════════════════════════════════════════════════════
find_package(Qt6 COMPONENTS Core Test Widgets QUIET)
if(TARGET Qt6::Test)
    set(CMAKE_AUTOMOC ON)

    # MF-220/MF-221: HardwareTab-driving Qt tests (test_hardware_tab_gui,
    # test_provider_switch) all link the same surface — HardwareTab + the
    # codegen wiring + all 9 V2 providers + their C backends. Factored
    # here so the two per-test blocks below stay in sync.
    set(_HARDWARETAB_TEST_SOURCES
        ${CMAKE_SOURCE_DIR}/src/hardwaretab.cpp
        ${CMAKE_SOURCE_DIR}/forms/tab_hardware.ui
        ${CMAKE_SOURCE_DIR}/generated/tab_hardware_wiring.gen.cpp
        ${CMAKE_SOURCE_DIR}/src/hardware_providers/greaseweazle_provider_v2.cpp
        ${CMAKE_SOURCE_DIR}/src/hal/uft_greaseweazle_full.c
        ${CMAKE_SOURCE_DIR}/src/hardware_providers/scp_provider_v2.cpp
        ${CMAKE_SOURCE_DIR}/src/hal/uft_scp_direct.c
        ${CMAKE_SOURCE_DIR}/src/hardware_providers/kryoflux_provider_v2.cpp
        ${CMAKE_SOURCE_DIR}/src/flux/uft_kryoflux_stream.c
        ${CMAKE_SOURCE_DIR}/src/hardware_providers/fluxengine_provider_v2.cpp
        ${CMAKE_SOURCE_DIR}/src/flux/uft_scp_parser.c
        ${CMAKE_SOURCE_DIR}/src/hardware_providers/fc5025_provider_v2.cpp
        ${CMAKE_SOURCE_DIR}/src/hardware_providers/xum1541_provider_v2.cpp
        ${CMAKE_SOURCE_DIR}/src/hardware_providers/applesauce_provider_v2.cpp
        ${CMAKE_SOURCE_DIR}/src/hardware_providers/adfcopy_provider_v2.cpp
        ${CMAKE_SOURCE_DIR}/src/hardware_providers/usbfloppy_provider_v2.cpp
        ${CMAKE_SOURCE_DIR}/src/hardware_providers/teensy_probe.cpp
        ${CMAKE_SOURCE_DIR}/src/hardware_providers/qprocess_subprocess_runner.cpp
        ${CMAKE_SOURCE_DIR}/src/hardware_providers/ufi_runners.cpp
        ${CMAKE_SOURCE_DIR}/src/hal/ufi.c
        ${CMAKE_SOURCE_DIR}/src/hal/ufi_backend.c
        # CI-1: ufi_backend.c references uft_ufi_linux_ops() under
        # `#if __linux__`; that symbol lives in ufi_linux.c. Without it the
        # Linux link fails (undefined reference) and both HardwareTab tests
        # became ctest "Not Run". ufi_linux.c is wholly `#ifdef __linux__`,
        # so on Windows/macOS it compiles to an empty object — safe to add
        # unconditionally.
        ${CMAKE_SOURCE_DIR}/src/hal/ufi_linux.c)

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

        # MF-220 / MF-221 (P3.3 #110): the HardwareTab-driving Qt tests
        # link HardwareTab + the codegen wiring + all 9 V2 providers +
        # their C backends (the _HARDWARETAB_TEST_SOURCES set above),
        # and need AUTOUIC for forms/tab_hardware.ui.
        if(${qt_test_name} STREQUAL "test_hardware_tab_gui" OR
           ${qt_test_name} STREQUAL "test_provider_switch")
            set_target_properties(${qt_test_name} PROPERTIES AUTOUIC ON)
            target_sources(${qt_test_name} PRIVATE
                ${_HARDWARETAB_TEST_SOURCES})
            target_include_directories(${qt_test_name} PRIVATE
                ${CMAKE_SOURCE_DIR}/src/hardware_providers)
        endif()

        # MF-169 (P1.17): the V1 `hardware_providers` cmake target was
        # deleted with the V1 provider hierarchy. Qt-side tests now
        # link only against Qt6::Core+Test+Widgets+SerialPort. The V2
        # provider tests live in `_HEADER_ONLY_CPP_TESTS` and pull
        # their own per-test target_sources for the specific V2 cpp.
        if(TARGET Qt6::SerialPort AND NOT ${qt_test_name} STREQUAL "test_wiring_runtime")
            target_link_libraries(${qt_test_name} PRIVATE Qt6::SerialPort)
        endif()
        # MF-254: HardwareTab tests pull uft_scp_direct.c — link
        # libusb when available so the SCP libusb path resolves.
        if(LIBUSB_FOUND)
            target_link_libraries(${qt_test_name} PRIVATE uft::libusb)
        endif()
        # MF-155 (P1.3): test_wiring_runtime exercises QPushButton + click,
        # so it needs Qt6::Widgets. Link unconditionally if available — costs
        # nothing for the few existing Qt-Test targets that ignore Widgets.
        if(TARGET Qt6::Widgets)
            target_link_libraries(${qt_test_name} PRIVATE Qt6::Widgets)
        endif()

        # CXX_STANDARD: most existing Qt-tests are C++17. test_wiring_runtime,
        # test_hardware_tab_gui and test_provider_switch pull in the C++20
        # type-driven HAL (concepts, std::visit, generic-lambda template
        # params), so they must explicitly opt in to C++20.
        if(${qt_test_name} STREQUAL "test_wiring_runtime" OR
           ${qt_test_name} STREQUAL "test_hardware_tab_gui" OR
           ${qt_test_name} STREQUAL "test_provider_switch")
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
        # MF-220 / MF-221: the HardwareTab-driving Qt tests construct
        # real Qt widgets — run them headless so ctest needs no display.
        # On Windows the Qt runtime DLLs are not on PATH by default, so
        # ctest needs both QT_QPA_PLATFORM=offscreen AND the Qt bin dir
        # added to PATH (UFT-T06, v4.1.5-hardening). Derived from the
        # active Qt6::Core target so it picks up whatever Qt the build
        # was configured against.
        if(WIN32)
            get_target_property(_qt_core_loc Qt6::Core LOCATION)
            get_filename_component(_qt_bin_dir "${_qt_core_loc}" DIRECTORY)
            # MF-254: build the PATH value with escaped semicolons.
            # CMake interprets the ENVIRONMENT property as a list with
            # ';' separating env-var assignments, so the internal ';'
            # in a Windows PATH value MUST be escaped as '\;'.
            set(_uft_test_path "${_qt_bin_dir}")
            if(DEFINED UFT_LIBUSB_DLL_DIR)
                set(_uft_test_path "${_qt_bin_dir}\\;${UFT_LIBUSB_DLL_DIR}")
            endif()
            set_tests_properties(${qt_test_name} PROPERTIES
                ENVIRONMENT "QT_QPA_PLATFORM=offscreen;PATH=${_uft_test_path}")
        elseif(${qt_test_name} STREQUAL "test_hardware_tab_gui" OR
               ${qt_test_name} STREQUAL "test_provider_switch" OR
               ${qt_test_name} STREQUAL "test_wiring_runtime")
            # CI-1: test_wiring_runtime constructs QPushButton, so on a
            # headless Linux runner it must also run offscreen — otherwise
            # Qt tries the xcb plugin, fails to init, and abort()s
            # ("Subprocess aborted"). Windows already gets offscreen via
            # the WIN32 branch above.
            set_tests_properties(${qt_test_name} PROPERTIES
                ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
        endif()
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

# ═══════════════════════════════════════════════════════════════════════════════
# Oracle-Registry (MF-499)
#
# Seit MF-498 braucht neuer Code im Format-/Decoder-Layer eine BENANNTE
# Referenz. tests/differential/oracles.py ist die Stelle, an der die
# benannten Referenzen stehen — und sie laeuft hier mit, damit sie nicht das
# naechste Stueck wird, das gebaut, geprueft und nie aufgerufen wird.
#
# Der Test verlangt KEIN installiertes Fremdwerkzeug: er prueft die Registry
# auf Widersprueche und die Aufloesungs-Logik gegen den laufenden
# Python-Interpreter. Ohne pytest, damit ihn jede CI ausfuehren kann.
# ═══════════════════════════════════════════════════════════════════════════════
if(Python3_Interpreter_FOUND)
    add_test(
        NAME oracle_registry
        COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_SOURCE_DIR}/tests/differential/test_oracles.py
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    set_tests_properties(oracle_registry PROPERTIES
        LABELS "ssot-compliance"
    )
    message(STATUS "UFT Oracle Registry ENABLED: oracle_registry")
else()
    message(STATUS "UFT Oracle Registry SKIPPED (Python3 not found)")
endif()

# ════════════════════════════════════════════════════════════════════════════
# Micro-benchmarks (MF-435) — OFF by default.
#
# Deliberately NOT registered with add_test(): a benchmark measures, it does
# not assert, and a number that moves with the machine has no business
# reddening CI. See tests/benchmarks/README.md for the baseline and for why
# neither decode hotpath is worth optimising.
# ════════════════════════════════════════════════════════════════════════════
option(UFT_ENABLE_BENCHMARKS "Build the micro-benchmarks (not run by ctest)" OFF)

if(UFT_ENABLE_BENCHMARKS)
    add_executable(bench_decode_hotpath
        ${CMAKE_CURRENT_SOURCE_DIR}/benchmarks/bench_decode_hotpath.c
        ${CMAKE_SOURCE_DIR}/src/flux/uft_flux_decoder.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_flux_histogram.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_flux_sync_search.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_dewarp.c
            ${CMAKE_SOURCE_DIR}/src/flux/uft_media_profile.c)
    target_include_directories(bench_decode_hotpath PRIVATE
        ${CMAKE_SOURCE_DIR}/include)
    # -O3 regardless of build type: a baseline from a debug build is noise.
    target_compile_options(bench_decode_hotpath PRIVATE -O3)
    target_compile_definitions(bench_decode_hotpath PRIVATE NDEBUG)
    set_target_properties(bench_decode_hotpath PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/benchmarks)
    message(STATUS "UFT Benchmarks ENABLED: bench_decode_hotpath")
endif()

message(STATUS "════════════════════════════════════════════════════")
message(STATUS "UFT Tests: Run 'ctest' after build to execute")
message(STATUS "════════════════════════════════════════════════════")
