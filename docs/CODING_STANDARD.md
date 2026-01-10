# UFT Coding Standard

**Version:** 1.0  
**Status:** VERBINDLICH

---

## 1. Naming Conventions

```c
// Functions: snake_case, prefix uft_
uft_error_t uft_open_disk(const char* path);

// Types: snake_case_t
typedef struct uft_disk uft_disk_t;

// Constants: UPPER_SNAKE_CASE
#define UFT_MAX_TRACKS 168

// Enums: UFT_ prefix, UPPER_SNAKE_CASE values
typedef enum {
    UFT_FORMAT_D64,
    UFT_FORMAT_ADF
} uft_format_t;

// Local variables: snake_case
int track_count;

// Static/private functions: underscore prefix
static void _cleanup_internal(void);
```

---

## 2. Error Handling

```c
// REGEL: Alle Funktionen returnen uft_error_t
uft_error_t do_work(void) {
    // Check preconditions
    UFT_CHECK_NULL(ptr);
    UFT_CHECK(size > 0);
    
    // Check nested calls
    UFT_CHECK_ERR(nested_call());
    
    return UFT_OK;
}

// Bei Cleanup-Bedarf: goto
uft_error_t complex_work(void) {
    uft_error_t err = UFT_OK;
    char* buf = malloc(100);
    UFT_CHECK_ALLOC_GOTO(buf, cleanup);
    
    UFT_CHECK_ERR_GOTO(step1(), cleanup);
    UFT_CHECK_ERR_GOTO(step2(), cleanup);
    
cleanup:
    free(buf);
    return err;
}
```

---

## 3. Memory Management

```c
// REGEL: Ownership dokumentieren (siehe uft_ownership.h)

// REGEL: Immer prüfen
char* buf = malloc(size);
if (!buf) return UFT_ERROR_NO_MEMORY;

// REGEL: free() dann NULL
free(ptr);
ptr = NULL;

// REGEL: sizeof(*ptr) statt sizeof(type)
int* arr = malloc(count * sizeof(*arr));
```

---

## 4. Integer Safety

```c
// REGEL: Safe Math für externe Werte
size_t total;
if (!uft_safe_mul_size(count, size, &total)) {
    return UFT_ERROR_OVERFLOW;
}

// REGEL: Bounds check
UFT_CHECK_BOUNDS(index, array_size);

// REGEL: Explizite Casts dokumentieren
uint32_t val = (uint32_t)offset;  // Safe: offset < 4GB verified
```

---

## 5. File I/O

```c
// REGEL: Immer Fehler prüfen
FILE* f = fopen(path, "rb");
if (!f) {
    UFT_LOG_ERROR("Cannot open: %s", path);
    return UFT_ERROR_FILE_OPEN;
}

// REGEL: fread/fwrite Rückgabe prüfen
if (fread(buf, size, 1, f) != 1) {
    fclose(f);
    return UFT_ERROR_FILE_READ;
}

// REGEL: fclose() Rückgabe bei Schreiben prüfen
if (fclose(f) != 0) {
    return UFT_ERROR_FILE_WRITE;
}
```

---

## 6. Comments

```c
/**
 * @brief Kurze Beschreibung
 * 
 * Längere Beschreibung wenn nötig.
 * 
 * @param x [OWNERSHIP] Beschreibung
 * @return UFT_OK bei Erfolg
 */
uft_error_t func(int x);

// Inline: Warum, nicht Was
offset += 2;  // Skip magic bytes

// TODO mit Issue-Nummer
// TODO(#123): Implement retry logic
```

---

## 7. Includes

```c
// Reihenfolge:
// 1. Eigener Header (für .c Files)
#include "uft_disk.h"

// 2. UFT Headers
#include "uft/core/uft_error.h"

// 3. System Headers
#include <stdint.h>
#include <stdio.h>

// 4. Platform-spezifisch mit #ifdef
#ifdef _WIN32
#include <windows.h>
#endif
```

---

## 8. Formatting

- Indent: 4 Spaces (keine Tabs)
- Line Length: max 100 Zeichen
- Braces: Allman Style
- Pointer: `int* ptr` (Stern bei Typ)

```c
uft_error_t
uft_read_track(uft_disk_t* disk, int cyl, int head,
               uft_track_t* track)
{
    if (!disk || !track)
    {
        return UFT_ERROR_NULL_POINTER;
    }
    
    for (int i = 0; i < count; i++)
    {
        process(i);
    }
    
    return UFT_OK;
}
```

---

**DOKUMENT ENDE**
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
