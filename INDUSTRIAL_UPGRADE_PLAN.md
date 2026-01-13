# UFT Industrial-Grade Upgrade Plan v3.8.0
## Umfassende Analyse & Verbesserungsplan

**Erstellt**: 2026-01-13
**Status**: Phase 1-10 Analyse abgeschlossen

---

# A) WEAKNESS MAP (P0-P3)

## P0 - KRITISCH (Sofort beheben)

| ID | Bereich | Problem | Impact | Fix |
|----|---------|---------|--------|-----|
| W-P0-001 | I/O | 10+ fopen() ohne NULL-Check | Crash bei fehlenden Dateien | Try/catch + Fehlerbehandlung |
| W-P0-002 | Memory | Fehlende free() in Fehlerpfaden | Memory Leaks | RAII-Pattern / Cleanup-Labels |
| W-P0-003 | Writer | "TODO" Kommentare in write_track() | Writer funktioniert nicht | Implementierung vervollständigen |
| W-P0-004 | Tests | Nur 3 Test-Dateien | Keine Regression-Sicherheit | Test-Suite erweitern |

## P1 - HOCH (Diese Woche)

| ID | Bereich | Problem | Impact | Fix |
|----|---------|---------|--------|-----|
| W-P1-001 | Cross-Platform | 56 POSIX-only APIs (fork, popen) | Windows bricht | Plattform-Abstraktionen |
| W-P1-002 | GUI | Parameter nicht bidirektional sync | GUI-Backend Drift | uft_param_bridge vervollständigen |
| W-P1-003 | Scoring | Score nur in flux_stat.c | Inkonsistente Bewertung | Unified Scoring System |
| W-P1-004 | Multi-Read | Revolution-Compare ohne Merge | Kein Best-of | Merge-Pipeline implementieren |
| W-P1-005 | PLL | 18 PLL-Dateien, keine Einheit | Inkonsistentes Verhalten | Unified PLL mit Config |

## P2 - MITTEL (Diese Woche)

| ID | Bereich | Problem | Impact | Fix |
|----|---------|---------|--------|-----|
| W-P2-001 | Parser | Verschiedene Error-Returns | Inkonsistente Diagnostik | Unified Result Object |
| W-P2-002 | Protection | Detection ohne Preserve-Pipeline | Kopierschutz geht verloren | Protection Container |
| W-P2-003 | Writer | Kein Verify nach Write | Stille Fehler | Write-Verify-Pipeline |
| W-P2-004 | GUI | 18 .ui Dateien, unklare Zuordnung | Wartbarkeit | UI Konsolidierung |

## P3 - NIEDRIG (Backlog)

| ID | Bereich | Problem | Impact | Fix |
|----|---------|---------|--------|-----|
| W-P3-001 | Docs | API nicht vollständig dokumentiert | Onboarding langsam | Doxygen vervollständigen |
| W-P3-002 | Logging | Kein strukturiertes Log-Format | Debug schwierig | JSON-Logging |
| W-P3-003 | Config | Einstellungen verstreut | Konfiguration komplex | Zentrale Config |

---

# B) PARSER MATRIX

| Format | Read | Analyze | Repair | Write | Verify | Lücken |
|--------|------|---------|--------|-------|--------|--------|
| **IBM PC** |
| IMG/IMA | ✅ | ✅ | ⚠️ | ✅ | ❌ | Verify fehlt |
| IMD | ✅ | ✅ | ⚠️ | ✅ | ❌ | Verify fehlt |
| DMK | ✅ | ✅ | ✅ | ✅ | ⚠️ | Partial verify |
| TD0 | ✅ | ✅ | ❌ | ❌ | ❌ | Nur Read |
| **Commodore** |
| D64 | ✅ | ✅ | ✅ | ✅ | ✅ | Komplett |
| G64 | ✅ | ✅ | ✅ | ✅ | ⚠️ | Weak bits |
| D71 | ✅ | ✅ | ⚠️ | ✅ | ❌ | Verify fehlt |
| D81 | ✅ | ✅ | ⚠️ | ✅ | ❌ | Verify fehlt |
| **Amiga** |
| ADF | ✅ | ✅ | ✅ | ✅ | ✅ | Komplett |
| ADZ | ✅ | ✅ | ⚠️ | ✅ | ❌ | Kompression-Verify |
| **Atari** |
| ST | ✅ | ✅ | ⚠️ | ✅ | ❌ | Verify fehlt |
| STX | ✅ | ✅ | ⚠️ | ✅ | ⚠️ | Protection preserve |
| MSA | ✅ | ⚠️ | ❌ | ✅ | ❌ | Nur basic |
| **Apple** |
| NIB | ✅ | ✅ | ⚠️ | ⚠️ | ❌ | Write incomplete |
| WOZ | ✅ | ✅ | ⚠️ | ❌ | ❌ | Nur Read |
| A2R | ✅ | ⚠️ | ❌ | ❌ | ❌ | Nur basic Read |
| **Flux** |
| SCP | ✅ | ✅ | ⚠️ | ✅ | ⚠️ | Timing verify |
| HFE | ✅ | ✅ | ⚠️ | ✅ | ❌ | Verify fehlt |
| IPF | ✅ | ✅ | ❌ | ❌ | ❌ | Read-only (DRM) |
| KF Stream | ✅ | ⚠️ | ❌ | ❌ | ❌ | Nur Read |
| **Andere** |
| D88 | ✅ | ✅ | ⚠️ | ✅ | ❌ | Verify fehlt |
| NSI | ✅ | ⚠️ | ❌ | ✅ | ❌ | Basic |

**Legende**: ✅ Vollständig | ⚠️ Partial/Basic | ❌ Fehlt

---

# C) MULTI-READ DESIGN

## Pipeline Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                    MULTI-READ PIPELINE                          │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌─────────┐   ┌───────────┐   ┌────────┐   ┌───────┐          │
│  │  READ   │──▶│ NORMALIZE │──▶│ DECODE │──▶│ SCORE │          │
│  │ Rev 1-N │   │  Timing   │   │ Format │   │ CRC++ │          │
│  └─────────┘   └───────────┘   └────────┘   └───┬───┘          │
│                                                  │               │
│  ┌─────────┐   ┌───────────┐   ┌────────┐       ▼               │
│  │ PRESERVE│◀──│   MERGE   │◀──│ SELECT │◀──[Ranking]          │
│  │ Output  │   │  Best-of  │   │  Best  │                      │
│  └─────────┘   └───────────┘   └────────┘                      │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

## Merge-Strategien

### 1. Bit-Level Merge (Flux)
```c
typedef struct {
    uint32_t timestamp;
    uint8_t  confidence;    /* 0-100 */
    uint8_t  rev_votes;     /* Revolutions agreeing */
    uint8_t  value;         /* 0 or 1 */
    uint8_t  flags;         /* WEAK_BIT, CONFLICT */
} uft_merged_bit_t;

/* Merge N revolutions bit-by-bit */
uft_merge_result_t uft_merge_flux_revolutions(
    const uft_flux_data_t **revs, int rev_count,
    uft_merge_config_t *config);
```

### 2. Sector-Level Merge
```c
typedef struct {
    int      cylinder, head, sector;
    uint8_t  data[MAX_SECTOR_SIZE];
    size_t   size;
    uint8_t  score;         /* 0-100 */
    uint16_t crc_read;
    uint16_t crc_calc;
    bool     crc_ok;
    uint8_t  source_rev;    /* Which revolution */
    uint8_t  read_count;    /* Times successfully read */
} uft_merged_sector_t;
```

### 3. Konfliktauflösung
```c
typedef enum {
    UFT_MERGE_MAJORITY,     /* Mehrheit gewinnt */
    UFT_MERGE_CRC_WINS,     /* CRC-OK hat Vorrang */
    UFT_MERGE_LATEST,       /* Letzte Rev gewinnt */
    UFT_MERGE_SCORED,       /* Höchster Score gewinnt */
} uft_merge_strategy_t;
```

## Betroffene Module

| Modul | Änderung |
|-------|----------|
| `src/analysis/uft_metrics.c` | Revolution-Compare erweitern |
| `src/analysis/uft_surface_analyzer.c` | Merge-Integration |
| `src/core/uft_multi_decoder.c` | Multi-Rev Decode |
| `src/flux_stat.c` | Sector-Level Merge |
| NEU: `src/core/uft_merge_engine.c` | Zentrale Merge-Engine |

---

# D) DRIFT/PLL PLAN

## Aktuelle PLL-Implementierungen (18 Dateien!)

| Datei | Typ | Status |
|-------|-----|--------|
| `src/pll/uft_pll.c` | Basic | ✅ Stabil |
| `src/algorithms/pll/uft_adaptive_pll.c` | Adaptive | ⚠️ Experimental |
| `src/algorithms/uft_kalman_pll.c` | Kalman | ⚠️ Experimental |
| `src/flux/uft_flux_pll.c` | Flux-spezifisch | ✅ Stabil |
| `src/decoder/uft_pll_v2.c` | v2 Decoder | ⚠️ Duplikat |
| `src/core/uft_dpll_wd1772.c` | WD1772-Emulation | ✅ Spezial |

## Unified PLL Design

```c
typedef struct {
    /* Clock Recovery */
    double clock_period_ns;
    double clock_min_ns;
    double clock_max_ns;
    
    /* PLL Tuning */
    double phase_gain;       /* 0.0-1.0, default 0.65 */
    double freq_gain;        /* 0.0-0.5, default 0.04 */
    double bit_error_tol;    /* 0.0-0.5, default 0.20 */
    
    /* Adaptive Parameters */
    bool   adaptive_clock;   /* Auto-detect from sync */
    double drift_rate_max;   /* Max allowed drift/rev */
    int    sync_loss_limit;  /* Max zeros before resync */
    
    /* Zone-specific (C64, Mac, Victor) */
    bool   variable_speed;
    int    zone_count;
    double zone_clocks[8];
} uft_pll_unified_config_t;

/* Presets */
extern const uft_pll_unified_config_t UFT_PLL_MFM_DD;
extern const uft_pll_unified_config_t UFT_PLL_MFM_HD;
extern const uft_pll_unified_config_t UFT_PLL_FM;
extern const uft_pll_unified_config_t UFT_PLL_GCR_C64;
extern const uft_pll_unified_config_t UFT_PLL_GCR_APPLE;
extern const uft_pll_unified_config_t UFT_PLL_GCR_MAC;
```

## Drift-Tracking

```c
typedef struct {
    double clock_drift_total;   /* Accumulated drift (ns) */
    double clock_drift_rate;    /* Drift per revolution */
    double jitter_stddev;       /* Timing jitter σ */
    int    sync_losses;         /* Number of resync events */
    double clock_min_observed;
    double clock_max_observed;
    uint8_t quality;            /* 0-100 */
} uft_pll_statistics_t;
```

---

# E) SCORING SCHEMA

## Score-Komponenten

| Komponente | Gewicht | Berechnung |
|------------|---------|------------|
| CRC/Checksum | 40% | +40 wenn OK, 0 wenn falsch |
| ID Plausibilität | 15% | +15 wenn Track/Sector in Range |
| Sektorfolge | 15% | +15 wenn konsekutiv |
| Header-Struktur | 10% | +10 wenn Sync/Gaps korrekt |
| Timing-Qualität | 15% | 15 * (1 - jitter/threshold) |
| Protection Match | 5% | +5 wenn erwarteter Schutz erkannt |

## Implementierung

```c
typedef struct {
    uint8_t crc_score;          /* 0-40 */
    uint8_t id_score;           /* 0-15 */
    uint8_t sequence_score;     /* 0-15 */
    uint8_t header_score;       /* 0-10 */
    uint8_t timing_score;       /* 0-15 */
    uint8_t protection_score;   /* 0-5 */
    uint8_t total;              /* 0-100 */
    
    /* Diagnostic */
    char    reason[256];        /* "CRC OK, ID valid, ..." */
    uint8_t confidence;         /* Separate from score */
} uft_decode_score_t;

/* Score berechnen */
uft_decode_score_t uft_calculate_score(
    const uft_decoded_sector_t *sector,
    const uft_format_profile_t *format,
    const uft_pll_statistics_t *pll_stats);
```

---

# F) WRITER MATRIX

| Format | Encode | Write | Verify | Gap-Timing | Precomp | Status |
|--------|--------|-------|--------|------------|---------|--------|
| **IBM MFM** |
| IMG | ✅ | ✅ | ❌ | ✅ | ✅ | Verify fehlt |
| IMD | ✅ | ✅ | ❌ | ✅ | ⚠️ | Verify fehlt |
| DMK | ✅ | ✅ | ⚠️ | ✅ | ✅ | Partial |
| **Commodore** |
| D64→1541 | ✅ | ✅ | ⚠️ | ⚠️ | N/A | Gap-Timing |
| G64→1541 | ✅ | ✅ | ⚠️ | ✅ | N/A | OK |
| ADF→Amiga | ✅ | ✅ | ✅ | ✅ | N/A | Komplett |
| **Apple** |
| NIB→II | ⚠️ | ⚠️ | ❌ | ⚠️ | N/A | WIP |
| WOZ→II | ❌ | ❌ | ❌ | ❌ | N/A | TODO |
| **Flux** |
| SCP | ✅ | ✅ | ⚠️ | ✅ | ✅ | OK |
| HFE | ✅ | ✅ | ❌ | ✅ | ⚠️ | Verify |

## Fehlende Writer-Bausteine

1. **Verify-Pipeline** für alle Formate
2. **Gap-Timing** für D64 (Speed-Zone aware)
3. **WOZ Writer** (komplett)
4. **Precompensation** für HD-Formate

---

# G) PROTECTION-KATALOG

## Erkannte Schemes

| Scheme | Platform | Signatur | Status |
|--------|----------|----------|--------|
| **Weak Bits** |
| Random Weak | All | Flux variance >30% | ✅ Detected |
| Deliberate Weak | C64 | Specific tracks | ✅ Detected |
| **Track Anomalies** |
| Long Track | All | >6400 cells | ✅ Detected |
| Short Track | All | <6100 cells | ✅ Detected |
| Extra Sectors | Amiga | >11 sectors | ✅ Detected |
| **Sync Anomalies** |
| Missing Sync | C64 | No $52 pattern | ⚠️ Partial |
| Fake Sync | All | Mid-sector sync | ⚠️ Partial |
| **CRC Traps** |
| Deliberate CRC Error | C64/Amiga | Header OK, Data Bad | ✅ Detected |
| Checksum Trap | Amiga | Wrong header checksum | ✅ Detected |
| **Format-specific** |
| Rob Northen | Amiga | RNC signature | ⚠️ Partial |
| CAPS/SPS | Amiga | IPF container | ✅ Via IPF |
| V-MAX! | C64 | Track 36+ | ⚠️ Partial |
| Rapidlok | C64 | Half-tracks | ❌ TODO |

## Integration

```c
typedef struct {
    const char *scheme_name;
    const char *version;
    uint8_t    confidence;      /* 0-100 */
    int        affected_tracks[84];
    int        track_count;
    uint32_t   flags;           /* PRESERVE_WEAK, PRESERVE_TIMING */
    char       details[512];
} uft_protection_info_t;

/* Detection API */
int uft_detect_protection(
    const uft_disk_t *disk,
    uft_protection_info_t *results,
    int max_results);
```

---

# H) GUI COVERAGE REPORT

## Tab-Coverage

| Tab | Backend-Module | Vollständig | Fehlend |
|-----|---------------|-------------|---------|
| Workflow | WorkflowTab | ⚠️ | Progress-Callbacks |
| Hardware | HardwareTab | ✅ | - |
| Status | StatusTab | ⚠️ | Real-time Hex-Update |
| Format | FormatTab | ⚠️ | Auto-Detect Feedback |
| Explorer | ExplorerTab | ⚠️ | Write-Support |
| Tools | ToolsTab | ⚠️ | Batch-Progress |
| Nibble | NibbleTab | ⚠️ | PLL-Visualisierung |
| XCopy | XCopyTab | ⚠️ | Multi-Pass Status |
| Protection | ProtectionTab | ⚠️ | Preserve-Toggle |
| Forensic | ForensicTab | ⚠️ | Report-Export |

## Fehlende GUI-Controls

| Control | Backend-Parameter | Priority |
|---------|------------------|----------|
| PLL Phase Slider | pll_phase | P1 |
| PLL Freq Slider | pll_adjust | P1 |
| Bit Error Threshold | bit_error_tol | P1 |
| Score Display | decode_score | P1 |
| Multi-Rev Spinner | revolutions | ✅ Vorhanden |
| Merge Strategy Combo | merge_strategy | P2 |
| Protection Preserve | preserve_protection | P2 |
| Verify After Write | write_verify | P2 |

---

# I) TEST/CI PLAN

## Neue Tests (Priorität)

### P0 - Smoke Tests
```c
/* test_smoke.c */
void test_app_starts(void);
void test_load_d64(void);
void test_load_adf(void);
void test_decode_track(void);
void test_export_image(void);
```

### P1 - Parser Golden Tests
```c
/* test_parser_golden.c */
void test_d64_standard(void);      /* Known-good D64 */
void test_d64_errors(void);        /* D64 with errors */
void test_adf_ofs(void);           /* Amiga OFS */
void test_adf_ffs(void);           /* Amiga FFS */
void test_scp_dd(void);            /* SCP DD disk */
void test_g64_protection(void);    /* G64 with protection */
```

### P2 - Multi-Read Tests
```c
/* test_multi_read.c */
void test_merge_identical(void);   /* Same data = same result */
void test_merge_one_bad(void);     /* Good wins over bad */
void test_merge_deterministic(void); /* Same input = same output */
```

### P3 - Edge Cases
```c
/* test_edge.c */
void test_empty_file(void);
void test_truncated_file(void);
void test_huge_file(void);
void test_readonly_output(void);
```

## CI Erweiterungen

```yaml
# .github/workflows/build.yml additions
  test-linux:
    runs-on: ubuntu-22.04
    needs: build-linux
    steps:
    - name: Run unit tests
      run: ./tests/run_all_tests.sh
    
    - name: Run golden tests
      run: ./tests/run_golden_tests.sh
    
    - name: Check for warnings
      run: |
        make clean
        make 2>&1 | tee build.log
        if grep -i "warning:" build.log; then
          echo "::error::Build has warnings"
          exit 1
        fi
```
