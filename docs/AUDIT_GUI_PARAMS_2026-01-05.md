# UFT GUI↔Backend Parameter-Audit
## Datum: 2026-01-05
## Version: 3.7.0

---

# 1. KURZSTATUS

## GUI-Abdeckung der Backend-Parameter: ~45%

**Begründung:**
- **GUI definiert:** 22 Parameter (in `uft_param_bridge.c`)
- **Backend definiert:** ~49 Parameter (in diversen Options-Structs)
- **Fehlende GUI-Anbindung:** ~27 Parameter
- **Neue Module ohne GUI:** Protection (21 Files), Recovery (17 Files), Decoder (11 Files)

| Kategorie | GUI | Backend | Abdeckung |
|-----------|-----|---------|-----------|
| General | 4 | 4 | 100% |
| Format | 4 | 6 | 67% |
| Hardware | 3 | 8 | 38% |
| Recovery | 3 | 12 | 25% |
| Encoding | 2 | 4 | 50% |
| PLL | 2 | 8 | 25% |
| Output | 2 | 6 | 33% |
| Protection | 0 | 14 | 0% ⚠️ |
| Forensic | 0 | 8 | 0% ⚠️ |

---

# 2. MAPPING-TABELLE GUI ↔ BACKEND

## 2.1 ✅ Vollständig gemappt (GUI → Backend)

| GUI-Widget | Backend-Param | Typ | Default | Range | Transformation |
|------------|---------------|-----|---------|-------|----------------|
| `inputEdit` | input | PATH | - | - | direkt |
| `outputEdit` | output | PATH | - | - | direkt |
| `verboseCheck` | verbose | BOOL | false | - | direkt |
| `quietCheck` | quiet | BOOL | false | - | direkt |
| `formatCombo` | format | ENUM | auto | 17 Werte | direkt |
| `cylindersSpin` | cylinders | INT | 80 | 1-200 | direkt |
| `headsSpin` | heads | INT | 2 | 1-2 | direkt |
| `sectorsSpin` | sectors | INT | 18 | 1-64 | direkt |
| `hardwareCombo` | hardware | ENUM | auto | 7 Werte | direkt |
| `deviceEdit` | device | STRING | - | - | direkt |
| `driveCombo` | drive | INT | 0 | 0-3 | direkt |
| `retriesSpin` | retries | INT | 3 | 0-100 | direkt |
| `revolutionsSpin` | revolutions | INT | 3 | 1-20 | direkt |
| `weakBitsCheck` | weak_bits | BOOL | true | - | direkt |
| `encodingCombo` | encoding | ENUM | auto | 5 Werte | direkt |
| `dataRateSpin` | data_rate | INT | 250 | 125-1000 | Expert |
| `pllPeriodSpin` | pll_period | INT | 2000 | 500-10000 | Expert |
| `pllAdjustSpin` | pll_adjust | FLOAT | 0.05 | - | Expert |
| `verifyCheck` | verify | BOOL | true | - | direkt |
| `previewCheck` | preview | BOOL | false | - | direkt |
| `debugCheck` | debug | BOOL | false | - | Expert |
| `dumpFluxCheck` | dump_flux | BOOL | false | - | Expert |

## 2.2 ⚠️ Backend ohne GUI (Feature existiert, GUI fehlt)

| Backend-Param | Typ | Default | Modul | Priorität |
|---------------|-----|---------|-------|-----------|
| `ignore_crc_errors` | BOOL | false | uft_types.h | P1 |
| `read_deleted` | BOOL | false | uft_types.h | P2 |
| `raw_mode` | BOOL | false | uft_types.h | P2 |
| `sync_word` | INT | 0 (auto) | uft_types.h | P3 |
| `format_track` | BOOL | false | uft_types.h | P1 |
| `gap3_size` | INT | 0 (default) | uft_types.h | P2 |
| `fill_byte` | INT | 0xE5 | uft_types.h | P2 |
| `precomp_ns` | INT | -1 (auto) | uft_types.h | P2 |
| `index_sync` | BOOL | true | gw_read_params | P1 |
| `erase_empty` | BOOL | false | gw_write_params | P2 |
| `skip_empty` | BOOL | false | uft_hal_read_params | P2 |
| `strategy` (MRV) | ENUM | majority | uft_mrv_params | P1 |
| `min_confidence` | INT | 50 | uft_mrv_params | P1 |
| `weak_threshold` | INT | 30 | uft_mrv_params | P2 |
| `detect_protection` | BOOL | false | uft_mrv_params | P1 |
| `preserve_weak` | BOOL | true | uft_mrv_params | P1 |
| `timing_tolerance_ns` | INT | 100 | uft_mrv_params | P2 |
| `detect_types` | BITMASK | 0xFFFF | uft_protection_params | P1 |
| `fuzzy_timing_min_us` | FLOAT | 4.3 | uft_protection_params | P2 |
| `fuzzy_timing_max_us` | FLOAT | 5.7 | uft_protection_params | P2 |
| `long_track_threshold` | FLOAT | 1.02 | uft_protection_params | P2 |
| `short_track_threshold` | FLOAT | 0.98 | uft_protection_params | P2 |
| `lowpass_radius` | INT | 100 | uft_gui_adaptive_params | P2 |
| `thresh_4us/6us/8us` | FLOAT | 2.0/3.0/4.0 | uft_gui_adaptive_params | P2 |
| `auto_zone` | BOOL | true | uft_gui_gcr_params | P2 |
| `force_zone` | INT | 0 | uft_gui_gcr_params | P3 |
| `pll_bandwidth` | FLOAT | 0.05 | uft_pll_v2.c | P2 |

## 2.3 ❌ GUI ohne Wirkung (GUI setzt, Backend ignoriert)

| GUI-Widget | Problem | Fix |
|------------|---------|-----|
| *Keine gefunden* | - | - |

## 2.4 ⚠️ Doppelte Parameter (zwei GUI-Controls für dasselbe)

| GUI-Widget 1 | GUI-Widget 2 | Backend | Problem |
|--------------|--------------|---------|---------|
| *Keine gefunden* | - | - | - |

## 2.5 ❌ Konflikte (GUI erlaubt, Backend kann nicht)

| GUI-Kombination | Backend-Limit | Fix |
|-----------------|---------------|-----|
| `format=d64` + `heads=2` | D64 ist single-sided | Validierung fehlt |
| `encoding=gcr_c64` + `format=img` | IMG ist nur MFM | Validierung fehlt |
| `revolutions=20` + `hardware=fc5025` | FC5025 max 5 revs | Validierung fehlt |

---

# 3. NEUE MODULE: BENÖTIGTE GUI-ERWEITERUNGEN

## 3.1 Protection-Modul (21 Files, 0% GUI-Abdeckung)

| Parameter | Typ | Default | GUI-Widget | Expert? |
|-----------|-----|---------|------------|---------|
| `detect_fuzzy` | BOOL | true | Checkbox | Nein |
| `detect_longtrack` | BOOL | true | Checkbox | Nein |
| `detect_crc_patterns` | BOOL | true | Checkbox | Nein |
| `detect_weak_bits` | BOOL | true | Checkbox | Nein |
| `fuzzy_min_reads` | INT | 3 | Spinner | Ja |
| `fuzzy_variance` | FLOAT | 0.1 | Slider | Ja |
| `generate_report` | BOOL | true | Checkbox | Nein |
| `report_format` | ENUM | json | Combo | Nein |

## 3.2 Recovery-Modul (17 Files, 25% GUI-Abdeckung)

| Parameter | Typ | Default | GUI-Widget | Expert? |
|-----------|-----|---------|------------|---------|
| `mrv_strategy` | ENUM | majority | Combo | Nein |
| `min_confidence` | INT | 50 | Slider | Nein |
| `preserve_weak` | BOOL | true | Checkbox | Nein |
| `repair_crc` | BOOL | false | Checkbox | Nein |
| `repair_bam` | BOOL | false | Checkbox | Ja |
| `rebuild_dir` | BOOL | false | Checkbox | Ja |
| `timing_tolerance` | INT | 100 | Spinner | Ja |

## 3.3 Forensic-Modul (neue Funktionen)

| Parameter | Typ | Default | GUI-Widget | Expert? |
|-----------|-----|---------|------------|---------|
| `hash_algorithm` | ENUM | sha256 | Combo | Nein |
| `create_audit_log` | BOOL | true | Checkbox | Nein |
| `log_detail_level` | ENUM | standard | Combo | Ja |
| `preserve_timestamps` | BOOL | true | Checkbox | Nein |
| `chain_of_custody` | BOOL | false | Checkbox | Ja |

## 3.4 Hardcoded Defaults → Parameter

| Konstante | Datei | Vorschlag |
|-----------|-------|-----------|
| `DEFAULT_CLOCK_NS = 2000` | uft_integration.c | `pll_clock_ns` |
| `PLL_DEFAULT_FREQ = 0.05` | uft_integration.c | bereits gemappt |
| `PLL_DEFAULT_PHASE = 0.60` | uft_integration.c | `pll_phase_gain` |
| `UFT_PLL_DEFAULT_BW = 0.05` | uft_pll_v2.c | `pll_bandwidth` |
| `DEFAULT_SECTORS = 32` | uft_unified_decoder.c | `max_sectors` |
| `D64_INTERLEAVE_DEFAULT = 10` | uft_cbm_fs.c | `interleave` |
| `FLUX_DEFAULT_SAMPLE_RATE` | uft_flux_buffer.c | `sample_rate` |

---

# 4. KONSISTENZ & SAFETY-CHECKS

## 4.1 Fehlende Validierungsregeln

| Regel | Problem | Fix |
|-------|---------|-----|
| Format↔Heads | D64 nur 1 Head | Validator hinzufügen |
| Format↔Encoding | Encoding muss zu Format passen | Validator hinzufügen |
| Hardware↔Revolutions | Controller-Limits | Validator hinzufügen |
| Cylinders↔Format | Format-spezifische Limits | Validator hinzufügen |

## 4.2 Preset-Serialization

| Preset | Vollständig? | Fehlende Params |
|--------|--------------|-----------------|
| amiga_dd | Ja | - |
| amiga_hd | Ja | - |
| c64_1541 | Ja | - |
| pc_dd | Ja | - |
| pc_hd | Ja | - |
| apple_dos33 | Ja | - |
| atari_sd | Ja | - |
| flux_preserve | Nein | min_confidence, strategy |
| flux_analyze | Nein | protection params |
| recovery_aggressive | Nein | mrv_strategy, preserve_weak |

## 4.3 Reproduzierbarkeit

| Export/Import | Status | Problem |
|---------------|--------|---------|
| JSON→CLI | ✅ | - |
| CLI→JSON | ✅ | - |
| JSON→JSON | ✅ | - |
| GUI→JSON | ⚠️ | Fehlende Protection/Recovery Params |

---

# 5. MERGED TODO-LISTE (GUI-Parameter)

## P0 - KRITISCH (Blocker)

*Keine kritischen GUI-Parameter-Bugs gefunden*

## P1 - HOCH (Sprint 1)

### P1-GUI-001: Protection-Parameter Tab
- **Komponente:** gui
- **Ort:** GUI Settings Dialog
- **Problem:** 21 Protection-Module ohne GUI-Steuerung
- **Lösung:** Neuen "Protection" Tab mit Checkboxen für detect_* Parameter
- **Akzeptanz:** Alle Protection-Flags über GUI steuerbar
- **Test:** Protection-Scan mit verschiedenen Flags via GUI starten
- **Aufwand:** M
- **Abhängig:** -

### P1-GUI-002: Recovery-Strategie Combo
- **Komponente:** gui
- **Ort:** Recovery Tab, `mrvStrategyCombo`
- **Problem:** MRV-Strategie nicht wählbar (hardcoded majority)
- **Lösung:** Combo mit: majority, best_confidence, timing_weighted
- **Akzeptanz:** Strategie beeinflusst Recovery-Ergebnis
- **Test:** Verschiedene Strategien auf beschädigtes Image
- **Aufwand:** S
- **Abhängig:** -

### P1-GUI-003: min_confidence Slider
- **Komponente:** gui
- **Ort:** Recovery Tab, `minConfidenceSlider`
- **Problem:** Confidence-Threshold nicht einstellbar
- **Lösung:** Slider 0-100%, Default 50%
- **Akzeptanz:** Slider-Wert wird an Backend durchgereicht
- **Test:** Recovery mit unterschiedlichen Thresholds
- **Aufwand:** S
- **Abhängig:** -

### P1-GUI-004: Format-Encoding Validierung
- **Komponente:** gui
- **Ort:** `uft_param_bridge.c:validate`
- **Problem:** Ungültige Format↔Encoding Kombinationen möglich
- **Lösung:** Validierungsregel + GUI-Warnung
- **Akzeptanz:** d64+mfm wird abgelehnt mit Fehlermeldung
- **Test:** Alle ungültigen Kombinationen probieren
- **Aufwand:** S
- **Abhängig:** -

### P1-GUI-005: ignore_crc_errors Checkbox
- **Komponente:** gui
- **Ort:** Recovery Tab, `ignoreCrcCheck`
- **Problem:** Backend-Option nicht über GUI erreichbar
- **Lösung:** Checkbox hinzufügen, default false
- **Akzeptanz:** CRC-Fehler werden bei Aktivierung ignoriert
- **Test:** Beschädigtes Image mit/ohne Flag lesen
- **Aufwand:** S
- **Abhängig:** -

### P1-GUI-006: index_sync Checkbox
- **Komponente:** gui
- **Ort:** Hardware Tab, `indexSyncCheck`
- **Problem:** Index-Synchronisation nicht steuerbar
- **Lösung:** Checkbox, default true
- **Akzeptanz:** Greaseweazle respektiert Flag
- **Test:** Capture mit/ohne Index-Sync
- **Aufwand:** S
- **Abhängig:** -

## P2 - MITTEL (Sprint 2)

### P2-GUI-007: Forensic-Parameter Panel
- **Komponente:** gui
- **Ort:** Neuer "Forensic" Tab
- **Problem:** Forensic-Module ohne GUI
- **Lösung:** Tab mit hash_algorithm, audit_log, detail_level
- **Akzeptanz:** Forensic-Optionen über GUI steuerbar
- **Test:** Forensic-Report mit verschiedenen Optionen
- **Aufwand:** M
- **Abhängig:** -

### P2-GUI-008: Write-Options Panel
- **Komponente:** gui
- **Ort:** Output Tab, Write-Gruppe
- **Problem:** format_track, gap3_size, fill_byte nicht steuerbar
- **Lösung:** Advanced-Panel mit diesen Optionen
- **Akzeptanz:** Write-Optionen beeinflussen Output
- **Test:** Track formatieren mit custom gap3
- **Aufwand:** M
- **Abhängig:** -

### P2-GUI-009: GCR-Zone Einstellungen
- **Komponente:** gui
- **Ort:** Encoding Tab, `gcrZoneGroup`
- **Problem:** auto_zone, force_zone nicht steuerbar
- **Lösung:** Radio-Buttons Auto/Manual + Zone-Spinner
- **Akzeptanz:** C64-Disks mit manueller Zone lesbar
- **Test:** C64 Disk mit falscher Auto-Detection
- **Aufwand:** S
- **Abhängig:** -

### P2-GUI-010: PLL-Bandwidth Slider
- **Komponente:** gui
- **Ort:** PLL Tab, `pllBandwidthSlider`
- **Problem:** Hardcoded 0.05 in uft_pll_v2.c
- **Lösung:** Slider 0.01-0.20, default 0.05
- **Akzeptanz:** Bandwidth beeinflusst Dekodierung
- **Test:** Noisy Flux mit verschiedenen Bandwidths
- **Aufwand:** S
- **Abhängig:** -

### P2-GUI-011: Preset Serialization Komplett
- **Komponente:** gui
- **Ort:** `uft_param_bridge.c:presets`
- **Problem:** flux_preserve etc. fehlen Protection/Recovery Params
- **Lösung:** Presets erweitern um alle neuen Params
- **Akzeptanz:** Preset-Export/Import verlustfrei
- **Test:** Round-Trip aller Presets
- **Aufwand:** M
- **Abhängig:** P1-GUI-001, P1-GUI-002

### P2-GUI-012: Hardware-Limit Validierung
- **Komponente:** gui
- **Ort:** `uft_param_bridge.c:validate`
- **Problem:** revolutions=20 bei FC5025 (max 5)
- **Lösung:** Hardware-spezifische Limits prüfen
- **Akzeptanz:** Ungültige Kombination wird abgelehnt
- **Test:** FC5025 + revolutions=10
- **Aufwand:** S
- **Abhängig:** -

## P3 - NIEDRIG (Backlog)

### P3-GUI-013: Interleave Einstellung
- **Komponente:** gui
- **Ort:** Format Tab, `interleaveSpin`
- **Problem:** Hardcoded Interleave-Werte
- **Lösung:** Spinner mit format-spezifischem Default
- **Akzeptanz:** Custom Interleave beim Schreiben
- **Test:** D64 mit Interleave 6 schreiben
- **Aufwand:** S
- **Abhängig:** -

### P3-GUI-014: Sample-Rate Einstellung
- **Komponente:** gui
- **Ort:** Flux Tab, `sampleRateSpin`
- **Problem:** Hardcoded 24 MHz
- **Lösung:** Combo mit: 12/24/48 MHz
- **Akzeptanz:** Sample-Rate wird an Flux-Buffer übergeben
- **Test:** Capture mit 48 MHz
- **Aufwand:** S
- **Abhängig:** -

### P3-GUI-015: Tooltips für Expert-Params
- **Komponente:** gui
- **Ort:** Alle Expert-Widgets
- **Problem:** Expert-Parameter ohne Erklärung
- **Lösung:** Tooltips mit Beschreibung + sicheren Defaults
- **Akzeptanz:** Jedes Expert-Widget hat Tooltip
- **Test:** Manueller Review
- **Aufwand:** S
- **Abhängig:** -

### P3-GUI-016: Parameter-Help Dialog
- **Komponente:** gui
- **Ort:** Neuer Help-Dialog
- **Problem:** Keine zentrale Parameter-Dokumentation
- **Lösung:** Dialog mit allen Params, Beschreibung, Defaults
- **Akzeptanz:** Alle 49 Backend-Params dokumentiert
- **Test:** Manueller Review
- **Aufwand:** M
- **Abhängig:** -

---

# 6. ZUSAMMENFASSUNG

| Prio | Tasks | Aufwand | Fokus |
|------|-------|---------|-------|
| P1 | 6 | ~3-4 Tage | Protection, Recovery, Validierung |
| P2 | 6 | ~4-6 Tage | Forensic, Write, Presets |
| P3 | 4 | ~2-3 Tage | Polish, Doku |
| **Gesamt** | **16** | **~9-13 Tage** | |

## Empfohlene Reihenfolge

1. **P1-GUI-001**: Protection-Tab (größte Lücke)
2. **P1-GUI-002/003**: Recovery-Strategie + Confidence
3. **P1-GUI-004**: Format-Encoding Validierung (Safety)
4. **P1-GUI-005/006**: Wichtige Backend-Flags
5. **P2-GUI-011**: Preset-Komplett (Reproduzierbarkeit)
6. Rest nach Bedarf

---

*Audit durchgeführt: 2026-01-05*
*Nächstes Audit: Nach Implementierung P1 Tasks*
