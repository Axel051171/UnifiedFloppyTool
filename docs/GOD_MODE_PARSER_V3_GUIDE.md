# GOD MODE Parser v3 - Upgrade Guide

## Was ist v3?

v3 Parser sind **vollständige Read/Write/Analyze/Verify** Parser mit allen Features:

| Feature | v2 Parser | v3 Parser |
|---------|-----------|-----------|
| Read | ✅ | ✅ |
| Write | ❌/⚠️ | ✅ |
| Multi-Rev Merge | ❌ | ✅ |
| Bit-Level Voting | ❌ | ✅ |
| Kopierschutz-Preservation | ⚠️ | ✅ |
| Track-Diagnose | ❌ | ✅ |
| Scoring-System | ❌ | ✅ |
| Verify-After-Write | ❌ | ✅ |
| Vollständige Parameter | ❌ | ✅ |

## Neue Parameter-Kategorien

### 1. Retry / Read Strategy
```c
params.retry.revolutions = 5;           // Mehr Umdrehungen
params.retry.adaptive_mode = true;      // Auto-increase bei Fehler
params.retry.rev_selection = UFT_REV_VOTE;  // Bit-Level Voting
params.retry.merge_strategy = UFT_MERGE_BEST_CRC;
```

### 2. Timing / PLL Control
```c
params.timing.rpm_target = 300;
params.timing.pll_mode = UFT_PLL_KALMAN; // Kalman-Filter PLL
params.timing.pll_bandwidth = 0.1f;
params.timing.bitcell_tolerance_percent = 15;
```

### 3. Error Handling
```c
params.error.accept_bad_crc = false;
params.error.attempt_crc_correction = true;
params.error.max_correction_bits = 2;
params.error.error_mode = UFT_ERR_SALVAGE;
```

### 4. Quality Metrics
```c
params.quality.weakbit_detect = true;
params.quality.preserve_weakbits = true;
params.quality.confidence_report = true;
params.quality.jitter_threshold_ns = 500;
```

### 5. Raw vs Cooked
```c
params.mode.output_mode = UFT_MODE_HYBRID; // Beides!
params.mode.preserve_sync = true;
params.mode.preserve_weak = true;
params.mode.preserve_timing = true;
```

### 6. Alignment / Sync
```c
params.alignment.sync_tolerant = true;
params.alignment.sync_window_bits = 1024;
params.alignment.splice_mode = UFT_SPLICE_GAP;
```

### 7. Verify After Write
```c
params.verify.verify_enabled = true;
params.verify.verify_mode = UFT_VERIFY_BITSTREAM;
params.verify.rewrite_on_fail = true;
params.verify.max_rewrites = 3;
```

## Diagnose-System

Jeder Track erklärt exakt WARUM er Probleme hat:

```c
// Diagnose generieren
uft_diagnosis_list_t* diag = uft_diagnosis_list_create();
parser->diagnose_track(track_data, diag);

// Report ausgeben
char* report = uft_diagnosis_to_text(diag);
printf("%s", report);
```

**Beispiel-Output:**
```
══════════════════════════════════════════════════════════════════
                    DISK DIAGNOSIS REPORT                         
══════════════════════════════════════════════════════════════════
Errors: 2     Warnings: 3     Info: 5     Quality: 87.3%        
══════════════════════════════════════════════════════════════════

── Track 17 ─────────────────────────────────────────
  ❌ T17.0 S05: Data CRC error
           → Try CRC correction or multi-rev voting
  ⚠️ T17.0 S05: Weak/fuzzy bits detected
           → PRESERVE - this is likely copy protection
  ✅ T17.0 S06: OK
```

## Scoring-System

Jeder Sektor bekommt einen Score (0-1):

```c
uft_score_t score;
parser->score_sector(sector, &score);

printf("Overall: %.1f%% (%s)\n", 
       score.overall * 100,
       uft_score_rating(score.overall));

// Komponenten:
// - crc_score: CRC Validität
// - id_score: Sektor-ID Plausibilität  
// - timing_score: Timing-Konsistenz
// - sequence_score: Sektor-Reihenfolge
// - sync_score: Sync-Qualität
// - jitter_score: Jitter-Level
```

## Multi-Rev Merge

```c
// Bit-Level Voting über alle Revolutions
uft_merge_sector_data(
    sector_data,      // uint8_t*[] - Daten pro Rev
    crc_valid,        // bool[] - CRC Status pro Rev
    rev_count,        // Anzahl Revolutions
    sector_size,      // Sektorgröße
    output,           // Ergebnis
    weak_mask,        // Weak-Bit Maske
    confidence,       // Confidence pro Byte
    &score            // Gesamt-Score
);
```

## v3 Parser Implementation Template

```c
uft_parser_v3_t my_format_parser = {
    .name = "MyFormat",
    .version = "3.0.0",
    .extensions = "mf,myf",
    
    .capabilities = {
        .can_read = true,
        .can_write = true,
        .can_analyze = true,
        .supports_multi_rev = true,
        .supports_protection = true,
        .supports_weak_bits = true,
        .supports_timing = true,
        .supports_verify = true
    },
    
    .probe = my_format_probe,
    .read = my_format_read,
    .write = my_format_write,
    .analyze = my_format_analyze,
    .verify = my_format_verify,
    .repair = my_format_repair,
    
    .read_track = my_format_read_track,
    .write_track = my_format_write_track,
    .diagnose_track = my_format_diagnose_track,
    
    .merge_revolutions = my_format_merge_revs,
    .detect_protection = my_format_detect_protection,
    .preserve_protection = my_format_preserve_protection,
    
    .get_default_params = my_format_get_defaults,
    .validate_params = my_format_validate_params,
    
    .free_disk = my_format_free_disk,
    .free_track = my_format_free_track,
    .free_sector = my_format_free_sector
};
```

## Migration von v2 zu v3

1. **Header ändern**: `#include "uft_parser_v3.h"`
2. **Interface implementieren**: Alle 20+ Funktionen
3. **Parameter nutzen**: `uft_params_v3_t` statt eigene
4. **Diagnose integrieren**: Bei jedem Fehler `uft_diagnosis_add()`
5. **Scoring implementieren**: `uft_score_sector()` nach jedem Read
6. **Multi-Rev nutzen**: `uft_merge_sector_data()` für Voting
7. **Writer implementieren**: `write()`, `write_track()`, `write_sector()`
8. **Verify implementieren**: Nach jedem Write

## Prioritäten für Upgrade

| Parser | Priorität | Grund |
|--------|-----------|-------|
| D64/G64 | 🔴 Hoch | Meistgenutzt, Kopierschutz-kritisch |
| ADF | 🔴 Hoch | Amiga-Szene aktiv |
| SCP | 🔴 Hoch | Flux-Referenz |
| NIB/WOZ | 🟡 Mittel | Apple II Preservation |
| STX/IPF | 🟡 Mittel | Kopierschutz-Formate |
| IMD/TD0 | 🟢 Niedrig | Archive-Formate |

## Dateien

- `include/uft_parser_v3.h` - Interface Definition (900+ LOC)
- `src/core/uft_parser_v3_core.c` - Core Implementation (700+ LOC)
- `docs/GOD_MODE_PARSER_AUDIT.md` - Status aller Parser
