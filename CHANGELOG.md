# Changelog

All notable changes to UnifiedFloppyTool will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

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
