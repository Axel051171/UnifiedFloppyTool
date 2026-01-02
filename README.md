# UnifiedFloppyTool v3.2.0

<p align="center">
  <b>A comprehensive floppy disk preservation and analysis tool</b>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/version-3.2.0-blue.svg" alt="Version">
  <img src="https://img.shields.io/badge/Qt-6.6+-green.svg" alt="Qt">
  <img src="https://img.shields.io/badge/license-GPL--3.0-orange.svg" alt="License">
  <img src="https://img.shields.io/badge/platform-Win%20%7C%20Linux%20%7C%20macOS-lightgrey.svg" alt="Platform">
</p>

---

## ✨ What's New in v3.2.0

### 🌙 Dark Mode
Toggle with `Ctrl+D` or `Settings → Dark Mode`

### 📊 Status Tab
- Real-time sector info with hex dump
- Tool buttons: Label Editor, BAM/FAT Viewer, Bootblock, Protection Analyzer

### 💡 Status LED
- Visual indicator for hardware connection status
- Colors: Gray → Green → Orange → Red

### 🔬 Disk Analyzer
- HxC-style visualization
- Side 0 / Side 1 views
- Track analysis format filters

### ⌨️ Keyboard Shortcuts
| Key | Action |
|-----|--------|
| `Ctrl+O` | Open |
| `Ctrl+S` | Save |
| `Ctrl+D` | Dark Mode |
| `F5` | Read Disk |
| `F6` | Write Disk |
| `F8` | Analyze |

### 📁 Drag & Drop
Drop disk images directly onto the window

### 🕐 Recent Files
Quick access to last 10 opened files

---

## 📥 Downloads

Pre-built binaries for all platforms:

| Platform | Download |
|----------|----------|
| **Windows x64** | [UnifiedFloppyTool-Windows-x64.zip](../../releases/latest) |
| **Linux x64** | [UnifiedFloppyTool-Linux-x64.tar.gz](../../releases/latest) |
| **macOS x64** | [UnifiedFloppyTool-macOS-x64.dmg](../../releases/latest) |

Or build from source (see below).

---

## 🖥️ Screenshots

### Light Mode
```
┌─ UnifiedFloppyTool v3.2.0 ──────────────────────────────────────┐
│ ● No hardware connected              No image loaded            │
├─────────────────────────────────────────────────────────────────┤
│ [Workflow] [Status] [Hardware] [Settings] [Catalog] [Tools]     │
│                                                                 │
│ ┌─ SOURCE ─────────────────────────────────────────────────────┐│
│ │ ◉ Physical Drive  ○ Image File                               ││
│ │ Drive: [Greaseweazle F7 ▼]  [Detect]                         ││
│ └──────────────────────────────────────────────────────────────┘│
└─────────────────────────────────────────────────────────────────┘
```

---

## 💾 Supported Formats

### Sector Images
| Platform | Formats |
|----------|---------|
| Commodore | D64, G64, D71, D81, D80, D82 |
| Amiga | ADF, ADZ, DMS, HDF |
| Apple | NIB, WOZ, DSK, DO, PO |
| Atari | ATR, XFD, DCM, ATX |
| PC/IBM | IMG, IMA, IMD, TD0 |

### Flux Images
| Format | Description |
|--------|-------------|
| SCP | SuperCard Pro |
| HFE | HxC Floppy Emulator |
| RAW | Greaseweazle Raw |
| KF | KryoFlux Stream |

---

## 🔌 Supported Hardware

| Controller | Status |
|------------|--------|
| Greaseweazle (F1/F7) | ✅ Full support |
| SuperCard Pro | ✅ Full support |
| KryoFlux | ✅ Full support |
| FluxEngine | ✅ Full support |
| FC5025 | ✅ Full support |
| USB Floppy Drive | ✅ Basic support |
| XUM1541 (C64 IEC) | ✅ Full support |

---

## 🛠️ Building from Source

### Prerequisites

- **Qt 6.6+** (or Qt 5.15+)
- **C++17** compiler
- **CMake 3.20+** or **qmake**

### Windows (MSVC)

```batch
mkdir build && cd build
qmake ../UnifiedFloppyTool.pro CONFIG+=release
nmake
```

### Linux

```bash
sudo apt install qt6-base-dev build-essential
mkdir build && cd build
qmake ../UnifiedFloppyTool.pro CONFIG+=release
make -j$(nproc)
```

### macOS

```bash
brew install qt@6
mkdir build && cd build
qmake ../UnifiedFloppyTool.pro CONFIG+=release
make -j$(sysctl -n hw.ncpu)
macdeployqt UnifiedFloppyTool.app -dmg
```

---

## 📖 Documentation

- [Features Overview](docs/FEATURES_v3.1.md)
- [Validation System](docs/VALIDATION.md)
- [Drive Detection](docs/DRIVE_DETECTION.md)
- [C64 Integration](docs/C64_INTEGRATION.md)

---

## 🤝 Contributing

Contributions are welcome! Please see [CONTRIBUTING.md](CONTRIBUTING.md).

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Submit a pull request

---

## 📄 License

This project is licensed under the **GPL-3.0 License** - see [LICENSE](LICENSE) for details.

---

## 👤 Author

**Axel Kramer**

- GitHub: [@axelmuhr](https://github.com/Axel051171)

---

## 🙏 Acknowledgments

- HxC Floppy Emulator project
- Greaseweazle project
- KryoFlux team
- All the retro computing community

---

<p align="center">
  Made with ❤️ for the retro computing community
</p>
