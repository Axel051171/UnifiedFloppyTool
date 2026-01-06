# UFT - TODO Liste

**Stand:** 2026-01-06 v3.4.0

## P0 - KRITISCH (Build-Blocker)

Keine offenen P0-Probleme.

## P1 - HOCH (Funktionalität)

### TODO-P1-001: fread Buffer Overflow Warning
- **Status:** OFFEN
- **Aufwand:** M
- **Beschreibung:** Linux CI zeigt `fread_chk_warn` Warnung
- **Betroffene Module:** unbekannt (generische glibc-Warnung)
- **Akzeptanzkriterien:** Keine fread_chk_warn in CI-Logs
- **Abhängigkeiten:** Vollständige Build-Reproduktion auf Ubuntu 24.04

## P2 - MITTEL (Qualität)

Keine offenen P2-Probleme - alle unused variable Warnungen behoben.

## P3 - NIEDRIG (Wartbarkeit)

### TODO-P3-001: Forward Declaration Optimierung
- **Status:** OFFEN
- **Aufwand:** S
- **Beschreibung:** `struct uft_dpll_config declared inside parameter list` Warnung
- **Betroffene Dateien:** include/uft/uft_gui_params.h
- **Akzeptanzkriterien:** Keine "declared inside parameter list" Warnungen
- **Abhängigkeiten:** Keine

## ABGESCHLOSSEN (v3.4.0)

### ✅ P0-001: flashfloppy Header fehlend
- Datei existierte lokal, war nicht im Repo
- Fix: Datei zum Repo hinzugefügt

### ✅ P0-002: forensic Header fehlend
- Datei existierte lokal, war nicht im Repo
- Fix: Datei zum Repo hinzugefügt

### ✅ P1-001: UFT_PACKED_BEGIN Redefinition
- Macro in 8 Dateien definiert
- Fix: #ifndef Guards zu allen Definitionen hinzugefügt

### ✅ P1-002 bis P1-005: POSIX Implicit Declarations
- gethostname, strdup, strcasecmp, clock_gettime, usleep
- Fix: _POSIX_C_SOURCE=200809L in CMakeLists.txt

### ✅ P1-007: UB Shifting Negative Value
- src/crc/poly.c ~(~0U << CHAR_BIT)
- Fix: Zu 0xFFU geändert

### ✅ P1-008: MSVC strncpy Deprecation
- Fix: _CRT_SECURE_NO_WARNINGS in CMakeLists.txt

### ✅ P2-001 bis P2-008: Unused Variables
- last_error, freq_error_diff, read_le16, lastbit, total_gap, dups, dup_count, expected
- Fix: (void) casts und __attribute__((unused))

### ✅ P3-001: UFT_PACKED Chaos
- Fix: Zentrale uft_packed.h erstellt

## STATISTIK

| Priorität | Offen | Behoben |
|-----------|-------|---------|
| P0 | 0 | 2 |
| P1 | 1 | 7 |
| P2 | 0 | 8 |
| P3 | 1 | 1 |
| **TOTAL** | **2** | **18** |
