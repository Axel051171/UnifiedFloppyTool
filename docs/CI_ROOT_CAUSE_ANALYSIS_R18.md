# UFT R18 - CI Root-Cause Analysis Report
## GitHub Actions Build Failures - 2026-01-05

---

## Executive Summary

Die CI-Builds auf **macOS** und **Windows** scheiterten aufgrund von:
1. C++ Namespace-Pollution (macOS)
2. Fehlende C11 Atomic-Unterstützung in MSVC (Windows)
3. Windows SDK Typ-Konflikte (Windows)
4. Fehlende Standard-Header (macOS/Windows)

**Linux-Build war erfolgreich** mit nur Warnings.

---

## Detaillierte Root-Cause Analyse

### 🍎 macOS - Xcode 15.4 (clang)

#### Problem 1: "no template named 'basic_streambuf'; did you mean '::std::basic_streambuf'?"

**Root-Cause:**
In `fdc_misc.cpp` wurden System-Header `<sstream>` und `<iostream>` **innerhalb** des `namespace fdc_misc { }` Blocks inkludiert:

```cpp
namespace fdc_misc {
#else
#include <iostream>   // ← INNERHALB des namespace!
#include <sstream>    // ← INNERHALB des namespace!
```

**Effekt:**
Alle `std::` Referenzen innerhalb dieser Header werden zu `fdc_misc::std::` aufgelöst, was nicht existiert.

**Korrekte Lösung:**
```cpp
// Platform-specific includes - MUST be before namespace block
#ifdef _WIN32
    #include <windows.h>
#else
    #include <iostream>
    #include <sstream>
#endif

namespace fdc_misc {  // ← Namespace erst NACH includes
```

#### Problem 2: "call to undeclared library function 'snprintf'"

**Root-Cause:**
`uft_apple2_protection.c` verwendete `snprintf()` ohne `#include <stdio.h>`.
C99+ erlaubt keine impliziten Funktionsdeklarationen.

---

### 🪟 Windows - MSVC 2022

#### Problem 1: "error C2061: syntax error: identifier 'g_total_allocated'"

**Root-Cause:**
Code verwendete C11 `atomic_size_t`:
```c
static atomic_size_t g_total_allocated = 0;  // MSVC kennt das nicht!
```

MSVC unterstützt `<stdatomic.h>` **nicht** für C-Code, nur für C++.

**Lösung:**
Verwendung der Cross-Platform Wrapper aus `uft_atomics.h`:
```c
static uft_atomic_size g_total_allocated = UFT_ATOMIC_INIT(0);
```

#### Problem 2: "error C2011: 'timespec_win': 'struct' type redefinition"

**Root-Cause:**
`uft_threading.h` definierte `struct timespec_win`, aber Windows SDK 10+ definiert bereits `struct timespec` in `<time.h>`.

**Lösung:**
Guard mit SDK-spezifischen Macros:
```c
#if !defined(_TIMESPEC_DEFINED) && !defined(__struct_timespec_defined)
    #define _TIMESPEC_DEFINED
    struct timespec {
        long tv_sec;
        long tv_nsec;
    };
#endif
```

#### Problem 3: "error C2371: 'free': redefinition; different basic types"

**Root-Cause:**
Debug-Memory Macros verwendeten GNU Statement Expressions:
```c
#define malloc(size) ({ void *p = malloc(size); ... })  // MSVC: FEHLER
```

Diese GCC-Extension ist nicht portabel.

**Lösung:**
Platform-Guard und alternative Implementierung für MSVC:
```c
#if defined(__GNUC__) || defined(__clang__)
    #define malloc(size) ({ ... })
#else
    // MSVC: inline functions instead
    static inline void* uft_debug_malloc(size_t size, ...) { ... }
#endif
```

---

## Verifikation

### Linux Build Test
```
[ 100%] Built target UnifiedFloppyTool
```
✅ **ERFOLGREICH** - Alle Module kompiliert ohne Fehler

### Ausstehende Verifikation
- [ ] macOS Build auf GitHub Actions nach Merge
- [ ] Windows Build auf GitHub Actions nach Merge

---

## Empfehlungen für zukünftige Development

1. **Regel: Includes IMMER außerhalb von namespace-Blöcken**
   - CI-Check: `grep -r "namespace.*{" | grep -B5 "#include"`

2. **Regel: Keine C11 atomics direkt verwenden**
   - Immer `uft_atomic_*` Wrapper aus `uft_atomics.h`
   - CI-Check: `grep -r "atomic_bool\|atomic_int\|atomic_size_t\|_Atomic"`

3. **Regel: Keine GNU-Extensions in portablem Code**
   - Statement Expressions `({})` sind GCC-only
   - Alternative: Inline-Funktionen

4. **Regel: Alle Standard-Includes explizit**
   - `stdio.h` für printf/snprintf/fopen
   - `stdlib.h` für malloc/free
   - `string.h` für memcpy/strlen
   - CI-Check: Script für fehlende Includes

5. **Regel: Windows SDK Kompatibilität testen**
   - SDK 10+ hat `struct timespec`
   - Guards mit `_TIMESPEC_DEFINED` verwenden

---

## Anhang: CI Log Snippets

### macOS Fehler (vor Fix)
```
sstream:300:14: error: no template named 'basic_streambuf'; 
                       did you mean '::std::basic_streambuf'?
```

### Windows Fehler (vor Fix)
```
uft_memory.c(45,22): error C2061: syntax error: identifier 'g_total_allocated'
uft_streaming_hash.c(114,5): error C2061: syntax error: identifier 'atomic_bool'
time.h(45,12): error C2011: 'timespec_win': 'struct' type redefinition
```

---

*Report generiert: 2026-01-05*
*UFT Version: 3.3.0-R18*
