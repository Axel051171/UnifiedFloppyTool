# UFT Coding Standards & Häufige Fehler

## 🚨 Häufige Fehler (aus CI-Erfahrung)

### 1. Fehlende Includes

**IMMER diese Header am Anfang jeder .c Datei:**

```c
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// Für UFT-spezifische Typen:
#include "uft_error.h"
#include "uft_error_compat.h"  // Legacy-Error-Codes
```

**Für Track/Sector-Operationen:**
```c
#include "uft_track.h"         // uft_track_t, uft_sector_t
#include "uft_mfm_flux.h"      // UFT_CRC16_*, UFT_MFM_MARK_*
```

**Für Multi-Threading (MSVC-kompatibel):**
```c
#include "uft_atomics.h"       // atomic_int, atomic_bool
// NICHT: #include <stdatomic.h>  (funktioniert nicht auf MSVC)
```

### 2. Error-Code Aliase

| Legacy-Code | Aktueller Code |
|-------------|----------------|
| `UFT_ERROR_FORMAT_NOT_SUPPORTED` | `UFT_ERR_NOT_SUPPORTED` |
| `UFT_ERR_INVALID_PARAM` | `UFT_ERR_INVALID_ARG` |
| `UFT_ERR_FILE_OPEN` | `UFT_ERR_IO` |
| `UFT_ERROR_NO_DATA` | `UFT_ERR_FORMAT` |

→ Benutze `uft_error_compat.h` für automatische Aliase!

### 3. Struct-Definitionen

**Prüfe IMMER ob der Typ schon existiert:**
```c
#ifndef UFT_SECTOR_T_DEFINED
#define UFT_SECTOR_T_DEFINED
typedef struct uft_sector { ... } uft_sector_t;
#endif
```

### 4. Include-Pfade

**RICHTIG:**
```c
#include "uft_track.h"           // Ohne Pfad
#include "uft/uft_track.h"       // Mit uft/ Präfix
```

**FALSCH:**
```c
#include "tracks/track_generator.h"   // Relativer Pfad!
```

→ CMake include_directories() nutzen!

### 5. MSVC-Kompatibilität

| Feature | GCC/Clang | MSVC Alternative |
|---------|-----------|------------------|
| `atomic_int` | `<stdatomic.h>` | `uft_atomics.h` |
| `__attribute__` | Ja | `UFT_PACKED`, `UFT_ALIGNED` |
| `typeof` | Ja | Nicht verfügbar |
| VLAs | Ja | Nicht verfügbar |

---

## ✅ Checkliste vor Commit

- [ ] Kompiliert auf Linux mit `-Wall -Wextra`
- [ ] Kompiliert auf Windows (MSVC)
- [ ] Keine `#include "relativer/pfad.h"` 
- [ ] Alle Error-Codes aus `uft_error.h`
- [ ] `bool` nur mit `#include <stdbool.h>`
- [ ] Atomics nur über `uft_atomics.h`

---

## 📁 Header-Hierarchie

```
include/uft/
├── uft_error.h          # Error-Codes (Master)
├── uft_error_compat.h   # Legacy-Aliase
├── uft_types.h          # Basis-Typen
├── uft_track.h          # Track/Sector
├── uft_atomics.h        # Portable Atomics
├── uft_mfm_flux.h       # MFM/CRC Konstanten
└── core/
    └── uft_error_compat.h
```
