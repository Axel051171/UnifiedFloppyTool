# ADR-0001: C99 als Sprachstandard

**Status:** Accepted  
**Datum:** 2025-01-11  
**Autor:** UFT Team

---

## Kontext

UFT muss auf drei Plattformen bauen:
- Windows (MSVC 2019+)
- Linux (GCC 9+)
- macOS (Clang/Xcode 12+)

Neuere C-Standards (C11, C17, C23) bieten Features wie `_Generic`, 
`_Static_assert`, und `<threads.h>`, aber die Compiler-Unterstützung 
ist uneinheitlich.

## Entscheidung

**Wir verwenden C99 als Basis-Standard mit selektiven C11-Features.**

CMake-Einstellung:
```cmake
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
```

## Begründung

| Feature | C99 | C11 | MSVC Support |
|---------|-----|-----|--------------|
| `stdint.h` | ✅ | ✅ | ✅ |
| `stdbool.h` | ✅ | ✅ | ✅ |
| Designated initializers | ✅ | ✅ | ✅ |
| `_Static_assert` | ❌ | ✅ | ✅ (als `static_assert`) |
| `_Generic` | ❌ | ✅ | ⚠️ Teilweise |
| `<threads.h>` | ❌ | ✅ | ❌ |
| VLAs | ✅ | Optional | ❌ |

**MSVC unterstützt C11/C17 erst ab VS2019 16.8+ vollständig.**

## Konsequenzen

### Positiv ✅
- Maximale Compiler-Kompatibilität
- Keine MSVC-Sonderbehandlung nötig
- Bewährter, stabiler Standard

### Negativ ⚠️
- Kein `_Generic` für type-safe Makros
- Kein `<threads.h>` (verwenden pthreads/Win32 API)
- Keine VLAs (verwenden `alloca` oder heap)

## Alternativen (verworfen)

### C11 durchgehend
- **Verworfen weil:** MSVC-Unterstützung lückenhaft, insbesondere `<threads.h>`

### C89/ANSI C
- **Verworfen weil:** Kein `stdint.h`, `stdbool.h`, schlechtere Lesbarkeit

---

## Änderungshistorie

| Datum | Autor | Änderung |
|-------|-------|----------|
| 2025-01-11 | UFT Team | Initial erstellt |
