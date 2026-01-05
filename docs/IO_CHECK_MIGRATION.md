# P1-IO-001: I/O Rückgabewerte Migration Guide

## Status: IN PROGRESS
**Datum:** 2026-01-05
**Gesamtaufwand:** ~1300 Stellen

## Safe I/O Header

Include hinzufügen:
```c
#include "uft/uft_safe_io.h"
```

## Verfügbare Macros

### Mit goto bei Fehler
```c
UFT_FREAD_OR_GOTO(ptr, size, count, stream, fail_label);
UFT_FWRITE_OR_GOTO(ptr, size, count, stream, fail_label);
UFT_FSEEK_OR_GOTO(stream, offset, whence, fail_label);
```

### Mit return bei Fehler
```c
UFT_FREAD_OR_RETURN(ptr, size, count, stream, retval);
UFT_FWRITE_OR_RETURN(ptr, size, count, stream, retval);
UFT_FSEEK_OR_RETURN(stream, offset, whence, retval);
```

### Funktionen
```c
bool uft_safe_fread(void* ptr, size_t size, size_t count, FILE* stream, size_t* bytes_read);
bool uft_safe_fwrite(const void* ptr, size_t size, size_t count, FILE* stream, size_t* bytes_written);
bool uft_safe_fseek(FILE* stream, long offset, int whence);
long uft_safe_ftell(FILE* stream);
ssize_t uft_read_exact(FILE* fp, void* buf, size_t size);
ssize_t uft_write_exact(FILE* fp, const void* buf, size_t size);
```

## Migration Patterns

### Pattern 1: fread ohne Check
```c
// VORHER
fread(buf, 1, 512, fp);

// NACHHER (Macro)
UFT_FREAD_OR_RETURN(buf, 1, 512, fp, -1);

// NACHHER (Funktion)
if (!uft_safe_fread(buf, 1, 512, fp, NULL)) {
    return -1;
}
```

### Pattern 2: fseek ohne Check
```c
// VORHER
fseek(fp, offset, SEEK_SET);

// NACHHER
if (fseek(fp, offset, SEEK_SET) != 0) {
    return UFT_ERR_SEEK;
}
```

### Pattern 3: fwrite ohne Check
```c
// VORHER
fwrite(buf, 1, 512, fp);

// NACHHER
UFT_FWRITE_OR_RETURN(buf, 1, 512, fp, -1);
```

## Kritische Dateien (Top 20)

| Datei | I/O Aufrufe | Status |
|-------|-------------|--------|
| afi_writer.c | 48 | Include ✅ |
| stx_loader.c | 36 | Include ✅ |
| uft_uff.c | 36 | Include ✅ |
| uft_ir_serialize.c | 35 | Teilweise ✅ |
| uft_flux_format.c | 32 | Include ✅ |
| uff_container.c | 29 | Include ✅ |
| imd_loader.c | 28 | Include ✅ |
| afi_loader.c | 28 | Ausstehend |
| h17_writer.c | 27 | Ausstehend |
| uft_batch.c | 26 | Teilweise ✅ |

## Verbleibende Aufgaben

1. **Kritisch (P0):** Dateien mit >20 I/O-Aufrufen vollständig migrieren
2. **Hoch (P1):** Dateien mit 10-20 I/O-Aufrufen migrieren
3. **Mittel (P2):** Restliche Dateien

## Statistik

| Kategorie | Vorher | Nachher | Reduziert |
|-----------|--------|---------|-----------|
| fread standalone | 421 | 153 | -268 |
| fwrite standalone | 455 | 427 | -28 |
| fseek standalone | 714 | 703 | -11 |

**Hinweis:** Viele Aufrufe sind in externem Code (HxC, etc.) und sollten vorsichtig behandelt werden.

