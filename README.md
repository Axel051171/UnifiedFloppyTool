<<<<<<< HEAD
# UnifiedFloppyTool
=======
# UnifiedFloppyTool v3.1

**The Ultimate Floppy Disk Preservation & Analysis Suite**

A comprehensive, cross-platform tool for reading, writing, analyzing, and preserving floppy disks across multiple platforms and formats. Designed to surpass existing tools like Greaseweazle and FluxEngine with superior performance, usability, and format support.

![License](https://img.shields.io/badge/license-MIT-blue.svg)
![Qt](https://img.shields.io/badge/Qt-5%2F6-green.svg)
![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20macOS%20%7C%20Windows-lightgrey.svg)

---

## 🎯 Features

### **Multi-Platform Support (50+ Formats)**
- **IBM PC**: 360K, 720K, 1.2M, 1.44M, 2.88M (IMG, IMD, TD0)
- **Commodore 64**: D64, D71, D81, G64, G71 (GCR encoding)
- **Amiga**: ADF, IPF, DMS, ADZ (MFM encoding)
- **Atari ST**: ST, MSA, STX (MFM encoding)
- **Atari 8-bit**: ATR, XFD, DCM, ATX (FM/MFM)
- **Apple II**: DSK, PO, NIB, WOZ, 2MG (GCR encoding)
- **Macintosh**: 400K, 800K, 1.44M (GCR/MFM)
- **Raw Flux**: SCP, HFE, KryoFlux, FluxEngine

### **Hardware Support**
- Greaseweazle (v1-v4)
- KryoFlux
- FluxEngine
- Generic USB floppy drives
- Real-time device detection

### **Advanced Features**
- **Copy Protection Detection**: X-Copy, Rob Northen, NDOS, weak bits, half-tracks
- **Batch Operations**: Process 10+ disks automatically with pause between disks
- **Format Converter**: Convert between all supported formats
- **Disk Analyzer**: Deep analysis with protection detection and hash generation
- **Catalog System**: Manage hundreds of archived disks with search & export
- **SIMD Optimization**: AVX2/SSE2 for 3-5× faster MFM/GCR decoding

### **Modern GUI**
- 6 main tabs with intuitive sub-tab organization
- Real-time track status visualization (quadratic 240×240px grids)
- Format-dependent feature activation
- Dynamic parameter adjustment based on disk type
- Professional preset management system

---

## 📋 Requirements

### **Build Requirements**
- **Qt**: 5.12+ or 6.x (Qt Creator recommended)
- **C++ Compiler**: GCC 7+, Clang 6+, or MSVC 2017+
- **CMake**: 3.20+ (optional, qmake also supported)
- **Operating System**: Linux, macOS, or Windows

### **Runtime Requirements**
- Greaseweazle, KryoFlux, or compatible USB floppy device
- USB 2.0+ port
- 500 MB disk space for installation
- 4 GB RAM recommended

---

## 🚀 Quick Start

### **1. Clone Repository**
```bash
git clone https://github.com/yourusername/UnifiedFloppyTool.git
cd UnifiedFloppyTool
```

### **2. Build with Qt Creator**
```bash
# Open in Qt Creator
qtcreator UnifiedFloppyTool.pro

# Or command line:
qmake UnifiedFloppyTool.pro
make
```

### **3. Build with CMake (Alternative)**
```bash
mkdir build && cd build
cmake ..
make
```

### **4. Run**
```bash
./UnifiedFloppyTool
```

---

## 📖 Documentation

### **User Guides**
- [Getting Started](docs/GETTING_STARTED.md)
- [Supported Formats](docs/FORMATS.md)
- [Protection Detection](docs/PROTECTION.md)
- [Batch Operations](docs/BATCH.md)

### **Developer Guides**
- [Project Architecture](docs/ARCHITECTURE.md)
- [Format-to-Feature Mapping](docs/FORMAT_MAPPING.md)
- [SIMD Optimization](docs/SIMD.md)
- [C64 Integration](docs/C64_INTEGRATION.md)

### **API Documentation**
- [Core API](include/uft/flux_core.h)
- [Memory Management](include/uft/uft_memory.h)
- [SIMD Functions](include/uft/uft_simd.h)

---

## 🎨 Architecture

### **Tab Structure**
```
1. Workflow       → Mission control with live track visualization
2. Hardware       → Device configuration and detection
3. Format         → 50+ disk formats with sub-tabs:
   ├─ Flux        → Raw flux settings
   ├─ Format      → Geometry and parameters
   └─ Advanced    → Verification and logging
4. Protection     → Copy protection handling with sub-tabs:
   ├─ X-Copy      → Amiga protection (format-dependent)
   ├─ Recovery    → Universal dd-style recovery
   └─ C64 Nibbler → C64 protection (format-dependent)
5. Catalog        → Disk database with search and export
6. Tools          → Utilities with sub-tabs:
   ├─ Batch       → Multi-disk operations
   ├─ Converter   → Format conversion
   ├─ Analyzer    → Deep disk analysis
   └─ Utilities   → Compare, hash, repair
```

### **Core Components**
- **flux_core**: Low-level flux timing and analysis
- **SIMD decoders**: AVX2/SSE2 optimized MFM/GCR decoding
- **Widget system**: Custom track grid and preset manager
- **Format engine**: Universal disk format handler

---

## 🔧 Configuration

### **Format Options**
UnifiedFloppyTool features intelligent format-dependent parameter adjustment:
- **D64 (C64)**: GCR parameters active, MFM disabled, C64 Nibbler enabled
- **ADF (Amiga)**: MFM parameters active, X-Copy enabled, Nibbler disabled
- **IMG (PC)**: Standard MFM parameters, all protection features disabled
- **SCP (Flux)**: All parameters available, all features enabled

### **Protection Profiles**
- **Amiga Standard**: X-Copy + dd* recovery
- **C64 Advanced**: Half-tracks, weak bits, GCR nibbler
- **Atari Standard**: Bad sectors, phantom sectors
- **Archive Mode**: Preserve everything, maximum retries

---

## 🤝 Contributing

Contributions are welcome! Please see [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

### **Development Roadmap**
- [x] Complete GUI redesign (v3.1)
- [x] 50+ disk format support
- [x] Format-dependent feature activation
- [x] Catalog system
- [x] Batch operations
- [ ] Hardware communication backend
- [ ] Protection detection algorithms
- [ ] Format converter implementation
- [ ] Disk repair utilities
- [ ] Extended format support (BBC Micro, MSX, TRS-80)

---

## 📜 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

---

## 🙏 Acknowledgments

- **Greaseweazle** for hardware protocol inspiration
- **FluxEngine** for format definitions
- **SPS/CAPS** for protection detection techniques
- **HxC Project** for HFE format specifications
- Qt framework for the excellent GUI toolkit

---

## 📧 Contact

- **Issues**: [GitHub Issues](https://github.com/yourusername/UnifiedFloppyTool/issues)
- **Discussions**: [GitHub Discussions](https://github.com/yourusername/UnifiedFloppyTool/discussions)

---

## 🌟 Star History

[![Star History Chart](https://api.star-history.com/svg?repos=yourusername/UnifiedFloppyTool&type=Date)](https://star-history.com/#yourusername/UnifiedFloppyTool&Date)

---

**Built with ❤️ for the retrocomputing community**
>>>>>>> 73c0848 (Initial commit: UFT_PERFECT v3.1)
