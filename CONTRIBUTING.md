# UFT Contributing Guidelines

## 🎯 Ziel: "Bei uns geht kein Bit verloren" - auch keine Build-Fehler!

Diese Guidelines basieren auf **tatsächlichen Build-Fehlern** die wir in der Entwicklung gefunden haben.
Wenn du sie befolgst, vermeidest du die häufigsten Probleme.

---

## ✅ Vor jedem Commit

```bash
./scripts/validate.sh
```

Dieses Script prüft automatisch auf die häufigsten Fehler.

---

## 📋 Coding Rules

### 1. Header-Dateien

#### Include-Guards sind PFLICHT
```c
// ✓ RICHTIG
#ifndef UFT_MEIN_HEADER_H
#define UFT_MEIN_HEADER_H
// ... code ...
#endif /* UFT_MEIN_HEADER_H */

// ✗ FALSCH - kein Include-Guard
// ... code ...
```

#### Typ-Definitionen brauchen Guards
```c
// ✓ RICHTIG - Verhindert Doppel-Definition
#ifndef UFT_SECTOR_T_DEFINED
#define UFT_SECTOR_T_DEFINED
typedef struct uft_sector {
    // ...
} uft_sector_t;
#endif

// ✗ FALSCH - Kann bei mehrfachem Include crashen
typedef struct uft_sector {
    // ...
} uft_sector_t;
```

### 2. Konstanten

#### Alle Konstanten mit Fallback definieren
```c
// ✓ RICHTIG
#ifndef UFT_CRC16_INIT
#define UFT_CRC16_INIT 0xFFFF
#endif

// ✗ FALSCH - Kann undeclared-Fehler geben
#define UFT_CRC16_INIT 0xFFFF  // ohne #ifndef
```

### 3. MSVC-Kompatibilität

#### Atomic-Typen immer über uft_atomics.h
```c
// ✓ RICHTIG
#include "uft/uft_atomics.h"
atomic_int counter;

// ✗ FALSCH - Kompiliert nicht auf MSVC
#include <stdatomic.h>
atomic_int counter;
```

#### Keine C99-VLAs
```c
// ✓ RICHTIG
uint8_t *buffer = malloc(size);

// ✗ FALSCH - MSVC unterstützt keine VLAs
uint8_t buffer[size];  // size ist Variable
```

### 4. Struct-Erweiterungen

#### Neue Members immer dokumentieren
```c
// ✓ RICHTIG
typedef struct {
    uint32_t id;              /**< Unique ID */
    uft_platform_t platform;  /**< Target platform */
    // ... existing members ...
} uft_prot_scheme_t;

// Wenn du einen Member hinzufügst, auch in CHANGELOG.md erwähnen!
```

### 5. Error-Codes

#### Immer uft_error_compat.h für Legacy-Codes
```c
// ✓ RICHTIG - Funktioniert immer
#include "uft/uft_error.h"
#include "uft/uft_error_compat.h"
return UFT_ERROR_FORMAT_NOT_SUPPORTED;

// ✗ FALSCH - Kann undeclared sein
return UFT_ERROR_FORMAT_NOT_SUPPORTED;  // ohne include
```

### 6. Include-Pfade

#### Relative Pfade verwenden
```c
// ✓ RICHTIG
#include "uft/uft_types.h"
#include "uft_local.h"

// ✗ FALSCH
#include "/home/user/uft/include/uft/uft_types.h"
#include "C:\\Projects\\uft\\include\\uft_types.h"
```

---

## 🧪 Tests

### Mindestens diese Tests müssen grün sein:

1. **Include-Guards**: Alle Header haben Guards
2. **Kritische Typen**: uft_sector_t, uft_track_t, etc. sind definiert
3. **CRC-Konstanten**: UFT_CRC16_INIT, UFT_CRC16_POLY
4. **MFM-Konstanten**: UFT_MFM_MARK_IDAM, UFT_MFM_MARK_DAM
5. **Cross-Platform**: Build auf Linux, macOS, Windows

---

## 📁 Verzeichnisstruktur

```
include/
├── uft/                    # Haupt-Header
│   ├── core/               # Kern-Funktionen
│   ├── crc/                # CRC-Implementierungen
│   ├── compat/             # Kompatibilitäts-Layer
│   ├── decoders/           # Format-Decoder
│   └── ...
├── tracks/                 # Track-Generatoren
└── ...

src/
├── core/                   # Kern-Implementierung
├── crc/                    # CRC-Code
├── loaders/                # Format-Loader
└── ...
```

---

## 🔄 Pull Request Checklist

- [ ] `./scripts/validate.sh` läuft ohne Fehler
- [ ] Neue Header haben Include-Guards
- [ ] Neue Typen haben Definition-Guards
- [ ] Keine absoluten Include-Pfade
- [ ] MSVC-kompatibel (kein VLA, uft_atomics.h)
- [ ] Error-Codes aus uft_error.h/uft_error_compat.h
- [ ] CHANGELOG.md aktualisiert

---

## 🐛 Häufige Fehler und Lösungen

| Fehler | Ursache | Lösung |
|--------|---------|--------|
| `undeclared identifier 'UFT_CRC16_INIT'` | Fehlender Include | `#include "uft/uft_mfm_flux.h"` |
| `'atomic_int' undeclared` | MSVC-Inkompatibilität | `#include "uft/uft_atomics.h"` |
| `redefinition of 'uft_sector_t'` | Fehlender Definition-Guard | `#ifndef UFT_SECTOR_T_DEFINED` |
| `'stats' is not a member` | Struct-Member fehlt | Member zur Struct hinzufügen |

---

*Letzte Aktualisierung: 2026-01-08*
