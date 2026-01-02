# UnifiedFloppyTool v3.2.0 - VISUAL Edition

Cross-platform floppy disk preservation and analysis tool.

![Build Status](https://github.com/Axel051171/UnifiedFloppyTool/actions/workflows/ci.yml/badge.svg)

## ✨ Features

- **Dark Mode Toggle** (Ctrl+D)
- **Status Tab** with Hex Dump viewer
- **Status LED Bar** for visual feedback
- **Disk Analyzer Window** for detailed analysis
- **Recent Files Menu** for quick access
- **Drag & Drop Support** for files
- **Full Keyboard Shortcuts**
- **Format Auto-Detection** (30+ formats)

## 🔧 Supported Formats

| Platform | Formats |
|----------|---------|
| Commodore | D64, G64, D71, D81, NIB |
| Amiga | ADF, IPF, ADZ, DMS |
| Apple | NIB, WOZ, DSK |
| Atari | ATR, XFD, ATX |
| PC/DOS | IMG, IMD, TD0 |
| BBC Micro | SSD, DSD |
| Flux | SCP, HFE, RAW, KF |

## 🏗️ Building

### Prerequisites

- CMake 3.16+
- Qt 6.6+
- C++17 compiler (GCC 9+, Clang 10+, MSVC 2019+)

### Build with CMake (Recommended)

```bash
cmake -B build -G Ninja
cmake --build build --config Release
```

### Build with qmake

```bash
mkdir build && cd build
qmake ../UnifiedFloppyTool.pro CONFIG+=release
make -j$(nproc)
```

## 📦 Downloads

Pre-built binaries are available on the [Releases](https://github.com/Axel051171/UnifiedFloppyTool/releases) page:

- **Windows** (x64) - `.zip`
- **macOS** (x64) - `.tar.gz`
- **Linux** (x64) - `.tar.gz`

## 📄 License

This project is for floppy disk preservation and research purposes.
