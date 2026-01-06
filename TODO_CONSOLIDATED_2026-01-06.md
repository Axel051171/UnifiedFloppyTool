# UFT TODO - Konsolidiert

**Stand:** 2026-01-06  
**Version:** 3.4.5  
**Nächster Review:** Nach Test-Suite Implementation

---

## LEGENDE

| Priorität | Bedeutung |
|-----------|-----------|
| **P0** | Datenverlust / Crash / Release-Blocker |
| **P1** | Falsche Ergebnisse / Schwere Bugs |
| **P2** | Performance / Architektur / Qualität |
| **P3** | Komfort / Doku / Polish |

| Aufwand | Zeit |
|---------|------|
| **S** | < 1 Tag |
| **M** | 1-3 Tage |
| **L** | > 3 Tage |

---

## P0 - KRITISCH

### P0-001: Test-Suite für Kernformate
- **Status:** OFFEN
- **Aufwand:** L
- **Module:** tests/
- **Beschreibung:** Nur 92 Test-Dateien für 416 Format-Module. Kernformate (D64, G64, ADF, HFE, SCP) haben keine automatisierten Tests.
- **Akzeptanzkriterien:** 
  - Mindestens 10 Kernformate mit Read/Write Tests
  - CI führt Tests automatisch aus
  - Test-Abdeckung > 20%
- **Abhängigkeiten:** Keine

### P0-002: CQM Dekompression
- **Status:** OFFEN
- **Aufwand:** M
- **Module:** src/formats/misc/cqm.c
- **Beschreibung:** CQM-Format kann nicht gelesen werden da Dekompression fehlt. Kommentar: "decompression not implemented in this no-deps module"
- **Akzeptanzkriterien:**
  - CQM-Dateien können gelesen werden
  - Test mit echten CQM-Images
- **Abhängigkeiten:** Keine

### P0-003: fread Buffer Overflow Warning
- **Status:** OFFEN
- **Aufwand:** M
- **Module:** unbekannt
- **Beschreibung:** Linux CI zeigt `fread_chk_warn` Warnung. Potenzielle Buffer Overflow bei File-Reads.
- **Akzeptanzkriterien:**
  - Keine fread_chk_warn in CI-Logs
  - ASan/Valgrind clean
- **Abhängigkeiten:** Vollständige Build-Reproduktion auf Ubuntu 24.04

---

## P1 - HOCH

### P1-001: Write-Tests für Kernformate
- **Status:** OFFEN
- **Aufwand:** M
- **Module:** tests/
- **Beschreibung:** 136 Formate haben Write-Support aber nur 4 sind getestet (IMD, DMK, TD0, SIMD).
- **Akzeptanzkriterien:**
  - D64, G64, ADF, HFE, SCP Write-Tests
  - Roundtrip-Tests (Read → Write → Read → Compare)
- **Abhängigkeiten:** P0-001

### P1-002: Memory Leak Audit
- **Status:** OFFEN
- **Aufwand:** M
- **Module:** src/batch/, src/forensic/, src/flux/
- **Beschreibung:** Potenzielle Memory Leaks in batch.c, forensic_report.c. Große Allokationen (8MB+) in kryoflux.c ohne Cleanup-Garantie.
- **Akzeptanzkriterien:**
  - Valgrind: Definitely lost = 0
  - Alle malloc haben entsprechenden free-Pfad
- **Abhängigkeiten:** Keine

### P1-003: Copy-Protection Stubs vervollständigen
- **Status:** OFFEN
- **Aufwand:** L
- **Module:** src/protection/
- **Beschreibung:** Viele TODO-Kommentare in Protection-Modulen:
  - uft_atarist_dec0de.c: 8 TODOs für Pattern
  - uft_fuzzy_bits.c: Flux-Level nicht implementiert
  - uft_protection_classify.c: Integration TODOs
- **Akzeptanzkriterien:**
  - Alle erkannten Protections funktionieren
  - Test-Images für jede Protection
- **Abhängigkeiten:** Keine

### P1-004: ATX Write-Support
- **Status:** OFFEN
- **Aufwand:** M
- **Module:** src/formats/atx/
- **Beschreibung:** ATX (Atari Protected) kann nur gelesen werden. Write fehlt.
- **Akzeptanzkriterien:**
  - ATX Write funktioniert
  - Roundtrip-Test
- **Abhängigkeiten:** Keine

### P1-005: KryoFlux Memory-Optimierung
- **Status:** OFFEN
- **Aufwand:** M
- **Module:** src/flux/uft_kryoflux.c
- **Beschreibung:** Zeilen 297, 358: `malloc(1000000 * sizeof(double))` = 8MB pro Aufruf. Keine Streaming-Verarbeitung.
- **Akzeptanzkriterien:**
  - Streaming-Parser für große Dateien
  - Max Memory < 1MB für Standard-Images
- **Abhängigkeiten:** Keine

### P1-006: Error-Recovery bei Parse-Fehlern
- **Status:** OFFEN
- **Aufwand:** M
- **Module:** src/parsers/
- **Beschreibung:** Bei beschädigten Images: sofortiger Abbruch statt Partial-Recovery.
- **Akzeptanzkriterien:**
  - Beschädigte Images werden soweit möglich gelesen
  - Fehler werden pro Track/Sektor gemeldet
- **Abhängigkeiten:** Keine

### P1-007: Batch CLI vervollständigen
- **Status:** OFFEN
- **Aufwand:** S
- **Module:** src/cli/uft_batch_cli.c
- **Beschreibung:** Batch-Funktionalität im Backend vorhanden aber CLI-Interface unvollständig.
- **Akzeptanzkriterien:**
  - `uft batch convert *.d64 --output adf/`
  - Progress-Anzeige
  - Fehler-Report
- **Abhängigkeiten:** Keine

---

## P2 - MITTEL

### P2-001: TODO/FIXME Audit
- **Status:** OFFEN
- **Aufwand:** L
- **Module:** projekt-weit
- **Beschreibung:** 2090 TODO/FIXME Einträge im Code. Viele sind veraltet oder erledigt.
- **Akzeptanzkriterien:**
  - Alle TODOs geprüft
  - Erledigte entfernt
  - Offene in Issue-Tracker
- **Abhängigkeiten:** Keine

### P2-002: GCR-Decoder vereinheitlichen
- **Status:** OFFEN
- **Aufwand:** M
- **Module:** src/decoders/, src/c64/, src/formats/commodore/
- **Beschreibung:** GCR-Dekodierung in 3+ Modulen dupliziert.
- **Akzeptanzkriterien:**
  - Ein zentraler GCR-Decoder
  - Alle Formate nutzen ihn
- **Abhängigkeiten:** Keine

### P2-003: CRC-Module konsolidieren
- **Status:** OFFEN
- **Aufwand:** M
- **Module:** src/crc/
- **Beschreibung:** Mehrere CRC-Implementierungen (crc16, crc32, ccitt, etc.). Keine einheitliche API.
- **Akzeptanzkriterien:**
  - Einheitliche uft_crc_* API
  - Redundante Implementierungen entfernt
- **Abhängigkeiten:** Keine

### P2-004: Public/Private API Trennung
- **Status:** OFFEN
- **Aufwand:** L
- **Module:** include/uft/
- **Beschreibung:** Keine klare Trennung zwischen Public API und internen Headern.
- **Akzeptanzkriterien:**
  - include/uft/public/ für externe API
  - include/uft/internal/ für interne
  - Dokumentierte API-Stabilität
- **Abhängigkeiten:** Keine

### P2-005: memcpy Optimierung
- **Status:** OFFEN
- **Aufwand:** M
- **Module:** projekt-weit
- **Beschreibung:** 1642 memcpy Aufrufe. Viele könnten durch Zero-Copy ersetzt werden.
- **Akzeptanzkriterien:**
  - Profiling identifiziert Hotspots
  - Top 10 optimiert
- **Abhängigkeiten:** Profiling-Infrastruktur

### P2-006: Forward-Declaration Warnung
- **Status:** OFFEN
- **Aufwand:** S
- **Module:** include/uft/uft_gui_params.h
- **Beschreibung:** `struct uft_dpll_config declared inside parameter list` Warnung
- **Akzeptanzkriterien:**
  - Keine "declared inside parameter list" Warnungen
- **Abhängigkeiten:** Keine

### P2-007: Hardware-Abstraction Layer
- **Status:** OFFEN
- **Aufwand:** L
- **Module:** src/hardware/
- **Beschreibung:** Hardware-Treiber haben keine einheitliche Abstraction. Jeder Treiber hat eigene API.
- **Akzeptanzkriterien:**
  - uft_hw_* einheitliche Interface
  - Hot-Plug Support
  - Einheitliches Error-Handling
- **Abhängigkeiten:** Keine

### P2-008: Redundante Format-Module
- **Status:** OFFEN
- **Aufwand:** M
- **Module:** src/formats/
- **Beschreibung:** Einige Formate haben mehrere Implementierungen (z.B. d64, adf).
- **Akzeptanzkriterien:**
  - Analyse welche Module redundant sind
  - Konsolidierung ohne Funktionsverlust
- **Abhängigkeiten:** P0-001 (Tests zuerst)

---

## P3 - NIEDRIG

### P3-001: API-Dokumentation
- **Status:** OFFEN
- **Aufwand:** M
- **Module:** docs/, include/
- **Beschreibung:** Header haben Doxygen-Kommentare aber keine generierte Doku.
- **Akzeptanzkriterien:**
  - Doxygen-Konfiguration
  - HTML-Dokumentation generiert
  - In CI automatisiert
- **Abhängigkeiten:** Keine

### P3-002: Beispiel-Programme
- **Status:** OFFEN
- **Aufwand:** S
- **Module:** examples/
- **Beschreibung:** Beispiele teils veraltet.
- **Akzeptanzkriterien:**
  - Alle Beispiele kompilieren
  - README mit Erklärungen
- **Abhängigkeiten:** Keine

### P3-003: Format-Katalog
- **Status:** OFFEN
- **Aufwand:** S
- **Module:** docs/FORMAT_CATALOG.md
- **Beschreibung:** Katalog unvollständig.
- **Akzeptanzkriterien:**
  - Alle 416 Formate gelistet
  - Read/Write/Repair Status
- **Abhängigkeiten:** Keine

### P3-004: Hardware-Setup Guides
- **Status:** OFFEN
- **Aufwand:** M
- **Module:** docs/
- **Beschreibung:** Keine Anleitungen für Hardware-Setup.
- **Akzeptanzkriterien:**
  - Guide für Greaseweazle
  - Guide für KryoFlux
  - Troubleshooting
- **Abhängigkeiten:** Keine

### P3-005: GUI Tooltips
- **Status:** OFFEN
- **Aufwand:** S
- **Module:** src/gui/
- **Beschreibung:** Wenig Tooltips/Hilfe in GUI.
- **Akzeptanzkriterien:**
  - Alle Buttons haben Tooltips
  - F1 öffnet Hilfe
- **Abhängigkeiten:** Keine

---

## ERLEDIGT (v3.4.5)

### ✅ Security Patches
- Command Injection in Tool Adapter gefixt
- Command Injection in OCR gefixt
- Syntax-Fehler in A2R/IPF Parser gefixt
- uft_security.h erstellt
- Security Compiler Flags (CMake)

### ✅ GitHub CI/CD
- ci.yml für Linux/macOS/Windows
- release.yml mit automatischen Releases
- CPack Konfiguration
- Qt6 Dependencies

### ✅ Cleanup
- 3.8 MB Duplikate entfernt
- Alte Library-Versionen gelöscht

### ✅ Build-Fixes (v3.4.6)
- **Windows C4146**: `-(crc & 1)` → Ternary-Operator in uft_writer_verify.c
- **macOS Linker**: `-z` und `-pie` Flags nur auf Linux (SecurityFlags.cmake)
- **Linux AppImage**: app_icon.png erstellt für linuxdeploy

---

## STATISTIK

| Priorität | Offen | Erledigt |
|-----------|-------|----------|
| P0 | 3 | 0 |
| P1 | 7 | 0 |
| P2 | 8 | 3 |
| P3 | 5 | 0 |
| **TOTAL** | **23** | **3** |

---

*Letzte Aktualisierung: 2026-01-06*
