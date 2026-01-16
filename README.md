# UnifiedFloppyTool v4.0.0

[![CI Build](https://github.com/Axel051171/UnifiedFloppyTool/actions/workflows/build.yml/badge.svg)](https://github.com/Axel051171/UnifiedFloppyTool/actions/workflows/build.yml)
[![Release](https://img.shields.io/github/v/release/Axel051171/UnifiedFloppyTool)](https://github.com/Axel051171/UnifiedFloppyTool/releases)
[![License](https://img.shields.io/github/license/Axel051171/UnifiedFloppyTool)](LICENSE)

**"Bei uns geht kein Bit verloren"** – The most comprehensive open-source floppy disk preservation tool.

---

## ⬇️ Downloads

| Platform | Download | Notes |
|----------|----------|-------|
| **Windows 64-bit** | [UnifiedFloppyTool-4.0.0-win64.zip](https://github.com/Axel051171/UnifiedFloppyTool/releases/latest) | Portable, no install needed |
| **Linux 64-bit** | [UnifiedFloppyTool-4.0.0-linux-x64.tar.gz](https://github.com/Axel051171/UnifiedFloppyTool/releases/latest) | Requires Qt6 |
| **Linux AppImage** | [UnifiedFloppyTool-4.0.0-x86_64.AppImage](https://github.com/Axel051171/UnifiedFloppyTool/releases/latest) | Self-contained |
| **Debian/Ubuntu** | [unifiedfloppytool_4.0.0_amd64.deb](https://github.com/Axel051171/UnifiedFloppyTool/releases/latest) | `sudo dpkg -i *.deb` |
| **macOS** | [UnifiedFloppyTool-4.0.0-macos.dmg](https://github.com/Axel051171/UnifiedFloppyTool/releases/latest) | Intel & Apple Silicon |

---

## ✨ What's New in v4.0.0

- **DMK Analyzer Panel** – Deep analysis of TRS-80 DMK disk images
- **Flux Histogram** – Real-time timing visualization with encoding detection
- **GW→DMK Direct** – Stream Greaseweazle flux directly to DMK format
- **TI-99/4A Support** – Complete sector formats (FM90, FM128, MFM256)
- **86Box 86F Format** – PC emulator flux format support
- **MAME MFI Format** – MAME floppy image support
- **Enhanced Validation** – `uft check` command for integrity verification

[Full Changelog →](CHANGELOG.md)

---

## 🎯 Features

### 470+ Disk Image Formats

| Platform | Formats |
|----------|---------|
| **Commodore** | D64, D71, D81, G64, G71, NIB, P64, T64 |
| **Amiga** | ADF, ADZ, DMS, HDF, IPF (CAPS/SPS) |
| **Atari** | ATR, ATX, XFD, ST, MSA, STX, Pasti |
| **Apple** | DSK, DO, PO, NIB, WOZ, 2IMG, DC42 |
| **TRS-80** | DMK, JV1, JV3, IMD |
| **TI-99/4A** | V9T9, PC99, FIAD, TIFILES |
| **IBM PC** | IMG, IMA, IMD, TD0, CQM, 86F, MFI |
| **Flux** | SCP, KF, HFE, A2R, MFM, RAW |
| **+ 400 more** | [Complete list →](docs/FORMATS.md) |

### Hardware Controllers

| Controller | Read | Write | Flux | Status |
|------------|:----:|:-----:|:----:|--------|
| Greaseweazle | ✅ | ✅ | ✅ | **Full Support** |
| KryoFlux | ✅ | ✅ | ✅ | **Full Support** |
| SuperCard Pro | ✅ | ✅ | ✅ | **Full Support** |
| FluxEngine | ✅ | ✅ | ✅ | **Full Support** |
| Catweasel | ✅ | ✅ | ✅ | Full Support |
| FC5025 | ✅ | ❌ | ❌ | Read only |
| XUM1541/OpenCBM | ✅ | ✅ | ❌ | C64 drives |
| USB Floppy | ✅ | ✅ | ❌ | Standard formats |

### Copy Protection Detection

Automatically detects and preserves:
- **Commodore**: V-MAX!, RapidLok, Vorpal, GEOS protections
- **Amiga**: Rob Northen Copylock, custom MFM variants
- **Atari ST**: Copylock, Speedlock, Macrodos, Fuzzy sectors
- **Apple II**: Spiral tracking, nibble count, half-tracks
- **PC**: Weak sectors, long tracks, non-standard formats

### Advanced Analysis

- **Flux Histogram** – Visualize timing distributions
- **PLL Analysis** – Phase-locked loop behavior
- **Weak Bit Detection** – Identify copy protection areas
- **ML-Assisted Recovery** – Machine learning for damaged disks

---

## 🚀 Quick Start

### GUI Mode

```bash
# Linux
./UnifiedFloppyTool

# Windows
UnifiedFloppyTool.exe
```

1. Connect your hardware controller
2. Go to **Hardware** tab → Detect device
3. Go to **Workflow** tab → Select Source/Destination
4. Click **▶ START**

### Command Line

```bash
# Read disk to SCP flux image
uft read -d greaseweazle -o disk.scp

# Convert SCP to ADF (Amiga)
uft convert disk.scp disk.adf

# Analyze DMK image
uft analyze disk.dmk

# Check disk integrity  
uft check disk.d64

# View flux histogram
uft hist disk.scp -t 0
```

---

## 🔧 Building from Source

### Requirements

- **CMake** 3.16+ or **qmake**
- **Qt** 6.4+ (Core, Widgets, SerialPort)
- **C++17** compiler (GCC 9+, Clang 10+, MSVC 2019+)
- **libusb** 1.0 (for hardware support)

### Linux (Ubuntu/Debian)

```bash
# Install dependencies
sudo apt update
sudo apt install build-essential cmake \
    qt6-base-dev qt6-tools-dev libqt6serialport6-dev \
    libusb-1.0-0-dev libgl1-mesa-dev

# Clone repository
git clone https://github.com/Axel051171/UnifiedFloppyTool.git
cd UnifiedFloppyTool

# Build with CMake
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# Run
./UnifiedFloppyTool
```

### Windows (Qt/MinGW)

```batch
:: Install Qt 6.4+ with MinGW from qt.io

:: Clone repository
git clone https://github.com/Axel051171/UnifiedFloppyTool.git
cd UnifiedFloppyTool

:: Build with qmake
qmake UnifiedFloppyTool.pro
mingw32-make release

:: Run
release\UnifiedFloppyTool.exe
```

### Windows (Visual Studio)

```batch
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

### macOS

```bash
# Install dependencies
brew install qt@6 cmake libusb

# Clone and build
git clone https://github.com/Axel051171/UnifiedFloppyTool.git
cd UnifiedFloppyTool
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=$(brew --prefix qt@6)
make -j$(sysctl -n hw.ncpu)
```

**Note: "App is damaged" warning on macOS**
```bash
xattr -cr /Applications/UnifiedFloppyTool.app
```

---

## 📚 Documentation

| Document | Description |
|----------|-------------|
| [User Manual](docs/USER_MANUAL.md) | Complete usage guide |
| [Hardware Setup](docs/HARDWARE.md) | Controller configuration |
| [Format Reference](docs/FORMATS.md) | All 470+ supported formats |
| [API Reference](docs/API.md) | Developer documentation |
| [FAQ](docs/FAQ.md) | Frequently asked questions |
| [Changelog](CHANGELOG.md) | Version history |

---

## 🤝 Contributing

We welcome contributions! See [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

### Reporting Issues

Please include:
1. UFT version and platform
2. Hardware controller (if applicable)
3. Steps to reproduce
4. Error messages or screenshots
5. Sample disk image (if possible)

---

## 📄 License

UnifiedFloppyTool is licensed under the [MIT License](LICENSE).

---

## 🙏 Acknowledgments

UFT builds upon the work of:

- [Greaseweazle](https://github.com/keirf/greaseweazle) by Keir Fraser
- [FluxEngine](https://github.com/davidgiven/fluxengine) by David Given
- [HxC Floppy Emulator](https://hxc2001.com/) by Jean-François DEL NERO
- [libdsk](https://github.com/lipro-cpm4l/libdsk) by John Elliott
- [MAME](https://github.com/mamedev/mame) floppy subsystem

---

**Made with ❤️ for the retro computing preservation community**
