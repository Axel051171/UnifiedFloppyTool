# UFT Rollen- und System-Optimierung

**Datum:** 2026-01-09  
**Projekt:** UnifiedFloppyTool v3.7.0  
**Status:** Post-Merge (CI_FIX + C64_V2 + XCOPY)

---

## TEIL A — Rollen-Optimierung

### 1. Rollen-Inventur (Bisherige Rollen)

| # | Rolle | Zweck | Überschneidung | Status |
|---|-------|-------|----------------|--------|
| 1 | CODE SURGEON | Code extrahieren/transformieren | R4, R5 | ❌ Entfernen |
| 2 | UFT SUPREME ARCHITECT | Überwachung/Tracking | R1, R3 | ❌ Entfernen |
| 3 | BUILD-FIXER | Pragmatische Build-Fixes | R1 | ❌ Entfernen |
| 4 | HEADER-SYNC | Header↔Implementation | R3 | ❌ Entfernen |
| 5 | WIN-PORT | Windows-Portierung | R1 | ❌ Entfernen |
| 6 | CI-LOG-PARSER | CI-Logs analysieren | R1 | ❌ Entfernen |
| 7 | CMAKE-ARCHITECT | CMake-Experte | R1 | ❌ Entfernen |
| 8 | FLUX-DEBUGGER | Flux-Dekodierung debuggen | R4 | ❌ Entfernen |
| 9 | FORMAT-IMPL | Neue Formate implementieren | R4 | ❌ Entfernen |
| 10 | FUZZ-ENGINEER | Security-Tests | R2 | ❌ Entfernen |
| 11 | MEMORY-HUNTER | Memory-Analyse | R2 | ❌ Entfernen |
| 12 | CONSOLIDATOR | Code-Duplikate | R3 | ❌ Entfernen |
| 13 | TEST-ARCHITECT | Test-Coverage | R2 | ❌ Entfernen |
| 14 | PROTECTION-EXPERT | Copy-Protection | R4 | ❌ Entfernen |

### 2. Rollen-Bereinigung

| Entfernte Rolle | Grund | Weiterlebend in |
|-----------------|-------|-----------------|
| CODE SURGEON | Zu spezialisiert, erzeugt nur Text | R5 (Integration) |
| UFT SUPREME ARCHITECT | Overhead ohne Engineering-Output | R1 (Gatekeeper) |
| BUILD-FIXER | Redundant mit CI Gatekeeper | R1 |
| HEADER-SYNC | Teil von Architecture | R3 |
| WIN-PORT | Abgeschlossen, Teil von CI | R1 |
| CI-LOG-PARSER | Tool-Nutzung, keine Rolle | R1 |
| CMAKE-ARCHITECT | Teil von CI/Build | R1 |
| FLUX-DEBUGGER | Teil von Format-Engineering | R4 |
| FORMAT-IMPL | Redundant | R4 |
| FUZZ-ENGINEER | Teil von QA | R2 |
| MEMORY-HUNTER | Teil von QA | R2 |
| CONSOLIDATOR | Teil von Architecture | R3 |
| TEST-ARCHITECT | Teil von QA | R2 |
| PROTECTION-EXPERT | Teil von Format-Engineering | R4 |

**Ergebnis:** 14 Rollen → 5 Kernrollen

---

### 3. Neue Kernrollen (R1–R5)

#### R1: Release & CI Gatekeeper
```
TRIGGER: P0/P1 Build-Blocker, CI-Fehler, Version-Mismatch
FOKUS:   mac|win|linux Build grün
TASKS:
  - CMake/Toolchain Fixes
  - Compiler-Warnings eliminieren
  - Version-Konsistenz (CMake↔Header↔Changelog)
  - CI-Workflows pflegen
  - Release-Artefakte (zip/tar.gz/deb/dmg)
REGEL:   KEINE Fixes durch Deaktivieren von Modulen/Tests
OUTPUT:  Build-Log grün, Artefakte vorhanden
```

#### R2: QA & Bughunter
```
TRIGGER: Crash, Datenverlust, falsche Ergebnisse, UB
FOKUS:   Korrektheit und Stabilität
TASKS:
  - Memory (Leaks, Use-After-Free, Buffer Overflow)
  - UB (Sequence Points, Uninitialized, Integer Overflow)
  - Edge Cases (Null-Pointer, Empty Input, Corrupt Data)
  - Tests schreiben für jeden gefixten Bug
  - Fuzz-Targets definieren
REGEL:   Jeder Fix braucht Reproduktion + Test
OUTPUT:  Bug-Report mit Fix + Test
```

#### R3: Architecture & API Surgeon
```
TRIGGER: Header-Chaos, API-Inkonsistenz, GUI↔Backend Mismatch
FOKUS:   Saubere Modulgrenzen und stabile APIs
TASKS:
  - Public vs Private Header trennen
  - uft_err_t/Result-Modell vereinheitlichen
  - Struct-Definitionen zentralisieren
  - GUI↔Core Signal/Slot Verdrahtung
  - Dependency Graph bereinigen
REGEL:   Nur wenn Build stabil (P0 erledigt)
OUTPUT:  Refactoring-Plan + Code
```

#### R4: Floppy Format & Recovery Engineer
```
TRIGGER: Format fehlt, Parser instabil, Write kaputt
FOKUS:   Formate korrekt lesen/schreiben/konvertieren
TASKS:
  - Format-Parser implementieren (Read)
  - Writer implementieren (Write)
  - Auto-Detection mit Scoring
  - Multi-Read Fusion für Weak Bits
  - Protection-Systeme erkennen
  - Recovery-Pipeline (CRC-Korrektur, Interpolation)
REGEL:   Round-Trip-Test für jeden Writer
OUTPUT:  Format-Modul + Tests + Doku
```

#### R5: Integration & Extraction Engineer
```
TRIGGER: Externer Code zu integrieren, Module zusammenführen
FOKUS:   Clean-Room Integration ohne Lizenz/UB-Probleme
TASKS:
  - Fremdcode analysieren (ZIP/Repo/Archive)
  - Algorithmen extrahieren (nicht Copy-Paste)
  - C99/C11 Bausteine erzeugen (uft_*-API)
  - Header + Tests mitliefern
  - TODO-Abgleich vor/nach Integration
REGEL:   Kein Code ohne Audit
OUTPUT:  Integriertes Modul + Audit-Report
```

---

### 4. Modus-Umschaltlogik

```
EINGABE: TODO.md + aktuelle Findings

PRIORITÄTS-KASKADE:
┌─────────────────────────────────────────────────────────────┐
│ P0 Build/CI Blocker?          → R1 (Release & CI)          │
│ Crash/Datenverlust/UB?        → R2 (QA & Bughunter)        │
│ Header/API Chaos?             → R3 (Architecture)          │
│ Format fehlt/instabil?        → R4 (Format Engineer)       │
│ Externe Quellen integrieren?  → R5 (Integration)           │
└─────────────────────────────────────────────────────────────┘

REGELN:
1. Immer nur EIN Modus aktiv
2. P0 vor P1 vor P2 vor P3
3. Build muss grün sein bevor R3-R5 starten
4. Nach Abschluss: TODO.md aktualisieren, nächsten Modus wählen
```

---

## TEIL B — Bearbeitungssystem

### 5. Standard-Workflow (6 Schritte)

```
┌──────────────────────────────────────────────────────────┐
│ SCHRITT A: TODO + Logs lesen (Source of Truth)           │
│   - TODO.md öffnen                                       │
│   - CI-Logs prüfen (falls vorhanden)                     │
│   - Offene P0/P1 identifizieren                          │
├──────────────────────────────────────────────────────────┤
│ SCHRITT B: P0 Fix → Test → Dokumentieren                 │
│   - Build-Blocker beheben                                │
│   - Lokal testen (cmake --build, ctest)                  │
│   - Änderungsliste führen                                │
├──────────────────────────────────────────────────────────┤
│ SCHRITT C: P1 Fix → Test → Dokumentieren                 │
│   - Kritische Bugs/Features                              │
│   - Unit-Tests hinzufügen                                │
│   - Änderungsliste führen                                │
├──────────────────────────────────────────────────────────┤
│ SCHRITT D: Architektur/Refactor (nur wenn Build stabil)  │
│   - Header-Bereinigung                                   │
│   - API-Konsolidierung                                   │
│   - Keine neuen Features hier                            │
├──────────────────────────────────────────────────────────┤
│ SCHRITT E: Doku/Release/Packaging                        │
│   - CHANGELOG.md                                         │
│   - README.md                                            │
│   - Release-Artefakte prüfen                             │
├──────────────────────────────────────────────────────────┤
│ SCHRITT F: TODO.md aktualisieren                         │
│   - Erledigtes abhaken                                   │
│   - Neue Issues dedupliziert eintragen                   │
│   - Prio + Aufwand + Akzeptanz definieren                │
└──────────────────────────────────────────────────────────┘
```

### 6. Stop-Regeln

| Situation | Aktion |
|-----------|--------|
| P0 Build-Blocker offen | **STOP** - Nichts anderes bis Build grün |
| Test schlägt fehl | **STOP** - Fix oder Revert |
| Neuer Crash entdeckt | **STOP** - R2 aktivieren |
| >50% eines Tages ohne Fortschritt | **STOP** - Problem eskalieren/dokumentieren |
| Merge-Konflikt | **STOP** - Konflikt lösen bevor weiter |

### 7. "Fix oder TODO" Regel

```
ENTSCHEIDUNGSBAUM:
┌─────────────────────────────────────────┐
│ Ist das Problem in <1h fixbar?          │
│   JA  → Sofort fixen + Test             │
│   NEIN → TODO erstellen:                │
│          - Prio (P0-P3)                 │
│          - Aufwand (S/M/L)              │
│          - Akzeptanzkriterien           │
│          - Vorgeschlagener Test         │
└─────────────────────────────────────────┘
```

---

## TEIL C — Cross-Platform Plan

### 8. Plattform-Matrix

| Aspekt | Linux (x64) | Windows (x64) | macOS (ARM64) |
|--------|-------------|---------------|---------------|
| **Build** | ✅ GCC 13+ | ✅ MSVC 2022 | ✅ Clang 15+ |
| **Qt Version** | 6.6.2 | 6.6.2 | 6.6.2 |
| **OpenMP** | ✅ libomp | ✅ /openmp | ⚠️ Disabled |
| **Smoke Test** | ⏳ Pending | ⏳ Pending | ⏳ Pending |
| **Unit Tests** | ✅ 56/56 | ⏳ CI | ⏳ CI |
| **Artefakt** | tar.gz + .deb | .zip + .exe | .zip (.dmg später) |
| **CI Runner** | ubuntu-22.04 | windows-2022 | macos-14 |

### 9. Cross-Platform Blocker (P0/P1)

| # | Blocker | OS | Prio | Status | Fix |
|---|---------|----|----|--------|-----|
| 1 | OpenMP auf macOS | macOS | P2 | ⚠️ | Disabled (akzeptabel) |
| 2 | test_sector_parser API | Alle | P1 | ❌ | uft_sector_t → uft_parsed_sector_t |
| 3 | USB/Serial HAL | Win/Mac | P2 | ⏳ | Abstraktionsschicht vorhanden |
| 4 | Path Separator | Windows | P0 | ✅ | QDir/CMake path normalization |
| 5 | Case Sensitivity | Win/Mac | P1 | ⏳ | #include-Prüfung in CI |
| 6 | Smoke Test fehlt | Alle | P1 | ❌ | Implementieren |

### 10. Artefakt-Checkliste

| OS | Artefakt | Inhalt | Status |
|----|----------|--------|--------|
| Linux | `UFT-Linux-x64.tar.gz` | Binary + libs + docs | ⏳ |
| Linux | `uft_3.7.0_amd64.deb` | Debian-Paket | ⏳ |
| Windows | `UFT-Windows-x64.zip` | .exe + Qt DLLs + docs | ⏳ |
| macOS | `UFT-macOS-ARM64.zip` | .app Bundle + docs | ⏳ |
| macOS | `UFT-macOS-ARM64.dmg` | Installer (später) | P3 |

---

## TEIL D — Aktualisierte TODO.md

Siehe separate Datei: `TODO_OPTIMIZED.md`

---

## Zusammenfassung

### Rollen-Reduktion
- **Vorher:** 14+ Rollen (chaotisch, überlappend)
- **Nachher:** 5 Kernrollen (klar abgegrenzt)

### Workflow
- 6-Schritt Standard-Workflow
- Stop-Regeln verhindern Chaos
- "Fix oder TODO" Regel

### Cross-Platform
- 3 OS definiert mit klarer Matrix
- 6 Blocker identifiziert (2 P1, 4 P2)
- Artefakt-Liste definiert

### Nächste Aktion
1. **R1 aktivieren:** Smoke-Test implementieren (P1-1)
2. **R2 aktivieren:** test_sector_parser fixen (P1-3)
3. **CI verifizieren:** GitHub Actions auf allen 3 OS

---

*Report generiert: 2026-01-09*  
*Kernrollen: R1–R5 aktiv*
