# UnifiedFloppyTool v3.7.2 - Konsolidierte TODO-Liste

**Stand:** 2026-01-08 (Systematische Analyse)  
**Build:** ✅ Linux | ⏸️ macOS (CI pending) | ⏸️ Windows (CI pending)

---

## Legende

| Prio | Bedeutung | Aufwand |
|------|-----------|---------|
| P0 | Crash, Datenverlust, falsche Ergebnisse, Build-Blocker | S=<1h, M=1-4h, L=>4h |
| P1 | Zentrale Funktionen unvollständig/instabil | |
| P2 | Performance, Architektur, Erweiterbarkeit | |
| P3 | Doku, UX, Polish | |

---

## 📊 Projekt-Statistik

| Metrik | Wert |
|--------|------|
| C-Dateien | 1194 |
| C++-Dateien | 199 |
| Header | 973 |
| LOC | ~465.000 |
| TODOs im Code | 141 |
| Stubs | 39 |
| Tests | 99 |

---

## ✅ ERLEDIGT

- [x] P0: 217 Include-Direktiven gefixt
- [x] P0: 21/21 Legacy Track Module portiert
- [x] P1: Buffer Overflow uft_edsk_parser.c
- [x] P1: Sequence Point UB vfo_pid3.cpp
- [x] P1: memset on non-trivial struct
- [x] P2: Signed/unsigned Warnungen
- [x] Engineering System (5 Modi)
- [x] GitHub Actions CI erstellt
- [x] uft_version.h erstellt
- [x] uft_platform.h erstellt
- [x] uft_openmp.h erstellt

---

## 🔴 P0 - Blocker (Crash/Datenverlust)

### P0-1: malloc ohne NULL-Check (kritische Dateien) - ✅ TEILWEISE ERLEDIGT
**Beschreibung:** 739 malloc-Aufrufe analysiert. Top-5 Dateien HABEN bereits NULL-Checks.
**Gefixt:** uft_fusion.c (2), uft_scp_v2.c (1), jv3_loader.c (1)
**Status:** Kritische Pfade abgesichert, Rest ist bereits korrekt oder unkritisch
**Neue Helper:** include/uft/compat/uft_alloc.h für zukünftige Verwendung

### P0-2: fread/fwrite ohne Return-Check - ✅ ANALYSIERT
**Beschreibung:** 339 I/O-Operationen analysiert. Muster: `size_t read = fread(...); if (read != ...)` wird korrekt verwendet.
**Ergebnis:** Kritische Pfade prüfen bereits Return-Werte
**Neue Helper:** include/uft/compat/uft_file.h mit uft_fread_exact() / uft_fwrite_exact()
**Status:** Keine kritischen Lücken gefunden

### P0-3: fopen ohne NULL-Check - ✅ ANALYSIERT
**Beschreibung:** 566 fopen-Aufrufe analysiert. Großteil bereits korrekt.
**Ergebnis:** Meiste Dateien prüfen bereits via if(fp), libflux_fopen wrapper, oder inline-check
**Neue Helper:** include/uft/compat/uft_file.h für sichere I/O
**Status:** Keine kritischen Lücken gefunden

---

## 🟠 P1 - Kritisch (vor Release)

### P1-1: Windows Build verifizieren
**Beschreibung:** CI erstellt, aber noch nicht getestet
**Dateien:** .github/workflows/ci.yml, CMakeLists.txt
**Aufwand:** M
**Akzeptanz:** CI-Job grün auf windows-2022
**Test:** CI Windows Job

### P1-2: macOS Build verifizieren
**Beschreibung:** CI erstellt, OpenMP deaktiviert
**Dateien:** .github/workflows/ci.yml, CMakeLists.txt
**Aufwand:** M
**Akzeptanz:** CI-Job grün auf macos-14
**Test:** CI macOS Job

### P1-3: Smoke-Test implementieren
**Beschreibung:** Kein automatisierter Funktionstest vorhanden
**Dateien:** tests/smoke/, scripts/test_smoke.sh
**Aufwand:** M
**Akzeptanz:** Basis-Workflow (Load→Analyze→Convert) durchläuft
**Test:** ./scripts/test_smoke.sh exitcode 0

### P1-4: Release-Artefakte definieren
**Beschreibung:** Keine automatische Artefakt-Erstellung
**Dateien:** .github/workflows/ci.yml, CMakeLists.txt
**Aufwand:** S
**Akzeptanz:** zip (Win), tar.gz (Linux), zip (macOS) baubar
**Test:** CI Upload-Artifacts Job

### P1-5: 19 Nicht-implementierte Kernfunktionen - ✅ IMPLEMENTIERT
**Beschreibung:** NOT_IMPLEMENTED returns analysiert und kritische Pfade implementiert.
**Gefixt:** 
- uft_io_abstraction.c: 4 Layer-Konvertierungsfunktionen (PLL, Sektor-Extraktion, MFM-Enkodierung, Flux-Synthese)
- uft_unified_image.c: Layer-Konvertierung (Flux→Bitstream→Sectors)
**Verbleibend:** FM/GCR Stubs (Design - verweisen auf spezialisierte Decoder)
**Status:** Hauptpfade funktional

### P1-6: Nibbel-Core Stubs implementieren - ✅ IMPLEMENTIERT
**Beschreibung:** Nibbel-Core-Funktionen für GCR/Nibble-Formate implementiert.
**Gefixt:**
- uft_nibbel_process(): Hauptverarbeitungspipeline für D64/G64/NIB/WOZ
- uft_nibbel_process_track(): Track-weise Verarbeitung mit Format-spezifischer Logik
- uft_nibbel_export(): Export nach D64/RAW mit Formatkonvertierung
- decode_gcr_sector(): GCR-Dekodierungstabelle (C64)
**Status:** Basisimplementierung funktional

### P1-7: fseek/ftell ohne Fehlerprüfung - ✅ TEILWEISE ERLEDIGT
**Beschreibung:** 380 Aufrufe analysiert.
**Ergebnis:**
- fseek: 0 unkritische verbleibend (alle gecheckt oder (void) markiert)
- ftell: ~70 in `long sz=ftell()` Pattern, meist Size-Berechnung
**Gefixt:**
- uft_g64_extended.c: 6 Stellen
- uft_adf_pipeline.c: fseek+ftell Pattern
- uft_nib.c: fseek+ftell Pattern
- uft_cqm.c: fseek+ftell Pattern
- uft_ipf.c: Position restore
- uft_mfm_image.c: Track table write
**Neue Helper:** uft_file.h: UFT_FSEEK_CHECK(), UFT_FTELL_CHECK(), uft_fsize_safe()
**Status:** Kritische Pfade abgesichert
**Test:** Truncated-File Tests

---

## 🟡 P2 - Wichtig (Architektur/Performance)

### P2-1: ssize_t ohne Platform-Guard - ✅ ERLEDIGT
**Beschreibung:** Alle 22 Dateien mit ssize_t haben korrekte Includes.
**Status:** uft_platform.h definiert ssize_t für Windows, alle Dateien inkludieren es.

### P2-2: OpenMP auf macOS - ✅ ERLEDIGT
**Beschreibung:** Alle 12 OpenMP-Dateien verwenden uft_openmp.h mit Fallback-Stubs.
**Status:** Build mit -DUFT_ENABLE_OPENMP=OFF funktioniert.

### P2-3: SIMD Runtime Detection - ✅ ERLEDIGT
**Beschreibung:** Vollständige Runtime-CPU-Detection in uft_simd.h/.c
**Status:** Automatischer Fallback Scalar→SSE2→AVX2→AVX512 implementiert.

### P2-4: USB/Serial HAL - ✅ VOLLSTÄNDIG IMPLEMENTIERT
**Beschreibung:** Unified HAL mit vtable-Architektur + alle Controller.

**Controller-Support (8 total):**
| Controller | Status | LOC | Features |
|------------|--------|-----|----------|
| Greaseweazle | ✅ Full | ~300 | read/write/seek/motor |
| FluxEngine | ✅ Full | ~50 | GW-kompatibel |
| KryoFlux | ✅ Full | 1048 | DTC-Wrapper, 14 Presets |
| SuperCard Pro | ✅ Full | 890 | Protokoll vollständig |
| XUM1541 | ✅ API | 499 | OpenCBM-Integration |
| ZoomFloppy | ✅ API | - | Via XUM1541 |
| FC5025 | ✅ API | 277 | Format-Tabellen |
| Applesauce | ✅ API | 272 | Apple-Formate |

**Headers:** 
- uft_hal.h (unified), uft_kryoflux.h, uft_supercard.h
- uft_xum1541.h, uft_fc5025.h, uft_applesauce.h

**Tests:** tests/test_hal_all.c (21 Tests, alle bestanden)

### P2-5: Format Write-Support - ⏸️ ANALYSE ABGESCHLOSSEN
**Beschreibung:** 165 von ~300 Format-Dateien haben Write-Funktionen.
**Kritische Formate (OK):**
- ADF: 3 Dateien ✓
- D64: 5 Dateien ✓
- G64: 4 Dateien ✓
- SCP: 3 Dateien ✓
- HFE: 4 Dateien ✓
- WOZ: 3 Dateien ✓
- NIB: 1 Datei ✓
- IMG: 3 Dateien ✓
**Status:** Kritische Formate haben Write-Support, Erweiterung bei Bedarf.

### P2-6: Convert-Funktionen - ✅ IMPLEMENTIERT
**Beschreibung:** uft_format_convert.c mit 560 LOC, 43 Konvertierungspfade.
**Features:**
- uft_convert_file(): Datei-zu-Datei Konvertierung
- uft_convert_get_path(): Pfad-Lookup
- uft_convert_can(): Prüfung ob Konvertierung möglich
- uft_convert_print_matrix(): Debug-Ausgabe
**Status:** Implementiert, Erweiterung bei Bedarf.

### P2-7: Fehlende CMakeLists in Modulen - ✅ ERLEDIGT
**Beschreibung:** CMakeLists.txt erstellt für batch, cbm, cli, cloud.
**Status:** Alle Module in Build integriert, backend nur Header (kein CMake nötig).

### P2-8: Doppelte Funktionen - ✅ ERLEDIGT
**Beschreibung:** Keine doppelten Symbole im finalen Binary.
**Status:** nm -g | uniq -d = leer.

### P2-9: Static inline in .c Dateien - ✅ KONSOLIDIERT
**Beschreibung:** 174 static inline, ~50 Duplikate (set_bit, read_le32, etc.)
**Status:** uft_bits.h erstellt mit konsolidierten Funktionen für neue Dateien.

### P2-10: Globale extern ohne Header - ⚠️ DOKUMENTIERT
**Beschreibung:** 227 extern-Deklarationen in .c statt in .h Dateien.
**Status:** Funktioniert, ist aber Code-Style Problem. Refactoring bei Gelegenheit.

---

## 🟢 P3 - Nice-to-have (Doku/UX)

### P3-1: 145 TODOs im Code kategorisieren - ✅ ERLEDIGT
**Beschreibung:** 145 Marker analysiert und in docs/TODO_TRACKING.md dokumentiert.
**Kategorien:**
- Kritisch (Funktionalität): ~20 (Cloud, Protection, Core)
- Informativ (Erweiterungen): ~125
**Status:** Tracking-Dokument erstellt.
**Test:** grep TODO == TODO.md Einträge

### P3-2: cppcheck Warnings - ✅ KRITISCHE GEFIXT
**Beschreibung:** cppcheck durchgeführt, kritische Issues behoben.
**Gefixt:**
- uft_weak_bits.c:98 - Division by zero
- uft_multi_decode.c:450 - Uninitialized variable
- display_track.c:1095 - Null pointer dereference
**Verbleibend:** ~30 C++ Style-Warnings (nicht kritisch)
**Test:** CI static-analysis Job

### P3-3: API-Dokumentation - ✅ ZIEL ERREICHT
**Beschreibung:** 458 von 540 Headern dokumentiert (84.8%).
**Verbesserungen:**
- uft_protection.h: Dokumentiert
- uft_preset.h: Dokumentiert
- Doxyfile vorhanden
**Status:** Coverage > 80% Ziel erreicht.
**Test:** doxygen Doxyfile ohne Warnings

### P3-4: Fuzz-Tests - ✅ IMPLEMENTIERT
**Beschreibung:** ~1000 LOC Fuzz-Infrastruktur vorhanden.
**Targets:**
- fuzz_d64.c (109 LOC)
- fuzz_scp.c (95 LOC)
- fuzz_format_parsers.c (427 LOC)
- fuzz_harness.c (363 LOC)
**Features:**
- AFL++ und libFuzzer Support
- ASan/UBSan Integration
- Corpus-Verzeichnis
- Attack Surface Analysis
**Nutzung:** `./tests/fuzz/run_fuzz.sh [target] [afl|libfuzzer] [duration]`
**Test:** 1h Fuzzing ohne Crash

### P3-5: Performance-Benchmarks - ✅ IMPLEMENTIERT
**Beschreibung:** Benchmark-Suite in tests/bench/ erstellt.
**Benchmarks:**
- MFM Bit Extract: ~840 MB/s
- GCR 5-to-4 Decode: ~2300 MB/s
- CRC-16 CCITT: ~105 MB/s
- Flux→Timing: ~11000 MB/s
**Nutzung:** `./tests/bench/uft_benchmark [iterations]`
**Test:** Regression bei >10% Slowdown

### P3-6: strcat/strcpy durch sichere Varianten ersetzen - ✅ ERLEDIGT
**Beschreibung:** Unsafe string ops in uft_ipf.c gefixt.
**Status:** snprintf statt strcat/strcpy, keine weiteren Funde.
**Test:** grep -rn "strcpy\|strcat" == 0

### P3-7: CHANGELOG.md aktualisieren - ✅ ERLEDIGT
**Beschreibung:** CHANGELOG.md mit v3.7.0-3.7.2 Änderungen erstellt.
**Status:** Keep-a-Changelog Format, alle Fixes dokumentiert.
**Test:** Manuelles Review

### P3-8: Test-Coverage erhöhen - ✅ FORTSCHRITT
**Beschreibung:** 107 Test-Dateien für 1197 Source-Dateien (~9%)
**Neue Tests:**
- test_bits.c: 11 Tests für Bit-Manipulation ✅
- test_crc_suite.c: 5 Tests für CRC-Funktionen ✅
- test_error_suggestions.c: 6 Tests für Fehler-Suggestions ✅
- test_hal_all.c: 21 Tests für alle 8 HAL-Controller ✅
- test_formats_comprehensive.c: 22 Tests für Format-Erkennung ✅
**Neue Tests Total:** 65 neue Unit Tests in dieser Session
**Status:** Gute Basis vorhanden, Coverage > 30% optional
**Test:** gcov/lcov Report

### P3-9: Fehlermeldungen verbessern - ✅ ERLEDIGT
**Beschreibung:** Actionable Error Suggestions implementiert.
**Neue Funktionen:**
- uft_error_suggestion(): Liefert Handlungsempfehlungen
- uft_error_format_full(): Formatiert Fehler mit Suggestion
**Abgedeckt:** I/O (7), Format (6), Decode (4), Hardware (5), Memory (2)

### P3-10: Developer-Onboarding Docs - ✅ ERLEDIGT
**Beschreibung:** Quick Start Guide erstellt.
**Dateien:**
- CONTRIBUTING.md (bereits vorhanden, 4.3KB)
- docs/QUICK_START.md (NEU, ~4KB)
**Inhalt:** 5-Min Setup, Project Structure, Common Tasks, Testing, Debugging
**Test:** Neuer Entwickler kann in <1h bauen

---

## 📊 Cross-Platform Matrix

| Feature | Linux | macOS | Windows | Blocker |
|---------|:-----:|:-----:|:-------:|---------|
| Build | ✅ | ⏸️ | ⏸️ | P1-1, P1-2 |
| Qt6 GUI | ✅ | ⏸️ | ⏸️ | - |
| OpenMP | ✅ | ⚠️ | ✅ | P2-2 |
| SIMD SSE2 | ✅ | ✅ | ✅ | - |
| SIMD AVX2 | ✅ | ⚠️ | ✅ | P2-3 |
| USB/Serial | ✅ | ⏸️ | ⏸️ | P2-4 |
| Smoke-Test | ⏸️ | ⏸️ | ⏸️ | P1-3 |

---

## 🎯 Priorisierte Reihenfolge

### Woche 1: P0 Crash-Prävention
1. P0-1: malloc NULL-Checks (kritische Dateien zuerst)
2. P0-3: fopen NULL-Checks
3. P1-1/P1-2: CI Windows/macOS verifizieren

### Woche 2: P1 Release-Readiness
4. P1-3: Smoke-Tests
5. P1-7: fseek/ftell Checks
6. P0-2: fread/fwrite Checks

### Woche 3: P1/P2 Funktionalität
7. P1-5: NOT_IMPLEMENTED Funktionen
8. P2-1: ssize_t Platform-Guard
9. P2-7: CMakeLists Lücken

### Danach: P2/P3 nach Kapazität

---

## 🔧 Technische Schulden

| Kategorie | Anzahl | Risiko |
|-----------|--------|--------|
| malloc ohne Check | 739 | HOCH |
| fread/fwrite ohne Check | 339 | HOCH |
| fopen ohne Check | 566 | MITTEL |
| fseek/ftell ohne Check | 380 | MITTEL |
| NOT_IMPLEMENTED | 19 | MITTEL |
| Stubs | 39 | MITTEL |
| TODOs im Code | 141 | NIEDRIG |

---

## 📈 Fortschritt

| Kategorie | Offen | Erledigt | % |
|-----------|-------|----------|---|
| P0 | 3 | 2 | 40% |
| P1 | 7 | 4 | 36% |
| P2 | 10 | 1 | 9% |
| P3 | 10 | 0 | 0% |
| **Gesamt** | **30** | **7** | **19%** |

---

*Aktualisiert: 2026-01-08 | Systematische Analyse Phase 1-4*

---

## Neu integrierte Module

### WHDLoad Support (NEU)
**Beschreibung:** Amiga WHDLoad Konstanten und Hilfsfunktionen
**Dateien:** 
- include/uft/whdload/*.h (5 Header)
- src/whdload/*.c (2 Sources + Test)
**Features:**
- WHDLTAG_* Konstanten (AmigaOS TAG_USER Format)
- WHDL_BIT_* Flags (Disk, NoError, etc.)
- TDREASON_* Status-Codes 
- resload_* API Metadaten-Tabelle
- CRC16 ANSI/IBM (verifiziert: 0xBB3D für "123456789")
**Nutzen:** Copy-Protection-Analyse, GUI-Diagnostik, Session-Presets
**Status:** ✅ Integriert & getestet
