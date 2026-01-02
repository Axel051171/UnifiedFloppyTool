# UFT Developer Guide - Fehlerfreie Integration

**Basierend auf GOD MODE Audit Erkenntnissen**  
**Version:** 1.0  
**Gültig ab:** Januar 2026

---

## Inhaltsverzeichnis

1. [Header-Regeln](#1-header-regeln)
2. [Include-Hierarchie](#2-include-hierarchie)
3. [Typ-Definitionen](#3-typ-definitionen)
4. [Error-Codes](#4-error-codes)
5. [Format-Module](#5-format-module)
6. [Checkliste für neue Dateien](#6-checkliste-für-neue-dateien)
7. [Anti-Patterns](#7-anti-patterns)

---

## 1. Header-Regeln

### 1.1 Include-Guard Konvention

```c
// KORREKT:
#ifndef UFT_MODULNAME_H
#define UFT_MODULNAME_H

// ... Inhalt ...

#endif // UFT_MODULNAME_H
```

### 1.2 Standard-Includes IMMER zuerst

```c
#ifndef UFT_MEIN_MODUL_H
#define UFT_MEIN_MODUL_H

// 1. Standard-Bibliotheken (alphabetisch)
#include <stdbool.h>
#include <stddef.h>    // PFLICHT wenn size_t verwendet wird!
#include <stdint.h>
#include <time.h>      // PFLICHT wenn time_t verwendet wird!

// 2. UFT Core-Header
#include "uft/uft_error.h"
#include "uft/uft_types.h"

// 3. UFT Modul-spezifische Header
#include "uft/formats/uft_floppy_device.h"  // Wenn FloppyDevice gebraucht

#ifdef __cplusplus
extern "C" {
#endif

// ... Deklarationen ...

#ifdef __cplusplus
}
#endif

#endif // UFT_MEIN_MODUL_H
```

### 1.3 NIEMALS Typen lokal definieren die woanders existieren

```c
// ❌ FALSCH - Lokale FloppyDevice Definition
typedef struct {
    uint32_t tracks, heads, sectors, sectorSize;
    bool flux_supported;
} FloppyDevice;

// ✅ RICHTIG - Include verwenden
#include "uft/formats/uft_floppy_device.h"
```

**Zentrale Typ-Header:**

| Typ | Definiert in |
|-----|--------------|
| `FloppyDevice` | `uft/formats/uft_floppy_device.h` |
| `FluxTimingProfile` | `uft/formats/uft_floppy_device.h` |
| `WeakBitRegion` | `uft/formats/uft_floppy_device.h` |
| `FluxMeta` | `uft/formats/uft_floppy_device.h` |
| `uft_error_t` | `uft/uft_error.h` |
| `uft_format_e` | `uft/uft_types.h` |
| `uft_encoding_t` | `uft/uft_types.h` |

---

## 2. Include-Hierarchie

### 2.1 Die goldene Include-Pyramide

```
                    ┌─────────────────────┐
                    │   uft/uft_types.h   │  ← Basis-Typen
                    │   uft/uft_error.h   │  ← Error-Codes
                    └─────────┬───────────┘
                              │
              ┌───────────────┼───────────────┐
              │               │               │
              ▼               ▼               ▼
    ┌─────────────────┐ ┌───────────┐ ┌─────────────────┐
    │ uft/core/*.h    │ │uft/safe.h │ │uft/uft_common.h │
    │ (Platform,      │ │(Safe Math)│ │(Utilities)      │
    │  Endian, etc.)  │ └───────────┘ └─────────────────┘
    └────────┬────────┘
             │
             ▼
    ┌─────────────────────────────────────────────────────┐
    │              uft/formats/uft_floppy_device.h        │
    │         (FloppyDevice, FluxTimingProfile, etc.)     │
    └────────────────────────┬────────────────────────────┘
                             │
         ┌───────────────────┼───────────────────┐
         │                   │                   │
         ▼                   ▼                   ▼
    ┌──────────┐       ┌──────────┐       ┌──────────┐
    │ d64.h    │       │ adf.h    │       │ scp.h    │
    │ g64.h    │       │ adz.h    │       │ woz.h    │
    │ ...      │       │ ...      │       │ ...      │
    └──────────┘       └──────────┘       └──────────┘
```

### 2.2 Zyklische Abhängigkeiten vermeiden

```c
// ❌ FALSCH - Zyklus
// In modul_a.h:
#include "modul_b.h"
// In modul_b.h:
#include "modul_a.h"

// ✅ RICHTIG - Forward Declaration
// In modul_a.h:
typedef struct modul_b_s modul_b_t;  // Forward Declaration
void funktion_mit_b(modul_b_t* b);

// In modul_a.c:
#include "modul_a.h"
#include "modul_b.h"  // Vollständige Definition nur in .c
```

---

## 3. Typ-Definitionen

### 3.1 Neue Strukturen richtig definieren

```c
// ✅ RICHTIG - Vollständige typedef
typedef struct {
    uint32_t field1;
    uint32_t field2;
} mein_typ_t;

// ✅ RICHTIG - Opaque Pointer (für private Implementierung)
typedef struct mein_privat_s mein_privat_t;

// ❌ FALSCH - Orphaned struct members
    uint32_t field1;
    uint32_t field2;
} mein_typ_t;  // typedef fehlt!
```

### 3.2 Existierende Typen NIE redefinieren

Bevor du einen neuen Typ definierst, prüfe:

```bash
# Suche nach existierenden Definitionen
grep -rn "typedef.*mein_typ" include/
```

### 3.3 Forward Declarations für Header-Zyklen

```c
// In uft_image.h
#ifndef UFT_IMAGE_H
#define UFT_IMAGE_H

// Forward Declaration statt #include
typedef struct uft_sector uft_sector_t;
typedef struct uft_track uft_track_t;

typedef struct uft_image {
    uft_track_t* tracks;
    // ...
} uft_image_t;

#endif
```

---

## 4. Error-Codes

### 4.1 Existierende Codes verwenden

**IMMER zuerst in `uft_error.h` nachschauen!**

```c
// Allgemeine Fehler
UFT_ERROR_INVALID_ARG       // Ungültiges Argument
UFT_ERROR_NULL_POINTER      // NULL-Pointer
UFT_ERROR_NOT_SUPPORTED     // Nicht unterstützt
UFT_ERROR_BOUNDS            // Bounds-Check fehlgeschlagen
UFT_ERROR_OVERFLOW          // Arithmetischer Overflow

// Datei-Fehler
UFT_ERROR_FILE_NOT_FOUND
UFT_ERROR_FILE_READ
UFT_ERROR_FILE_WRITE

// Format-Fehler
UFT_ERROR_FORMAT_UNKNOWN
UFT_ERROR_FORMAT_INVALID
UFT_ERROR_BAD_MAGIC
UFT_ERROR_BAD_CHECKSUM

// Disk-Fehler
UFT_ERROR_CRC_ERROR
UFT_ERROR_SECTOR_NOT_FOUND
UFT_ERROR_WEAK_BITS
```

### 4.2 Neue Error-Codes hinzufügen

Wenn ein neuer Code benötigt wird:

1. Kategorie bestimmen (siehe Bereiche in `uft_error.h`)
2. Code in richtigen Bereich einfügen
3. Dokumentation hinzufügen

```c
// In uft_error.h, im richtigen Bereich:
UFT_ERROR_MEIN_NEUER_CODE   = -XXX, ///< Beschreibung
```

---

## 5. Format-Module

### 5.1 Template für neues Format

```c
// include/uft/formats/mein_format.h
#ifndef UFT_MEIN_FORMAT_H
#define UFT_MEIN_FORMAT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "uft_floppy_device.h"

#ifdef __cplusplus
extern "C" {
#endif

// Format-spezifische Metadaten
typedef struct {
    uint32_t version;
    // ...
} MeinFormatMeta;

// Unified API (PFLICHT für alle Formate)
int floppy_open(FloppyDevice *dev, const char *path);
int floppy_close(FloppyDevice *dev);
int floppy_read_sector(FloppyDevice *dev, uint32_t t, uint32_t h, uint32_t s, uint8_t *buf);
int floppy_write_sector(FloppyDevice *dev, uint32_t t, uint32_t h, uint32_t s, const uint8_t *buf);
int floppy_analyze_protection(FloppyDevice *dev);

// Format-spezifische Funktionen
const MeinFormatMeta* mein_format_get_meta(const FloppyDevice *dev);

#ifdef __cplusplus
}
#endif

#endif // UFT_MEIN_FORMAT_H
```

### 5.2 Quelldatei-Template

```c
// src/formats/mein_format/mein_format.c

// WICHTIG: Vollständiger Include-Pfad!
#include "uft/formats/mein_format.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Implementierung...
```

### 5.3 Include-Pfad in Quelldateien

```c
// ❌ FALSCH
#include "mein_format.h"

// ✅ RICHTIG
#include "uft/formats/mein_format.h"
```

---

## 6. Checkliste für neue Dateien

### 6.1 Header-Datei Checkliste

- [ ] Include-Guard vorhanden (`#ifndef UFT_XXX_H`)
- [ ] Standard-Includes zuerst (`stdint.h`, `stdbool.h`, `stddef.h`)
- [ ] `size_t` verwendet → `#include <stddef.h>` vorhanden
- [ ] `time_t` verwendet → `#include <time.h>` vorhanden
- [ ] `FloppyDevice` verwendet → `#include "uft_floppy_device.h"` vorhanden
- [ ] Keine lokalen Typ-Duplikate
- [ ] Alle structs haben `typedef`
- [ ] `extern "C"` für C++ Kompatibilität
- [ ] Doxygen-Kommentare vorhanden

### 6.2 Quelldatei Checkliste

- [ ] Vollständiger Include-Pfad (`uft/formats/xxx.h`, nicht `xxx.h`)
- [ ] Nur existierende Error-Codes verwendet
- [ ] Keine neuen inline-Funktionen die woanders existieren
- [ ] Kompiliert mit `-Wall -Wextra` ohne Errors

### 6.3 Vor dem Commit

```bash
# 1. Header-Test
for f in include/uft/formats/*.h; do
    echo '#include "'$f'"' | gcc -c -fsyntax-only -Wall -Wextra -I include -x c -
done

# 2. Quellen-Test
gcc -c -fsyntax-only -Wall -Wextra -I include src/formats/mein_format/*.c

# 3. Duplikat-Check
grep -rn "typedef.*MeinTyp" include/
```

---

## 7. Anti-Patterns

### 7.1 ❌ Lokale Typ-Definitionen

```c
// ❌ NIEMALS in Format-Headern:
typedef struct {
    uint32_t tracks, heads, sectors, sectorSize;
    bool flux_supported;
    void (*log_callback)(const char* msg);
    void *internal_ctx;
} FloppyDevice;

// Diese Definition existiert bereits in uft_floppy_device.h!
```

### 7.2 ❌ Inline-Funktionen duplizieren

```c
// ❌ NIEMALS - existiert bereits in uft_safe.h
static inline bool uft_safe_mul_size(size_t a, size_t b, size_t* result) {
    // ...
}

// ✅ STATTDESSEN
#include "uft/uft_safe.h"
```

### 7.3 ❌ Fehlende Standard-Includes

```c
// ❌ FEHLER - size_t ohne stddef.h
int process_data(size_t count);  // Compiler: "unknown type name 'size_t'"

// ✅ RICHTIG
#include <stddef.h>
int process_data(size_t count);
```

### 7.4 ❌ Relative Include-Pfade in Quelldateien

```c
// ❌ FALSCH
#include "d64.h"
#include "../common/utils.h"

// ✅ RICHTIG
#include "uft/formats/d64.h"
#include "uft/common/utils.h"
```

### 7.5 ❌ Erfundene Error-Codes

```c
// ❌ FALSCH - Code existiert nicht!
return UFT_ERROR_MEIN_FEHLER;

// ✅ RICHTIG - Existierenden Code verwenden oder neuen definieren
return UFT_ERROR_FORMAT_INVALID;  // Existiert bereits
```

---

## Quick Reference Card

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        UFT DEVELOPER QUICK REFERENCE                        │
├─────────────────────────────────────────────────────────────────────────────┤
│  INCLUDE ORDER:                                                             │
│    1. <stdint.h> <stdbool.h> <stddef.h>                                    │
│    2. "uft/uft_error.h" "uft/uft_types.h"                                  │
│    3. "uft/formats/uft_floppy_device.h" (wenn FloppyDevice gebraucht)      │
│    4. Modul-spezifische Header                                             │
├─────────────────────────────────────────────────────────────────────────────┤
│  ZENTRALE TYP-HEADER:                                                       │
│    FloppyDevice, FluxMeta     → uft/formats/uft_floppy_device.h            │
│    uft_error_t, Error-Codes   → uft/uft_error.h                            │
│    uft_format_e, uft_encoding → uft/uft_types.h                            │
│    uft_safe_mul/add_*         → uft/uft_safe.h                             │
│    uft_read/write_le/be*      → uft/core/uft_endian.h                      │
├─────────────────────────────────────────────────────────────────────────────┤
│  VOR COMMIT:                                                                │
│    gcc -c -fsyntax-only -Wall -Wextra -I include <datei.c>                 │
│    grep -rn "typedef.*MeinTyp" include/  # Duplikat-Check                  │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

*Dieses Dokument basiert auf dem GOD MODE Audit vom Januar 2026.*  
*Bei Fragen: UFT Issue Tracker*
