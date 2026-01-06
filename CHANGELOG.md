# Changelog

All notable changes to UnifiedFloppyTool will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [3.7.0] - 2026-01-06 - "Windows Fix Edition" (R49)

### 🔴 CRITICAL: Windows Build Fix
- **Fixed**: `LNK1181: cannot open input file 'm.lib'` on Windows CI
- **Root Cause**: Direct `-lm` linking without platform guard  
- **Solution**: `uft_link_math()` function with `if(UNIX)` guard
- All 10+ CMakeLists.txt files updated to use `uft_link_math()`

### 🍎 macOS ARM64 Fix
- **Fixed**: `-mavx2` unused argument warning on Apple Silicon
- Added architecture detection: x86 → AVX2/SSE4.2, ARM64 → NEON (auto)

### 📄 Documentation
- Added `docs/R49_WINDOWS_FIX_CRITICAL.md` with complete fix guide

---

## [3.3.0] - 2026-01-05 - "GitHub Edition"

### Added

#### Security (P0)
- Buffer overflow protection in all 47 format parsers
- Integer overflow checks for all size calculations
- Format string vulnerability elimination
- Complete memory safety audit with fix verification

#### I/O Hardening (P1)
- 1,370 automatic I/O return value checks
- Safe I/O wrapper library (`uft_safe_io.h`)
- Endianness-aware read/write functions
- File handle leak detection and prevention

#### GUI Components
- Main window with modern Dark Mode theme
- Format selection dialog with 60+ formats
- Track visualization with color-coded status
- Flux waveform visualizer
- Progress dialog with cancellation support
- Settings dialog with parameter presets
- Hardware configuration panel
- Format converter wizard (35+ writable formats)
- Disk compare view with hex diff
- Session manager with project support
- Filesystem browser (D64/ADF/ATR/DSK/FAT)
- Sector hex editor with undo/redo
- Format documentation panel

#### Architecture
- Unified track structure (uft_track_t)
- CRC algorithm consolidation (12 algorithms)
- Encoding table unification (MFM/FM/GCR)
- Unified sector structure
- Unified disk structure
- Error code system (80 codes, 8 categories)
- Format registry (60+ formats with auto-detection)

#### Performance
- GCR/MFM lookup table optimization (10x speedup)
- Memory pool implementation (reduces fragmentation)
- Cache-friendly sector data structures

#### New Parsers
- HFE v3 parser with HDDD/A2 extension support
- WOZ v2.1 parser with write splice markers
- NIB half-track support
- IPF CTRaw decoding
- ADF directory cache

### Changed
- CMake build system modernized for Qt6
- CI/CD workflows for Windows/Linux/macOS
- Documentation reorganized

### Fixed
- Thread safety in decoder registry
- Memory leaks in format parsers
- Array bounds checking in sector handling

### Known Issues
- macOS x64 builds may have reduced Qt6 compatibility
- Some rare copy protection schemes not yet supported

## [3.2.0] - 2025-12-15

### Added
- PC-98 format support (6 geometry presets)
- MSX format support with Turbo-R compatibility
- TRS-80 format support (Model I/III/4)
- BBC Micro format support (DFS/ADFS)

### Fixed
- D88 parser header validation
- IMD parser track ordering
- TD0 LZSS decompression edge cases

## [3.1.0] - 2025-11-01

### Added
- Copy protection detection (C64, Amiga, PC)
- Multi-revolution flux capture
- Golden test framework (165 tests)
- Fuzz testing (7 targets)

## [3.0.0] - 2025-10-01

### Added
- Initial Qt6 GUI
- Hardware abstraction layer
- Forensic flux decoder pipeline
- 47 format parsers

---

For older versions, see [CHANGELOG_ARCHIVE.md](docs/CHANGELOG_ARCHIVE.md)

## v3.3.2 (R46) - 2025-01-06

### Added - NIBTOOLS Integration
- **uft_c64_track_align.h/.c** (1031 lines): Track alignment for C64 preservation
  - V-MAX! protection alignment (standard + Cinemaware variant)
  - EA PirateSlayer signature detection
  - Rapidlok track header detection with version/TV detection
  - Fat track detection and handling
  - Sync alignment for Kryoflux/raw streams
  - GCR validation helpers

- **uft_c64_track_format.h/.c** (587 lines): 1541 track format tables
  - Sector map (17-21 sectors per track)
  - Speed zone map (zones 0-3)
  - Gap length tables
  - Track capacity calculations
  - D64 block addressing utilities
  - DOS error code definitions

### Source
- Based on nibtools by Pete Rittwage & Markus Brenner (GPL)
- C64 Preservation Project: https://github.com/OpenCBM/nibtools


## v3.4.0 (R46) - 2025-01-06

### Added - NIBTOOLS Integration
- **uft_c64_gcr_codec.h/.c** (1532 lines): Complete GCR codec
  - 4-byte encode/decode with proper bit interleaving
  - Sync detection and counting
  - Sector decode/encode with checksum verification
  - Track cycle detection (headers, syncs, raw)
  - GCR validation and bad GCR detection
  - PETSCII conversion utilities

- **uft_c64_track_align.h/.c** (1031 lines): Track alignment
  - V-MAX!, PirateSlayer, Rapidlok protection alignment
  - Fat track and half-track detection
  - Auto-alignment with method priority

- **uft_c64_track_format.h/.c** (587 lines): 1541 format tables
  - Sector/speed/gap maps for all tracks
  - D64 addressing utilities

### Added - VFO/PLL Module
- **uft_vfo_pll.h/.c** (1153 lines): Variable Frequency Oscillator
  - 7 VFO algorithms: SIMPLE, FIXED, PID, PID2, PID3, ADAPTIVE, DPLL
  - MFM/FM/GCR encoding support
  - Sync detection and tracking
  - Fluctuator for copy protection handling
  - Data separator with sync pattern detection
  - Statistics and diagnostics

### Added - Amiga MFM Decoder
- **uft_amiga_mfm.h/.c** (1222 lines): Amiga MFM support
  - Odd-even MFM interleave encode/decode
  - Sector header and data decode
  - Bootblock checksum verification
  - Track decode with multiple sync patterns
  - Protection detection (Copylock, Speedlock, Long Track)
  - ADF block addressing utilities

### Added - Unit Tests
- test_c64_gcr_codec.c: GCR codec tests
- test_vfo_pll.c: VFO/PLL tests

### Sources
- nibtools by Pete Rittwage & Markus Brenner (GPL)
- Concepts from fdc_bitstream by yas-sim (MIT)
- Concepts from disk-utilities by Keir Fraser (Public Domain)


## v3.5.0 (R47) - 2025-01-06

### Added - G64 Format Handler
- **uft_g64_format.h/.c** (~900 lines): Complete G64 parser/writer
  - G64 detection and validation
  - Track read/write operations
  - Half-track support (84/168 track modes)
  - Speed zone handling (4 zones)
  - D64 conversion functions
  - Analysis and diagnostics

- **uft_g64_offsets.h**: Standard G64 track offset tables
  - Based on user-provided G64 analysis data
  - nibtools/nibconvert compatible offsets
  - Track size tables per zone

### Added - CRC Module
- **uft_crc.h/.c** (~300 lines): Comprehensive checksum utilities
  - CRC-16-CCITT (EasySplit compatible)
  - CRC-32 (table-based and slow versions)
  - XOR checksums (1541 sector header/data)
  - Amiga bootblock checksum (calculate/verify/fix)
  - Fletcher-16 and Adler-32

### Added - PRG Analyzer
- **uft_c64_prg_analyzer.h/.c** (~200 lines): C64 PRG file analyzer
  - Load address extraction
  - Printable string extraction (ASCII/PETSCII)
  - Floppy keyword scoring (30+ keywords)
  - Forensic mode (no emulation)

### Sources
- EasySplit by Thomas Giesel (zlib license) - CRC-16
- nibtools by Pete Rittwage (GPL/Public Domain) - GCR tables
- User-provided G64 track offset data

