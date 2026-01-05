# UFT CHEAT SHEET - Fehlerfreie Integration

## 🚨 DIE 7 HÄUFIGSTEN FEHLER (und wie man sie vermeidet)

### 1. FloppyDevice lokal definieren
```c
// ❌ FALSCH
typedef struct { uint32_t tracks; ... } FloppyDevice;

// ✅ RICHTIG
#include "uft_floppy_device.h"
```

### 2. size_t ohne stddef.h
```c
// ❌ FALSCH
int foo(size_t len);  // Compiler: "unknown type"

// ✅ RICHTIG
#include <stddef.h>
int foo(size_t len);
```

### 3. Include-Pfad ohne "uft/"
```c
// ❌ FALSCH (in .c Dateien)
#include "d64.h"

// ✅ RICHTIG
#include "uft/formats/d64.h"
```

### 4. Safe-Math Funktionen duplizieren
```c
// ❌ FALSCH - existiert bereits!
static inline bool uft_safe_mul_size(...) { }

// ✅ RICHTIG
#include "uft/uft_safe.h"
```

### 5. Erfundene Error-Codes
```c
// ❌ FALSCH
return UFT_ERROR_MEIN_CODE;  // Existiert nicht!

// ✅ RICHTIG - existierenden Code verwenden
return UFT_ERROR_FORMAT_INVALID;
```

### 6. Orphaned Struct (fehlendes typedef)
```c
// ❌ FALSCH
    uint32_t field;
} MeinTyp;

// ✅ RICHTIG
typedef struct {
    uint32_t field;
} MeinTyp;
```

### 7. Zyklische Includes
```c
// ❌ FALSCH
// a.h: #include "b.h"
// b.h: #include "a.h"

// ✅ RICHTIG - Forward Declaration
typedef struct b_s b_t;  // statt #include
```

---

## 📋 CHECKLISTE VOR COMMIT

```
□ #include <stddef.h> wenn size_t verwendet
□ #include <time.h> wenn time_t verwendet  
□ #include "uft_floppy_device.h" wenn FloppyDevice verwendet
□ Vollständiger Include-Pfad in .c Dateien (uft/formats/xxx.h)
□ Keine lokalen Kopien von FloppyDevice/FluxMeta/etc.
□ Keine lokalen Kopien von uft_safe_* / uft_read_le*
□ Nur existierende Error-Codes aus uft_error.h
□ Alle structs haben typedef
□ Kompiliert mit: gcc -Wall -Wextra -I include
```

---

## 🗂️ WO IST WAS DEFINIERT?

| Typ/Funktion | Header |
|--------------|--------|
| `FloppyDevice` | `uft/formats/uft_floppy_device.h` |
| `FluxTimingProfile` | `uft/formats/uft_floppy_device.h` |
| `FluxMeta` | `uft/formats/uft_floppy_device.h` |
| `WeakBitRegion` | `uft/formats/uft_floppy_device.h` |
| `uft_error_t` | `uft/uft_error.h` |
| `uft_format_e` | `uft/uft_types.h` |
| `uft_encoding_t` | `uft/uft_types.h` |
| `uft_safe_mul_size()` | `uft/uft_safe.h` |
| `uft_safe_add_size()` | `uft/uft_safe.h` |
| `uft_read_le16()` | `uft/core/uft_endian.h` |
| `uft_read_le32()` | `uft/core/uft_endian.h` |

---

## 📝 TEMPLATE: Neuer Format-Header

```c
#ifndef UFT_MEINFORMAT_H
#define UFT_MEINFORMAT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "uft_floppy_device.h"  // ← PFLICHT!

#ifdef __cplusplus
extern "C" {
#endif

// Eigene Strukturen hier...
typedef struct { ... } MeinFormatMeta;

// Unified API (PFLICHT)
int floppy_open(FloppyDevice *dev, const char *path);
int floppy_close(FloppyDevice *dev);
int floppy_read_sector(FloppyDevice *dev, uint32_t t, uint32_t h, uint32_t s, uint8_t *buf);
int floppy_write_sector(FloppyDevice *dev, uint32_t t, uint32_t h, uint32_t s, const uint8_t *buf);

#ifdef __cplusplus
}
#endif
#endif
```

---

## 🔧 SCHNELL-VALIDIERUNG

```bash
# Einzelne Datei testen
gcc -c -fsyntax-only -Wall -Wextra -I include meine_datei.c

# Header testen
echo '#include "include/uft/formats/mein.h"' | gcc -fsyntax-only -Wall -I include -x c -

# Automatische Validierung
./scripts/uft_validate.sh meine_datei.c
```

---

## ⚡ INCLUDE-REIHENFOLGE

```c
// 1. Standard-Bibliotheken
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// 2. UFT Core
#include "uft/uft_error.h"
#include "uft/uft_types.h"

// 3. UFT Module (nur wenn benötigt)
#include "uft/formats/uft_floppy_device.h"
#include "uft/uft_safe.h"
```

---

*Vollständige Dokumentation: docs/UFT_DEVELOPER_GUIDE.md*
