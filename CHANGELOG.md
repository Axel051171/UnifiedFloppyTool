# UnifiedFloppyTool Changelog

## v3.7.0 - Release

**Datum:** 2025-01-11

### Neue Formate (RetroGhidra Integration)

#### Tape Formate
- **BBC Micro UEF**: Chunk-basiertes Tape-Format mit BeebEm State Support
  - Magic: "UEF File!" | Header: 12 bytes
  - Chunk-IDs: Tape Data (0x00-0xFF), Emulator State (0x04XX)
  - Maschinen: BBC Model A/B/B+/Master, Acorn Electron

#### Snapshot Formate
- **ZX Spectrum SNA**: Memory Snapshot für ZX Spectrum Emulatoren
  - 48K: 49179 bytes | 128K: 131103/147487 bytes
  - Z80 State: Alle Register inkl. Alternate Set
  - Memory Layout: Display (6144B) + Attributes (768B) + RAM

- **Amstrad CPC SNA**: Memory Snapshot für CPC Emulatoren
  - Magic: "MV - SNA" | Header: 256 bytes
  - Modelle: CPC 464, CPC 664, CPC 6128
  - Hardware State: Z80, CRTC, Gate Array, PPI, PSG

#### Cartridge Formate
- **C64 CRT**: Commodore 64 Cartridge Images
  - Magic: "C64 CARTRIDGE   " (Big Endian!)
  - CHIP Packets: ROM/RAM/Flash mit Banking
  - 61 Cartridge-Typen (Normal, EasyFlash, Action Replay, ...)
  - CBM80 Autostart-Erkennung

### Tape Formate (v3.8.3-3.8.5)
- **TZX v1.20**: ZX Spectrum Tape mit 24 Block-Typen
- **TAP**: Simple ZX Spectrum Tape Format
- **PZX**: Perfect ZX Tape mit Full PULS Spec (Simple/Extended/Repeat)
- **CSW**: Compressed Square Wave (v1/v2 RLE)
- **C64 TAP**: Commodore 64 Pulse Format (v0/v1/v2, PAL/NTSC)
- **T64**: C64 Tape Archive mit PETSCII

### DDR Tape Formate (v3.8.1-3.8.2)
- **KC85 CAOS FSK**: 8 Dateitypen, 128-byte Header
- **Z1013 Headersave**: 32-byte Header mit XOR Checksum
- **7 Turboloader-Profile**: TURBOTAPE bis BASICODE

### Tests
- 18 Test-Suites
- 518 Sub-Tests
- Alle PASS ✅

---

## v3.7.0 - Initial Release

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
