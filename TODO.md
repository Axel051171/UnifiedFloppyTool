# UFT TODO - Single Source of Truth
## Version: 3.9.0 | Updated: 2026-01-13

---

# P0 - KRITISCH ✅ 100%
# P1 - HOCH ✅ 100%
# P2 - MITTEL ✅ 100%
# P3 - BACKLOG ✅ 100%

---

## NEW: Amiga ADF Module (P3-010) ✅

### Files
- **uft_adf.h** (521 Z) - Complete API
- **uft_adf.c** (843 Z) - Implementation
- **test_adf.c** - 17/17 Tests ✅

### Features
- DD (880KB) and HD (1.76MB) support
- OFS/FFS/INTL/DC/LNFS filesystem types
- Volume open/close/info
- Directory listing with hash table traversal
- File header parsing
- Boot block handling
- ADF image creation
- Checksum calculation/verification
- Amiga date ↔ Unix time conversion
- Filename hash calculation
- Protection bits parsing

---

## Apple HFS/HFS+ (P3-009) ✅
- hfs_format.h (740 Z)
- uft_hfs.h (321 Z)
- uft_hfs.c (365 Z)
- 14/14 Tests ✅

---

## FatFs Integration (P3-008) ✅
- FatFs R0.16 Core (8,443 Z)
- UFT Wrapper (640 Z)
- FAT12 floppy support
- 12/12 Tests ✅

---

# SESSION TOTAL v3.9.0

| Komponente | Zeilen |
|------------|--------|
| Amiga ADF | 1,364 |
| Apple HFS | 1,426 |
| FatFs | 9,083 |
| Format Extensions | 738 |
| Config Manager | 1,133 |
| Recovery | 666 |
| Track Visualization | 456 |
| **Session Total** | ~15,000 |

## Tests: 181+

---

## P0 - PARAMETER BRIDGE FIX (NEU)

### Status: ✅ IMPLEMENTIERT (bedingt)

Die syncToBackend()/syncFromBackend() Funktionen wurden implementiert,
aber sind bedingt kompiliert mit `#ifdef UFT_HAS_PARAM_BRIDGE`.

### Um vollständig zu aktivieren:

1. In CMakeLists.txt hinzufügen:
   ```cmake
   add_compile_definitions(UFT_HAS_PARAM_BRIDGE)
   ```

2. Link gegen uft_param_bridge Library

### Geänderte Dateien:
- src/gui/UftParameterModel.h
- src/gui/UftParameterModel.cpp

---

## BUILD-FIX (2026-01-13)

### ✅ Kritische Fehler behoben

1. **HardwareProvider Basis-Klasse erweitert**
   - Neue Typen: `TrackData`, `OperationResult`, `ReadParams`, `WriteParams`
   - Neue virtuelle Methoden: `connect()`, `disconnect()`, `readTrack()`, etc.
   - Neue Signale: `operationError`, `connectionStateChanged`, `progressChanged`
   - Geändert: `src/hardware_providers/hardwareprovider.h`

2. **QSerialPort jetzt required**
   - Geändert: `UnifiedFloppyTool.pro` Zeile 15
   - Alt: `qtHaveModule(serialport): QT += serialport`
   - Neu: `QT += serialport`

3. **Unused Parameter Warnungen behoben**
   - `src/formattab.cpp:687` - updateFluxOptions
   - `src/formattab.cpp:1329` - onDPMAnalysisChanged
   - `src/flux/fdc_bitstream/vfo_fixed.cpp:5` - calc

### ⚠️ Verbleibende Warnungen (nicht kritisch)

- Sign-compare Warnungen in `bit_array.cpp` (int vs size_t)
- Diese sind harmlos und stammen aus externem fdc_bitstream Code

### 📋 Windows Build-Voraussetzungen

Qt SerialPort Modul muss installiert sein:
- Qt Maintenance Tool → Add/Remove Components
- Wähle "Qt Serial Port" unter dem verwendeten Qt Version
