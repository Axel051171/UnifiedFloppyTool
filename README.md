# UnifiedFloppyTool (UFT) v3.4.5

[![CI Build](https://github.com/Axel051171/UnifiedFloppyTool/actions/workflows/ci.yml/badge.svg)](https://github.com/Axel051171/UnifiedFloppyTool/actions/workflows/ci.yml)
[![Release](https://github.com/Axel051171/UnifiedFloppyTool/actions/workflows/release.yml/badge.svg)](https://github.com/Axel051171/UnifiedFloppyTool/actions/workflows/release.yml)
[![License: GPL-2.0](https://img.shields.io/badge/License-GPL%202.0-blue.svg)](LICENSE)
[![Version](https://img.shields.io/badge/version-3.4.5-green.svg)](https://github.com/Axel051171/UnifiedFloppyTool/releases/latest)

> **"Bei uns geht kein Bit verloren"** - No bit gets lost

Professional-grade floppy disk preservation and analysis tool supporting 115+ disk formats across 20+ historical computing platforms.

## Features

- **115+ Format Support**: Commodore (D64, G64, NIB), Amiga (ADF, ADZ, DMS), Atari (ATR, ATX, XFD), Apple (DSK, WOZ, 2IMG), PC (IMD, TD0, DMK), and many more
- **Forensic Quality**: Preserve copy protection, weak bits, timing variations
- **Multi-Revolution**: Capture multiple disk revolutions for enhanced recovery
- **Cross-Platform**: Windows, Linux, macOS (ARM64 & x86_64)
- **Qt6 GUI**: Modern interface with Dark Mode support
- **Hardware Support**: Greaseweazle, KryoFlux, FluxEngine, SuperCard Pro, FC5025

## Platform Support

| Platform | Status | Notes |
|----------|--------|-------|
| Linux (x64) | ✅ Fully supported | Ubuntu 22.04+, Debian 12+ |
| Windows (x64) | ✅ Fully supported | Windows 10/11 |
| macOS (ARM64) | ✅ Fully supported | macOS 14 Sonoma+ |
| macOS (x64) | ⚠️ Limited | macOS 12 Monterey+ |

> **Note**: macOS builds require macOS 14+ for full Qt6 compatibility.

## Quick Start

### Download

Get the latest release from [GitHub Releases](https://github.com/Axel051171/UnifiedFloppyTool/releases):

- **Linux**: `uft-v3.3.0-linux-x64.tar.gz` or `uft-v3.3.0-linux-amd64.deb`
- **Windows**: `uft-v3.3.0-windows-x64.zip`
- **macOS**: `uft-v3.3.0-macos-arm64.zip`

### Build from Source

#### Prerequisites

- CMake 3.16+
- Qt 6.5+
- C++17 compiler (GCC 10+, Clang 12+, MSVC 2019+)
- libusb 1.0 (for hardware support)

#### Linux

```bash
# Install dependencies (Ubuntu/Debian)
sudo apt install build-essential cmake qt6-base-dev qt6-tools-dev libusb-1.0-0-dev

# Build
git clone https://github.com/Axel051171/UnifiedFloppyTool.git
cd UnifiedFloppyTool
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# Run
./build/uft
```

#### Windows

```powershell
# Install Qt6 via Qt Installer or vcpkg
# Install CMake

git clone https://github.com/Axel051171/UnifiedFloppyTool.git
cd UnifiedFloppyTool
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release

# Run
.\build\Release\uft.exe
```

#### macOS

```bash
# Install dependencies
brew install qt@6 cmake libusb

# Build
git clone https://github.com/Axel051171/UnifiedFloppyTool.git
cd UnifiedFloppyTool
export Qt6_DIR=$(brew --prefix qt@6)/lib/cmake/Qt6
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(sysctl -n hw.ncpu)

# Run
./build/uft
```

## Usage

### GUI Mode

```bash
./uft
```

### Command Line

```bash
# Show version
./uft --version

# Convert D64 to ADF
./uft convert input.d64 output.adf

# Analyze disk image
./uft analyze disk.g64

# Detect format
./uft detect unknown.img
```

## Documentation

- [Architecture](docs/ARCHITECTURE.md) - System design and data flow
- [Format Catalog](docs/FORMAT_CATALOG.md) - Supported formats reference
- [API Reference](docs/API_REFERENCE.md) - Developer API documentation
- [Parser Guide](docs/PARSER_WRITING_GUIDE.md) - Writing new format parsers

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

## Security

See [SECURITY.md](SECURITY.md) for security policy and reporting vulnerabilities.

## License

This project is licensed under the GPL-2.0 License - see [LICENSE](LICENSE) for details.

## Credits

Maintainer: **Axel Kramer**

Special thanks to:
- HxCFloppyEmulator project
- FluxEngine project
- Greaseweazle project
- MAME floppy preservation team
- All contributors and testers

---

**UnifiedFloppyTool** - Preserving digital history, one floppy at a time.
