# UFT - Bekannte Fehler & Lösungen

> Diese Datei dokumentiert alle Fehler, die während der Entwicklung aufgetreten sind,
> damit sie nicht wiederholt werden.

---

## 🔴 Kritische Fehler

### 1. `UFT_CRC16_INIT undeclared`

**Symptom:**
```
error: use of undeclared identifier 'UFT_CRC16_INIT'
```

**Ursache:** Header nicht inkludiert

**Lösung:**
```c
#include "uft_mfm_flux.h"  // Enthält UFT_CRC16_INIT, UFT_CRC16_POLY
```

**Betroffene Dateien:** `src/core/uft_crc16_ccitt.c`, alle MFM-Decoder

---

### 2. `atomic_int` / `atomic_bool` MSVC Error

**Symptom (MSVC):**
```
error C2061: syntax error: identifier 'atomic_int'
```

**Ursache:** MSVC kennt `<stdatomic.h>` nicht (vor C11)

**Lösung:**
```c
#include "uft_atomics.h"  // Cross-Platform Wrapper
```

**Betroffene Dateien:** `src/core/uft_parallel.c`, alle Thread-Code

---

### 3. `uft_prot_scheme_t` fehlende Members

**Symptom:**
```
error C2039: 'indicators': is not a member of 'uft_prot_scheme_t'
```

**Ursache:** Struct wurde ohne alle erforderlichen Members definiert

**Lösung:** Stelle sicher dass `include/uft/uft_protection.h` diese Members hat:
- `uint32_t id`
- `uft_platform_t platform`
- `uft_prot_indicator_t indicators[16]`
- `char notes[256]`

---

### 4. `tracks/track_generator.h: No such file`

**Symptom:**
```
fatal error: tracks/track_generator.h: No such file or directory
```

**Ursache:** Include-Pfad nicht konfiguriert

**Lösung:** In CMakeLists.txt:
```cmake
include_directories(${CMAKE_SOURCE_DIR}/include/tracks)
```

Oder Header kopieren:
```bash
cp src/tracks/track_generator.h include/tracks/
```

---

### 5. `uft_md_session` hat kein `stats` Member

**Symptom:**
```
error C2039: 'stats': is not a member of 'uft_md_session'
```

**Lösung:** In `include/uft/uft_multi_decode.h`:
```c
typedef struct uft_md_session {
    // ... andere Members ...
    
    struct {
        uint32_t total_sectors;
        uint32_t resolved_count;
        uint32_t conflict_count;
        float avg_confidence;
    } stats;
    
    // ...
} uft_md_session_t;
```

---

## 🟡 Häufige Warnungen

### 1. `redefinition of struct`

**Ursache:** Typ in mehreren Headern definiert

**Lösung:** Include-Guard verwenden:
```c
#ifndef UFT_SECTOR_T_DEFINED
#define UFT_SECTOR_T_DEFINED
typedef struct uft_sector { ... } uft_sector_t;
#endif
```

---

### 2. `implicit declaration of function`

**Ursache:** Funktion verwendet bevor Header inkludiert

**Lösung:** Header am Anfang der Datei inkludieren

---

## 📋 Checkliste: Neuer Code

Vor jedem Commit prüfen:

- [ ] Alle verwendeten Konstanten haben zugehörigen `#include`
- [ ] Keine direkten `<stdatomic.h>` Includes (verwende `uft_atomics.h`)
- [ ] Struct-Erweiterungen: Alle Verwendungen geprüft
- [ ] Header haben Include-Guards
- [ ] `./scripts/validate.sh` läuft durch

---

## 🔧 Automatische Prüfung

```bash
# Lokal vor Commit
./scripts/validate.sh

# Git Hook installieren (prüft automatisch)
./scripts/install-hooks.sh
```

---

*Letzte Aktualisierung: 2026-01-08*
