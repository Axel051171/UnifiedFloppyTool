# Planned APIs (MF-011 DOCUMENT+IMPLEMENT-Wellen)

**Stand:** auto-generated, 151 headers · 2939 unimplemented declarations

Every header listed below declares public `uft_*` functions that are promised by the API surface but have no implementation in `src/`. DOCUMENT-bucket files have a `/* PLANNED FEATURE */` banner (zero impls); IMPLEMENT-bucket files have a `/* PARTIALLY IMPLEMENTED */` banner (some impls, some stubs). Consumers are warned before adding new call sites. Implementation belongs to M2/M3 depending on subsystem.

## DOCUMENT/Batch processing

| decls | missing | consumers | header |
|------:|--------:|----------:|--------|
| 30 | 30 | 2 | `include/uft/batch/uft_batch.h` |

## DOCUMENT/CRC engines

| decls | missing | consumers | header |
|------:|--------:|----------:|--------|
| 10 | 10 | 1 | `include/uft/crc/uft_crc_extended.h` |

## DOCUMENT/Core infrastructure

| decls | missing | consumers | header |
|------:|--------:|----------:|--------|
| 36 | 36 | 1 | `include/uft/core/uft_json_export.h` |
| 21 | 21 | 7 | `include/uft/core/uft_track_base.h` |
| 15 | 15 | 3 | `include/uft/core/uft_crc_unified.h` |
| 15 | 15 | 1 | `include/uft/core/uft_histogram.h` |
| 14 | 14 | 1 | `include/uft/core/uft_limits.h` |
| 11 | 11 | 7 | `include/uft/core/uft_encoding.h` |

## DOCUMENT/Decoder pipeline

| decls | missing | consumers | header |
|------:|--------:|----------:|--------|
| 16 | 16 | 1 | `include/uft/decoder/uft_forensic_flux_decoder.h` |
| 14 | 14 | 1 | `include/uft/decoder/uft_gcr.h` |
| 10 | 10 | 7 | `include/uft/decoder/uft_pll.h` |

## DOCUMENT/Filesystem layer

| decls | missing | consumers | header |
|------:|--------:|----------:|--------|
| 47 | 47 | 3 | `include/uft/fs/uft_trsdos.h` |
| 43 | 43 | 3 | `include/uft/fs/uft_bbc_dfs.h` |
| 36 | 36 | 2 | `include/uft/fs/uft_ti99_fs.h` |
| 34 | 34 | 6 | `include/uft/fs/uft_apple_dos.h` |
| 24 | 24 | 1 | `include/uft/fs/uft_fbasic_fs.h` |
| 23 | 23 | 3 | `include/uft/fs/uft_fat_badblock.h` |
| 18 | 18 | 3 | `include/uft/fs/uft_fat_atari.h` |
| 12 | 12 | 4 | `include/uft/fs/uft_fat_boot.h` |

## DOCUMENT/Flux analysis

| decls | missing | consumers | header |
|------:|--------:|----------:|--------|
| 19 | 19 | 1 | `include/uft/flux/uft_fluxstat.h` |
| 16 | 16 | 3 | `include/uft/flux/uft_adaptive_decoder.h` |
| 12 | 12 | 1 | `include/uft/flux/uft_pattern_generator.h` |
| 12 | 12 | 1 | `include/uft/flux/uft_sector_overlay.h` |
| 10 | 10 | 1 | `include/uft/flux/uft_flux_instability.h` |

## DOCUMENT/Format plugins

| decls | missing | consumers | header |
|------:|--------:|----------:|--------|
| 24 | 24 | 5 | `include/uft/formats/uft_pce.h` |
| 15 | 15 | 1 | `include/uft/formats/uft_tc.h` |
| 13 | 13 | 2 | `include/uft/formats/uft_dfi.h` |
| 10 | 10 | 1 | `include/uft/formats/uft_x68k.h` |

## DOCUMENT/HAL internals

| decls | missing | consumers | header |
|------:|--------:|----------:|--------|
| 27 | 27 | 1 | `include/uft/hal/internal/uft_nibtools.h` |
| 21 | 21 | 1 | `include/uft/hal/internal/uft_pauline.h` |
| 19 | 19 | 1 | `include/uft/hal/internal/uft_linux_fdc.h` |
| 18 | 18 | 1 | `include/uft/hal/internal/uft_zoomtape.h` |
| 17 | 17 | 1 | `include/uft/hal/internal/uft_ieee488.h` |
| 17 | 17 | 1 | `include/uft/hal/internal/uft_xa1541.h` |

## DOCUMENT/Hardware abstraction

| decls | missing | consumers | header |
|------:|--------:|----------:|--------|
| 26 | 26 | 3 | `include/uft/hal/uft_xum1541.h` |
| 20 | 20 | 3 | `include/uft/hal/uft_applesauce.h` |
| 20 | 20 | 1 | `include/uft/hal/uft_fc5025.h` |
| 20 | 20 | 1 | `include/uft/hal/uft_latency_tracking.h` |

## DOCUMENT/Label OCR

| decls | missing | consumers | header |
|------:|--------:|----------:|--------|
| 27 | 27 | 1 | `include/uft/ocr/uft_ocr.h` |

## DOCUMENT/Machine-learning decode

| decls | missing | consumers | header |
|------:|--------:|----------:|--------|
| 34 | 34 | 2 | `include/uft/ml/uft_ml_training_gen.h` |
| 22 | 22 | 7 | `include/uft/ml/uft_ml_decoder.h` |

## DOCUMENT/Parameter registry

| decls | missing | consumers | header |
|------:|--------:|----------:|--------|
| 24 | 24 | 4 | `include/uft/params/uft_canonical_params.h` |

## DOCUMENT/Recovery pipeline

| decls | missing | consumers | header |
|------:|--------:|----------:|--------|
| 16 | 16 | 7 | `include/uft/recovery/uft_recovery.h` |

## DOCUMENT/Root-level API

| decls | missing | consumers | header |
|------:|--------:|----------:|--------|
| 37 | 37 | 1 | `include/uft/uft_hardware_mock.h` |
| 32 | 32 | 3 | `include/uft/uft_apd_format.h` |
| 31 | 31 | 1 | `include/uft/uft_forensic_imaging.h` |
| 30 | 30 | 1 | `include/uft/uft_algorithms.h` |
| 30 | 30 | 3 | `include/uft/uft_sos_protection.h` |
| 28 | 28 | 6 | `include/uft/uft_param_bridge.h` |
| 27 | 27 | 3 | `include/uft/uft_image_db.h` |
| 27 | 27 | 1 | `include/uft/uft_multirev.h` |
| 27 | 27 | 1 | `include/uft/uft_multi_decoder.h` |
| 26 | 26 | 1 | `include/uft/uft_bit_confidence.h` |
| 26 | 26 | 1 | `include/uft/uft_config_manager.h` |
| 26 | 26 | 1 | `include/uft/uft_session.h` |
| 26 | 26 | 2 | `include/uft/uft_vfs.h` |
| 24 | 24 | 1 | `include/uft/uft_blkid.h` |
| 24 | 24 | 1 | `include/uft/uft_error_chain.h` |
| 24 | 24 | 2 | `include/uft/uft_writer_verify.h` |
| 24 | 24 | 1 | `include/uft/uft_write_transaction.h` |
| 23 | 23 | 1 | `include/uft/uft_multi_decode.h` |
| 23 | 23 | 1 | `include/uft/uft_recovery_advanced.h` |
| 22 | 22 | 1 | `include/uft/uft_audit_trail.h` |
| 22 | 22 | 1 | `include/uft/uft_gui_params_extended.h` |
| 22 | 22 | 1 | `include/uft/samdisk/uft_samdisk.h` |
| 20 | 20 | 3 | `include/uft/uft_pll_unified.h` |
| 20 | 20 | 1 | `include/uft/web/uft_web_viewer.h` |
| 19 | 19 | 2 | `include/uft/uft_flux_statistics.h` |
| 18 | 18 | 1 | `include/uft/uft_bitstream_preserve.h` |
| 18 | 18 | 1 | `include/uft/uft_mmap.h` |
| 18 | 18 | 19 | `include/uft/uft_unified_image.h` |
| 18 | 18 | 2 | `include/uft/uft_writer_backend.h` |
| 17 | 17 | 3 | `include/uft/uft_gui_bridge.h` |
| 17 | 17 | 3 | `include/uft/uft_settings.h` |
| 17 | 17 | 1 | `include/uft/hardware/uft_gw2dmk.h` |
| 16 | 16 | 3 | `include/uft/uft_capability.h` |
| 16 | 16 | 1 | `include/uft/uft_gui_params.h` |
| 16 | 16 | 1 | `include/uft/hardware/uft_crosstalk_filter.h` |
| 15 | 15 | 16 | `include/uft/uft_memory.h` |
| 15 | 15 | 3 | `include/uft/uft_process.h` |
| 15 | 15 | 1 | `include/uft/uft_write_preview.h` |
| 15 | 15 | 2 | `include/uft/track/uft_track_generator.h` |
| 15 | 15 | 1 | `include/uft/track/uft_track_layout.h` |
| 14 | 14 | 3 | `include/uft/uft_crc_cache.h` |
| 14 | 14 | 6 | `include/uft/uft_device_manager.h` |
| 14 | 14 | 1 | `include/uft/uft_usb.h` |
| 13 | 13 | 1 | `include/uft/uft_forensic_report.h` |
| 13 | 13 | 1 | `include/uft/ir/uft_ir_timing.h` |
| 12 | 12 | 1 | `include/uft/hardware/uft_latency_tracking.h` |
| 12 | 12 | 1 | `include/uft/hardware/uft_write_precomp.h` |
| 12 | 12 | 2 | `include/uft/track/uft_sector_extractor.h` |
| 11 | 11 | 4 | `include/uft/uft_apple_gcr.h` |
| 11 | 11 | 1 | `include/uft/uft_cache.h` |
| 11 | 11 | 1 | `include/uft/uft_test_framework.h` |
| 11 | 11 | 12 | `include/uft/uft_tool_adapter.h` |
| 10 | 10 | 3 | `include/uft/uft_format_verify.h` |
| 10 | 10 | 1 | `include/uft/uft_xdf.h` |
| 10 | 10 | 2 | `include/uft/generate/uft_track_generator.h` |

## IMPLEMENT/Core infrastructure

| decls | missing | consumers | header |
|------:|--------:|----------:|--------|
| 23 | 22 | 6 | `include/uft/core/uft_integration.h` |
| 19 | 18 | 10 | `include/uft/core/uft_sector.h` |
| 22 | 18 | 1 | `include/uft/core/uft_logging_v2.h` |
| 17 | 15 | 7 | `include/uft/core/uft_disk.h` |
| 16 | 14 | 6 | `include/uft/core/uft_format_registry.h` |
| 12 | 11 | 1 | `include/uft/core/uft_crc_v2.h` |

## IMPLEMENT/Decoder pipeline

| decls | missing | consumers | header |
|------:|--------:|----------:|--------|
| 21 | 18 | 1 | `include/uft/decoder/uft_format_hints.h` |
| 16 | 15 | 1 | `include/uft/decoder/uft_cell_analyzer.h` |
| 15 | 13 | 1 | `include/uft/decoder/uft_sector_confidence.h` |
| 12 | 10 | 1 | `include/uft/decoder/uft_unified_decoder.h` |
| 11 | 9 | 2 | `include/uft/decoder/uft_fusion.h` |

## IMPLEMENT/Filesystem layer

| decls | missing | consumers | header |
|------:|--------:|----------:|--------|
| 54 | 53 | 3 | `include/uft/fs/uft_cbm_fs.h` |
| 51 | 50 | 1 | `include/uft/fs/uft_cpm_fs.h` |
| 42 | 38 | 5 | `include/uft/fs/uft_atari_dos.h` |
| 22 | 20 | 1 | `include/uft/fs/uft_bbc_fs.h` |
| 20 | 17 | 3 | `include/uft/fs/uft_fat32.h` |

## IMPLEMENT/Flux analysis

| decls | missing | consumers | header |
|------:|--------:|----------:|--------|
| 16 | 15 | 2 | `include/uft/flux/uft_flux_pll_v20.h` |
| 16 | 15 | 1 | `include/uft/flux/uft_woz_parser.h` |
| 15 | 14 | 3 | `include/uft/flux/uft_pll_pi.h` |
| 15 | 14 | 3 | `include/uft/flux/pll/uft_pll_pi.h` |
| 11 | 10 | 1 | `include/uft/flux/uft_libflux_pll_enhanced.h` |
| 11 | 10 | 3 | `include/uft/flux/uft_pll_params.h` |
| 10 | 9 | 3 | `include/uft/flux/uft_mfm_flux.h` |

## IMPLEMENT/Format plugins

| decls | missing | consumers | header |
|------:|--------:|----------:|--------|
| 21 | 19 | 2 | `include/uft/formats/ipf/uft_ipf.h` |
| 23 | 19 | 18 | `include/uft/formats/uft_fat12.h` |
| 19 | 18 | 3 | `include/uft/formats/uft_86f.h` |
| 16 | 15 | 3 | `include/uft/formats/uft_dmk.h` |
| 15 | 12 | 1 | `include/uft/formats/uft_jv3.h` |
| 12 | 11 | 3 | `include/uft/formats/uft_woz.h` |

## IMPLEMENT/Hardware abstraction

| decls | missing | consumers | header |
|------:|--------:|----------:|--------|
| 33 | 28 | 3 | `include/uft/hal/uft_supercard.h` |
| 23 | 19 | 3 | `include/uft/hal/uft_hal_v3.h` |

## IMPLEMENT/RIDE integration

| decls | missing | consumers | header |
|------:|--------:|----------:|--------|
| 62 | 57 | 15 | `include/uft/ride/uft_flux_decoder.h` |
| 24 | 23 | 1 | `include/uft/ride/uft_dos_recognition.h` |

## IMPLEMENT/Root-level API

| decls | missing | consumers | header |
|------:|--------:|----------:|--------|
| 43 | 36 | 15 | `include/uft/uft_hardware.h` |
| 32 | 29 | 1 | `include/uft/uft_public_api.h` |
| 32 | 27 | 40 | `include/uft/uft_platform.h` |
| 24 | 22 | 2 | `include/uft/uft_crc_polys.h` |
| 20 | 19 | 1 | `include/uft/ir/uft_ir_serialize.h` |
| 21 | 18 | 3 | `include/uft/c64/uft_cbm_disk.h` |
| 22 | 18 | 6 | `include/uft/uft_integration.h` |
| 18 | 17 | 1 | `include/uft/uft_parallel.h` |
| 18 | 17 | 1 | `include/uft/uft_protection_pipeline.h` |
| 17 | 16 | 4 | `include/uft/uft_write_verify.h` |
| 16 | 15 | 3 | `include/uft/uft_dmk.h` |
| 15 | 14 | 3 | `include/uft/uft_applesauce.h` |
| 14 | 13 | 3 | `include/uft/uft_operation_result.h` |
| 15 | 13 | 9 | `include/uft/uft_decoder_plugin.h` |
| 16 | 13 | 11 | `include/uft/uft_format_detect.h` |
| 13 | 12 | 20 | `include/uft/uft_simd.h` |
| 14 | 12 | 1 | `include/uft/floppy/uft_floppy_v2.h` |
| 11 | 10 | 3 | `include/uft/uft_adaptive_decoder.h` |
| 11 | 9 | 1 | `include/uft/uft_io_abstraction.h` |
| 10 | 8 | 1 | `include/uft/uft_operations.h` |

