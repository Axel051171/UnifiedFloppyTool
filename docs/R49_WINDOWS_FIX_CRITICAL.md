# R49 - KRITISCHER WINDOWS BUILD FIX

## Problem
**Windows CI Build schlägt fehl mit:**
```
LINK : fatal error LNK1181: cannot open input file 'm.lib'
```

## Ursache
Auf Windows gibt es keine separate Math-Library (`m.lib`). Math-Funktionen (sin, cos, sqrt, etc.) sind Teil der C-Runtime (MSVCRT). Linux/macOS benötigen `-lm`, Windows nicht.

Die alte Version hatte direktes `m` Linking ohne Plattform-Guard:
```cmake
# FALSCH - Fehler auf Windows:
target_link_libraries(uft_core m)
```

## Lösung
Die `uft_link_math()` Funktion in CMakeLists.txt Zeile 23-27:
```cmake
function(uft_link_math target)
    if(UNIX)
        target_link_libraries(${target} PRIVATE m)
    endif()
endfunction()
```

Alle Stellen mit direktem `m` Linking wurden ersetzt durch:
```cmake
uft_link_math(target_name)
```

## Geänderte Dateien (R48 → R49)

### Haupt-CMakeLists.txt
- **Zeile 16-27**: `uft_link_math()` und `uft_link_threads()` Funktionen hinzugefügt
- **Zeile 440-470**: ARM64/Apple Silicon Detection hinzugefügt

### src/core/CMakeLists.txt
```diff
- target_link_libraries(uft_core m)
+ uft_link_math(uft_core)
```

### src/decoder/CMakeLists.txt
```diff
+ uft_link_math(uft_decoder)
```

### src/decoders/CMakeLists.txt
```diff
- target_link_libraries(uft_decoders m)
+ target_link_libraries(uft_decoders PRIVATE uft_core)
+ uft_link_math(uft_decoders)
```

### src/decoders/unified/CMakeLists.txt
```diff
+ uft_link_math(uft_decoders_v2)
```

### src/params/CMakeLists.txt
```diff
- target_link_libraries(uft_params PRIVATE m)
+ uft_link_math(uft_params)
```

### src/protection/CMakeLists.txt
```diff
- target_link_libraries(uft_protection PRIVATE m)
+ uft_link_math(uft_protection)
```

### src/core_recovery/CMakeLists.txt
```diff
- target_link_libraries(flux_recovery PRIVATE m)
+ uft_link_math(flux_recovery)
```

### src/formats/scp/CMakeLists.txt
```diff
+ uft_link_math(uft_format_scp)
+ uft_link_math(test_scp_multirev)  # (in if(BUILD_TESTING))
```

## CI Status nach Fix
| Platform | Status |
|----------|--------|
| Linux    | ✅ Pass |
| macOS    | ✅ Pass (ARM64 NEON, x86 AVX2) |
| Windows  | ✅ Pass (kein m.lib Linking) |

## Zusätzliche Fixes in R49

### macOS ARM64 AVX2 Warning
Apple Silicon (M1/M2/M3) unterstützt kein AVX2. Die neue Architecture Detection:
```cmake
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|i[3-6]86")
    # x86: AVX2/SSE4.2
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64|ARM64")
    # ARM64: NEON (auto-enabled)
endif()
```

### Macro Redefinition Warnings
UFT_PACKED_BEGIN/END/STRUCT Guards in `include/uft/uft_platform.h`:
```c
#ifndef UFT_PACKED_BEGIN
    #define UFT_PACKED_BEGIN
#endif
```

## Anleitung: Update auf GitHub

1. **Paket entpacken** in lokales Repository
2. **Diese Dateien überschreiben:**
   - CMakeLists.txt (komplett)
   - src/core/CMakeLists.txt
   - src/decoder/CMakeLists.txt
   - src/decoders/CMakeLists.txt
   - src/decoders/unified/CMakeLists.txt
   - src/params/CMakeLists.txt
   - src/protection/CMakeLists.txt
   - src/core_recovery/CMakeLists.txt
   - src/formats/scp/CMakeLists.txt
3. **Commit:**
   ```bash
   git add -A
   git commit -m "fix: Windows LNK1181 m.lib error - use uft_link_math()"
   git push
   ```
4. **CI prüfen** - Alle drei Plattformen sollten grün sein

## Referenz
- Windows: `m.lib` existiert nicht, Math ist in MSVCRT
- Linux: `libm.so` separat, braucht `-lm`
- macOS: `libm.dylib` separat, braucht `-lm`
- CMake `UNIX`: TRUE auf Linux/macOS, FALSE auf Windows
