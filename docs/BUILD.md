# UnifiedFloppyTool - Build Instructions

## Version 5.28.0 GOD MODE EXTENDED

---

## Quick Start

### Mit CMake (empfohlen)

```bash
# Erstelle Build-Verzeichnis
mkdir build && cd build

# Konfiguriere
cmake .. -DCMAKE_BUILD_TYPE=Release

# Baue
cmake --build . -j$(nproc)

# Optional: Installiere
sudo cmake --install .
```

### Mit Make (Alternative)

```bash
# Standard-Build
make

# Debug-Build
make DEBUG=1

# Mit Warnings
make WARNINGS=1

# Clean
make clean

# Statistiken
make stats
```

---

## CMake Optionen

### Build-Typen

| Option | Beschreibung |
|--------|--------------|
| `-DCMAKE_BUILD_TYPE=Release` | Optimierter Build |
| `-DCMAKE_BUILD_TYPE=Debug` | Debug-Build mit Symbolen |
| `-DCMAKE_BUILD_TYPE=RelWithDebInfo` | Release mit Debug-Info |

### Module

| Option | Standard | Beschreibung |
|--------|----------|--------------|
| `-DUFT_MODULE_FORMATS=ON` | ON | 382 Format-Parser |
| `-DUFT_MODULE_RECOVERY=ON` | ON | GOD MODE Recovery System |
| `-DUFT_MODULE_FILESYSTEMS=ON` | ON | VFS Filesystems (11) |
| `-DUFT_MODULE_ALGORITHMS=ON` | ON | Flux/Bitstream-Algorithmen |
| `-DUFT_MODULE_LOADERS=ON` | ON | Format-Loader (118) |
| `-DUFT_MODULE_HARDWARE=ON` | ON | Hardware-Support |

### Features

| Option | Standard | Beschreibung |
|--------|----------|--------------|
| `-DUFT_ENABLE_SIMD=ON` | ON | SSE2/AVX2 Optimierungen |
| `-DUFT_ENABLE_OPENMP=OFF` | OFF | OpenMP Parallelisierung |
| `-DUFT_ENABLE_LTO=OFF` | OFF | Link-Time Optimization |
| `-DUFT_ENABLE_SANITIZER=OFF` | OFF | AddressSanitizer |
| `-DUFT_ENABLE_UBSAN=OFF` | OFF | UndefinedBehaviorSanitizer |

### Build-Targets

| Option | Standard | Beschreibung |
|--------|----------|--------------|
| `-DUFT_BUILD_SHARED=ON` | ON | Shared Library (libuft.so) |
| `-DUFT_BUILD_STATIC=ON` | ON | Static Library (libuft.a) |
| `-DUFT_BUILD_CLI=ON` | ON | CLI-Tool |
| `-DUFT_BUILD_GUI=OFF` | OFF | Qt GUI (benötigt Qt5/6) |
| `-DUFT_BUILD_TESTS=ON` | ON | Test-Suite |

---

## Beispiel-Builds

### Minimaler Build (nur Library)

```bash
cmake .. -DCMAKE_BUILD_TYPE=Release \
    -DUFT_BUILD_CLI=OFF \
    -DUFT_BUILD_TESTS=OFF
```

### Debug mit Sanitizer

```bash
cmake .. -DCMAKE_BUILD_TYPE=Debug \
    -DUFT_ENABLE_SANITIZER=ON \
    -DUFT_ENABLE_UBSAN=ON
```

### Voller Release-Build

```bash
cmake .. -DCMAKE_BUILD_TYPE=Release \
    -DUFT_ENABLE_LTO=ON \
    -DUFT_ENABLE_SIMD=ON \
    -DUFT_BUILD_CLI=ON \
    -DUFT_BUILD_TESTS=ON
```

### Mit Qt GUI

```bash
cmake .. -DCMAKE_BUILD_TYPE=Release \
    -DUFT_BUILD_GUI=ON \
    -DCMAKE_PREFIX_PATH=/path/to/Qt
```

---

## Abhängigkeiten

### Erforderlich

- C11-kompatibler Compiler (GCC 7+, Clang 5+, MSVC 2019+)
- C++17-kompatibler Compiler
- CMake 3.16+ oder Make

### Optional

| Paket | Für |
|-------|-----|
| zlib | Komprimierte Formate (ADZ, DMS, etc.) |
| Qt 5/6 | GUI-Anwendung |
| OpenMP | Parallelisierung |
| libusb | Hardware-Controller |

### Installation (Debian/Ubuntu)

```bash
sudo apt-get install build-essential cmake
sudo apt-get install zlib1g-dev        # Optional: zlib
sudo apt-get install libusb-1.0-0-dev  # Optional: USB
sudo apt-get install qtbase5-dev       # Optional: Qt5
```

### Installation (macOS)

```bash
brew install cmake
brew install zlib           # Optional
brew install libusb         # Optional
brew install qt@6           # Optional
```

### Installation (Windows)

- Visual Studio 2019 oder neuer
- CMake von cmake.org
- Optional: vcpkg für Abhängigkeiten

---

## Projektstruktur

```
uft/
├── CMakeLists.txt          # Haupt-CMake
├── Makefile                # Alternative zu CMake
├── build.sh                # Build-Script
├── include/                # Öffentliche Header
├── src/
│   ├── algorithms/         # Flux/Bitstream-Algorithmen
│   │   ├── flux/           # FluxStreamAnalyzer (HxC)
│   │   ├── bitstream/      # BitstreamDecoder (SAMdisk)
│   │   ├── tracks/         # Track-Extractors (27)
│   │   └── disk/           # Disk-Utilities
│   ├── core/               # Kern-Funktionen
│   ├── formats/            # 382 Format-Parser
│   ├── recovery/           # GOD MODE Recovery (9 Module)
│   ├── filesystems/        # VFS (11 Dateisysteme)
│   ├── loaders/            # Format-Loader (118)
│   ├── hardware/           # Hardware-Support
│   └── cli/                # CLI-Tool
├── tests/                  # Tests
└── docs/                   # Dokumentation
```

---

## Statistiken

| Komponente | Dateien | LOC |
|------------|---------|-----|
| **Gesamt** | 2,504 | 537,985 |
| Format-Parser | 382 | 36,367 |
| Recovery | 9 | 5,266 |
| Algorithmen | 303 | 81,604 |
| FluxEngine | ~200 | ~50,000 |
| SAMdisk | ~50 | ~20,000 |
| Loaders | 118 | ~30,000 |

---

## Troubleshooting

### CMake findet Qt nicht

```bash
cmake .. -DCMAKE_PREFIX_PATH=/path/to/Qt/6.x/gcc_64
```

### Compiler-Fehler in externem Code

Strict Warnings deaktivieren:
```bash
cmake .. -DUFT_STRICT_WARNINGS=OFF
```

### Link-Fehler mit math.h

```bash
# Linux: -lm wird benötigt
# Ist bereits in CMakeLists.txt konfiguriert
```

### Build auf Windows

```bash
# Mit Visual Studio
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

---

## Cross-Compilation

### Für ARM64 (Raspberry Pi)

```bash
cmake .. -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-aarch64.cmake
```

### Für Windows (MinGW)

```bash
cmake .. -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-mingw64.cmake
```

---

## Entwicklung

### Compile Commands für IDE

```bash
cmake .. -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
# Erzeugt build/compile_commands.json für clangd, etc.
```

### Tests ausführen

```bash
cd build
ctest --output-on-failure
```

### Coverage

```bash
cmake .. -DCMAKE_BUILD_TYPE=Debug -DUFT_ENABLE_COVERAGE=ON
make
make coverage
```
