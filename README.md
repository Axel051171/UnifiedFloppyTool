# UnifiedFloppyTool

[![Release](https://img.shields.io/github/v/release/Axel051171/UnifiedFloppyTool)](https://github.com/Axel051171/UnifiedFloppyTool/releases)
[![Build](https://github.com/Axel051171/UnifiedFloppyTool/actions/workflows/ci.yml/badge.svg)](https://github.com/Axel051171/UnifiedFloppyTool/actions)
[![License: GPL-2.0](https://img.shields.io/badge/License-GPL%202.0-blue.svg)](LICENSE)

**"Kein Bit verloren"** — Open-source forensic floppy disk preservation tool with 212+ format parsers and 11 hardware controllers.

---

## Downloads

**[Latest Release](https://github.com/Axel051171/UnifiedFloppyTool/releases/latest)**

| Platform | File | Notes |
|----------|------|-------|
| macOS | `UnifiedFloppyTool-4.1.3-macOS.dmg` | Universal (Intel + Apple Silicon) |
| Linux | `unifiedfloppytool_4.1.3_amd64.deb` | Ubuntu/Debian |
| Linux | `UnifiedFloppyTool-4.1.3-linux-amd64.tar.gz` | Portable binary |
| Windows | `UnifiedFloppyTool-4.1.3-windows-x64.tar.gz` | Portable, Qt DLLs included |

> **macOS:** Falls "App ist beschadigt" erscheint: `xattr -cr UnifiedFloppyTool.app`

---

## What's New in v4.1.3

### Hardware Protocol Rewrite
Complete audit and rewrite of all hardware controller protocols against official specifications:
- **SuperCard Pro**: Binary SDK v1.7 protocol (was fabricated ASCII)
- **Greaseweazle**: 16 protocol bugs fixed against `gw_protocol.h` v1.23
- **KryoFlux**: DTC subprocess wrapper (proprietary USB protocol)
- **FC5025**: SCSI-like CBW/CSW protocol framework (read-only hardware)
- **Pauline**: HTTP/SSH architecture (DE10-nano FPGA)

### Parser Hardening
- ~610 silent error handling fixes (fseek + fread/fwrite)
- Integer overflow guards in 9 critical parsers (SCP, IPF, A2R, STX, TD0, DMK, D88, IMD, WOZ)
- Real SHA-256 (FIPS 180-4) replacing placeholder hash
- Path traversal security fix (component-level walk)
- Compiler hardening: `-fstack-protector-strong`, `-D_FORTIFY_SOURCE=2`

### CI/CD
- GitHub Actions: Linux (GCC), macOS (Clang), Windows (MinGW)
- Sanitizer workflows (ASan + UBSan)
- Code coverage with Codecov
- Automated release packaging (.deb, .dmg, .tar.gz)

### Copy Protection
- Unified cross-platform detection API (55+ schemes, 10 platforms)
- Track Alignment Module (nibtools-compatible): V-MAX!, RapidLok, Pirate Slayer
- NIB/NB2/NBZ format support

[Full Changelog](CHANGELOG.md)

---

## Features

### 212+ Disk Image Formats

| Platform | Formats |
|----------|---------|
| Commodore | D64, D71, D81, G64, G71, NIB, NB2, T64, CRT, PRG, P00 |
| Amiga | ADF, ADZ, DMS, HDF, IPF (own implementation, no libcaps) |
| Atari ST/8-bit | ATR, ATX, XFD, ST, MSA, STX/Pasti, DCM |
| Apple | DSK, DO, PO, NIB, WOZ (v1/v2/2.1), 2IMG, DC42, EDD |
| IBM PC | IMG, IMA, IMD, TD0, CQM, DMK |
| Amstrad/Spectrum | DSK, EDSK, TRD, SCL, MGT, TAP, TZX |
| BBC/Acorn | SSD, DSD, ADF, UEF |
| Japanese | D88, D77, NFD, HDM, XDF, DIM, FDX |
| Flux | SCP, KryoFlux stream, HFE (v1/v2/v3), A2R, DFI |
| Other | MSX, Thomson, TI-99, Roland, HP LIF, CP/M, Micropolis |

### Hardware Controllers

| Controller | Read | Write | Flux | Notes |
|------------|:----:|:-----:|:----:|-------|
| Greaseweazle | Yes | Yes | Yes | Protocol v1.23, 72MHz capture |
| SuperCard Pro | Yes | Yes | Yes | SDK v1.7, 25MHz capture |
| KryoFlux | Yes | Limited | Yes | Via DTC tool, write conditional |
| ADF-Copy | Yes | Yes | Yes | Amiga specialist (ADF + SCP flux) |
| Applesauce | Yes | Yes | Yes | Text-based serial protocol |
| XUM1541/ZoomFloppy | Yes | Yes | No | OpenCBM IEC bus (C64/1541) |
| FluxEngine | Yes | Yes | Yes | Via libfluxengine |
| FC5025 | Yes | No | No | CLI wrapper (read-only HW) |
| Pauline | Yes | No | Yes | HTTP/SSH (DE10-nano FPGA), not in default build |
| Catweasel | Yes | No | Yes | Minimal (stub provider) |
| USB Floppy | Yes | Yes | No | UFI SCSI pass-through (Linux SG_IO + Windows) |

### Copy Protection Analysis

Automatic detection and preservation of 55+ protection schemes:

- **Commodore** -- V-MAX!, RapidLok, Vorpal, Pirate Slayer, GEOS
- **Amiga** -- Rob Northen Copylock, custom MFM
- **Atari ST** -- Copylock, Speedlock, Macrodos, Fuzzy sectors
- **Apple II** -- Spiral tracking, nibble count, half-tracks
- **PC** -- Weak sectors, long tracks, non-standard formats

### Analysis Tools

- Flux timing histogram with encoding auto-detection
- PLL phase analysis and clock recovery
- Track alignment (V-MAX!, RapidLok, Pirate Slayer)
- Weak bit and copy protection mapping
- Sector-level hex editor
- Side-by-side disk comparison

---

## Quick Start

### GUI

```bash
./UnifiedFloppyTool          # Linux
open UnifiedFloppyTool.app   # macOS
UnifiedFloppyTool.exe        # Windows
```

1. **Hardware** tab: select controller, click Connect
2. **Workflow** tab: configure source/destination
3. Click **Start**

---

## Building from Source

### Requirements

- Qt 6.5+ (Core, Widgets, SerialPort)
- C++17 compiler (GCC 9+, Clang 10+, MSVC 2019+)
- libusb 1.0 (optional, for hardware support)

### Linux (Ubuntu/Debian)

```bash
sudo apt install build-essential qt6-base-dev qt6-tools-dev \
    libqt6serialport6-dev libusb-1.0-0-dev libgl1-mesa-dev

git clone https://github.com/Axel051171/UnifiedFloppyTool.git
cd UnifiedFloppyTool

mkdir build && cd build
qmake ../UnifiedFloppyTool.pro CONFIG+=release
make -j$(nproc)
```

### macOS

```bash
brew install qt@6 libusb

git clone https://github.com/Axel051171/UnifiedFloppyTool.git
cd UnifiedFloppyTool
mkdir build && cd build
qmake ../UnifiedFloppyTool.pro CONFIG+=release
make -j$(sysctl -n hw.ncpu)
```

### Windows (MinGW)

```batch
:: Install Qt 6.5+ from qt.io (MinGW kit)
git clone https://github.com/Axel051171/UnifiedFloppyTool.git
cd UnifiedFloppyTool
mkdir build && cd build
qmake ..\UnifiedFloppyTool.pro CONFIG+=release
mingw32-make -j%NUMBER_OF_PROCESSORS%
```

CMake is also supported for tests -- see [CMakeLists.txt](CMakeLists.txt).

---

## udev Rules (Linux)

For direct USB access to floppy controllers without root:

```bash
sudo cp tools/99-floppy-devices.rules /etc/udev/rules.d/
sudo udevadm control --reload-rules
```

---

## Documentation

| Document | Description |
|----------|-------------|
| [Changelog](CHANGELOG.md) | Full version history |
| [Contributing](CONTRIBUTING.md) | How to contribute |

---

## Acknowledgments

UFT builds upon the work of these projects:

- [Greaseweazle](https://github.com/keirf/greaseweazle) -- Keir Fraser
- [FluxEngine](https://github.com/davidgiven/fluxengine) -- David Given
- [HxC Floppy Emulator](https://hxc2001.com/) -- Jean-Francois DEL NERO
- [Pauline](https://github.com/jfdelnero/Pauline) -- Jean-Francois DEL NERO
- [libdsk](https://github.com/lipro-cpm4l/libdsk) -- John Elliott
- [MAME](https://github.com/mamedev/mame) floppy subsystem
- [samdisk](https://simonowen.com/samdisk/) -- Simon Owen
- [OpenCBM](https://github.com/opencbm/opencbm) -- CBM community

---

## License

GPL-2.0 -- see [LICENSE](LICENSE) for details.
