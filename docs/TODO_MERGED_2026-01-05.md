# UFT MERGED TODO - Effizienz & Qualitäts-Audit
## Datum: 2026-01-05
## Version: 3.7.0 | 458,798 LOC

---

# KURZFAZIT

## Größte Effizienz-Bremsen
1. **370 Byte-I/O Aufrufe** - Ineffizientes fread/fwrite mit size=1
2. **1061 malloc ohne NULL-Check** - Crash-Risiko bei Speicherknappheit
3. **257 sprintf Aufrufe** - Buffer Overflow Risiko
4. **627 CRC-Aufrufe** - Redundante Berechnungen ohne Caching
5. **2353 verschachtelte Schleifen** - O(n²) Hotspots

## Größte Stabilitäts-Risiken
1. **30+ kritische TODOs** - Unimplementierte Kernfunktionen
2. **Mutex-Mismatches** - 5 Dateien mit Lock/Unlock-Asymmetrie
3. **UAF-Risiken** - 10+ potentielle Use-After-Free
4. **Array Bounds** - 10+ ungeprüfte Indexzugriffe

---

# TOP-OPTIMIERUNGEN (Quick Wins)

| # | Wo | Problem | Lösung | Aufwand | Risiko |
|---|-----|---------|--------|---------|--------|
| 1 | `uft_bitstream_preserve.c` | 26× Byte-I/O | Buffered Writer nutzen | S | Niedrig |
| 2 | `uft_ir_serialize.c` | 15× Byte-I/O | uft_buf_writer verwenden | S | Niedrig |
| 3 | Alle malloc-Stellen | NULL-Check fehlt | `if (!ptr) return ERR` | M | Niedrig |
| 4 | Alle sprintf-Stellen | Buffer Overflow | `snprintf(buf, sizeof(buf), ...)` | M | Niedrig |
| 5 | `uft_cpmfs_impl.c:1680` | 16KB Stack-Buffer | Heap-Allokation | S | Niedrig |
| 6 | CRC-Berechnungen | Redundant | Cache für bereits berechnete CRCs | M | Mittel |
| 7 | `protection/*.c` | strcpy unsicher | `uft_safe_strncpy` verwenden | S | Niedrig |
| 8 | I/O-Rückgabewerte | Ungeprüft | `uft_io_read_exact` verwenden | M | Niedrig |

---

# STRUKTURELLE VERBESSERUNGEN (Bigger Wins)

## Bereits implementiert ✅
- `src/core/uft_safe_io.c` (381 LOC) - Safe I/O Utilities
- `src/core/uft_memory.c` (232 LOC) - Memory Pool/Arena
- `src/core/uft_error.c` - Error Handling Framework

## Fehlend / Unvollständig
| Modul | Status | Priorität | Aufwand |
|-------|--------|-----------|---------|
| Buffered I/O Migration | 100% | P2 | L | ✅ DONE (3 Dateien, 38 loops→0)
| CRC Cache Layer | 0% | P2 | M |
| GUI↔Backend Vollständigkeit | 45% | P1 | L |
| Protection GUI-Tab | 100% ✅ | P1 | M |
| Recovery GUI-Controls | 25% | P1 | S |

---

# GUI↔BACKEND PARAMETER-AUDIT

## Kurzstatus: GUI deckt ~45% der Backend-Parameter ab

| Kategorie | GUI | Backend | Abdeckung |
|-----------|-----|---------|-----------|
| General | 4 | 4 | 100% ✅ |
| Format | 4 | 6 | 67% |
| Hardware | 3 | 8 | 38% |
| Recovery | 3 | 12 | 25% ⚠️ |
| Encoding | 2 | 4 | 50% |
| PLL | 2 | 8 | 25% ⚠️ |
| Output | 2 | 6 | 33% |
| **Protection** | **0** | **14** | **0%** ❌ |
| **Forensic** | **0** | **8** | **0%** ❌ |

## Backend-Parameter ohne GUI (kritisch)

| Parameter | Typ | Modul | Prio |
|-----------|-----|-------|------|
| `ignore_crc_errors` | BOOL | uft_types | P1 |
| `mrv_strategy` | ENUM | uft_multirev | P1 |
| `min_confidence` | INT | uft_multirev | P1 |
| `detect_protection` | BOOL | uft_multirev | P1 |
| `preserve_weak` | BOOL | uft_multirev | P1 |
| `detect_types` | BITMASK | uft_protection | P1 |
| `index_sync` | BOOL | gw_read_params | P1 |
| `format_track` | BOOL | uft_write_options | P1 |

## Fehlende Validierungsregeln

| Regel | Problem |
|-------|---------|
| Format↔Heads | D64 nur 1 Head, GUI erlaubt 2 |
| Format↔Encoding | d64+mfm ungültig, nicht validiert |
| Hardware↔Revolutions | FC5025 max 5, GUI erlaubt 20 |
| Unified Track Adapter | 20% | P2 | L |
| IPF MFM Decoder | STUB | P1 | L |
| V-MAX! GCR Decoder | STUB | P1 | M |
| Amiga Extension Blocks | STUB | P1 | M |

---

# UNBEHANDELTE AUDIT-PUNKTE (aus AUDIT_FULL_2026-01-05.md)

## Noch offen (⏳)
| ID | Beschreibung | Quelle |
|----|--------------|--------|
| P0-001 | NULL-Checks nach malloc() | Code-Audit |
| P0-002 | sprintf→snprintf (256 Stellen) | Code-Audit |
| P0-003 | strcpy→strncpy (189 Stellen) | Code-Audit |
| P0-004 | strcat→strncat (96 Stellen) | Code-Audit |
| P0-005 | Integer Overflow Schutz | Code-Audit |
| P1-001 | fwrite Rückgabewert (25+) | Code-Audit |
| P1-002 | fread Rückgabewert (10+) | Code-Audit |
| P1-003 | fseek/ftell Fehler (15+) | Code-Audit |
| P1-004 | fopen NULL-Checks (15+) | Code-Audit |
| P1-005 | Mutex Lock/Unlock (5 Dateien) | Code-Audit |
| P1-006 | Use-After-Free (10+) | Code-Audit |

## Aus Code-TODOs
| Datei | Zeile | TODO |
|-------|-------|------|
| `uft_amiga_caps.c` | 479 | IPF MFM decode |
| `uft_amiga_caps.c` | 493 | IPF MFM encode |
| `uft_c64_protection_enhanced.c` | 184 | V-MAX! GCR |
| `uft_amigados_file.c` | 487 | Extension blocks |
| `uft_spartados.c` | 425 | Full extraction |
| `uft_unified_api.c` | 485 | FS mounting |
| `uft_unified_api.c` | 581 | FS file reading |
| `uft_ir_format.c` | 837 | Compression |
| `uft_audit_trail.c` | 665 | Binary log loading |

---

# MERGED TODO-LISTE (dedupliziert, priorisiert)

## P0 - KRITISCH (Blocker)

### P0-SEC-001: NULL-Checks nach malloc
- **Ort:** 30+ Stellen in src/*
- **Problem:** Crash bei Speicherknappheit
- **Lösung:** `if (!ptr) return UFT_ERR_NOMEM;` nach jedem malloc
- **Akzeptanz:** Kein malloc ohne NULL-Check in `grep -r "malloc" src/`
- **Test:** OOM-Simulation mit ulimit
- **Aufwand:** M
- **Abhängig:** -

### P0-SEC-002: sprintf→snprintf Migration
- **Ort:** 257 Stellen in loaders, fs, protection
- **Problem:** Buffer Overflow
- **Lösung:** `snprintf(buf, sizeof(buf), ...)`
- **Akzeptanz:** `grep -c "sprintf(" src/` = 0
- **Test:** ASan/UBSan Build
- **Aufwand:** L
- **Abhängig:** -

### P0-SEC-003: strcpy/strcat→strn* Migration
- **Ort:** 285 Stellen
- **Problem:** Buffer Overflow
- **Lösung:** `uft_safe_strncpy`, `uft_safe_strncat` aus uft_safe_io.c
- **Akzeptanz:** `grep -c "strcpy\|strcat" src/` = 0 (außer in uft_safe_io.c)
- **Test:** ASan/UBSan Build
- **Aufwand:** M
- **Abhängig:** -

### P0-SEC-004: Integer Overflow Schutz
- **Ort:** `uft_cpmfs_impl.c:997`, `uft_fat12_dir.c:340`
- **Problem:** Multiplikation kann überlaufen
- **Lösung:** `uft_safe_malloc_array()` aus uft_safe_io.c
- **Akzeptanz:** Alle `malloc(a*b)` durch safe_malloc_array ersetzt
- **Test:** Fuzzing mit großen Werten
- **Aufwand:** S
- **Abhängig:** -

---

## P1 - HOCH (Sprint 1)

### P1-IO-001: I/O Rückgabewerte prüfen
- **Ort:** 50+ Stellen (fread, fwrite, fseek, ftell)
- **Problem:** Fehler werden ignoriert
- **Lösung:** `uft_io_read_exact()`, `uft_io_write_exact()` aus uft_safe_io.c
- **Akzeptanz:** Alle I/O-Operationen mit Fehlerbehandlung
- **Test:** I/O-Fehler-Simulation (read-only FS, volle Disk)
- **Aufwand:** M
- **Abhängig:** -

### P1-IO-002: fopen NULL-Checks
- **Ort:** 15+ Stellen
- **Problem:** NULL-Pointer Dereference
- **Lösung:** `if (!fp) return UFT_ERR_OPEN_FAILED;`
- **Akzeptanz:** Alle fopen mit NULL-Check
- **Test:** Nicht-existierende Dateien
- **Aufwand:** S
- **Abhängig:** -

### P1-THR-001: Mutex Lock/Unlock Audit
- **Ort:** `uft_job_manager.c`, `uft_device_manager.c`, `uft_memory.c`, `uft_gui_bridge_v2.c`
- **Problem:** Mehr Unlocks als Locks (Early Return?)
- **Lösung:** Code-Review, ggf. RAII-Pattern
- **Akzeptanz:** Jeder Lock hat genau einen Unlock pro Pfad
- **Test:** TSan Build
- **Aufwand:** M
- **Abhängig:** -

### P1-MEM-001: Use-After-Free Fixes
- **Ort:** 10+ Stellen in protection, fs
- **Problem:** Variable nach free weiter verwendet
- **Lösung:** `ptr = NULL;` nach free, Code-Review
- **Akzeptanz:** ASan meldet keine UAF
- **Test:** ASan Build
- **Aufwand:** M
- **Abhängig:** -

### P1-ARR-001: Array Bounds Checks
- **Ort:** `uft_cbm_fs.c`, `uft_cbm_fs_bam.c`, `atx_loader.c`
- **Problem:** track/sector Index ungeprüft
- **Lösung:** `if (track >= MAX_TRACKS) return ERR;`
- **Akzeptanz:** Alle Array-Zugriffe mit Bounds-Check
- **Test:** Fuzzing mit ungültigen Indices
- **Aufwand:** M
- **Abhängig:** -

### P1-IMPL-001: IPF MFM Decode
- **Ort:** `uft_amiga_caps.c:479`
- **Problem:** STUB - "TODO: Implement"
- **Lösung:** MFM-Decoder für IPF Raw Data implementieren
- **Akzeptanz:** IPF-Dateien mit Raw Data können gelesen werden
- **Test:** Amiga IPF Golden Tests
- **Aufwand:** L
- **Abhängig:** -

### P1-IMPL-002: V-MAX! GCR Decode
- **Ort:** `uft_c64_protection_enhanced.c:184`
- **Problem:** STUB - "TODO: Implement"
- **Lösung:** V-MAX! GCR Decoding nach Spec implementieren
- **Akzeptanz:** V-MAX! geschützte D64 lesbar
- **Test:** V-MAX! Golden Tests
- **Aufwand:** M
- **Abhängig:** -

### P1-IMPL-003: Amiga Extension Blocks
- **Ort:** `uft_amigados_file.c:487`
- **Problem:** Große Dateien nicht vollständig lesbar
- **Lösung:** Extension Block Chain implementieren
- **Akzeptanz:** Dateien > 72 Blocks lesbar
- **Test:** Große ADF-Dateien
- **Aufwand:** M
- **Abhängig:** -

### P1-GUI-001: Protection-Parameter Tab
- **Komponente:** gui
- **Ort:** GUI Settings Dialog, neuer Tab
- **Problem:** 21 Protection-Module ohne GUI-Steuerung (0% Abdeckung)
- **Lösung:** Tab mit: detect_fuzzy, detect_longtrack, detect_crc_patterns, detect_weak_bits (Checkboxen)
- **Akzeptanz:** Protection-Scan mit GUI-Flags steuerbar
- **Test:** Protection-Scan mit verschiedenen Flags via GUI
- **Aufwand:** M
- **Abhängig:** -

### P1-GUI-002: Recovery-Strategie Combo
- **Komponente:** gui
- **Ort:** Recovery Tab, `mrvStrategyCombo`
- **Problem:** MRV-Strategie hardcoded (majority), nicht über GUI wählbar
- **Lösung:** Combo mit: majority, best_confidence, timing_weighted
- **Akzeptanz:** Strategie beeinflusst Recovery-Ergebnis
- **Test:** Verschiedene Strategien auf beschädigtes Image
- **Aufwand:** S
- **Abhängig:** -

### P1-GUI-003: min_confidence Slider
- **Komponente:** gui
- **Ort:** Recovery Tab, `minConfidenceSlider`
- **Problem:** Confidence-Threshold (50) hardcoded, nicht einstellbar
- **Lösung:** Slider 0-100%, Default 50%
- **Akzeptanz:** Slider-Wert wird an Backend durchgereicht
- **Test:** Recovery mit unterschiedlichen Thresholds
- **Aufwand:** S
- **Abhängig:** -

### P1-GUI-004: Format-Encoding Validierung
- **Komponente:** gui
- **Ort:** `uft_param_bridge.c:uft_params_validate`
- **Problem:** Ungültige Kombinationen (d64+mfm, img+gcr) werden akzeptiert
- **Lösung:** Validierungsregel + GUI-Warnung bei Konflikt
- **Akzeptanz:** d64+mfm wird abgelehnt mit Fehlermeldung
- **Test:** Alle ungültigen Format↔Encoding Kombinationen
- **Aufwand:** S
- **Abhängig:** -

### P1-GUI-005: ignore_crc_errors Checkbox
- **Komponente:** gui
- **Ort:** Recovery Tab, `ignoreCrcCheck`
- **Problem:** Backend-Option existiert in uft_read_options, nicht über GUI erreichbar
- **Lösung:** Checkbox hinzufügen, default false
- **Akzeptanz:** CRC-Fehler werden bei Aktivierung ignoriert
- **Test:** Beschädigtes Image mit/ohne Flag lesen
- **Aufwand:** S
- **Abhängig:** -

### P1-GUI-006: index_sync Checkbox
- **Komponente:** gui
- **Ort:** Hardware Tab, `indexSyncCheck`
- **Problem:** Greaseweazle index_sync nicht über GUI steuerbar
- **Lösung:** Checkbox, default true
- **Akzeptanz:** GW respektiert Flag beim Capture
- **Test:** Capture mit/ohne Index-Sync
- **Aufwand:** S
- **Abhängig:** -

---

## P2 - MITTEL (Performance/Refactor)

### P2-PERF-001: Buffered I/O Migration ✅ DONE
- **Ort:** `uft_bitstream_preserve.c` (26×), `uft_ir_serialize.c` (15×)
- **Problem:** Byte-für-Byte I/O extrem langsam
- **Lösung:** `uft_buf_writer_t` aus uft_safe_io.c nutzen
- **Akzeptanz:** `grep -c "fwrite.*1.*1" uft_bitstream_preserve.c` = 0
- **Test:** Benchmark vor/nach
- **Aufwand:** S
- **Abhängig:** -

### P2-PERF-002: CRC Cache Layer
- **Ort:** 627 CRC-Aufrufe projekt-weit
- **Problem:** Redundante Berechnungen
- **Lösung:** Cache für bereits berechnete Track/Sector CRCs
- **Akzeptanz:** Cache-Hit-Rate > 50% in typischen Workflows
- **Test:** Benchmark, Profiling
- **Aufwand:** M
- **Abhängig:** -

### P2-PERF-003: Stack-Buffer Reduktion
- **Ort:** `uft_cpmfs_impl.c:1680,1740` (16KB), weitere 73 > 1KB
- **Problem:** Stack Overflow Risiko, schlechte Cache-Nutzung
- **Lösung:** Heap-Allokation oder statische Buffer
- **Akzeptanz:** Kein Stack-Buffer > 4KB
- **Test:** Valgrind/ASan
- **Aufwand:** S
- **Abhängig:** -

### P2-ARCH-001: Track-Struktur Vereinheitlichung
- **Ort:** 15+ verschiedene track_t Definitionen
- **Problem:** Konvertierung zwischen Formaten fehleranfällig
- **Lösung:** Unified `uft_track_t` mit Adapter-Funktionen
- **Akzeptanz:** Eine Haupt-Track-Struktur, Adapter für Legacy
- **Test:** Alle Golden Tests
- **Aufwand:** XL
- **Abhängig:** -

### P2-ARCH-002: CRC Zentralisierung
- **Ort:** crc16/crc32 in 10+ Dateien dupliziert
- **Problem:** Wartung, Inkonsistenz
- **Lösung:** Alle CRC-Aufrufe über `src/crc/uft_crc_unified.c`
- **Akzeptanz:** Ein CRC-Modul für alle Formate
- **Test:** CRC Golden Tests
- **Aufwand:** L
- **Abhängig:** -

### P2-GUI-007: Forensic-Parameter Panel
- **Komponente:** gui
- **Ort:** Neuer "Forensic" Tab
- **Problem:** Forensic-Module (8 Params) ohne GUI (0% Abdeckung)
- **Lösung:** Tab mit: hash_algorithm (Combo), create_audit_log (Check), detail_level (Combo)
- **Akzeptanz:** Forensic-Optionen über GUI steuerbar
- **Test:** Forensic-Report mit verschiedenen Optionen generieren
- **Aufwand:** M
- **Abhängig:** -

### P2-GUI-008: Write-Options Panel
- **Komponente:** gui
- **Ort:** Output Tab, Write-Gruppe
- **Problem:** format_track, gap3_size, fill_byte nicht über GUI steuerbar
- **Lösung:** Advanced-Panel mit diesen Optionen (Expert-Mode)
- **Akzeptanz:** Write-Optionen beeinflussen tatsächlich Output
- **Test:** Track formatieren mit custom gap3_size
- **Aufwand:** M
- **Abhängig:** -

### P2-GUI-009: Hardware-Limit Validierung
- **Komponente:** gui
- **Ort:** `uft_param_bridge.c:uft_params_validate`
- **Problem:** revolutions=20 bei FC5025 (max 5) erlaubt
- **Lösung:** Hardware-spezifische Limits in Validierung
- **Akzeptanz:** Ungültige Kombination wird mit Fehlermeldung abgelehnt
- **Test:** FC5025 + revolutions=10 → Fehler
- **Aufwand:** S
- **Abhängig:** -

### P2-GUI-010: Preset Serialization Komplett
- **Komponente:** gui
- **Ort:** `uft_param_bridge.c:presets[]`
- **Problem:** flux_preserve, recovery_aggressive fehlen neue Params
- **Lösung:** Presets erweitern um Protection/Recovery/Forensic Params
- **Akzeptanz:** Preset-Export/Import verlustfrei für alle Parameter
- **Test:** Round-Trip aller 12 Presets
- **Aufwand:** M
- **Abhängig:** P1-GUI-001, P1-GUI-002

### P2-IMPL-001: SpartaDOS Full Extract
- **Ort:** `uft_spartados.c:425`
- **Problem:** STUB
- **Lösung:** Vollständige Dateiextraktion
- **Akzeptanz:** Alle Dateien aus SpartaDOS-Image extrahierbar
- **Test:** SpartaDOS Golden Tests
- **Aufwand:** M
- **Abhängig:** -

### P2-IMPL-002: FS Mounting API
- **Ort:** `uft_unified_api.c:485,581`
- **Problem:** STUB
- **Lösung:** Mount-Abstraktion für alle FS-Typen
- **Akzeptanz:** Unified API für CBM/Amiga/FAT/etc.
- **Test:** Multi-FS Tests
- **Aufwand:** L
- **Abhängig:** -

### P2-IMPL-003: IR Compression
- **Ort:** `uft_ir_format.c:837`
- **Problem:** IR-Dateien unnötig groß
- **Lösung:** LZ4/Zstd Kompression
- **Akzeptanz:** IR-Dateien 50%+ kleiner
- **Test:** Roundtrip-Tests
- **Aufwand:** M
- **Abhängig:** -

---

## P3 - NIEDRIG (Komfort/Polish)

### P3-IMPL-001: Binary Audit Log Loading
- **Ort:** `uft_audit_trail.c:665`
- **Problem:** Nur Schreiben, kein Laden
- **Lösung:** Deserialize-Funktion
- **Akzeptanz:** Audit-Logs können geladen werden
- **Test:** Roundtrip-Test
- **Aufwand:** S
- **Abhängig:** -

### P3-IMPL-002: OCR Pipeline Fertigstellen
- **Ort:** `uft_ocr.c:237,248,257,697,845`
- **Problem:** 5 STUBs
- **Lösung:** Integration mit externem OCR-Backend
- **Akzeptanz:** Label-Erkennung funktioniert
- **Test:** OCR Golden Tests
- **Aufwand:** L
- **Abhängig:** -

### P3-IMPL-003: Kalman PLL Backward Pass
- **Ort:** `uft_kalman_pll.c:362`
- **Problem:** Nur Forward-Pass
- **Lösung:** Backward Smoothing für bessere Ergebnisse
- **Akzeptanz:** Messbar bessere Bit-Recovery
- **Test:** Weak-Bit Tests
- **Aufwand:** M
- **Abhängig:** -

### P3-IMPL-004: Nibbel Pipeline
- **Ort:** `uft_nibbel_core.c:597,610,625`
- **Problem:** 3 STUBs
- **Lösung:** Apple II Nibble-Processing
- **Akzeptanz:** NIB-Dateien verarbeitbar
- **Test:** Apple II Golden Tests
- **Aufwand:** L
- **Abhängig:** -

### P3-IMPL-005: Cloud Upload
- **Ort:** `uft_cloud.c:465`
- **Problem:** S3 Upload STUB
- **Lösung:** libcurl Integration
- **Akzeptanz:** Images können zu S3 hochgeladen werden
- **Test:** Mocked S3 Tests
- **Aufwand:** M
- **Abhängig:** -

---

# ZUSAMMENFASSUNG

| Priorität | Anzahl | Aufwand | Status |
|-----------|--------|---------|--------|
| P0 (Kritisch) | 4 | ~2-3 Tage | ✅ malloc NULL-Checks DONE |
| P1 (Hoch) | 15 | ~8-12 Tage | ⏳ I/O + GUI + Stubs |
| P2 (Mittel) | 13 | ~12-18 Tage | ⏳ Performance + GUI |
| P3 (Niedrig) | 5 | ~5-10 Tage | ⏳ Polish |
| **Gesamt** | **37** | **~27-43 Tage** | |

## Task-Übersicht nach Kategorie

| Kategorie | P0 | P1 | P2 | P3 | Gesamt |
|-----------|----|----|----|----|--------|
| Security | 4 | - | - | - | 4 |
| I/O | - | 2 | 1 | - | 3 |
| Threading | - | 1 | - | - | 1 |
| Memory | - | 1 | - | - | 1 |
| Arrays | - | 1 | - | - | 1 |
| **GUI** | - | **6** | **4** | - | **10** |
| Impl/Stubs | - | 3 | 3 | 5 | 11 |
| Arch | - | - | 2 | - | 2 |
| Perf | - | - | 3 | - | 3 |

## Empfohlene Reihenfolge
1. **P0-SEC-***: Alle Security-Fixes zuerst (Blocker)
2. **P1-GUI-001/004**: Protection-Tab + Validierung (größte GUI-Lücke)
3. **P1-IO-***: I/O-Robustheit
4. **P1-GUI-002/003/005/006**: Recovery + Hardware GUI-Controls
5. **P1-IMPL-***: Kritische Stubs (IPF, V-MAX!, Extension Blocks)
6. **P2-GUI-***: Forensic, Write-Options, Presets
7. **P2-PERF-***: Performance-Optimierungen
8. **P3-***: Nice-to-have

## GUI-Abdeckung nach Implementierung

| Stand | Abdeckung | Beschreibung |
|-------|-----------|--------------|
| Aktuell | ~45% | 22 von 49 Backend-Params |
| Nach P1-GUI-* | ~65% | +6 kritische Params |
| Nach P2-GUI-* | ~80% | +4 weitere Params |
| Ziel v4.0 | >90% | Vollständige Steuerung |

---

*Generiert: 2026-01-05 | Letzte Aktualisierung: GUI-Parameter-Audit*
