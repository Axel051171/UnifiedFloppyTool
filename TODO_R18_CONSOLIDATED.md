# UFT R18 - Konsolidierte TODO Liste
## Cross-Platform Build Fixes - 2026-01-05

---

## ✅ P0 - Build-Breaker (ALLE GEFIXT)

### P0-PACK-001: __attribute__((packed)) → UFT_PACK_BEGIN/END ✅
- **Problem:** 72+ Stellen mit GCC-spezifischem `__attribute__((packed))`
- **Auswirkung:** MSVC versteht diese Syntax nicht → Build-Fehler
- **Fix:** Automatische Konvertierung zu `UFT_PACK_BEGIN` / `UFT_PACK_END`
- **Dateien:** 51 Dateien in src/ und include/ gefixt
- **Script:** `/tmp/fix_all_packed.py` + `/tmp/fix_pack_end.py`

### P0-MAC-001: fdc_misc.cpp namespace pollution ✅
- **Problem:** `#include <sstream>` innerhalb `namespace fdc_misc {}` Block
- **Fix:** Includes VOR den namespace Block verschoben
- **Datei:** `src/flux/fdc_bitstream/fdc_misc.cpp`

### P0-WIN-001: atomic_size_t nicht erkannt (MSVC) ✅
- **Problem:** C11 `atomic_size_t` nicht in MSVC C-Mode
- **Fix:** `atomic_size_t` → `uft_atomic_size`
- **Datei:** `src/core/uft_memory.c`

### P0-WIN-002: atomic_bool / _Atomic Fehler (MSVC) ✅
- **Problem:** C11 atomics nicht in MSVC
- **Fix:** `atomic_bool` → `uft_atomic_bool`
- **Datei:** `src/core/uft_streaming_hash.c`

### P0-MAC-002: snprintf implicit declaration ✅
- **Problem:** Fehlender `#include <stdio.h>`
- **Fix:** stdio.h zu 20+ Dateien hinzugefügt
- **Dateien:** `src/protection/*.c`, `src/formats_v2/*.c`

### P0-WIN-003: malloc/free Macro Redefinition ✅
- **Problem:** GNU Statement Expressions nicht in MSVC
- **Fix:** Platform-Guard für GCC/Clang vs MSVC
- **Datei:** `include/uft/uft_memory.h`

### P0-WIN-004: timespec_win Redefinition ✅
- **Problem:** Windows SDK 10+ definiert `struct timespec`
- **Fix:** Guard mit `_TIMESPEC_DEFINED`
- **Datei:** `include/uft/uft_threading.h`

---

## 📊 Build-Status nach R18

| Plattform | Status | Notizen |
|-----------|--------|---------|
| Linux | ✅ BUILD OK | 100% kompiliert |
| macOS | 🔄 READY | Alle Fixes angewendet |
| Windows | 🔄 READY | Alle Fixes angewendet |

---

## 📝 Statistiken

- **Geänderte Dateien:** 51+ Dateien
- **Packed Struct Fixes:** 145+ Stellen
- **Atomic Fixes:** 15+ Stellen
- **Include Fixes:** 20+ Dateien
- **Build Zeit (Linux):** ~2 Minuten

---

## 🔧 Wichtige neue Header/Patterns

### Cross-Platform Packed Structs
```c
#include "uft/uft_compiler.h"

UFT_PACK_BEGIN
typedef struct {
    uint8_t  signature[3];
    uint16_t version;
} my_header_t;
UFT_PACK_END
```

### Cross-Platform Atomics
```c
#include "uft/uft_atomics.h"

static uft_atomic_size counter = UFT_ATOMIC_INIT(0);
uft_atomic_fetch_add(&counter, 1);
size_t val = uft_atomic_load(&counter);
```

---

## 🔜 Nächste Schritte

1. **CI Push:** ZIP entpacken und zu GitHub pushen
2. **CI Verify:** Warten auf grüne Builds auf allen 3 Plattformen
3. **Release Tag:** `git tag -a v3.3.0 -m "Release"` → automatisches Release
