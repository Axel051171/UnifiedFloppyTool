# UFT v3.4.5 GitHub CI Fix

## Problem-Zusammenfassung

Die GitHub CI-Builds (Run #53647418349) sind auf allen Plattformen fehlgeschlagen:

| Platform | Exit Code | Hauptfehler |
|----------|-----------|-------------|
| Linux    | 2         | `uft/uft_compat.h: No such file or directory` |
| macOS    | 2         | `'uft/uft_compat.h' file not found` |
| Windows  | 1         | `uft_edsk_parser.c(159): error C2143: syntax error` |

## Root Cause

Der `include/` Ordner mit den kritischen Header-Dateien wurde nicht korrekt committed oder die CMake Include-Pfade sind nicht konsistent über alle Sub-Libraries.

## Fix-Inhalte

Dieses Paket enthält:

### 1. Kritische Header (`include/uft/`)
- `uft_compat.h` - Windows/POSIX Kompatibilität (strcasecmp, bzero, ssize_t)
- `uft_compiler.h` - Zentrale Compiler-Makros (UFT_UNUSED, UFT_PACKED, UFT_INLINE)
- `uft_config.h` - Projekt-Konfiguration
- `uft_common.h` - Gemeinsame Typdefinitionen

### 2. CMakeLists.txt mit korrekten Include-Pfaden
Alle Sub-Library CMakeLists.txt haben jetzt:
```cmake
target_include_directories(TARGET PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)
```

### 3. Betroffene Module
- `lib/floppy/` - Low-Level Disk I/O
- `src/formats/amstrad/` - EDSK Parser
- Alle anderen Format-Module

## Anwendung

### Option A: Kompletter Ersatz
```bash
# Backup erstellen
mv UnifiedFloppyTool UnifiedFloppyTool.bak

# Fix anwenden
unzip UFT_v3_4_5_GITHUB_CI_FIX.zip
cd uft_github_ci_fix
git init
git add .
git commit -m "Apply CI fix v3.4.5"
git remote add origin https://github.com/Axel051171/UnifiedFloppyTool.git
git push --force origin main
```

### Option B: Selektives Kopieren
```bash
# Nur fehlende Header kopieren
cp -r uft_github_ci_fix/include/ UnifiedFloppyTool/

# CMakeLists.txt aktualisieren
cp uft_github_ci_fix/lib/floppy/CMakeLists.txt UnifiedFloppyTool/lib/floppy/
cp uft_github_ci_fix/src/formats/amstrad/CMakeLists.txt UnifiedFloppyTool/src/formats/amstrad/
```

## Lokaler Build-Test (vor Push)

```bash
# Linux/macOS
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Windows (MSVC)
cmake -B build -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Windows (MinGW)
cmake -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## CI Workflow Status

Nach dem Push sollten alle 3 Jobs erfolgreich sein:
- ✅ build-linux
- ✅ build-windows
- ✅ build-macos

## Version

- UFT: 3.4.5
- Fix-Date: 2026-01-06
- CI-Run: 53647418349
