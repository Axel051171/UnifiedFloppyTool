# UnifiedFloppyTool

[![Build](https://github.com/Axel051171/UnifiedFloppyTool/actions/workflows/build.yml/badge.svg)](https://github.com/Axel051171/UnifiedFloppyTool/actions/workflows/build.yml)
[![Release](https://img.shields.io/github/v/release/Axel051171/UnifiedFloppyTool)](https://github.com/Axel051171/UnifiedFloppyTool/releases)
[![License](https://img.shields.io/github/license/Axel051171/UnifiedFloppyTool)](LICENSE)

**"Bei uns geht kein Bit verloren"** - A comprehensive floppy disk forensics and preservation tool.

## Downloads

| Platform | Download |
|----------|----------|
| Windows | [UnifiedFloppyTool-x.x.x-windows-x64.tar.gz](https://github.com/Axel051171/UnifiedFloppyTool/releases/latest) |
| macOS | [UnifiedFloppyTool-x.x.x-macos-x64.tar.gz](https://github.com/Axel051171/UnifiedFloppyTool/releases/latest) |
| Linux | [UnifiedFloppyTool-x.x.x-linux-x64.tar.gz](https://github.com/Axel051171/UnifiedFloppyTool/releases/latest) |
| Linux (.deb) | [unifiedfloppytool_x.x.x_amd64.deb](https://github.com/Axel051171/UnifiedFloppyTool/releases/latest) |

## Features

### Format Support
- **Commodore**: C64/128, VIC-20, Plus/4, 1541/1571/1581
- **Amiga**: ADF, OFS, FFS, extended formats
- **Atari**: ST, XL/XE series
- **Apple**: Apple II, Macintosh 400K/800K
- **PC/DOS**: FAT12, DMF, XDF

### Flux-Level Operations
- Read/write raw flux data
- Adaptive PLL with configurable parameters
- Weak bit and no-flux area detection
- Revolution merging and timing analysis

### Protection Detection
- Weak bits, long tracks, half-tracks
- Timing variations, custom sync patterns
- No-flux areas, density variations

### Copy Protection Preservation
- Forensic-grade data preservation
- SHA-256 hashing for verification
- Detailed analysis reports

## Screenshots

![Format Tab](docs/screenshots/format-tab.png)

## Build Requirements

- Qt 6.2+ (tested with Qt 6.6.2)
- CMake 3.16+ or qmake
- C++17 compiler (GCC 9+, MSVC 2019+, Clang 10+)

## Building from Source

### Linux (Ubuntu/Debian)
```bash
sudo apt install qt6-base-dev qt6-serialport-dev build-essential
git clone https://github.com/Axel051171/UnifiedFloppyTool.git
cd UnifiedFloppyTool
qmake6 && make
```

### Windows
```batch
# Install Qt 6.6+ with MinGW
git clone https://github.com/Axel051171/UnifiedFloppyTool.git
cd UnifiedFloppyTool
qmake && mingw32-make
```

### macOS
```bash
brew install qt@6
git clone https://github.com/Axel051171/UnifiedFloppyTool.git
cd UnifiedFloppyTool
qmake && make
```

## Supported Hardware

| Controller | Status |
|------------|--------|
| Greaseweazle (F7/F7+) | ✅ Full Support |
| FluxEngine | ✅ Full Support |
| SuperCard Pro | ✅ Full Support |
| KryoFlux | 🟡 Basic Support |

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

## License

This project is licensed under the terms in the [LICENSE](LICENSE) file.

## Acknowledgments

- [Greaseweazle](https://github.com/keirf/greaseweazle) by Keir Fraser
- [FluxEngine](https://github.com/davidgiven/fluxengine) by David Given
- [HxCFloppyEmulator](https://hxc2001.com/) by Jean-François DEL NERO
