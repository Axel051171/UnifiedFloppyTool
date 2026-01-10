# UnifiedFloppyTool Changelog

## v3.7.0 - Consolidated Release

**Datum:** 2025-01-10

### Überblick

UnifiedFloppyTool (UFT) ist ein forensisches Disk-Preservation-Tool für historische Floppy-Formate.

**Motto:** "Bei uns geht kein Bit verloren"

### Highlights dieser Version

#### Tests & Stabilität
- **18 Test-Suites** mit **322 Sub-Tests** - alle bestanden
- Smoke Tests, Parser Tests, Adapter Tests, Algorithm Tests
- Version Consistency Tests
- Cross-Platform Build verifiziert

#### SAMdisk Integration (NEU)
- SAMdisk C++ Core aktiviert (171 Dateien)
- libuft_samdisk_core.a (510KB)
- C-API Wrapper: uft_samdisk_adapter.h
- Features: BitBuffer, BitstreamDecoder, FluxDecoder, CRC16

#### Format-Adapter (4 Complete)
- **ADF** (Amiga DD/HD) - OFS/FFS Auto-Detection
- **D64** (Commodore 64) - Variable Sectors, Error Bytes
- **IMG** (PC FAT12) - 360K/720K/1.2M/1.44M/2.88M
- **TRD** (ZX Spectrum TR-DOS) - 640K/320K/180K

#### Neue Module
- **XCopy Pro Algorithms** - Track Length, Multi-Revolution, Calibration
- **Write Safety Gate** - SHA-256, Snapshots, Fail-Closed Policy
- **Z80 Disassembler** - 53 Tests, tzxtools Port
- **C64 Forensik** - 6502 Disasm, PRG Parser, Drive Scanner, CBM Disk

#### Parser Cleanup
- 90 aktive Floppy-Format-Verzeichnisse
- 403 archivierte Non-Floppy Formate
- SAMdisk C++ vorbereitet für Integration

### Features

#### Core Engine
- 21 Track-Module für verschiedene Plattformen
- Multi-Format Support (115+ Formate)
- Cross-Platform (Linux, macOS, Windows)

#### Unterstützte Plattformen
- **Commodore**: C64, C128, VIC-20, Plus/4, PET
- **Amiga**: OFS, FFS, alle Varianten
- **Atari**: ST, 8-Bit
- **Apple**: Apple II, Macintosh
- **PC**: IBM PC, DOS, FAT12/16
- **Weitere**: ZX Spectrum, BBC Micro, MSX, CPC, TI-99

#### Hardware-Unterstützung (HAL)
- Greaseweazle
- FluxEngine
- KryoFlux
- SuperCard Pro
- XUM1541/Nibtools
- FC5025
- Applesauce

#### XDF Container Familie
- AXDF (Amiga Extended Disk Format)
- DXDF (C64 Extended Disk Format)
- PXDF (PC Extended Disk Format)
- TXDF (Atari ST Extended Disk Format)
- ZXDF (ZX Spectrum Extended Disk Format)
- MXDF (Multi-Format Bundle)

#### XDF API
- Universelle API für alle Formate
- Auto-Detection
- 7-Phasen Forensik-Pipeline
- Batch Processing
- JSON/REST Interface
- Event System

#### Forensik-Features
- Multi-Revolution Capture
- Weak-Bit Detection
- Copy Protection Analysis
- CRC Correction (1-Bit, 2-Bit)
- Confidence Scoring
- Audit Trail

### Build

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### Anforderungen

- CMake 3.16+
- C99 Compiler
- Qt6 6.2+ (optional, für GUI)

### Lizenz

Open Source - Siehe LICENSE Datei
