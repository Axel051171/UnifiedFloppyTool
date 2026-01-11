# ADR-0003: Einheitliche Error Codes (uft_rc_t)

**Status:** Accepted  
**Datum:** 2025-01-11  
**Autor:** UFT Team

---

## Kontext

Das Projekt hatte historisch gemischte Return-Value-Konventionen:
- `int` mit -1/0/1
- `bool` für Erfolg/Fehler
- `NULL` für Pointer-Fehler
- Verschiedene Error-Enums pro Modul

Dies führte zu:
- Inkonsistenter Fehlerbehandlung
- Schwieriger Debugging
- Keine einheitliche Logging-Strategie

## Entscheidung

**Wir verwenden `uft_rc_t` als einheitlichen Return-Typ für alle 
Funktionen, die fehlschlagen können.**

```c
typedef enum {
    UFT_OK = 0,
    UFT_ERROR = -1,
    UFT_EINVAL = -2,      // Invalid argument
    UFT_EIO = -3,         // I/O error
    UFT_ENOMEM = -4,      // Out of memory
    UFT_ENOENT = -5,      // Not found
    UFT_EEXIST = -6,      // Already exists
    UFT_ENOSYS = -7,      // Not implemented
    UFT_EFORMAT = -8,     // Format error
    UFT_ECRC = -9,        // CRC/checksum error
    UFT_ETRUNC = -10,     // Truncated data
} uft_rc_t;
```

Zusätzlich: `uft_diag_t` für detaillierte Fehlermeldungen:
```c
typedef struct {
    char message[256];
    const char *file;
    int line;
} uft_diag_t;

#define uft_diag_set(d, msg) do { \
    if (d) { snprintf((d)->message, 256, "%s", msg); \
             (d)->file = __FILE__; (d)->line = __LINE__; } \
} while(0)
```

## Begründung

| Ansatz | Typsicher | Erweiterbar | Debuggbar |
|--------|-----------|-------------|-----------|
| `int` return | ❌ | ❌ | ❌ |
| `bool` | ❌ | ❌ | ❌ |
| `errno` global | ⚠️ | ✅ | ⚠️ |
| **`uft_rc_t` enum** | ✅ | ✅ | ✅ |

## Konsequenzen

### Positiv ✅
- Einheitliche Fehlerbehandlung im gesamten Projekt
- Compiler-Warnung bei unbehandelten Enum-Werten
- Einfaches Error-Logging

### Negativ ⚠️
- Bestehender Code muss migriert werden
- Mehr Boilerplate bei einfachen Funktionen

### Coding-Pattern
```c
// Funktion mit Fehlerbehandlung
uft_rc_t uft_image_load(const char *path, uft_image_t *img, uft_diag_t *diag) {
    if (!path || !img) {
        uft_diag_set(diag, "Invalid arguments");
        return UFT_EINVAL;
    }
    
    FILE *f = fopen(path, "rb");
    if (!f) {
        uft_diag_set(diag, "Failed to open file");
        return UFT_EIO;
    }
    
    // ...
    return UFT_OK;
}

// Aufruf
uft_diag_t diag = {0};
uft_rc_t rc = uft_image_load("disk.d64", &img, &diag);
if (rc != UFT_OK) {
    fprintf(stderr, "Error: %s (%s:%d)\n", diag.message, diag.file, diag.line);
}
```

---

## Änderungshistorie

| Datum | Autor | Änderung |
|-------|-------|----------|
| 2025-01-11 | UFT Team | Initial erstellt |
