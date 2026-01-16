# Changelog

All notable changes to UnifiedFloppyTool will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [4.0.0] - 2026-01-16

### Added

#### New GUI Panels
- **DMK Analyzer Panel** – Deep analysis of TRS-80 DMK disk images
  - Track-by-track sector analysis
  - IDAM pointer validation
  - FM/MFM encoding detection
  - Export to raw sector image
- **Flux Histogram Panel** – Real-time flux timing visualization
  - Interactive histogram with zoom
  - Automatic encoding detection (MFM/FM/GCR)
  - Peak analysis and cell time measurement
  - CSV export for external analysis
- **GW→DMK Direct Panel** – Greaseweazle to DMK streaming
  - Direct hardware-to-image pipeline
  - TRS-80 Model I/III/4 presets
  - Real-time progress tracking

#### New CLI Commands
- `uft hist` – Flux histogram analysis from command line
- `uft check` – Enhanced validation with detailed reports
- Enhanced `uft analyze` with more format support

#### New Formats
- **86Box 86F** – PC emulator flux format (read/write)
- **MAME MFI** – MAME floppy image format (read/write)
- **TI-99/4A** – Complete sector format support
  - FM90 (90KB single density)
  - FM128 (128 byte sectors)
  - MFM256 (256KB double density)
  - MFM512 (360KB, 512 byte sectors)
- **Catweasel Raw** – Native Catweasel flux format

#### Improvements
- Integrated analysis panels into existing tabs (cleaner UI)
- Qt 6.10 compatibility fixes
- Windows qmake build improvements
- Better error messages throughout

### Fixed
- AmigaDOS extended API conflicts resolved
- HDF parser warnings eliminated
- Deprecated Qt API usage updated (`globalPos()` → `globalPosition()`)
- GUI panel linker errors on Windows

### Changed
- DMK Analysis moved from standalone tab to Status tab button
- Flux Histogram moved from standalone tab to Workflow tab button
- GW→DMK moved from standalone tab to Settings tab section

---

## [3.9.0] - 2026-01-15

### Added

#### New Formats (from libdsk analysis)
- **ApriDisk** – Compressed floppy image format
- **NanoWasp** – Microbee disk format
- **QRST** – Quick and Reliable Sector Transfer
- **CFI** – Compressed Floppy Image
- **YDSK** – Extended DSK with metadata
- **SIMH** – SIMH simulator format
- **Logical** – Logical disk format
- **POSIX** – POSIX floppy access
- **OPUS** – Opus Discovery format
- **MGT** – Miles Gordon Technology (SAM Coupé)
- **LDBS** – Level Data Block Structure
- **RCPMFS** – Remote CP/M filesystem
- **DFI** – DiscFerret Image
- **MYZ80** – MYZ80 emulator format

#### CP/M Disk Definitions
- 43 new CP/M disk definitions from libdsk
- Includes Amstrad PCW, Epson PX-4/8, Kaypro, Osborne, and more

#### Greaseweazle Protocol Fixes
- Fixed flux data parsing for newer firmware
- Improved revolution detection
- Better error recovery

### Fixed
- CQM format reader improvements
- Various memory leaks in format handlers

---

## [3.8.0] - 2026-01-14

### Added

#### TI-99/4A Support
- Complete filesystem implementation
- FIAD/TIFILES container support
- V9T9 and PC99 sector formats
- Directory listing and file extraction

#### FAT12 Extensions
- Long filename (LFN) support
- Bad block handling
- Boot sector analysis
- VFAT compatibility

### Fixed
- D64 writer sector interleave
- G64 speed zone handling
- Apple GCR decoder edge cases

---

## [3.7.0] - 2026-01-13

### Added
- Initial GUI implementation with Qt6
- Tab-based interface (Workflow, Hardware, Status, Format, Explorer, Tools)
- Hardware detection for Greaseweazle, KryoFlux, SCP
- Real-time decode progress display
- Visual disk map

### Changed
- Restructured codebase for GUI/CLI separation
- Improved CMake build system

---

## [3.6.0] - 2026-01-12

### Added
- ML-assisted decoder for damaged disks
- Bayesian format detection
- Protection detection framework
- Forensic report generation (HTML/JSON)

---

## [3.5.0] - 2026-01-11

### Added
- Unified decoder registry
- Format conversion pipeline
- Session management
- Configuration persistence

---

## [3.0.0] - 2026-01-10

### Added
- Complete rewrite of core engine
- Support for 400+ disk image formats
- Hardware abstraction layer
- Cross-platform build system

---

## [2.0.0] - 2025-12-01

### Added
- Initial public release
- Basic format support (D64, ADF, IMG)
- Greaseweazle support
- Command-line interface

---

## Version History Summary

| Version | Date | Highlights |
|---------|------|------------|
| 4.0.0 | 2026-01-16 | DMK Analyzer, Flux Histogram, GW→DMK, 86F/MFI formats |
| 3.9.0 | 2026-01-15 | libdsk formats, 43 CP/M definitions, GW protocol fixes |
| 3.8.0 | 2026-01-14 | TI-99/4A support, FAT12 LFN |
| 3.7.0 | 2026-01-13 | Qt6 GUI, hardware detection |
| 3.6.0 | 2026-01-12 | ML decoder, forensic reports |
| 3.5.0 | 2026-01-11 | Unified decoder registry |
| 3.0.0 | 2026-01-10 | Core rewrite, 400+ formats |
| 2.0.0 | 2025-12-01 | Initial release |

---

[4.0.0]: https://github.com/Axel051171/UnifiedFloppyTool/compare/v3.9.0...v4.0.0
[3.9.0]: https://github.com/Axel051171/UnifiedFloppyTool/compare/v3.8.0...v3.9.0
[3.8.0]: https://github.com/Axel051171/UnifiedFloppyTool/compare/v3.7.0...v3.8.0
[3.7.0]: https://github.com/Axel051171/UnifiedFloppyTool/compare/v3.6.0...v3.7.0
[3.6.0]: https://github.com/Axel051171/UnifiedFloppyTool/compare/v3.5.0...v3.6.0
[3.5.0]: https://github.com/Axel051171/UnifiedFloppyTool/compare/v3.0.0...v3.5.0
[3.0.0]: https://github.com/Axel051171/UnifiedFloppyTool/compare/v2.0.0...v3.0.0
[2.0.0]: https://github.com/Axel051171/UnifiedFloppyTool/releases/tag/v2.0.0
