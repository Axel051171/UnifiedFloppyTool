# UFT Versioning Policy

**Version:** 1.0  
**Status:** VERBINDLICH

---

## 1. Semantic Versioning

UFT verwendet [Semantic Versioning 2.0.0](https://semver.org/):

```
MAJOR.MINOR.PATCH

2.9.0
│ │ └── Patch: Bug-Fixes, keine API-Änderungen
│ └──── Minor: Neue Features, rückwärtskompatibel
└────── Major: Breaking Changes
```

---

## 2. Breaking Changes

### 2.1 Was ist ein Breaking Change?

- Entfernung einer public API-Funktion
- Änderung von Funktions-Signaturen
- Änderung von Struct-Layouts
- Änderung von Return-Werten/Semantik
- Entfernung von Format-Support

### 2.2 Was ist KEIN Breaking Change?

- Neue Funktionen
- Neue Formate
- Bug-Fixes
- Performance-Verbesserungen
- Neue optionale Parameter

### 2.3 Breaking Change Prozess

1. **Deprecation Warning** in Minor Version N
2. **Documentation** der Migration
3. **Removal** in Major Version N+1 (frühestens)
4. **Mindestens 6 Monate** zwischen Deprecation und Removal

---

## 3. Deprecation

### 3.1 Markierung

```c
// Header: Deprecated-Marker
UFT_DEPRECATED("Use uft_open_v2() instead, will be removed in v3.0")
uft_error_t uft_open(uft_disk_t* disk, const char* path);

// Implementation: Warning
#pragma message("uft_open is deprecated, use uft_open_v2")
```

### 3.2 Dokumentation

Jede Deprecation muss enthalten:
- Grund für Deprecation
- Empfohlene Alternative
- Geplantes Removal-Datum
- Migration Guide

---

## 4. Version Macros

```c
// include/uft/uft_version.h

#define UFT_VERSION_MAJOR 2
#define UFT_VERSION_MINOR 9
#define UFT_VERSION_PATCH 0

#define UFT_VERSION_STRING "2.9.0"

#define UFT_VERSION_CHECK(major, minor, patch) \
    ((UFT_VERSION_MAJOR > (major)) || \
     (UFT_VERSION_MAJOR == (major) && UFT_VERSION_MINOR > (minor)) || \
     (UFT_VERSION_MAJOR == (major) && UFT_VERSION_MINOR == (minor) && \
      UFT_VERSION_PATCH >= (patch)))

// Runtime check
const char* uft_version_string(void);
int uft_version_major(void);
int uft_version_minor(void);
int uft_version_patch(void);
```

---

## 5. ABI Stability

### 5.1 Regeln

- **Struct-Größen:** Nicht ändern zwischen Minor Versions
- **Enum-Werte:** Nur am Ende hinzufügen
- **Function Signatures:** Nicht ändern

### 5.2 ABI Version

```c
#define UFT_ABI_VERSION 1

// Bei jedem Breaking Change: ABI_VERSION++
```

---

## 6. Changelog Format

Wir verwenden [Keep a Changelog](https://keepachangelog.com/):

```markdown
# Changelog

## [Unreleased]

## [2.9.0] - 2026-01-15

### Added
- Hardened parsers for SCP, D64, G64, HFE, ADF
- Fuzz testing infrastructure
- HAL v2 unified hardware interface

### Changed
- Improved format detection confidence scoring

### Deprecated
- `uft_open()` - use `uft_open_v2()` instead

### Fixed
- BUG-001: SCP header checksum overflow
- BUG-002: D64 error info bounds check

### Security
- Fixed integer overflow in track offset calculation
```

---

**DOKUMENT ENDE**
