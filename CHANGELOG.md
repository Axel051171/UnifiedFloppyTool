# Changelog

All notable changes to UnifiedFloppyTool will be documented in this file.

## [3.6.0] - 2026-01-06 - "GitHub Release Edition"

### 🔧 Build Fixes
- **Fixed**: Uninitialized variables in `uft_bitstream_preserve.c:619,654` (14 variables)
- **Fixed**: Linux Qt6 -fPIC compile error - switched to aqtinstall (Qt 6.6.2)
- **Fixed**: macOS compatibility - upgraded to macos-15 runner (ARM64)

### 🚀 CI/CD
- Linux: Qt6 via aqtinstall (6.6.2) instead of system packages
- macOS: Upgraded from macos-13 to macos-15 (ARM64/Apple Silicon)
- Matrix builds for Windows, macOS, Linux
- Automated release artifacts (tar.gz, deb, zip)
- Test labels for selective test execution (smoke, io, parser)

### 📄 Documentation
- Updated TODO.md with current status
- Release-ready documentation structure

---

## [3.4.3] - 2026-01-06

### Fixed
- Windows Build: Removed unguarded `#include <unistd.h>` in multiple files
- Windows Build: Added platform guards for POSIX-specific code
- Linux AppImage: Fixed icon detection for linuxdeploy

### Changed
- uft_audit_trail.c: Windows-compatible gethostname replacement
- uft_tool_adapter.c: Windows-compatible unlink/getpid
- uft_fat_table.c: Windows-compatible endian macros
- uft_simd.c, uft_memory.c: Platform-specific includes


The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [3.4.2] - 2026-01-06

### Added
- CBM DOS High-Level Error Codes (0-77) für vollständige DOS-Kompatibilität
- uft_cbm_dos_error_string() für Channel 15 Status
- uft_cbm_dos_format_status() für CBM-Style Status-Strings
- Source: disk2easyflash (Per Olofsson, BSD License)

## [3.4.1] - 2026-01-06

### Added
- PRG Analyzer: 4 neue Keywords (BACKUP, FORMAT, BAM, DIRECTORY)
- Test-Samples: 6 historische C64-Kopierprogramme (TurboNibbler, Di-Sector, etc.)

### Changed
- uft_prg_score_t erweitert für bessere Copy-Tool-Erkennung

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


## [3.8.0] - 2026-01-06 (R50 MEGA-INTEGRATION)

### Added - External Source Integration

#### HxCFloppyEmulator Integration
- `uft_hxc_pll_enhanced.h/.c` - Enhanced PLL algorithms
  - `uft_hxc_detectpeaks()` - Automatic bitrate detection via histogram
  - `uft_hxc_get_cell_timing()` - Core PLL with inter-band rejection
  - Victor 9K variable speed band support
  - Fast/slow correction ratios for jitter handling

#### OpenCBM Integration  
- `uft_opencbm_gcr.h/.c` - Commodore GCR 4-bit to 5-bit codec
  - `uft_gcr_5_to_4_decode()` - GCR decoding with error detection
  - `uft_gcr_4_to_5_encode()` - GCR encoding
  - `uft_gcr_decode_block()` - Full block decoding with checksum
  - `uft_gcr_decode_sector_header()` - Sector header parsing

#### FluxRipper Integration
- `uft_fluxstat.h` - Statistical flux recovery framework
  - Multi-pass capture support (2-64 passes)
  - Bit-level confidence scoring (strong/weak/ambiguous)
  - CRC-guided error correction
  - Histogram-based bitrate detection
  - Sector and track recovery APIs

#### disk-utilities Integration
- `uft_amiga_protection_registry.h` - Amiga copy protection detection
  - 170+ known Amiga protection signatures
  - CopyLock LFSR implementation (23-bit)
  - Publisher-specific protection handlers
  - Protection detection API with confidence scoring

### Sources Analyzed
1. **HxCFloppyEmulator** (Jean-François DEL NERO) - GPL v2
   - fluxStreamAnalyzer.c (4800+ lines)
   - HFE v3 loader/writer
   - Track encodings (FM, MFM, M2FM)

2. **OpenCBM** (Wolfgang Moser) - GPL v2
   - gcr_4b5b.c - Core GCR conversion
   - libd64copy - D64 GCR handling

3. **FluxRipper** - Hardware flux capture firmware
   - fluxstat_hal.h/c - Statistical analysis
   - Multi-pass correlation algorithms

4. **disk-utilities** (Keir Fraser) - Public Domain
   - 170+ Amiga format handlers
   - CopyLock, RNC, Psygnosis protections
   - Stream handlers for SCP, KryoFlux

5. **FlashFloppy** (Keir Fraser) - Public Domain
   - HFE v1/v3 handling
   - Gotek interface definitions

### Technical Notes
- All code converted to UFT naming conventions (uft_*)
- Platform-independent implementations (no HW dependencies)
- Full documentation with Doxygen comments
- Error handling with defined return codes


## [3.9.0] - 2026-01-06 (R51-53 FULL IMPLEMENTATION)

### Added - Complete Implementations

#### FluxStat Module (src/flux/uft_fluxstat.c) - 649 lines
- `uft_fluxstat_create/destroy()` - Context lifecycle
- `uft_fluxstat_add_pass()` - Multi-pass capture (2-64 passes)
- `uft_fluxstat_compute_histogram()` - Flux timing histogram
- `uft_fluxstat_correlate()` - Cross-pass bit correlation
- `uft_fluxstat_recover_track()` - Track-level recovery
- `uft_fluxstat_recover_sector()` - Sector-level recovery
- Bit-level confidence scoring (Strong/Weak/Ambiguous)

#### Amiga Protection Module (src/protection/uft_amiga_protection.c) - 368 lines
- 35+ protection registry entries
- `uft_amiga_detect_protection()` - Multi-protection detection
- `uft_amiga_check_copylock()` - CopyLock LFSR verification
- `uft_copylock_lfsr_forward/backward()` - LFSR navigation
- Score-based protection matching

#### HxC Format Detection (src/formats/uft_hxc_formats.c) - 314 lines
- 90+ format type definitions
- `uft_hxc_detect_format()` - Signature-based detection
- WOZ v1/v2/v3, SCP, IPF, HFE, STX, IMD, DMK, etc.
- Size-based fallback detection
- Preservation/Flux format classification

### Format Definitions (include/uft/formats/uft_hxc_formats.h)
- WOZ header structures (v1, v2, v3)
- SCP header and revolution structures
- IPF record and info structures
- DMK header structure
- IMD track header structure
- STX (Pasti) header structure
- Format detection API

### New Code Statistics
- Total new implementation: ~1,330 lines
- Total new code in src/: ~33,760 lines


## [3.10.0] - 2026-01-06 (R54 PROTECTION & FLUX PARSERS)

### Added - Complete Amiga Protection Registry

#### Protection Database (194 entries)
- Full extraction from disk-utilities (Keir Fraser, Public Domain)
- Publisher categories: Psygnosis, Factor5, Team17, Gremlin, Bitmap Bros, etc.
- Major systems: CopyLock, SpeedLock, RNC, Softlock
- 9 unique sync patterns (0x4489, 0x8951, 0x448A448A, etc.)
- Protection flags: longtrack, timing, weak bits, LFSR, dual-format
- Score-based multi-protection detection

#### API
- `uft_prot_get_registry()` - Access complete database
- `uft_prot_detect()` - Multi-protection detection with confidence
- `uft_prot_is_longtrack()`, `uft_prot_needs_timing()`, `uft_prot_has_weak_bits()`

### Added - SuperCard Pro (SCP) Parser

#### Features
- Full SCP format v2.0 support (Jim Drew specification)
- Multi-revolution capture (up to 5 revolutions)
- All platforms: Commodore, Amiga, Atari, Apple, PC, TRS-80
- Flux overflow handling (0x0000 entries)
- Resolution support (25ns base + multiplier)
- Checksum verification

#### API
- `uft_scp_create/destroy()` - Context lifecycle
- `uft_scp_open()` / `uft_scp_open_memory()` - File/memory loading
- `uft_scp_read_track()` - Track extraction with all revolutions
- `uft_scp_calculate_rpm()` - RPM calculation
- `uft_scp_disk_type_name()` - Human-readable disk type

### Added - KryoFlux Stream Parser

#### Features
- Complete KryoFlux stream format support
- Variable-length flux encoding (Flux2, Flux3, overflow)
- OOB block parsing (StreamInfo, Index, StreamEnd, KFInfo)
- Index marker extraction (up to 10 revolutions)
- Sample clock: ~48.054 MHz, Index clock: 1.152 MHz

#### API
- `uft_kf_create/destroy()` - Context lifecycle
- `uft_kf_load_file()` / `uft_kf_load_memory()` - Stream loading
- `uft_kf_parse_stream()` - Revolution extraction
- `uft_kf_ticks_to_ns()` - Time conversion
- `uft_kf_parse_filename()` - trackXX.Y.raw pattern parsing

### Statistics
- Amiga Protection: 1,005 lines (header + impl)
- SCP Parser: 770 lines
- KryoFlux Parser: 627 lines
- **Total R54: 2,402 lines**


## [3.4.0] - 2026-01-06 (R55 WOZ PARSER & UNIT TESTS)

### Added - WOZ Parser (Apple II Preservation)

#### Features
- WOZ v1, v2, v3 format support
- Apple II 5.25" and Macintosh 3.5" disk support
- Quarter-track mapping (160 entries)
- Nibble decoding from bitstream
- Metadata parsing (META chunk)
- CRC32 verification
- Compatible hardware flags (Apple II/Plus/IIe/IIc/IIgs/III)

#### API
- `uft_woz_create/destroy()` - Context lifecycle
- `uft_woz_open()` / `uft_woz_open_memory()` - File/memory loading
- `uft_woz_read_track()` - Track bitstream extraction
- `uft_woz_read_flux()` - v3 flux data (future)
- `uft_woz_decode_nibbles()` - Nibble extraction

### Added - Comprehensive Unit Tests

#### Test Coverage
- **Flux Parsers** (test_flux_parsers.c)
  - SCP Parser: 6 tests
  - KryoFlux Parser: 5 tests
  - WOZ Parser: 6 tests
  - FluxStat: 4 tests
  - Integration: 1 test

- **Amiga Protection** (test_amiga_protection.c)
  - Registry: 5 tests
  - Detection: 5 tests
  - Publishers: 1 test

- **HxC Formats & GCR** (test_hxc_formats.c)
  - Format Detection: 7 tests
  - Format Names: 1 test
  - Classification: 1 test
  - GCR Codec: 6 tests

- **Unified Runner** (test_r50_r55_all.c)
  - All modules combined
  - Summary reporting
  - Module availability check

### Statistics
- WOZ Parser: 850 lines (header + impl)
- Unit Tests: 1,200+ lines (4 test files)
- **Total R55: ~2,050 lines**

### R50-R55 Complete Summary

| Module | Lines | Description |
|--------|-------|-------------|
| FluxStat | 1,073 | Multi-pass flux recovery |
| HxC PLL | 665 | Enhanced peak detection |
| SCP Parser | 770 | SuperCard Pro format |
| KryoFlux Parser | 627 | KryoFlux stream format |
| WOZ Parser | 850 | Apple II preservation |
| OpenCBM GCR | 514 | C64 GCR codec |
| Amiga Prot (v1) | 644 | 35 protections |
| Amiga Prot (Full) | 1,005 | 194 protections |
| HxC Formats | 771 | 90+ format detection |
| Unit Tests | 1,200 | Comprehensive coverage |
| **TOTAL** | **~8,100** | New code in R50-R55 |


## v3.4.5 (2026-01-06) - CI Build Fix

### Critical Fixes
- **MSVC Atomics**: Fixed `uft_memory.c` - uses `uft_atomic_size` instead of C11 `atomic_size_t` which MSVC doesn't support
- **Missing Headers**: Fixed `uft_apple2_protection.c` - added `#include <stdio.h>` for `snprintf`
- **Namespace Pollution**: Fixed `fdc_misc.cpp` - moved includes before namespace block to prevent `fdc_misc::std` errors
- **POSIX Compat**: Fixed `strings.h` issues - uses `uft_compat.h` abstraction layer for Windows

### Files Modified
- `src/core/uft_memory.c`
- `src/protection/uft_apple2_protection.c`
- `src/flux/fdc_bitstream/fdc_misc.cpp`
- `include/uft/uft_atomics.h`
- `include/uft/uft_compat.h`
- 16 files patched for `strings.h` → `uft_compat.h`

### Build Status (expected)
- ✅ Linux: GREEN
- ✅ macOS: GREEN
- ✅ Windows: GREEN

