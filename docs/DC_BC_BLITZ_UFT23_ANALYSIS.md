# Analysis: DC-BC-Edit, BLITZ & UFT-Step23

**Analysiert:** 2024-12-30
**Archive:** dc-bc-edit.zip, BLITZ.zip, UnifiedFloppyTool-Step23-FormatPresetsGUI.zip

---

## 1. DC-BC-EDIT (Backup Configuration Language)

### Was ist es?
Eine **Kopierschutz-Konfigurations-Sprache** für historische Atari ST Backup-Tools. Die Datei `DBKUPCF.S` enthält Kopier-Profile für verschiedene Spiele/Disketten.

### Syntax-Struktur

```
SS 80 TRKS FLUX 0 & 79 REST INDEX    # Single-Sided, 80 Tracks
                                      # Flux-Copy für Track 0 & 79
                                      # Rest: Index-Kopie

!                                      # Start Side 0 (Top)
0 : W 0 0 6450 1                       # Track 0: Write Flux (6450 bytes, 1 rev)
1 : U 1 0                              # Track 1: Use Index copy
R : 78                                 # Repeat to Track 78
79 : W 0 0 6450 1                      # Track 79: Write Flux

)                                      # End Side 0

S                                      # Start Side 1 (Bottom)
...
)                                      # End Side 1

]                                      # End Config
```

### Befehle

| Cmd | Bedeutung | Parameter |
|-----|-----------|-----------|
| `W` | Write Flux | offset, flags, size, revs |
| `U` | Use Index | mode, flags |
| `R` | Repeat | track_count |
| `!` | Start Side 0 | - |
| `S` | Start Side 1 | - |
| `)` | End Side | - |
| `]` | End Config | - |

### UFT-Relevanz: HOCH

Diese Sprache ist ein **Format-Preset-System**! Wir können das Konzept übernehmen:

```c
typedef struct {
    uft_track_mode_t mode;   // UFT_TRACK_FLUX, UFT_TRACK_INDEX, UFT_TRACK_SECTOR
    uint32_t offset;
    uint32_t size;
    uint8_t revolutions;
    uint8_t flags;
} uft_track_preset_t;

typedef struct {
    const char *name;        // "Operation Wolf"
    uint8_t tracks;          // 80
    uint8_t sides;           // 1 or 2
    uft_track_preset_t *per_track;  // Array von Presets
} uft_disk_copy_profile_t;
```

---

## 2. BLITZ Kopier-System (Atari ST)

### Was ist es?
Ein **revolutionäres Low-Level Flux-Kopiersystem** für Atari ST (1989-1990).

### Kerninnovation

**Simultanes Lesen/Schreiben:**
```
Drive A (Source) ---> Printer Port ---> Drive B (Destination)
          READ              TRANSFER           WRITE
          ↑                    ↑                  ↑
        ← ← ← ← ← SAME TIME ← ← ← ← ←
```

Das ist GENIALISTISCH! Während normale Kopierer:
1. Track lesen → RAM
2. Track schreiben → Disk

Macht BLITZ:
1. Track lesen + gleichzeitig schreiben

### Hardware-Verbindung

```
┌─────────────────────────────────────────────────────────────┐
│ ATARI ST                                                    │
│  ┌────────────┐    ┌────────────┐    ┌────────────┐        │
│  │ Drive Port │    │ Printer    │    │ Drive Port │        │
│  │ (14-pin    │    │ (25-pin    │    │ (14-pin    │        │
│  │  DIN)      │    │  DB25)     │    │  DIN)      │        │
│  └──────┬─────┘    └──────┬─────┘    └──────┬─────┘        │
│         │                 │                 │               │
│         │      BLITZ CABLE (selbstgebaut)  │               │
│         │                 │                 │               │
│         ▼                 ▼                 ▼               │
│    ┌─────────┐      ┌──────────┐      ┌─────────┐          │
│    │ Drive A │      │ Control  │      │ Drive B │          │
│    │ (Source)│◄─────│ Signals  │─────►│ (Dest)  │          │
│    └─────────┘      └──────────┘      └─────────┘          │
└─────────────────────────────────────────────────────────────┘
```

### Timing-Kritische Aspekte

```
Drive Speed: 300 RPM ± 0.9 RPM (kritisch!)
            = 5 Umdrehungen/Sekunde
            = 200ms pro Umdrehung

Maximale Abweichung: 0.09 RPM zwischen Laufwerken

Sync-Verlust: Wenn >1 Sekunde Pause → Abbrechen!
```

### Modi

| Modus | Beschreibung | Geschwindigkeit |
|-------|--------------|-----------------|
| TURBO | Simultanes R/W | 20s SS, 40s DS |
| NORMAL | Sequentiell | Langsamer |

### Disk-Typen

```
Atari 3.5": TRACKS=81, SIDES=2, MODE=TURBO
IBM 5.25":  TRACKS=41, SIDES=1, MODE=NORMAL
Games:      TRACKS=81, SIDES=1, MODE=TURBO
```

### UFT-Relevanz: MITTEL-HOCH

**Konzepte für UFT:**

1. **Streaming-Copy-Pipeline:**
```c
typedef struct {
    flux_reader_t *reader;
    flux_writer_t *writer;
    uint32_t buffer_size;
    bool streaming_enabled;
} uft_stream_copy_t;
```

2. **RPM-Matching:**
```c
typedef struct {
    double rpm_source;
    double rpm_dest;
    double tolerance;
    bool sync_ok;
} uft_rpm_sync_t;

bool uft_check_rpm_sync(uft_rpm_sync_t *sync) {
    double diff = fabs(sync->rpm_source - sync->rpm_dest);
    return diff <= sync->tolerance;  // 0.09 RPM
}
```

3. **Sync-Loss Detection:**
```c
typedef struct {
    uint64_t last_activity_ns;
    uint64_t timeout_ns;  // 1000000000 (1s)
    bool sync_lost;
} uft_sync_detector_t;
```

---

## 3. UFT-Step23 (Unser Projekt!)

### Architektur-Übersicht

```
┌─────────────────────────────────────────────────────────────┐
│                    Qt GUI (MainWindow)                       │
├─────────────────────────────────────────────────────────────┤
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────┐  │
│  │ Presets     │  │ ParamSchema │  │ BackendArgMapper    │  │
│  │ (JSON)      │  │ (Validate)  │  │ (CLI Args)          │  │
│  └─────────────┘  └─────────────┘  └─────────────────────┘  │
├─────────────────────────────────────────────────────────────┤
│                    Core C Library                            │
│  ┌─────────┐  ┌─────────┐  ┌──────────┐  ┌──────────────┐   │
│  │ PLL     │  │ Recovery│  │ Formats  │  │ Hardware     │   │
│  │         │  │ (Multi- │  │ (15+     │  │ Providers    │   │
│  │         │  │  pass)  │  │  types)  │  │              │   │
│  └─────────┘  └─────────┘  └──────────┘  └──────────────┘   │
└─────────────────────────────────────────────────────────────┘
```

### Preset-System (Excellent!)

```cpp
struct UftPreset {
    QString id;          // "fast", "balanced", "aggressive"
    QString name;        // UI name
    QString description;
    
    // JSON blobs
    QString recoveryJson;
    QString formatJson;
    QHash<uft_output_format_t, QString> outputJsonByFmt;
};

// Preset-Stufen
static void applyCommonFast(QJsonObject &r) {
    r.insert("passes", 1);
    r.insert("offset_steps", 0);
    r.insert("pll_bandwidth", 0.20);
    r.insert("jitter_ns", 120);
    r.insert("splice_mode", "best-crc");
}

static void applyCommonBalanced(QJsonObject &r) {
    r.insert("passes", 3);
    r.insert("offset_steps", 3);
    r.insert("pll_bandwidth", 0.25);
    r.insert("jitter_ns", 150);
    r.insert("vote_threshold", 0.55);
    r.insert("splice_mode", "vote");
}

static void applyCommonAggressive(QJsonObject &r) {
    r.insert("passes", 8);
    r.insert("offset_steps", 6);
    r.insert("pll_bandwidth", 0.55);
    r.insert("jitter_ns", 250);
    r.insert("vote_threshold", 0.52);
    r.insert("splice_mode", "hybrid");
    r.insert("emit_map", true);
    r.insert("emit_metrics", true);
    r.insert("emit_log", true);
}
```

### ParamSchema (Validation)

```cpp
// Schema-basierte JSON Validierung
QString ParamSchema::sanitize(
    const uft_param_def_t *defs,
    size_t count,
    const QString &jsonInput,
    bool *ok,
    QStringList *warnings
) {
    // 1. Parse JSON
    // 2. Coerce types (bool/int/float/enum)
    // 3. Clamp ranges
    // 4. Fill defaults
    // 5. Remove unknown keys
    // 6. Return normalized JSON
}
```

### PLL Implementation

```c
uft_pll_cfg_t uft_pll_cfg_default_mfm_dd(void) {
    uft_pll_cfg_t c;
    c.cell_ns = 2000;          // 2µs (DD)
    c.cell_ns_min = 1600;
    c.cell_ns_max = 2400;
    c.alpha_q16 = 3277;        // ~0.05 (bandwidth)
    c.max_run_cells = 8;
    return c;
}

// PLL Update Loop
for (size_t i = 0; i + 1 < count; ++i) {
    uint64_t delta = b - a;
    uint32_t run = (delta + cell/2) / cell;  // Rounded
    
    // Emit bits
    for (uint32_t k = 0; k + 1 < run; ++k)
        set_bit(out, bitpos++, 0);
    set_bit(out, bitpos++, 1);
    
    // PLL correction
    int64_t err = delta - run * cell;
    int64_t adj = (err / run * alpha) >> 16;
    cell = clamp(cell + adj, min, max);
}
```

### Multi-Pass Recovery

```c
size_t uft_recover_mfm_track_multipass(
    const flux_track_t *const *passes,
    size_t pass_count,
    const uft_recovery_cfg_t *cfg_in,
    uint8_t *out_bits,
    size_t out_capacity_bits,
    float *out_quality
) {
    // 1. Decode each pass with PLL
    // 2. Find sync markers
    // 3. Align passes
    // 4. Vote per bit
    // 5. Calculate quality score
}
```

### Backend ArgMapper

```cpp
UftRunPlan BackendArgMapper::buildPlan(
    uft_disk_format_id_t fmt,
    uft_output_format_t outFmt,
    const QString &formatJson,
    const QString &recoveryJson,
    const QString &outputJson,
    const QString &outputDir,
    const QString &baseName
) {
    // Generates:
    // --format.key=value
    // --recovery.key=value
    // --output.key=value
    // --io.output_dir=...
    // --io.base_name=...
    
    // Plus artifacts list:
    // - image (primary output)
    // - map (sector status)
    // - metrics (run stats)
    // - log (text log)
    // - profile (reproducible config)
}
```

---

## 4. Integration & Verbesserungen

### 4.1 Aus DC-BC-EDIT: Track-Preset-System

```c
// uft_track_preset.h

typedef enum {
    UFT_TRACK_AUTO = 0,
    UFT_TRACK_FLUX = 1,    // Full flux copy
    UFT_TRACK_INDEX = 2,   // Index-to-index copy
    UFT_TRACK_SECTOR = 3,  // Sector-level copy
    UFT_TRACK_SKIP = 4     // Skip track
} uft_track_mode_t;

typedef struct {
    uft_track_mode_t mode;
    uint32_t flux_offset;      // Offset in flux data
    uint32_t flux_size;        // Size of flux data
    uint8_t revolutions;       // Number of revolutions
    uint8_t retry_count;       // Retries on error
    uint16_t flags;            // UFT_TRACK_FLAG_*
} uft_track_config_t;

typedef struct {
    const char *name;          // Profile name
    const char *description;
    uint8_t track_count;
    uint8_t side_count;
    uft_track_config_t *tracks; // Array[track_count * side_count]
} uft_copy_profile_t;

// Load from text file (DC-BC-EDIT style)
int uft_copy_profile_parse(const char *text, uft_copy_profile_t *out);
```

### 4.2 Aus BLITZ: Streaming Copy

```c
// uft_stream_copy.h

typedef struct {
    // Source
    flux_reader_t *reader;
    uint8_t source_track;
    uint8_t source_side;
    
    // Destination
    flux_writer_t *writer;
    uint8_t dest_track;
    uint8_t dest_side;
    
    // Buffer
    uint8_t *buffer;
    size_t buffer_size;
    
    // Status
    bool streaming;
    uint64_t bytes_transferred;
    uint64_t last_activity_ns;
    
    // RPM sync
    double rpm_source;
    double rpm_dest;
    double rpm_tolerance;  // 0.09
} uft_stream_copy_t;

// Initialize streaming copy
int uft_stream_copy_init(uft_stream_copy_t *ctx,
                         flux_reader_t *reader,
                         flux_writer_t *writer);

// Copy single track (streaming)
int uft_stream_copy_track(uft_stream_copy_t *ctx,
                          uint8_t track, uint8_t side);

// Check sync status
bool uft_stream_copy_check_sync(uft_stream_copy_t *ctx);
```

### 4.3 RPM Drift Detection

```c
// uft_rpm.h

typedef struct {
    double rpm_measured;
    double rpm_nominal;   // 300.0 oder 360.0
    double drift_percent;
    bool in_spec;        // ± 0.3%
} uft_rpm_status_t;

// Measure RPM from index pulses
int uft_rpm_measure(const flux_track_t *track, uft_rpm_status_t *out);

// Check if two drives are synchronized
bool uft_rpm_drives_synced(double rpm1, double rpm2, double tolerance);
```

---

## 5. Zusammenfassung

### Neue Features aus Analyse

| Feature | Quelle | Priorität | Status |
|---------|--------|-----------|--------|
| Track-Preset-System | DC-BC-EDIT | HOCH | NEU |
| Streaming Copy | BLITZ | MITTEL | NEU |
| RPM Sync Detection | BLITZ | HOCH | NEU |
| Sync-Loss Detection | BLITZ | HOCH | NEU |
| JSON Preset Bundles | UFT-Step23 | ✅ DONE | - |
| Multi-Pass Recovery | UFT-Step23 | ✅ DONE | - |
| Schema Validation | UFT-Step23 | ✅ DONE | - |
| Namespaced CLI Args | UFT-Step23 | ✅ DONE | - |

### Code-Qualität UFT-Step23

| Aspekt | Bewertung |
|--------|-----------|
| Architektur | ⭐⭐⭐⭐⭐ Exzellent |
| PLL | ⭐⭐⭐⭐ Gut |
| Recovery | ⭐⭐⭐⭐ Gut |
| Preset-System | ⭐⭐⭐⭐⭐ Exzellent |
| GUI | ⭐⭐⭐⭐ Gut |
| Dokumentation | ⭐⭐⭐ Mittel |

### Empfehlungen

1. **Track-Preset-System integrieren** (aus DC-BC-EDIT)
2. **RPM-Monitoring hinzufügen** (aus BLITZ)
3. **Streaming-Copy-Modus** als Option
4. **Sync-Loss-Detection** für Hardware-Operationen
