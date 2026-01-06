# UFT CI-LOG Findings Report R48

**Datum:** 2026-01-06  
**Analysierte Logs:** `logs_53611559528.zip`, `logs_53611620000.zip`  
**Plattformen:** Windows, Linux, macOS  

---

## Executive Summary

| Kategorie | Gefunden | Gefixt | Verbleibend |
|-----------|----------|--------|-------------|
| P0 - Build-Breaker | 2 | 2 | 0 |
| P1 - Funktionale Probleme | 7 | 7 | 0 |
| P2 - Warnings/Code-Qualität | 15+ | 5 | ~10 |
| P3 - Polish/Dokumentation | 5 | 0 | 5 |

**Build Status nach Fixes:**
- ✅ Linux: ERFOLGREICH (700KB Binary)
- ⏳ Windows: Fixes angewendet, CI-Verifizierung ausstehend
- ⏳ macOS: Fixes angewendet, CI-Verifizierung ausstehend

---

## P0: Kritische Build-Breaker (ALLE GEFIXT)

### [P0-WIN-001] Windows Build Failed - ssize_t undefined ✅ GEFIXT

**Symptom:**
```
D:\a\UnifiedFloppyTool\lib\floppy\include\encoding/uft_gcr.h(287): error C2061: syntax error: identifier 'uft_gcr_find_sync'
```

**Root Cause:** `ssize_t` ist POSIX-spezifisch und nicht in MSVC definiert.

**Fix:** Windows-kompatible `ssize_t` Definition in allen `uft_gcr.h` Dateien:
```c
#ifdef _WIN32
    #ifndef _SSIZE_T_DEFINED
        #define _SSIZE_T_DEFINED
        #include <basetsd.h>
        #ifdef _WIN64
            typedef __int64 ssize_t;
        #else
            typedef int ssize_t;
        #endif
    #endif
#else
    #include <sys/types.h>
#endif
```

**Betroffene Dateien:**
- `lib/floppy/include/encoding/uft_gcr.h`
- `src/extract/uft_floppy_lib_v2/include/encoding/uft_gcr.h`
- `src/floppy_lib/include/encoding/uft_gcr.h`
- `src/lib_v2/encoding/uft_gcr.h`

### [P0-LIN-001] Linux AppImage Failed - Icon fehlt ✅ GEFIXT

**Symptom:**
```
ERROR: Could not find icon executable for Icon entry: UnifiedFloppyTool
```

**Root Cause:** Desktop-File referenziert Icon, das nicht existiert.

**Fix:**
1. SVG Icon erstellt: `resources/icons/UnifiedFloppyTool.svg`
2. PNG Icon kopiert: `resources/icons/UnifiedFloppyTool.png`
3. CMakeLists.txt: Icon-Installation hinzugefügt
4. release.yml: Korrekter Icon-Pfad

---

## P1: Funktionale Probleme (ALLE GEFIXT)

### [P1-MAC-001] Pragma Pack Issues ✅ GEFIXT

**Symptom:**
```
warning: unterminated '#pragma pack (push, ...)' at end of file
```

**Betroffene Dateien:**
- `include/uft/uft_imd.h` - 1 fehlender `#pragma pack(pop)`
- `include/uft/uft_td0.h` - 5 fehlende `#pragma pack(pop)`

**Fix:** Matching `#pragma pack(pop)` nach jedem Struct hinzugefügt.

### [P1-ALL-001] Struct Forward Declaration fehlt ✅ GEFIXT

**Symptom:**
```
warning: 'struct uft_dpll_config' declared inside parameter list
```

**Fix:** Forward declarations hinzugefügt in:
- `include/uft/uft_gui_params.h`
- `include/uft/uft_dmk.h`
- `include/uft/uft_td0.h`

### [P1-ALL-002] Implicit Function Declarations ✅ GEFIXT

**Betroffene Funktionen:** `strdup`, `strcasecmp`, `strncasecmp`, `clock_gettime`, `usleep`, `gethostname`

**Fix:** Feature-Makros + passende Includes:
```c
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE 1
#endif
#include <strings.h>  /* strcasecmp, strncasecmp */
#include <unistd.h>   /* gethostname */
```

### [P1-MAC-002] Macro Redefinition ✅ GEFIXT

**Symptom:**
```
warning: 'UFT_PACKED_BEGIN' macro redefined (22x!)
```

**Fix:** Include-Guards in `include/uft/uft_platform.h`:
```c
#ifndef UFT_PACKED_BEGIN
    // ... definitions
#endif
```

---

## P2: Warnings & Code-Qualität (TEILWEISE GEFIXT)

### Gefixt:

| Issue | Fix |
|-------|-----|
| [P2-C-001] Shift negative value (UB) | `~0` → `~0U` in `src/crc/poly.c` |
| [P2-C-003] Array bounds overflow | `63` → `31` in `uft_fuzzy_bits.c` |
| [P2-MAC-002] AVX2 not supported on ARM | Architektur-Check in CMakeLists.txt |

### Verbleibend (Low Priority):

| Issue | Beschreibung | Empfohlener Fix |
|-------|--------------|-----------------|
| [P2-CPP-001] | Signed/unsigned mismatch (20+) | `int` → `size_t` |
| [P2-CPP-002] | Constructor initialization order | Member-Reihenfolge anpassen |
| [P2-C-002] | Uninitialized variables | Initialisierung hinzufügen |
| [P2-ALL-001] | Unused functions/variables (30+) | `(void)` cast oder entfernen |
| [P2-MAC-001] | Unknown warning option | Compiler-spezifische Guards |

---

## P3: Polish & Dokumentation (TODO)

| Issue | Status |
|-------|--------|
| CHANGELOG.md | TODO |
| CONTRIBUTING.md | TODO |
| SECURITY.md | TODO |
| ARCHITECTURE.md | TODO |
| Release-Workflow | Verbessert |

---

## Geänderte Dateien

```
include/uft/uft_platform.h          # PACKED macro guards
include/uft/uft_gui_params.h        # Forward declarations
include/uft/uft_imd.h               # pragma pack(pop)
include/uft/uft_td0.h               # pragma pack(pop) + forward decl
include/uft/uft_dmk.h               # Forward declaration

lib/floppy/include/encoding/uft_gcr.h  # Windows ssize_t
src/extract/uft_floppy_lib_v2/include/encoding/uft_gcr.h
src/floppy_lib/include/encoding/uft_gcr.h
src/lib_v2/encoding/uft_gcr.h

src/core/uft_core.c                 # Feature macros
src/core/uft_format_plugin.c        # strings.h
src/core/uft_audit_trail.c          # Feature macros + unistd.h
src/core/uft_streaming_hash.c       # strings.h
src/core/uft_track_preset.c         # strings.h
src/recovery/uft_recovery_meta.c    # Feature macros
src/crc/poly.c                      # ~0 → ~0U
src/protection/uft_fuzzy_bits.c     # Array bounds fix

CMakeLists.txt                      # ARM SIMD check, icon install
.github/workflows/release.yml       # Icon path fix
resources/icons/UnifiedFloppyTool.svg  # NEW
resources/icons/UnifiedFloppyTool.png  # NEW
```

---

## Nächste Schritte

1. **CI verifizieren:** Push und CI-Ergebnis für Windows/macOS prüfen
2. **P2 Warnings:** Systematisch abarbeiten
3. **P3 Dokumentation:** CHANGELOG, CONTRIBUTING erstellen
4. **Release vorbereiten:** Tag setzen, Release-Notes schreiben

---

## Build-Anleitung

```bash
# Linux/macOS
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# Windows (Visual Studio)
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

---

*Report generiert: 2026-01-06 R48*
