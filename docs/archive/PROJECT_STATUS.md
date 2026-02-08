# Unified Floppy Tool (UFT) - Project Status

## Overview

UFT is a comprehensive media preservation tool supporting:
- Floppy disk preservation (primary)
- Nintendo Switch cartridge dumping (MIG Dumper integration)
- Multiple hardware interfaces

## Statistics

| Metric | Count |
|--------|-------|
| Source Files | 1,810 |
| Header Files | 1,317 |
| UI Forms | 18 |
| **Total LOC** | **1,013,911** |

## Components

### 1. Floppy Preservation (95% Complete)

| Category | Count | Examples |
|----------|-------|----------|
| HAL Providers | 10 | Greaseweazle, KryoFlux, SuperCard Pro |
| Disk Formats | 50+ | ADF, D64, DMK, SCP, IPF, WOZ |
| Filesystems | 15+ | AmigaDOS, C64, CP/M, FAT |
| Protection | 28 | Copylock, Speedlock, RapidLok |

### 2. Nintendo Switch / MIG Dumper (100% Complete)

```
src/switch/
├── uft_mig_dumper.c/h     - MIG Hardware Interface
├── uft_switch_types.h     - XCI/NCA Types
├── uft_xci_parser.h       - XCI Parser API
└── hactool/               - Full hactool (~125k LOC)

include/uft/nintendo/
├── uft_xci.h              - XCI Gamecard Format
├── uft_nca.h              - NCA Content Archive
├── uft_hfs.h              - Hash FS Format
└── uft_nx_usb.h           - nxdumptool USB Protocol

src/gui/
└── uft_switch_panel.cpp/h - Full Qt GUI (756 LOC)
```

**Features:**
- MIG Dumper device discovery & connection
- Cartridge detection & authentication
- XCI dumping with progress display
- Certificate & Card UID export
- XCI file browser
- Partition extraction

**USB Protocols:**
- MIG Dumper (ESP32-S2)
- nxdumptool ABI v1.2

### 3. UFI Protocol V3.1.1 (100% Complete)

- 96 Commands, 22 Events, 45 Capabilities
- STM32H723 firmware headers
- CM5 Python processing module
- Qt wrapper classes

## Build Requirements

- Qt 6.5+
- CMake 3.16+ or qmake
- C++17 compiler
- libusb-1.0

## License

Mixed licensing:
- Core UFT code: Proprietary/GPL
- hactool integration: ISC
- nxdumptool structures: GPL-3.0
