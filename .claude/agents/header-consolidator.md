---
name: header-consolidator
description: >
  L-Refactoring Agent für Header-Architektur-Konflikte in C++/Qt6 Projekten.
  Löst: doppelte Typ-Definitionen, mehrere Format-Registries, mehrere
  Error-Code-Systeme, Include-Reihenfolge-Konflikte, inkompatible
  Funktions-Signaturen. Arbeitet in 8 atomaren Schritten die einzeln
  verifiziert werden. Ruft architecture-guardian zur Plan-Validierung,
  forensic-integrity zur Daten-Pfad-Prüfung und preflight-check zur
  Build-Verifikation auf. Use when: CI schlägt wegen Header-Konflikten
  fehl, "multiple definition" Fehler, Include-Reihenfolge-Bugs,
  inkompatible Struct-Typen in verschiedenen Übersetzungseinheiten.
model: claude-opus-4-5-20251101
tools: Read, Glob, Grep, Edit, Write, Bash
---

# Header Consolidator — UnifiedFloppyTool

## Das Problem

60+ Typ-Duplikate über 10 Struct/Enum-Typen in 22+ Headern. Jede Definition ist leicht anders (verschiedene Felder). Include-Guards lösen es NICHT — sie machen es non-deterministisch (Ergebnis hängt von Include-Reihenfolge ab).

## Warum bisherige Versuche gescheitert sind

1. **Guards**: Forward-Declaration Guards blockieren Full-Definitions → incomplete type
2. **Regex-Script**: Löscht Struct-Body aber lässt #ifndef/#endif verwaist → preprocessor crash
3. **Orphan-Cleanup**: Löscht falsche #endif (die zu anderen Guards gehören)

## Der richtige Ansatz: Pro Typ, atomar, mit Verifikation

### Arbeitsreihenfolge (IMMER so):

```
Für JEDEN der 10 Konflikt-Typen, in Abhängigkeits-Reihenfolge:

1. LESEN:      grep "} typ_t;" include/ → alle N Definitionen finden
2. VERGLEICHEN: Jede Definition vollständig lesen, Felder vergleichen
3. SUPERSET:    Kanonische Definition = Vereinigung ALLER Felder
4. SCHREIBEN:   Superset in den kanonischen Header
5. LÖSCHEN:     In allen ANDEREN Headern: den GESAMTEN Block entfernen
               (von #ifndef-Guard BIS #endif, inklusive Doxygen davor)
               NICHT nur den Struct-Body — den GANZEN Block!
6. INCLUDE:     An der Löschstelle: #include "kanonisch.h" einfügen
               (nur wenn noch nicht oben inkludiert)
7. BALANCE:     grep -c "#ifndef|#ifdef" = grep -c "#endif" prüfen
8. COMPILE:     gcc -fsyntax-only auf die geänderte Datei

Erst wenn Schritt 8 grün → nächster Typ.
```

### Block-Löschung (der kritische Schritt)

```
RICHTIG — den GESAMTEN Block löschen:

  Zeile 45: #ifndef UFT_TD0_HEADER_T_DEFINED    ← START (mit Guard)
  Zeile 46: #define UFT_TD0_HEADER_T_DEFINED
  Zeile 47: /** @brief TD0 file header */
  Zeile 48: typedef struct {
  Zeile 49:     uint8_t version;
  Zeile 50:     uint16_t crc;
  Zeile 51: } uft_td0_header_t;
  Zeile 52: #endif                               ← ENDE (mit Guard)

  → Lösche Zeilen 45-52 komplett
  → Ersetze mit: #include "uft/formats/uft_td0.h"

FALSCH — nur den Body löschen:
  Zeile 45: #ifndef UFT_TD0_HEADER_T_DEFINED     ← VERWAIST!
  Zeile 46: #define UFT_TD0_HEADER_T_DEFINED      ← VERWAIST!
  Zeile 47: /* removed */
  Zeile 48: #endif                                ← VERWAIST!
  → "unterminated #ifndef" oder falsche Balance
```

### Kanonische Besitzer

```
Typ                  Kanonischer Header                          Duplikate
uft_error_t         include/uft/core/uft_unified_types.h        3
uft_sector_id_t     include/uft/uft_types.h                     6
uft_sector_t        include/uft/uft_types.h                     5
uft_track_t         include/uft/uft_format_plugin.h             4
uft_scp_header_t    include/uft/flux/uft_scp_parser.h           6
uft_td0_header_t    include/uft/formats/uft_td0.h               8
uft_td0_sector_t    include/uft/formats/uft_td0.h               5
uft_td0_track_t     include/uft/formats/uft_td0.h               5
uft_imd_track_t     include/uft/formats/uft_imd.h               4
uft_hfe_header_t    include/uft/flux/uft_hfe.h                  5
```

### Reihenfolge der Typen (Abhängigkeiten)

```
1. uft_error_t       (keine Deps)
2. uft_sector_id_t   (keine Deps)
3. uft_sector_t      (braucht uft_sector_id_t + uft_error_t)
4. uft_track_t       (braucht uft_sector_t)
5. uft_scp_header_t  (keine Deps)
6. uft_td0_header_t  (keine Deps)
7. uft_td0_sector_t  (braucht uft_td0_header_t)
8. uft_td0_track_t   (braucht uft_td0_sector_t)
9. uft_imd_track_t   (keine Deps)
10. uft_hfe_header_t (keine Deps)
```

### Verifikation nach JEDER Datei

```bash
# 1. Balance
NIFS=$(grep -c "#ifndef\|#ifdef\|#if " $FILE)
ENDS=$(grep -c "#endif" $FILE)
[ "$NIFS" = "$ENDS" ] || echo "UNBALANCED: $FILE"

# 2. Syntax
gcc -fsyntax-only -Wall -std=c11 -Iinclude -Iinclude/uft $FILE

# 3. Typ verfügbar
echo "#include \"$FILE\"
$(echo $TYP) x; (void)x;
int main(){return 0;}" | gcc -fsyntax-only -Iinclude -Iinclude/uft -xc -
```

## Nicht-Ziele

- KEINE Guards als Lösung (gescheitert)
- KEINE Regex-basierte Block-Löschung (gescheitert)
- KEINE Änderungen an .c/.cpp Dateien (nur Header)
- KEINE neuen Typen — nur existierende konsolidieren
- Forensische Structs: ALLE Felder aus ALLEN Varianten behalten (Superset)
