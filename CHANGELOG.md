# Changelog

## [Unreleased] (planned 4.1.4 hot-fix)

### Fixed
- **Windows COM-port-prefix bug across four QSerialPort-based hardware
  providers** (MF-145 / FB user-report). Greaseweazle, SuperCard Pro,
  Applesauce, and ADF-Copy each pre-applied the Win32 device-namespace
  prefix `\\.\` before calling `QSerialPort::setPortName()`. Qt's
  `QSerialPort` adds that prefix internally; the pre-application caused
  `DeviceNotFoundError` on Qt 6.7+ or silently broken handles on older
  Qt. Symptom matched the user report: "hardware will not access the
  floppy drive" on Windows. The bug never surfaced for Greaseweazle on
  the production code path (UI → C-HAL `uft_gw_open()` uses
  `CreateFileA` directly, where the prefix is correct), but it did
  affect SCP / Applesauce / ADF-Copy which route through the Qt
  provider. **All four providers fixed in one patch — affected users
  of any of these four controllers should test v4.1.4.**
  USB-Floppy provider's `\\.\A:` and `\\.\B:` paths are intentionally
  retained: that provider uses `DeviceIoControl` SCSI pass-through
  where the prefix is the correct Win32 convention for direct
  volume access.
- **Applesauce ↔ ADF-Copy VID/PID disambiguation** (MF-146). Both
  controllers ship with the generic PJRC Teensy USB ID
  (VID `0x16C0` / PID `0x0483`). Previously, both providers' detect
  paths candidate-matched any 0x16C0:0x0483 device, then attempted
  their own protocol handshake — racing for the same port during
  auto-detect. Each provider now skips ports whose USB
  manufacturer / description string identifies the *other*
  controller (`Evolution Interactive` / `Applesauce` for ADF-Copy
  scan; `ADF-Copy` / `ADF-Drive` for Applesauce scan). Eliminates
  the double-claim on systems where only one device is attached.
- **Greaseweazle bootloader-mode detection** (MF-129). `uft_gw_open()`
  now refuses bootloader-firmware boards with an actionable error
  (`UFT_GW_ERR_BOOTLOADER`), surfaced in the UI as recovery hints
  instead of a generic "I/O error".
- **MFM IDAM/DAM standalone sector parser** (MF-141 / AUD-002).
  SCP→ADF/IMG/D64 conversion path now decodes sectors via a
  reusable API with explicit CRC validation and CHRN-keyed LBA
  mapping. Previously the inline parser dropped sectors after IDAM,
  ignored CRCs, and placed payloads in scan order instead of by
  sector ID.

### Internal
- Restored 10 hardware Qt-provider classes that had been deleted in
  MF-132 (FluxEngine, KryoFlux, SCP, Applesauce, FC5025, XUM1541,
  Greaseweazle, ADF-Copy, USB-Floppy, Mock — 10 341 LOC of real
  subprocess / serial / libusb implementations). They now route
  through HardwareManager from `hardwaretab.cpp` (MF-143).
- `uft_version.h` is regenerated from `VERSION.txt` at every qmake
  parse / CMake configure (MF-131). Removes the `DEFINES+=
  UFT_VERSION_STRING=\\\"...\\\"` quoting fragility from
  `release.yml` (MF-145, eliminates the suspected v4.1.0-shows-on-
  v4.1.3 cause).
- `scripts/check_consistency.py --check version` extended to also
  scan `RELEASE_NOTES.md` headline + Doxygen `@version` tags
  (MF-128). Pre-push gate now blocks 5 distinct version-drift
  classes.

## [4.1.3] - 2026-04-16

### Added
- CRC / status flag propagation across the PLL → sector pipeline
- IMD read_track (ImageDisk sector extraction with metadata)
- FDI read_track (Formatted Disk Image with track offset table)
- Plugin-B parsers for DO, PO, ADL, V9T9, VDK (Apple / Atari / Acorn / TI / Dragon)
- STX (Atari Pasti) + PRO (Atari 8-bit) Plugin-B — all 5 Tier-1 formats complete (27 plugins)
- ATX + ADF + SCL Plugin-B + restored registry (25 plugins)
- write_track for TRD, D64, ATR, SAD, SSD, HFE, D80, D82, D71, D81
- Probe-quality improvements for 6 formats (fewer false positives)

### Fixed
- 3 silent I/O errors in SAD, SSD, HFE
- NULL-checks + silent-error fixes in 8 registry plugins
- Error aliases, total_sectors, SSD write, protection includes
- Memory allocation limits, bounds checks, forensic fill (security pass)
- Struct compat aliases, write-verify types, track_init signature
- CMake Sanitizer / Coverage builds — dead file references removed
- Test include paths repaired

### Changed
- Format registry: 138 → 149 fully implemented parsers
- Cleanup: 674 orphaned source files deleted (~386 000 LOC dead code)
- 109 dead format files removed from disk
- 165 non-floppy stubs removed (32 active plugins after pruning)

## [4.1.2] - 2026-04-15

### Added
- D64 Plugin-B wrapper (Commodore 1541, variable SPT zones)
- TD0 read_track (Teledisk: raw/repeat/pattern decode)
- FDI read_track (track offset table + sector descriptors)
- MSA Plugin-B (Atari ST RLE decompression)
- WOZ Plugin-B (Apple II v1/v2/v2.1 via woz_load API)
- IPF Plugin-B (CAPS/SPS identification)
- G71 Plugin-B (Commodore 1571 GCR raw)
- Format registry: 22 plugins, all with read_track
- MFM sector extraction in SCP→sector flux converter

### Fixed
- 4 legacy MFM encoder CRC placeholders → real CRC-CCITT (0xCDB4)
- 86F CRC verification (was always true, now computed)
- CRC32 combine XOR bug → sentinel value
- uft_track_add_sector return type harmonized (int → uft_error_t)
- Duplicate uft_ufi_read_sectors removed from backend
- USBFloppyHardwareProvider interface mismatch with base class
- 10 broken tests repaired (include paths: 78→88 OK)
- 3 missing forwarding headers created

### Changed
- Format registry expanded from 21 to 22 plugins (D64 added)
- IPF read_track returns NOT_IMPLEMENTED honestly
- Test compilation: 88 of 94 tests compile cleanly

## [4.1.1] - 2026-04-14

### Added
- 11 new Plugin-B format parsers: ST, D77, DIM, DC42, FDS, CAS, MFI, PRI, EDSK, KFX
- A2R parser (1104 LOC) activated from existing code
- Decode Pipeline: unified Flux → PLL → Bits → Sectors → OTDR session
- Canonical PLL API header (uft_pll.h) with 11 platform presets

### Fixed
- WOZ 1.0 TRKS chunk: track bitstream data now accessible (was empty)
- 10 macro redefinition warnings eliminated (#ifndef guards)
- Qt Charts made optional (CMake builds without Charts module)
- 4 previously disabled tests re-enabled (ti99, fat_extensions, mega65_fat32, new_formats)

### Changed
- PLL consolidation: vfo_fixed removed, vfo_experimental + kalman_pll optional (CONFIG flags)
- Format count: 138 → 149 fully implemented parsers

## [Unreleased] (planned 4.2.0)

### Added
- DeepRead forensic modules: write-splice, aging, cross-track, fingerprint, soft-decode
- OTDR adaptive decode, weighted voting, encoding boost
- ML anomaly detection and copy protection classifier
- 9 format parsers: FDS, CHD, NDIF, EDD, DART, Aaru, HxCStream, 86F, SaveDskF
- FC5025, XUM1541/ZoomFloppy, Applesauce hardware providers
- Unified copy protection API (55+ schemes, 10 platforms)
- Triage mode, sector compare, provenance chain, recovery wizard, format suggestion
- GUI: state machine, smart export dialog, recovery wizard dialog, compare dialog
- GUI: drag & drop, keyboard shortcuts, status bar info
- CI: sanitizers.yml (ASan+UBSan), coverage.yml (Codecov)
- 25 specialized agent definitions

### Fixed
- ~610 silent fseek/fread/fwrite error handling fixes
- Integer overflow guards in 9 parsers
- Real SHA-256 replacing fake FNV1a-based hash
- SIMD rounding mismatch (AVX2 vs SSE2/scalar)
- Multi-revolution negative alignment offset blocking
- Amiga MFM header buffer overread
- Apple II DOS-to-ProDOS sector map (3 conflicting tables → 1 canonical)
- Atari ST boot detection (proper 0x1234 checksum)
- C64 40-track D64 spt table incomplete
- Path traversal security (component-level walk)
- Thread safety: uft_safe_alloc, UftParameterModel, HardwareManager
- Compiler hardening: -fstack-protector-strong, -D_FORTIFY_SOURCE=2

### Changed
- God-file split: uft_format_convert.c (3944 lines) → 7 modular files
- 33 protection source files added to build (were dead code)
- SCP Extension Footer parsing implemented
- WOZ 2.1 FLUX chunk decoding implemented
- Format detection improved: D88/D77, IMG/IMA, Atari ST, Apple DO/PO, HFE v2

## [4.1.0] - 2026-02-08

### Fixed – Hardware Protocol Audit
- **SuperCard Pro**: Complete rewrite from SDK v1.7 reference
  - Replaced fabricated ASCII protocol with correct binary commands (0x80-0xD2)
  - Fixed checksum (init 0x4A), endianness (big-endian), USB interface (FTDI FT240-X)
  - Implemented proper RAM model (512K SRAM, SENDRAM_USB/LOADRAM_USB)
  - Corrected flux read flow: READFLUX→GETFLUXINFO→SENDRAM_USB
  - Corrected flux write flow: LOADRAM_USB→WRITEFLUX
- **FC5025**: Complete rewrite from driver source v1309
  - Replaced fabricated opcodes with SCSI-like CBW/CSW protocol (sig 'CFBC')
  - Correct opcodes: SEEK=0xC0, FLAGS=0xC2, READ_FLEXIBLE=0xC6, READ_ID=0xC7
  - Device correctly marked as read-only (no write support in hardware)
  - Density control via FLAGS bit 2
- **KryoFlux**: Replaced invented USB commands with DTC subprocess wrapper
  - Proprietary USB protocol acknowledged, official tool used instead
  - Stream format parser (OOB 0x08-0x0D) verified and retained
- **Pauline**: Replaced fabricated binary protocol (0x01-0x90) with HTTP/SSH
  - Matches real architecture: embedded Linux web server on DE10-nano FPGA
  - SSH commands for drive control, HTTP for data transfer
- **Greaseweazle**: 16 protocol bugs fixed against gw_protocol.h v1.23
  - Fixed USB command codes, bandwidth negotiation, SET_BUS_TYPE
  - Corrected checksum and packet framing

### Added
- GitHub Actions CI/CD release workflow (macOS/Linux/Windows)
- Linux .deb packaging with desktop integration
- macOS .dmg with universal binary support
- CMakeLists.txt for cmake-based builds alongside qmake
- Linux .desktop entry and udev rules for floppy devices

### Changed
- Version bump: 4.0.0 → 4.1.0
- Cleaned up development-only files from distribution

## [4.0.0] - 2026-01-16

### Added
- DMK Analyzer, Flux Histogram
- MAME MFI & 86Box 86F formats
- Documentation suite
- CI/CD pipeline

## [3.9.0] - 2026-01-15

### Added
- TI-99/4A format support
- FAT filesystem extensions
- 43 CP/M disk definitions
