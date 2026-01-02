# UnifiedFloppyTool - BUILD ANLEITUNG

**Version:** 4.2.0 (Cleaned)  
**Plattformen:** Linux, macOS, Windows  

---

## 🚀 QUICK START

### Linux (Ubuntu/Debian)

```bash
# 1. Dependencies installieren
sudo apt update
sudo apt install -y \
    build-essential \
    cmake \
    qt6-base-dev \
    libusb-1.0-0-dev \
    pkg-config

# 2. Projekt entpacken
tar xzf UFT_MERGED_CLEANED.tar.gz
cd UFT_MERGED

# 3. Bauen
./build.sh

# 4. Starten
./build/UnifiedFloppyTool
```

---

### macOS

```bash
# 1. Dependencies (Homebrew)
brew install cmake qt@6 libusb pkg-config

# 2. Qt Pfad setzen
export PATH="/opt/homebrew/opt/qt@6/bin:$PATH"
export CMAKE_PREFIX_PATH="/opt/homebrew/opt/qt@6"

# 3. Projekt entpacken
tar xzf UFT_MERGED_CLEANED.tar.gz
cd UFT_MERGED

# 4. Bauen
./build.sh

# 5. Starten
./build/UnifiedFloppyTool
```

---

### Windows (MSYS2/MinGW)

```bash
# 1. MSYS2 installieren: https://www.msys2.org/

# 2. Dependencies in MSYS2 MinGW64 Terminal
pacman -S mingw-w64-x86_64-gcc \
          mingw-w64-x86_64-cmake \
          mingw-w64-x86_64-qt6-base \
          mingw-w64-x86_64-libusb

# 3. Projekt entpacken und bauen
tar xzf UFT_MERGED_CLEANED.tar.gz
cd UFT_MERGED
mkdir build && cd build
cmake .. -G "MinGW Makefiles"
mingw32-make -j4

# 4. Starten
./UnifiedFloppyTool.exe
```

---

## 📋 DEPENDENCIES

### Minimum

| Package | Version | Zweck |
|---------|---------|-------|
| CMake | 3.16+ | Build-System |
| GCC/Clang | 9+ | C/C++ Compiler |
| Qt6 oder Qt5 | 6.2+ / 5.15+ | GUI Framework |

### Optional (empfohlen)

| Package | Zweck |
|---------|-------|
| libusb-1.0 | Hardware-Zugriff (Greaseweazle!) |
| pkg-config | Library Detection |

---

## 🔧 BUILD OPTIONEN

### CMake Optionen

```bash
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \     # Release/Debug
    -DUFT_BUILD_GUI=ON \             # GUI bauen (default: ON)
    -DUFT_BUILD_EXAMPLES=ON \        # Examples bauen
    -DUFT_BUILD_TESTS=ON \           # Tests bauen
    -DUFT_ENABLE_SIMD=ON             # SIMD Optimierungen
```

### Nur Libraries (ohne GUI)

```bash
cmake .. -DUFT_BUILD_GUI=OFF
```

### Debug Build

```bash
cmake .. -DCMAKE_BUILD_TYPE=Debug
```

---

## ⚠️ TROUBLESHOOTING

### Problem: Qt nicht gefunden

```
CMake Error: Could not find Qt6
```

**Lösung Linux:**
```bash
sudo apt install qt6-base-dev
# oder für Qt5:
sudo apt install qtbase5-dev
```

**Lösung macOS:**
```bash
brew install qt@6
export CMAKE_PREFIX_PATH="/opt/homebrew/opt/qt@6"
```

---

### Problem: libusb nicht gefunden

```
-- libusb: Not found
```

**Lösung Linux:**
```bash
sudo apt install libusb-1.0-0-dev
```

**Lösung macOS:**
```bash
brew install libusb
```

> **Hinweis:** Ohne libusb funktioniert der Hardware-Zugriff (Greaseweazle etc.) nicht!

---

### Problem: Compiler Fehler

```
error: unknown type name 'uft_rc_t'
```

**Lösung:** Header include Pfad prüfen
```bash
# Im Code sollte stehen:
#include "uft/uft_error.h"
# NICHT:
#include "uft_error.h"
```

---

### Problem: Link Fehler

```
undefined reference to `uft_flux_to_bits_pll'
```

**Lösung:** flux_recovery Library linken
```cmake
target_link_libraries(MyApp PRIVATE flux_recovery)
```

---

## 📁 WAS WIRD GEBAUT

Nach erfolgreichem Build:

```
build/
├── UnifiedFloppyTool       # GUI Anwendung (Hauptprogramm!)
├── libflux_core.a          # Core Library (~2 MB)
├── libflux_format.a        # Format Library (~1.5 MB)
├── libflux_hw.a            # Hardware Library (~200 KB)
├── libflux_recovery.a      # Recovery Library (~100 KB)
└── libhardware_providers.a # C++ Hardware Provider (~80 KB)
```

---

## 🧪 TESTEN

### Basic Test

```bash
# GUI starten
./build/UnifiedFloppyTool

# Sollte ein Fenster mit 6 Tabs öffnen:
# - Workflow
# - Hardware
# - Format
# - Protection
# - Catalog
# - Tools
```

### Greaseweazle Test (wenn angeschlossen)

1. UnifiedFloppyTool starten
2. Hardware Tab öffnen
3. "Detect Devices" klicken
4. Greaseweazle sollte erscheinen

---

## 🔌 GREASEWEAZLE SETUP

### Linux udev Regel

Damit Greaseweazle ohne root funktioniert:

```bash
# Datei erstellen
sudo nano /etc/udev/rules.d/50-greaseweazle.rules

# Inhalt einfügen:
SUBSYSTEM=="usb", ATTR{idVendor}=="1209", ATTR{idProduct}=="4d69", MODE="0666"
SUBSYSTEM=="usb", ATTR{idVendor}=="1209", ATTR{idProduct}=="0001", MODE="0666"

# Regel aktivieren
sudo udevadm control --reload-rules
sudo udevadm trigger
```

### macOS

Keine spezielle Konfiguration nötig.

### Windows

Zadig Tool verwenden um WinUSB Driver zu installieren:
https://zadig.akeo.ie/

---

## 📊 BUILD VERIFIZIEREN

```bash
# Prüfen ob alle Libraries gebaut wurden
ls -la build/*.a

# Sollte zeigen:
# libflux_core.a
# libflux_format.a
# libflux_hw.a
# libflux_recovery.a
# libhardware_providers.a

# Prüfen ob Executable existiert
ls -la build/UnifiedFloppyTool

# Größe sollte ~5-10 MB sein
```

---

## 🆘 HILFE BEKOMMEN

### Build Log speichern

```bash
./build.sh 2>&1 | tee build.log
```

### Fehler melden

Wenn du Fehler hast, schick mir:
1. Dein OS (z.B. Ubuntu 22.04)
2. Die `build.log` Datei
3. Output von `cmake --version`
4. Output von `gcc --version`

---

## ✅ NÄCHSTE SCHRITTE NACH BUILD

1. **Starten:** `./build/UnifiedFloppyTool`
2. **Hardware Tab:** Greaseweazle verbinden
3. **Workflow Tab:** Erste Diskette lesen
4. **Format Tab:** Format auswählen (Amiga, C64, etc.)

---

**Viel Erfolg beim Bauen!** 🛠️💾
