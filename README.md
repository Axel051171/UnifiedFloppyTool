# UnifiedFloppyTool (UFT)

**Industrial-Grade Floppy Disk Preservation & Analysis System**

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![C Standard](https://img.shields.io/badge/C-C11-blue.svg)](https://en.wikipedia.org/wiki/C11_(C_standard_revision))
[![Platform](https://img.shields.io/badge/Platform-Linux%20%7C%20macOS%20%7C%20Windows-lightgrey.svg)]()

> *"Bei uns geht kein Bit verloren"* — No bit gets lost

---

## Overview

UFT is a comprehensive toolkit for reading, analyzing, writing, and preserving floppy disk images across **115+ formats** from **20+ platforms** including:

- **Commodore** (C64, C128, VIC-20, Amiga)
- **Atari** (8-bit, ST)
- **Apple** (II, Macintosh)
- **IBM PC** (FAT12, various formats)
- **Japanese** (PC-98, X68000, FM-Towns)
- **British** (BBC Micro, Amstrad CPC, Spectrum)
- **Others** (TRS-80, TI-99, MSX, and more)

## Features

### Core Capabilities
- 🔬 **Forensic Imaging** — Bit-perfect preservation with hash verification
- 🔄 **Format Conversion** — Convert between 100+ disk formats
- 🛡️ **Copy Protection Analysis** — Detect and analyze protection schemes
- 📊 **Flux Analysis** — Direct flux stream processing with PLL decoding
- 🔧 **Recovery Tools** — Recover data from damaged media

### Technical Highlights
- Pure **C11** with no mandatory external dependencies
- Cross-platform (Linux, macOS, Windows)
- Optional **Qt6 GUI** for visual analysis
- Hardware support: Greaseweazle, FluxEngine, KryoFlux, SuperCard Pro

## Quick Start

### Build
```bash
mkdir build && cd build
cmake ..
cmake --build . -j$(nproc)
```

### Basic Usage
```bash
# Analyze a disk image
uft info disk.d64

# Convert formats
uft convert input.adf output.ipf

# Read from hardware
uft read --device gw --format amiga -o disk.adf
```

## Project Structure

```
UnifiedFloppyTool/
├── include/uft/          # Public headers
│   ├── core/             # Core types and utilities
│   ├── formats/          # Format-specific headers
│   └── flux/             # Flux processing
├── src/                  # Implementation
│   ├── core/             # Core modules
│   ├── formats/          # Format handlers
│   ├── algorithms/       # PLL, encoding, CRC
│   └── hardware/         # Hardware drivers
├── tools/                # CLI tools
├── tests/                # Test suite
├── docs/                 # Documentation
└── gui/                  # Qt6 GUI (optional)
```

## Supported Formats

| Platform | Formats |
|----------|---------|
| Commodore | D64, D71, D81, G64, NIB, ADF, IPF |
| Atari | ATR, XFD, DCM, ATX, ST, MSA |
| Apple | DSK, DO, PO, NIB, WOZ |
| IBM PC | IMG, IMA, IMD, TD0, FDI |
| Amstrad | DSK, EDSK |
| Spectrum | TRD, SCL, TAP, TZX |
| Flux | SCP, KF, RAW, A2R |

## Documentation

- [Architecture Guide](docs/ARCHITECTURE.md)
- [API Reference](docs/API_REFERENCE.md)
- [Format Specifications](docs/formats/)
- [Hardware Setup](docs/HARDWARE.md)

## Building with Qt GUI

```bash
cmake -DUFT_BUILD_GUI=ON ..
cmake --build .
```

## License

MIT License — See [LICENSE](LICENSE) for details.

## Contributing

Contributions welcome! Please read [CONTRIBUTING.md](CONTRIBUTING.md) first.

## Acknowledgments

- Software Preservation Society (SPS)
- MAME/MESS team
- Various open-source floppy tools (HxCFloppyEmulator, FluxEngine, SAMdisk)

---

**Version 3.7.0** | [Changelog](CHANGELOG.md) | [Issues](https://github.com/your-repo/issues)
