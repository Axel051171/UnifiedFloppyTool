# UnifiedFloppyTool v4.0 - Merged Edition

## What Changed?

This is a **merged version** that combines:

- **GUI** from UFT_PERFECT_v3.1 (Qt Designer-based interface)
- **Backend** from UnifiedFloppyTool_COMPLETE (industrial-grade libraries)

### Added Components

1. **libflux_core** (1.1 MB)
   - Universal Flux Model (UFM)
   - MFM/GCR decoding
   - C64 protection analysis (80+ traits!)
   - Flux consensus algorithms
   - Parameter compensation

2. **libflux_format** (986 KB)
   - 105+ format converters
   - Auto-detection via VTable
   - Formats: ADF, D64, WOZ, HFE, SCP, ATR, IMD, TD0, and more!

3. **libflux_hw** (147 KB)
   - Hardware abstraction layer
   - Backends: KryoFlux, FluxEngine, Applesauce, XUM1541

4. **libflux_unified** (33 KB)
   - Unified API layer

5. **Documentation** (230 KB)
   - Comprehensive specs
   - Integration guides
   - Bootblock documentation

6. **Examples** (22 files, 278 KB)
   - Working code examples
   - Each library demonstrated

### Removed Components

- Old `src/core/` directory (superseded by libflux_core)
- Old `include/uft/` directory (superseded by library headers)

## Building

### Quick Start

```bash
./build.sh
./build/UnifiedFloppyTool
```

### Manual Build

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
./UnifiedFloppyTool
```

### Dependencies

**Debian/Ubuntu:**
```bash
sudo apt-get install build-essential cmake qt6-base-dev qt6-tools-dev libusb-1.0-0-dev
```

**Fedora:**
```bash
sudo dnf install gcc-c++ cmake qt6-qtbase-devel qt6-qttools-devel libusb-devel
```

**macOS:**
```bash
brew install cmake qt@6 libusb
```

## Architecture

```
UnifiedFloppyTool/
├── src/                   # Qt GUI (from UFT_PERFECT_v3.1)
├── forms/                 # Qt .ui files
├── libflux_core/          # Core flux processing (NEW!)
├── libflux_format/        # Format converters (NEW!)
├── libflux_hw/            # Hardware backends (NEW!)
├── libflux_unified/       # Unified API (NEW!)
├── format_modules/        # Additional formats (NEW!)
├── docs/                  # Documentation (MERGED)
├── examples/              # Code examples (NEW!)
└── CMakeLists.txt         # Build system (NEW!)
```

## Features

### GUI (Qt 6)
- Tab-based interface (6 tabs)
- Workflow tab (fully functional)
- Worker thread for long operations
- Dark mode support (optional)
- Visual disk analyzer
- Preset manager

### Backend (C Libraries)
- **70,000+ lines** of professional C code
- **105+ disk formats** supported
- **80+ C64 protection traits** detected
- **6 hardware backends** (KryoFlux, FluxEngine, etc.)
- **Research-grade** flux preservation
- **Nanosecond precision** flux timings
- **Multi-revolution archival**

## Next Steps

1. **Build the project** (see above)
2. **Test GUI** (workflow tab should work)
3. **Integrate GUI with backend** (connect tabs to libflux_* APIs)
4. **Run examples** (learn the API)
5. **Write tests** (ensure quality)

## Known Issues

- GUI tabs 2-6 are stubs (need implementation)
- Backend not yet integrated with GUI (needs glue code)
- Some examples may need updates

## Documentation

- See `docs/` for comprehensive documentation
- See `examples/` for code examples
- See `libflux_*/include/` for API headers

## License

See LICENSE file.

## Version

- GUI Base: v3.1.3 (UFT_PERFECT_v3.1)
- Backend: v2.8.0+ (UnifiedFloppyTool_COMPLETE)
- Merged: v4.0.0 (This version)

---

**Happy Flux Preservation! 💾🔬**
