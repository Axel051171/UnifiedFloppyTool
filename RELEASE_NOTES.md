# UnifiedFloppyTool v3.8.0 Release Notes

## Neue Features

### TI-99/4A Formate
- TIFILES (XMODEM) Format Support mit vollständiger Header-Validierung
- FIAD (Files in a Directory) Format für direkte Dateisystem-Speicherung
- V9T9 Format Kompatibilität

### FAT Extensions
- FAT32 Support für MEGA65 und große Medien
- Boot-Template System für verschiedene Plattformen (MS-DOS, FreeDOS, DR-DOS)
- Bad Block Import/Export (CHKDSK, SCANDISK kompatibel)
- Erweiterte Atari ST FAT-Varianten

## Fixes

### Cross-Platform Kompatibilität
- **Windows**: `memmem()` POSIX-Funktion jetzt verfügbar
- **Windows**: `usleep()` korrigiert
- **Linux**: Qt6 QMap/QPair includes in visualdiskdialog.h

### Build System
- Verbesserte CMake Konfiguration
- Test-Suite mit 27 funktionierenden Tests
- CI/CD Pipeline für Windows, macOS, Linux

## Bekannte Einschränkungen

- GUI erfordert Qt6 Installation
- AppImage-Erstellung experimentell
- Einige Test-Module noch in Entwicklung

## Build-Anforderungen

### Linux
```bash
sudo apt install build-essential cmake qt6-base-dev qt6-tools-dev \
    libqt6serialport6-dev libgl1-mesa-dev libusb-1.0-0-dev
```

### macOS
```bash
brew install cmake qt@6
```

### Windows
- Visual Studio 2019/2022 mit C++ Workload
- Qt 6.5+ (MSVC Build)
- CMake 3.20+

## Unterstützte Hardware
- Greaseweazle (alle Versionen)
- FluxEngine
- SuperCard Pro
- KryoFlux
- FC5025
- XUM1541

## Unterstützte Formate
- **Flux**: SCP, HFE, IPF, KryoFlux raw, A2R, WOZ
- **Sector**: ADF, D64, D71, D81, DSK, ST, MSA, IMG, IMA
- **Apple**: DO, PO, NIB, 2MG, WOZ
- **Archiv**: DMS, ADZ, ZIP

---
*UnifiedFloppyTool - Professional Floppy Disk Preservation*
