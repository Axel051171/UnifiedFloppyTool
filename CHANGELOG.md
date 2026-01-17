# Changelog

All notable changes to UnifiedFloppyTool will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [4.0.0] - 2026-01-17

### 🎉 Major Release - Complete Rewrite

This is the first stable release of the completely rewritten UnifiedFloppyTool.

### Added

- **Multi-Platform Support**
  - Windows (x64)
  - macOS 14+ (Apple Silicon & Intel)
  - Linux (x64, .deb package available)

- **Hardware Support**
  - Greaseweazle (all versions)
  - KryoFlux
  - SuperCard Pro
  - FluxEngine
  - Applesauce
  - FC5025
  - CatWeasel

- **Disk Formats**
  - Commodore: D64, D71, D81, D80, D82, G64, G71, NIB
  - Amiga: ADF, ADZ, DMS
  - Atari ST: ST, STX, MSA
  - Atari 8-bit: ATR, XFD, DCM
  - Apple II: DO, PO, WOZ, NIB, 2IMG, A2R
  - IBM PC: IMG, IMA, DSK, XDF, CQM
  - Japanese: D88, D77, FDI, DIM
  - ZX Spectrum: TRD, SCL
  - BBC Micro: SSD, DSD
  - Amstrad CPC: DSK, EDSK
  - TRS-80: DMK, JV1, JV3
  - Flux formats: SCP, HFE, IPF, KFX, MFI

- **Analysis Features**
  - Copy protection detection (V-MAX!, Rapidlok, etc.)
  - Weak bit analysis
  - Flux visualization
  - Track timing analysis
  - Sector error detection

- **Recovery Features**
  - GOD MODE multi-strategy recovery
  - Flux-level recovery
  - Bitstream reconstruction
  - Filesystem-aware recovery

### Changed

- Complete Qt6 rewrite (requires Qt 6.5+)
- Modern C++17/C++20 codebase
- Unified error handling system
- Consolidated format detection engine

### Fixed

- Header consolidation (no more duplicate definitions)
- Cross-platform build compatibility
- macOS 14 compatibility

---

## [3.x] - Previous Versions

See git history for changes in previous development versions.
