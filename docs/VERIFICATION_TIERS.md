# Format-Verifikations-Stufen (generiert)

**NICHT von Hand editieren** — erzeugt von `scripts/gen_verification_tiers.py` (MF-364). Definitionen: [`VERIFICATION_PLAN.md`](VERIFICATION_PLAN.md).

Ein T3 mit Test-Eintrag bedeutet: es existiert ein synthetischer Test, aber die Byte-Struktur wurde nie gegen eine autoritative externe Quelle verifiziert — genau die Konstellation, in der die fabrizierten Parser (FMT-2/3/10/11/12) gruen waren.

## Zusammenfassung

| Stufe | Formate |
|---|---|
| T1 | 2 |
| T1b | 7 |
| T2 | 11 |
| T3 | 68 |
| **gesamt** | **88** |

## Pro Format

| Plugin | Stufe | Tests | Spec-Quelle | Evidenz | Korpus-Images |
|---|---|---|---|---|---|
| `fdi` | **T1** | `test_corpus_fdi`, `test_fdi_spectrum` | SAMdisk ReadFDI (src/samdisk/fdi.cpp, in-tree) + WoS format FAQ | MF-359 | 1 |
| `g64` | **T1** | `test_c64_metrics_corpus`, `test_c64_protection_real_corpus`, `test_plugin_probe_real` | — | — | 3 |
| `adf` | **T1b** | `test_adf_write_roundtrip`, `test_corpus_adf` | — | — | 1 |
| `atr` | **T1b** | `test_atr_512`, `test_atr_write_roundtrip`, `test_corpus_atr`, `test_plugin_probe_real` | — | — | 1 |
| `d64` | **T1b** | `test_corpus_d64`, `test_d64_42track`, `test_d64_errormap`, `test_d64_geometry_zones`, `test_d64_write_roundtrip`, `test_plugin_probe_real` | VICE D64 sizes incl. error-block trailer + 40/42-track variants | MF-333, MF-350 | 1 |
| `d71` | **T1b** | `test_corpus_d71`, `test_d71_write_roundtrip`, `test_plugin_probe_real` | — | — | 1 |
| `d81` | **T1b** | `test_corpus_d81`, `test_d81_write_roundtrip`, `test_plugin_probe_real` | — | — | 1 |
| `hfe` | **T1b** | `test_corpus_hfe`, `test_hfe_v3_weak`, `test_plugin_probe_real` | HxC hfev3_loader.c opcode semantics (v3 decode + RAND weak bits); HxC HFE docs (v1) | MF-354, MF-362 | 1 |
| `scp` | **T1b** | `test_corpus_scp`, `test_protection_probe`, `test_scp_footer_roundtrip`, `test_scp_weakbit_multirev`, `test_scp_writer_roundtrip` | cbmstuff SCP image spec (48-byte FPCS footer, bitcell track length) | MF-318, MF-351 | 1 |
| `adf_ext` | **T2** | `test_adf_ext_plugin` | WinUAE disk.cpp read_header_ext2 (UAE-1ADF) | MF-352 | — |
| `akai_s900` | **T2** | `test_akai_s900_plugin` | akaiutil (kmi9000) geometry: DD 5x1024/819200, HD 10x1024/1638400 | MF-348 | — |
| `d88` | **T2** | `test_d88_error_marks`, `test_plugin_probe_real` | pc98.org D88 + MAME d88_dsk (DDAM @+07, FDC status @+08) | MF-336 | — |
| `dc42` | **T2** | `test_dc42_checksum_roundtrip`, `test_plugin_probe_real` | DiscFerret/Mini-vMac DC42 checksum (BE16 word add, ROR32 1) | MF-324 | — |
| `dmk` | **T2** | `test_dmk_crc` | David Keil DMK spec (openMSX DMK-Format-Details) + WD177x CRC-CCITT pinned to check value 0x29B1 | MF-353 | — |
| `dsk_cpc` | **T2** | `test_edsk_error_marks` | EDSK uPD765 ST1/ST2 status-bit semantics (bit5 CRC, ST2 bit6 deleted); MF-332 verified the dsk_cpc implementation (the separate 'edsk' plugin in amstrad/ remains untested) | MF-332 | — |
| `korg_dss1` | **T2** | `test_korg_dss1_plugin` | chickensys Korg DSS-1 geometry (80x2x5x1024) | MF-347 | — |
| `lisa_twiggy` | **T2** | `test_lisa_twiggy_plugin` | bitsavers Lisa Twiggy ZCAV zone table (46 tracks/side, 22..15 spt) | MF-349 | — |
| `nfd` | **T2** | `test_nfd_r0` | pc98.org nfdr0/nfdr1 + tomari/d88split nfd2mhlt.pl (r1 skip accounting spec-only, no real r1 corpus yet) | MF-358, MF-360 | — |
| `stx` | **T2** | `test_stx_error_marks` | Pasti STX descriptor spec (atari.8bitchip.info/STXdesc) | MF-335 | — |
| `woz` | **T2** | `test_diskcopy`, `test_moof_roundtrip`, `test_plugin_probe_real`, `test_woz_roundtrip` | Applesauce WOZ reference v1/v2/2.1 (chunk layout, CRC32, WRIT logical refs) | MF-317, MF-357, MF-361 | — |
| `2img` | **T3** | — | — | — | — |
| `86f` | **T3** | — | — | — | — |
| `adf_arc` | **T3** | — | — | — | — |
| `adl` | **T3** | — | — | — | — |
| `apridisk` | **T3** | — | — | — | — |
| `atx` | **T3** | `test_plugin_probe_real` | — | — | — |
| `cas` | **T3** | — | — | — | — |
| `cfi` | **T3** | — | — | — | — |
| `cpm` | **T3** | `test_cpm_fs` | — | — | — |
| `cqm` | **T3** | `test_plugin_probe_real` | — | — | — |
| `d13` | **T3** | — | — | — | — |
| `d67` | **T3** | — | VICE/Schepers D67 690-block geometry (176640 bytes) | MF-314 | — |
| `d77` | **T3** | — | — | — | — |
| `d80` | **T3** | `test_d80_write_roundtrip` | — | — | — |
| `d82` | **T3** | `test_d82_write_roundtrip` | — | — | — |
| `dcm` | **T3** | — | — | — | — |
| `dim` | **T3** | — | — | — | — |
| `dim_atari` | **T3** | — | — | — | — |
| `dms` | **T3** | `test_uft_dms` | — | — | — |
| `do` | **T3** | `test_do_write_roundtrip`, `test_plugin_probe_real` | — | — | — |
| `edk` | **T3** | — | — | — | — |
| `edsk` | **T3** | `test_plugin_probe_real` | — | — | — |
| `fdi_pc98` | **T3** | — | — | — | — |
| `fds` | **T3** | — | — | — | — |
| `g71` | **T3** | `test_g71_read` | — | — | — |
| `hardsector` | **T3** | — | — | — | — |
| `imd` | **T3** | `test_imd_error_marks`, `test_imd_write_roundtrip`, `test_plugin_probe_real` | — | — | — |
| `img` | **T3** | `test_img_write_roundtrip`, `test_plugin_probe_real` | — | — | — |
| `ipf` | **T3** | `test_ipf_air_accessors` | — | — | — |
| `jv1` | **T3** | `test_plugin_probe_real` | — | — | — |
| `jv3` | **T3** | — | — | — | — |
| `jvc` | **T3** | — | — | — | — |
| `kfx` | **T3** | — | — | — | — |
| `logical` | **T3** | — | — | — | — |
| `mfi` | **T3** | — | — | — | — |
| `mgt` | **T3** | — | — | — | — |
| `micropolis` | **T3** | — | — | — | — |
| `msa` | **T3** | `test_msa`, `test_plugin_probe_real` | — | — | — |
| `msx_disk` | **T3** | — | — | — | — |
| `myz80` | **T3** | — | — | — | — |
| `nanowasp` | **T3** | — | — | — | — |
| `nib` | **T3** | `test_plugin_probe_real` | — | — | — |
| `northstar` | **T3** | — | — | — | — |
| `opus` | **T3** | — | — | — | — |
| `pdp` | **T3** | — | — | — | — |
| `po` | **T3** | `test_plugin_probe_real`, `test_po_write_roundtrip` | — | — | — |
| `posix` | **T3** | — | — | — | — |
| `pri` | **T3** | — | — | — | — |
| `pro` | **T3** | `test_atari`, `test_floppy_formats`, `test_st_plugin` | — | — | — |
| `qrst` | **T3** | — | — | — | — |
| `rcpmfs` | **T3** | — | — | — | — |
| `sad` | **T3** | — | — | — | — |
| `sam` | **T3** | — | — | — | — |
| `sap_thomson` | **T3** | — | — | — | — |
| `scl` | **T3** | — | — | — | — |
| `ssd` | **T3** | — | — | — | — |
| `st` | **T3** | `test_st_write_roundtrip` | — | — | — |
| `syn` | **T3** | — | — | — | — |
| `t1k` | **T3** | — | — | — | — |
| `tan` | **T3** | — | — | — | — |
| `td0` | **T3** | `test_plugin_probe_real`, `test_td0_error_marks` | — | — | — |
| `trd` | **T3** | — | — | — | — |
| `udi` | **T3** | — | — | — | — |
| `v9t9` | **T3** | — | — | — | — |
| `vdk` | **T3** | — | — | — | — |
| `victor9k` | **T3** | — | — | — | — |
| `xdm86` | **T3** | — | — | — | — |
| `xfd` | **T3** | `test_plugin_probe_real` | — | — | — |
