# UnifiedFloppyTool

[![Release](https://img.shields.io/github/v/release/Axel051171/UnifiedFloppyTool)](https://github.com/Axel051171/UnifiedFloppyTool/releases)
[![Build](https://github.com/Axel051171/UnifiedFloppyTool/actions/workflows/release.yml/badge.svg)](https://github.com/Axel051171/UnifiedFloppyTool/actions)
[![License: GPL-2.0](https://img.shields.io/badge/License-GPL%202.0-blue.svg)](LICENSE)
[![Coverage](https://codecov.io/gh/Axel051171/UnifiedFloppyTool/branch/main/graph/badge.svg)](https://codecov.io/gh/Axel051171/UnifiedFloppyTool)

**„Bei uns geht kein Bit verloren"** — Open-source forensic floppy disk preservation for 700+ formats and 8 hardware controllers.

---

## Downloads

**[→ Latest Release](https://github.com/Axel051171/UnifiedFloppyTool/releases/latest)**

| Platform | File | Notes |
|----------|------|-------|
| macOS | `UnifiedFloppyTool-x.x.x-macOS.dmg` | Universal (Intel + Apple Silicon) |
| macOS | `UnifiedFloppyTool-x.x.x-macOS.tar.gz` | .app bundle |
| Linux | `unifiedfloppytool_x.x.x_amd64.deb` | Ubuntu/Debian: `sudo dpkg -i *.deb` |
| Linux | `UnifiedFloppyTool-x.x.x-linux-amd64.tar.gz` | Portable binary |
| Windows | `UnifiedFloppyTool-x.x.x-windows-x64.tar.gz` | Portable, Qt DLLs included |

> **macOS:** Falls „App ist beschädigt" erscheint: `xattr -cr UnifiedFloppyTool.app`

---

## What's New in v4.2.0

### DeepRead Forensic Analysis
- 5 forensic modules: Write-Splice Detection, Magnetic Aging, Cross-Track Correlation, Revolution Fingerprint, Soft-Decision LLR
- 3 decode boosters: Adaptive Decode, Weighted Voting, Encoding Boost
- GUI: DeepRead panel with Lite/Full mode, persistent settings

### ML-Based Analysis
- Anomaly detection (autoencoder-inspired statistical scoring)
- Copy protection classifier (15 known scheme signatures)
- Bayesian format detection

### New Format Parsers (9)
- FDS (Famicom Disk System), CHD (MAME), NDIF (Mac DiskCopy 6.x)
- EDD (Essential Data Duplicator), DART (Mac compressed), Aaru (DICF)
- HxCStream (HxC native flux), 86F (86Box), SaveDskF (IBM OS/2)

### Hardware Controllers
- FC5025 USB 5.25" controller (full implementation)
- XUM1541/ZoomFloppy with OpenCBM IEC bus protocol
- Applesauce with text-based USB serial protocol

### Copy Protection (55+ schemes)
- Unified cross-platform detection API (10 platforms)
- 19 new scheme detectors (C64, Amiga, Atari ST)
- ML-based unknown protection detection

### Forensic Features
- Triage Mode: 10-second quick quality assessment
- Sector Compare: byte-for-byte disk image diff
- Provenance Chain: SHA-256 cryptographic chain-of-custody
- Recovery Wizard: guided step-by-step recovery workflow
- Format Suggestion: intelligent export format recommendation

### GUI Overhaul
- Application state machine (8 states, mutual exclusion)
- Smart Export Dialog with preservation flags
- Recovery Wizard (QWizard, 5 steps)
- Drag & Drop, keyboard shortcuts, status bar info

### Parser Hardening
- ~610 silent error handling fixes (fseek + fread/fwrite)
- Integer overflow guards in 9 critical parsers
- Real SHA-256 (FIPS 180-4) replacing fake hash
- Path traversal security fix

### CI/CD
- Sanitizer workflows (ASan + UBSan)
- Code coverage with Codecov
- SIMD runtime detection (9 dispatch points)

→ [Full Changelog](CHANGELOG.md)

---

## Features

### 700+ Disk Image Formats

| Platform | Formats |
|----------|---------|
| Commodore | D64, D71, D81, G64, G71, NIB, P64, T64 |
| Amiga | ADF, ADZ, DMS, HDF, IPF (CAPS/SPS) |
| Atari | ATR, ATX, XFD, ST, MSA, STX, Pasti |
| Apple | DSK, DO, PO, NIB, WOZ, 2IMG, DC42 |
| TRS-80 | DMK, JV1, JV3, IMD |
| TI-99/4A | V9T9, PC99, FIAD, TIFILES |
| IBM PC | IMG, IMA, IMD, TD0, CQM, 86F, MFI |
| CP/M | Various (YDSK, SIMH, MYZ80, RCPMFS) |
| Flux | SCP, KryoFlux stream, HFE, A2R, DFI, RAW |
| Nintendo | NDS, 3DS, Switch (NSP/XCI) |

### Hardware Controllers

| Controller | Read | Write | Flux | Protocol Source |
|------------|:----:|:-----:|:----:|----------------|
| Greaseweazle | ✅ | ✅ | ✅ | `gw_protocol.h` v1.23 |
| SuperCard Pro | ✅ | ✅ | ✅ | SDK v1.7 + samdisk |
| KryoFlux | ✅ | ⚠️ | ✅ | DTC tool (official) |
| FluxEngine | ✅ | ✅ | ✅ | libfluxengine |
| FC5025 | ✅ | ❌ | ❌ | Driver v1309 (read-only HW) |
| Pauline | ✅ | ⚠️ | ✅ | HTTP/SSH (DE10-nano) |
| XUM1541/OpenCBM | ✅ | ✅ | ❌ | OpenCBM library |
| USB Floppy | ✅ | ✅ | ❌ | Standard block I/O |

⚠️ = limited / firmware-dependent

### Copy Protection Analysis

Automatic detection and preservation of protection schemes:

- **Commodore** — V-MAX!, RapidLok, Vorpal, Pirate Slayer, GEOS
- **Amiga** — Rob Northen Copylock, custom MFM
- **Atari ST** — Copylock, Speedlock, Macrodos, Fuzzy sectors
- **Apple II** — Spiral tracking, nibble count, half-tracks
- **PC** — Weak sectors, long tracks, non-standard formats

### Analysis Tools

- Flux timing histogram with encoding auto-detection
- PLL phase analysis and clock recovery
- Track alignment (V-MAX!, RapidLok, Pirate Slayer)
- Weak bit and copy protection mapping
- Sector-level hex editor
- Side-by-side disk comparison
- Session management for long preservation runs

---

## Quick Start

### GUI

```bash
./UnifiedFloppyTool          # Linux
open UnifiedFloppyTool.app   # macOS
UnifiedFloppyTool.exe        # Windows
```

1. **Hardware** tab → select controller → Connect
2. **Workflow** tab → configure source/destination
3. Click **Start**

### CLI

```bash
uft read -d greaseweazle -o disk.scp      # Read to flux image
uft convert disk.scp disk.adf             # SCP → ADF (Amiga)
uft analyze disk.dmk                       # Analyze DMK image
uft check disk.d64                         # Integrity check
```

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

### Windows (MSVC)

```batch
:: Install Qt 6.5+ from qt.io (MSVC kit)
git clone https://github.com/Axel051171/UnifiedFloppyTool.git
cd UnifiedFloppyTool
mkdir build && cd build
qmake ..\UnifiedFloppyTool.pro CONFIG+=release
nmake
```

CMake is also supported as alternative build system — see [CMakeLists.txt](CMakeLists.txt).

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
| [User Manual](docs/USER_MANUAL.md) | Complete usage guide |
| [Hardware Setup](docs/HARDWARE.md) | Controller wiring and configuration |
| [FAQ](docs/FAQ.md) | Frequently asked questions |
| [API Reference](docs/API.md) | Developer documentation |
| [Changelog](CHANGELOG.md) | Full version history |
| [Contributing](CONTRIBUTING.md) | How to contribute |

---

## Acknowledgments

UFT builds upon the work of these projects:

- [Greaseweazle](https://github.com/keirf/greaseweazle) — Keir Fraser
- [FluxEngine](https://github.com/davidgiven/fluxengine) — David Given
- [HxC Floppy Emulator](https://hxc2001.com/) — Jean-François DEL NERO
- [Pauline](https://github.com/jfdelnero/Pauline) — Jean-François DEL NERO
- [libdsk](https://github.com/lipro-cpm4l/libdsk) — John Elliott
- [MAME](https://github.com/mamedev/mame) floppy subsystem
- [samdisk](https://simonowen.com/samdisk/) — Simon Owen
- [OpenCBM](https://github.com/opencbm/opencbm) — CBM community

---

## License

[GPL-2.0-or-later](LICENSE) — Copyright © 2024–2026 Axel Kramer
