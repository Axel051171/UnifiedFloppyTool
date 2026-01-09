# UnifiedFloppyTool Changelog

## v3.7.0 - Release

**Datum:** 2025-01-09

### Überblick

UnifiedFloppyTool (UFT) ist ein forensisches Disk-Preservation-Tool für historische Floppy-Formate.

**Motto:** "Bei uns geht kein Bit verloren"

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
