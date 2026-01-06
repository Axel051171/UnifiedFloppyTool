# UFT GitHub CI Fix Analysis

## Build-Fehler Zusammenfassung

### Linux + macOS (Exit Code 2)
```
lib/floppy/src/formats/uft_image.c:11:10: fatal error: uft/uft_compat.h: No such file or directory
```

**Root Cause:** Der Include-Pfad `${CMAKE_SOURCE_DIR}/include` ist nicht korrekt gesetzt oder der Header fehlt im Repository.

### Windows MSVC (Exit Code 1)
```
src/formats/amstrad/uft_edsk_parser.c(159): error C2143: syntax error: missing ')' before '('
```

**Root Cause:** Das `UFT_UNUSED` Makro wird nicht korrekt aufgelöst, weil entweder:
1. Der Header `uft/uft_compiler.h` nicht gefunden wird
2. Oder die Compiler-Detection fehlschlägt

## Korrektur-Maßnahmen

### 1. Include-Pfade für alle Sub-Libraries
Jede Library muss `${PROJECT_SOURCE_DIR}/include` im Include-Pfad haben:

```cmake
target_include_directories(TARGET_NAME PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)
```

### 2. Header-Struktur
Der `include/` Ordner muss committed sein mit:
- `include/uft/uft_compat.h` - Windows/POSIX Kompatibilität
- `include/uft/uft_compiler.h` - Compiler-Makros (UFT_UNUSED, UFT_PACKED, etc.)
- `include/uft/uft_config.h` - Konfigurationseinstellungen
- `include/uft/uft_common.h` - Gemeinsame Definitionen

### 3. CMakeLists.txt Änderungen
Die folgenden CMakeLists.txt müssen Include-Pfade haben:
- `lib/floppy/CMakeLists.txt`
- `src/formats/amstrad/CMakeLists.txt`
- Alle anderen Sub-Library CMakeLists.txt

## Betroffene Dateien im Fix
1. `lib/floppy/CMakeLists.txt` - Include-Pfad hinzufügen
2. `include/uft/uft_compat.h` - Header committen
3. `include/uft/uft_compiler.h` - Header committen
4. `src/formats/amstrad/CMakeLists.txt` - Include-Pfad prüfen

## Test-Empfehlung
Nach dem Fix:
```bash
# Linux/macOS
cmake -B build -G Ninja
cmake --build build

# Windows
cmake -B build -G "NMake Makefiles"
cmake --build build
```
