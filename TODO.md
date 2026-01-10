# UFT TODO - v3.7.0

## ✅ PHASE 1: Parser Cleanup COMPLETE (v3.7.0)
- [x] 369 nicht-kompilierte Format-Verzeichnisse archiviert
  - 61 Nicht-Floppy Formate (multimedia, gaming, archives)
  - 308 sonstige nicht-Floppy Formate
  - Echte Floppy-Stubs behalten (adf, atr, d64, etc.)
- [x] 34 v2 Parser archiviert (ersetzt durch v3)
- [x] 171 samdisk Legacy-Dateien archiviert (C++ nicht integriert)
- [x] 50 Floppy-relevante Format-Verzeichnisse behalten

## ✅ PHASE 1: Coupling Fixes COMPLETE
- [x] Score-Type Konsolidierung
  - Created: include/uft/core/uft_score.h
  - Unified uft_format_score_t replaces 55+ local *_score_t definitions
  - Score weights, thresholds, and format IDs standardized
  - Audit trail with match history
- [x] Error-Code Aliase erweitert
  - Added: UFT_ERR_INVALID_PARAM, UFT_ERR_ALREADY_EXISTS, UFT_ERR_OVERFLOW, UFT_ERR_NOMEM

## ✅ PHASE 2: XDF-API Booster COMPLETE
- [x] XDF Adapter Interface
  - Created: include/uft/xdf/uft_xdf_adapter.h
  - Created: src/xdf/uft_xdf_adapter.c
  - Format adapter registration system
  - Universal track/sector data containers
  - Plugin-style format integration
- [x] XDF Adapter Tests
  - Created: tests/xdf/test_xdf_adapter.c
  - 11 tests for Score and Adapter functionality

## ✅ P1 COMPLETE (6/6)
- [x] P1-1: Smoke Test (version, workflow)
- [x] P1-2: XDF API Fix (uft_xdf_format_info)
- [x] P1-3: CI Macro Guard (UFT_LINK_MATH)
- [x] P1-4: Track Consolidation (77 sub-tests)
- [x] P1-5: FAT Detection (28 sub-tests)
- [x] P1-6: XCopy Integration (53 profiles)

## ✅ P2 COMPLETE (4/4)
- [x] P2-15: Registry Consolidation
- [x] P2-16: HAL Separation
- [x] P2-17: HxC Cleanup
- [x] P2-18: SCL Format (16 tests, TRX Drive)

## ✅ P3 SESSION (3/3)
- [x] P3-Z80: Z80 Disassembler (53 tests, tzxtools port)
- [x] P3-C64: C64 Tests aktiviert (58 tests total)
  - test_6502_disasm: 15 tests
  - test_drive_scan: 13 tests  
  - test_prg_parser: 13 tests
  - test_cbm_disk: 17 tests
- [x] P3-API: CBM Drive Scan API erweitert
  - uft_cbm_tool_type_t enum
  - uft_cbm_classify_tool()
  - uft_cbm_has_dos_command()
  - uft_cbm_identify_tool()
  - uft_cbm_extract_strings()
  - uft_cbm_tool_type_name()
  - uft_cbm_drive_scan_prg()

## 📋 P3 BACKLOG

### ✅ Completed This Session
- [x] P3-HAL: HAL Profile Types (uft_controller_caps_t)
  - Drive profiles: 11 types (525DD/HD, 35DD/HD/ED, 8SD/DD, 1541, Apple, Amiga)
  - Controller caps: 6 controllers (Greaseweazle, FluxEngine, Kryoflux, SCP, FC5025, XUM1541)
  - New functions: uft_hal_controller_has_feature(), uft_hal_recommend_controller()
- [x] P3-GATE: Write Safety Gate (from Safety Pack)
  - SHA-256: uft_sha256.c (FIPS 180-4 compliant, ~200 lines)
  - Snapshot: uft_snapshot.c (backup + verify)
  - Gate: uft_write_gate.c (fail-closed policy)
  - Policies: STRICT, IMAGE_ONLY, RELAXED
  - Tests: 12 tests (SHA-256, Format Detection, Gate Policy, Snapshot)
- [x] P3-TZX: TZX Format (ZX Spectrum/Amstrad CPC tape)
  - Parser + WAV export + TAP conversion
- [x] P3-TRD: TRD Format (TR-DOS, ZX Spectrum)
  - 640K/320K/180K sizes, directory parsing
- [x] P3-D88: D88 Format (PC-88/98)
  - Variable track/sector, FM/MFM, 2D/2DD/2HD
- [x] P3-CPM: CP/M Format Implementation
  - DPB parsing, directory, file extraction
- [x] P3-XCOPY: XCopy Pro Algorithm Integration
  - Track length measurement (getracklen from XCopy Pro)
  - Multi-revolution reading (NibbleRead from XCopy Pro)
  - Per-drive calibration (mestrack/drilen[] from XCopy Pro)
  - Timed sector scanning (FD_TIMED_SCAN_RESULT from ManageDsk)
  - Copy protection detection (Variable Sectors, Long Track, etc.)
  - Copy mode selection (DOS/BAM/Nibble/Flux)
  - Tests: 12 new tests for XCopy algorithms

### Medium Priority  
- [x] P3-MACRO: Macro Duplicates (UFT_PACKED, UFT_INLINE)
  - Created central uft_macros.h with all compiler/platform macros
  - Updated uft_config.h to include uft_macros.h
  - Updated uft_floppy_types.h with fallback for standalone builds
  - Macros consolidated: UFT_INLINE, UFT_PACKED, UFT_UNUSED, UFT_ALIGNED, etc.
  - Added: UFT_LIKELY/UNLIKELY, UFT_RESTRICT, UFT_NORETURN, byte swap macros

### Low Priority
- [x] ZX BASIC Tokenizer (from tzxtools)
  - Full token table (0xA5-0xFF, 91 keywords)
  - Number parsing (ZX 5-byte floating point)
  - Line detokenization
  - Program parsing and listing
  - Variable parsing
  - TAP header parsing
  - UDG and block graphics support
  - 16 tests passing
- [x] ZX Screen Converter (SCREEN$→BMP)
  - 6912-byte screen data parsing (6144 bitmap + 768 attributes)
  - Complex ZX Spectrum memory layout decoding
  - 15-color palette (normal + bright)
  - RGB/RGBA export with optional border
  - BMP file export
  - 12 tests passing
- [ ] libflux_format stubs (37+ formats) - P4 LOW

## ✅ PHASE 3: Format Adapter Implementation COMPLETE
- [x] ADF Adapter (Amiga DD/HD)
  - Probe: DOS signature, bootblock checksum, size validation
  - Open/Read/Geometry/Close implemented
  - 80 tracks × 2 sides × 11/22 sectors × 512 bytes
- [x] D64 Adapter (Commodore 64)
  - Probe: Size, BAM pointer, DOS type, disk name validation
  - Variable sectors per track (21/19/18/17)
  - Error byte support for error-info D64 files
- [x] IMG Adapter (PC FAT12)
  - Probe: x86 jump, media descriptor, boot signature
  - Supports: 360K, 720K, 1.2M, 1.44M, 2.88M
  - BPB parsing for geometry detection
- [x] TRD Adapter (ZX Spectrum TR-DOS)
  - Probe: TR-DOS ID, disk type, free sectors validation
  - 80/40 tracks × 1/2 sides × 16 sectors × 256 bytes
  - System sector parsing

## 📊 Test Summary

| Test Suite          | Tests | Status |
|---------------------|-------|--------|
| smoke_version       | 1     | ✅     |
| smoke_version_consistency | 6 | ✅     |
| smoke_workflow      | 1     | ✅     |
| track_unified       | 77    | ✅     |
| fat_detect          | 28    | ✅     |
| xdf_xcopy_integration| 1    | ✅     |
| xdf_adapter         | 11    | ✅     |
| format_adapters     | 18    | ✅     |
| scl_format          | 16    | ✅     |
| zxbasic             | 16    | ✅     |
| zxscreen            | 12    | ✅     |
| z80_disasm          | 53    | ✅     |
| c64_6502_disasm     | 15    | ✅     |
| c64_drive_scan      | 13    | ✅     |
| c64_prg_parser      | 13    | ✅     |
| c64_cbm_disk        | 17    | ✅     |
| write_gate          | 12    | ✅     |
| xcopy_algo          | 12    | ✅     |
| **TOTAL**           | **322**| ✅    |

## 🔧 Build Info
- Version: 3.7.0 (Single Source of Truth: include/uft/uft_version.h)
- Platforms: Linux ✅ | Windows ⏳CI | macOS ⏳CI
- Qt: 6.6.2 (GUI optional)
- Tests: 18 test executables, 320+ sub-tests ✅
- Active Source Files: 2186
- SAMdisk Module: 171 files (C++17, libuft_samdisk_core.a 510KB)
- Archived Files: 410 (non-floppy, legacy)
- Format Directories: 90 (all floppy-relevant)
- Format Adapters: ADF, D64, IMG, TRD (4 complete)

## 📋 P3 BACKLOG (Medium Priority)

### SAMdisk Integration (P3-SAMDISK) - IN PROGRESS
- [x] SAMdisk C++ Module aktivieren (Simon Owen's Bitstream Decoders)
  - Quelle: https://github.com/simonowen/samdisk
  - Location: src/samdisk/ (172 Dateien, C++17)
  - Status: **Core Library kompiliert (507KB)**
  
  **Aktivierte Features:**
  - ✅ BitBuffer (Bit-level Buffer)
  - ✅ BitstreamDecoder (PLL-basierte Dekodierung)
  - ✅ BitstreamEncoder (Encoding für Write-Back)
  - ✅ FluxDecoder (Flux→Bitstream)
  - ✅ FluxTrackBuilder (Track-Aufbau)
  - ✅ CRC16 (CRC Berechnung)
  - ✅ DiskUtil (Utility Funktionen)
  
  **Deaktivierte Features (externe Dependencies):**
  - MemFile (benötigt lzma.h)
  - Hardware-Treiber (KryoFlux, SCP, etc.)
  - Komprimierung (zlib, bzip2, lzma)
  
  **Verbleibende Integration Steps:**
  1. [x] config.h erstellen (Platform Detection, UFT Type Mappings)
  2. [x] CMake aktivieren (UFT_ENABLE_SAMDISK=ON)
  3. [ ] Adapter Layer: uft_samdisk_adapter.h/c (C API Wrapper)
  4. [ ] Tests mit bekannten Flux-Dumps
  5. [ ] Cross-Platform Verify (Linux/Windows/macOS)
  
  - Aufwand: M (Medium, Core done)
  - Build: libuft_samdisk_core.a (507KB)

### Duplicate src/algorithms/bitstream/ (P3-SAMDISK-DUP) ✅ DONE
- [x] src/algorithms/bitstream/*.cpp nach _archive/ verschoben
  - BitBuffer.cpp, BitstreamDecoder.cpp, BitstreamEncoder.cpp, FluxDecoder.cpp
  - Ersetzt durch SAMdisk Core (src/samdisk/)
  - Location: _archive/legacy_imports/algorithms_bitstream/

## 📋 P4 BACKLOG (Low Priority Refactoring)

### CRC Consolidation (P4-REFACTOR)
- [ ] 30 CRC-Dateien → 1 kanonische Implementation
- [ ] Betroffene Verzeichnisse: src/crc/, src/core/, src/decoder/, src/tracks/, src/algorithms/
- [ ] Zentrale API: include/uft/core/uft_crc.h
- Aufwand: L (Large)
- Abhängigkeiten: Keine kritischen

### Floppy-Stub Aktivierung (P4-FORMAT)
- [ ] 32 Floppy-Format-Stubs zu vollständigen Adaptern ausbauen
- [ ] Priorität: ADF, D64, WOZ, HFE, IMD (häufig genutzt)
- Aufwand: M-L je Format
- Abhängigkeiten: XDF Adapter System
