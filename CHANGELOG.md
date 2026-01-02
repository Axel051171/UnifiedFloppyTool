# Changelog

All notable changes to UnifiedFloppyTool will be documented in this file.

## [3.2.0] - 2026-01-02

### 🎨 New User Interface Features

#### Dark Mode
- **Toggle**: `Settings → Dark Mode` or `Ctrl+D`
- Complete dark theme for all widgets
- Setting saved and restored on restart

#### Status Tab (NEW)
- Real-time Track/Side display
- Sector information panel (ID, CRC, Datamark, etc.)
- Hex dump viewer with scroll
- 4 Tool buttons:
  - 🏷️ Label Editor
  - 📊 BAM/FAT Viewer
  - 🥾 Bootblock Viewer
  - 🛡️ Protection Analyzer

#### Status LED Bar
- Hardware connection indicator at top of window
- Colors: Gray (disconnected), Green (connected), Orange (busy), Red (error)
- Shows current loaded image info

#### Disk Analyzer Window
- HxC-style disk visualization
- Side 0 / Side 1 views
- Track/Sector selection
- Hex dump with ASCII
- Track analysis format filters (ISO MFM, Amiga MFM, C64 GCR, Apple II, etc.)

### 🔧 Usability Improvements

#### Drag & Drop
- Drop disk images directly onto main window to open

#### Recent Files Menu
- `File → Recent Files` - Last 10 opened files
- Quick access to previously used images
- Clear history option

#### Keyboard Shortcuts
| Shortcut | Action |
|----------|--------|
| `Ctrl+O` | Open |
| `Ctrl+S` | Save |
| `Ctrl+Shift+S` | Save As |
| `Ctrl+D` | Toggle Dark Mode |
| `F1` | Help |
| `F2` | Connect Hardware |
| `F5` | Read Disk |
| `F6` | Write Disk |
| `F7` | Verify Disk |
| `F8` | Analyze |
| `Alt+F4` | Exit |

#### Complete Menu Structure
```
File     Drive       Tools              Settings     Help
├─Open   ├─Connect   ├─Convert          ├─Dark Mode  ├─Help
├─Save   ├─Disconnect├─Compare          ├─Language   ├─Shortcuts
├─Save As├─Read Disk ├─Repair           └─Preferences└─About
├─Recent ├─Write Disk├─Analyze
└─Exit   ├─Verify    ├─Label Editor
         ├─Motor On  ├─BAM Viewer
         └─Motor Off ├─Bootblock Viewer
                     ├─Protection Analyzer
                     └─Checksum Database
```

#### Multi-Language Support
- Built-in: English, Deutsch, Français
- Load external language files for other languages

### 🪟 Window Behavior
- Sub-windows now follow main window when moved
- `Qt::Tool` flag for consistent behavior

### 🛠️ Build System
- GitHub Actions CI/CD for automatic builds
- Pre-built releases for Windows, Linux, macOS
- Qt 6.6.0 support

---

## [3.1.3] - 2024-12-29

### Fixed
- Cross-platform path handling
- Memory management improvements
- Build system fixes for all platforms

---

## [3.1.0] - 2024-12-28

### Added
- Initial PERFECT Edition release
- 10 Tabs with 175+ parameters
- 11 Protection Profiles
- DiskDupe (dd*) detection
- Expert Mode
- Batch Operations
- Disk Catalog
- Comparison Tool
- Health Analyzer

---

## Supported Platforms

| Platform | Build Status |
|----------|--------------|
| Windows x64 | ✅ MSVC 2019/2022 |
| Linux x64 | ✅ GCC 11+ |
| macOS x64 | ✅ Clang (Apple Silicon via Rosetta) |

## Supported Disk Formats

### Commodore
- D64, G64, D71, D81, D80, D82

### Amiga
- ADF (OFS/FFS), ADZ, DMS, HDF

### Apple
- NIB, WOZ, DSK, DO, PO

### Atari
- ATR, XFD, DCM, ATX

### PC/IBM
- IMG, IMA, IMD, TD0

### Flux Formats
- SCP, HFE, RAW, KF, CT

---

## Contributors
- Axel Muhr (Lead Developer)
