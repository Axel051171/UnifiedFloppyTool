# UFT v5.3 DD_MODULE - Merge Report

**Datum:** 2025-01-02  
**Quelle:** UFT_FINAL_v5_3_DD_MODULE_1_.tar.gz  
**Ziel:** UnifiedFloppyTool v3.9.5+  

---

## 📊 Übersicht

| Metrik | Wert |
|--------|------|
| **Gesamtzeilen** | ~91.000 |
| **C-Dateien** | 323 |
| **Header** | 263 |
| **C++-Dateien** | 29 |
| **Qt UI-Dateien** | 9 |
| **Format-Module ZIPs** | 65 |
| **Examples** | 23 |

---

## 🔥 KRITISCHE NEUE KOMPONENTEN

### 1. DD-Modul (Forensic Data Duplication)

**Location:** `include/uft/uft_dd.h`, `libflux_format/src/uft_dd.c`  
**Zeilen:** 831 + 448 = 1.279  

**Features kombiniert aus:**
- **dd_rescue:** Recovery-Algorithmen, Reverse Reading, Adaptive Block Sizes
- **DC3DD:** Forensic Hashing (MD5/SHA1/SHA256/SHA512), Wipe Patterns, Verification
- **dcfldd:** Multiple Outputs, Hash-on-Copy, File Splitting

**Strukturen:**
```c
typedef struct {
    dd_blocksize_t blocksize;    // Soft/Hard/DIO
    dd_recovery_t recovery;      // Reverse, Sparse, Retry
    dd_hash_t hash;              // MD5/SHA/Verify
    dd_wipe_t wipe;              // DoD 3/7-Pass, Gutmann
    dd_output_t output;          // Split, Direct I/O
    dd_floppy_t floppy;          // Direct Floppy Write!
} dd_config_t;
```

**Floppy-spezifische Features:**
- Direct Sector Write über Greaseweazle/FluxEngine/KryoFlux
- Format before Write
- Verify Sectors
- Bad Sector Handling
- Step/Settle/Motor Delays konfigurierbar

**GUI-Constraints definiert:**
- Alle MIN/MAX/DEFAULT-Werte für Qt SpinBoxes
- Validierung über `dd_config_validate()`

---

### 2. C64 Protection Traits System

**Location:** `libflux_core/include/c64/`, `libflux_core/src/c64/`  
**Zeilen:** ~3.500  
**Dateien:** 19 Header + 19 Implementierungen

**Trait-Detektoren:**
| Trait | Beschreibung |
|-------|-------------|
| `uft_c64_trait_weakbits` | Weak Bit Regions |
| `uft_c64_trait_longtrack` | Überlange Tracks |
| `uft_c64_trait_halftracks` | Half-Track Daten |
| `uft_c64_trait_illegal_gcr` | Illegale GCR-Nibbles |
| `uft_c64_trait_long_sync_run` | Extra-lange Sync-Runs |
| `uft_c64_trait_bitlen_jitter` | Bitlängen-Variation |
| `uft_c64_trait_speed_anomaly` | Geschwindigkeits-Anomalien |
| `uft_c64_trait_dirtrack_anomaly` | Directory-Track Anomalien |
| `uft_c64_trait_marker_bytes` | Protection Marker |
| `uft_c64_trait_multirev_recommended` | Multi-Rev empfohlen |

**Spezifische Decoder:**
- `uft_c64_rapidlok.h/c` - RapidLok v1
- `uft_c64_rapidlok2_detect.h/c` - RapidLok v2
- `uft_c64_rapidlok6.h/c` - RapidLok v6
- `uft_c64_ealoader.h/c` - Electronic Arts Loader
- `uft_c64_geos_gap_protection.h/c` - GEOS Gap Protection
- `uft_c64_scheme_detect.h/c` - Scheme Auto-Detection
- `uft_c64_protection_taxonomy.h/c` - Taxonomy Framework

**Taxonomy API:**
```c
typedef struct {
    uft_c64_prot_type_t type;
    int track_x2;              // Half-track support
    int severity_0_100;
} uft_c64_prot_hit_t;

bool uft_c64_prot_analyze(const uft_c64_track_metrics_t* tracks, 
                          size_t track_count,
                          uft_c64_prot_hit_t* hits, size_t hits_cap,
                          uft_c64_prot_report_t* out);
```

---

### 3. IUniversalDrive HAL (Hardware Abstraction Layer)

**Location:** `libflux_core/include/uft_iuniversaldrive.h`  
**Zeilen:** 397

**Design-Prinzipien:**
1. Hardware Lock-in Prevention
2. Plug-and-Play Device Support
3. Testability (Mock Devices)
4. Future-Proof Architecture

**Capability System:**
```c
typedef enum {
    UFT_CAP_FLUX_READ           = (1 << 0),
    UFT_CAP_FLUX_WRITE          = (1 << 1),
    UFT_CAP_INDEX_SIGNAL        = (1 << 2),
    UFT_CAP_DENSITY_DETECT      = (1 << 3),
    UFT_CAP_WRITE_PROTECT       = (1 << 4),
    UFT_CAP_TRACK0_SENSOR       = (1 << 5),
    UFT_CAP_MOTOR_CONTROL       = (1 << 6),
    UFT_CAP_VARIABLE_SPEED      = (1 << 7),
    UFT_CAP_MULTIPLE_REVS       = (1 << 9),
    UFT_CAP_HALF_TRACK          = (1 << 11),
    UFT_CAP_WEAK_BIT_REPEAT     = (1 << 12),
    UFT_CAP_HIGH_PRECISION      = (1 << 13)
} uft_drive_capability_t;
```

**Flux Stream (normalisiert auf Nanosekunden):**
```c
typedef struct {
    uint32_t* transitions_ns;   // Array of transition times
    uint32_t count;             // Number of transitions
    uint32_t index_offset;      // Index pulse offset
    uint32_t total_time_ns;     // Total track time
    uint8_t revolution;         // Revolution number
} uft_flux_stream_t;
```

**Provider vtable:**
```c
typedef struct uft_drive_ops {
    uft_rc_t (*open)(void* ctx, const char* device_path);
    uft_rc_t (*close)(void* ctx);
    uft_rc_t (*get_info)(void* ctx, uft_drive_info_t* info);
    uft_rc_t (*select_drive)(void* ctx, uint8_t drive_id);
    uft_rc_t (*set_motor)(void* ctx, uft_motor_state_t state);
    uft_rc_t (*calibrate)(void* ctx);
    uft_rc_t (*seek)(void* ctx, uint8_t track, uint8_t head);
    uft_rc_t (*read_flux)(void* ctx, uft_flux_stream_t** flux_stream);
    uft_rc_t (*write_flux)(void* ctx, const uft_flux_stream_t* flux_stream);
    uft_rc_t (*get_density)(void* ctx, uft_density_t* density);
    uft_rc_t (*is_write_protected)(void* ctx, bool* protected);
    uft_rc_t (*is_track0)(void* ctx, bool* at_track0);
} uft_drive_ops_t;
```

---

### 4. Recovery Parameters (GUI-kontrollierbar)

**Location:** `include/uft/recovery_params.h`  
**Zeilen:** 468

**Parameter-Gruppen:**
| Gruppe | Parameter | GUI-Widget |
|--------|----------|------------|
| **MFM Timing** | timing_4us, timing_6us, timing_8us, threshold_offset | SpinBox |
| **Adaptive** | rate_of_change, lowpass_radius, warmup_samples, max_drift | Slider/SpinBox |
| **PLL** | gain, phase_tolerance, freq_tolerance, reset_on_sync, soft_pll | SpinBox/Checkbox |
| **Error Correction** | max_bit_flips, search_region_size, timeout_ms | SpinBox |
| **Retry** | max_retries, retry_delay_ms, seek_distance, speed_variation | SpinBox/Slider |
| **Analysis** | generate_histogram, generate_entropy, log_level | Checkbox/ComboBox |
| **Amiga** | format, ignore_header_errors, read_extended_tracks | ComboBox/Checkbox |
| **PC** | format, sector_size, interleave | ComboBox/SpinBox |

**Presets:**
```c
typedef enum {
    PRESET_DEFAULT,        // Balanced
    PRESET_FAST,           // Speed over accuracy
    PRESET_THOROUGH,       // Maximum recovery
    PRESET_AGGRESSIVE,     // Very damaged disks
    PRESET_GENTLE,         // Fragile media
    PRESET_AMIGA_STANDARD,
    PRESET_AMIGA_DAMAGED,
    PRESET_PC_STANDARD,
    PRESET_PC_DAMAGED,
    PRESET_CUSTOM
} recovery_preset_t;
```

**Widget-Beschreibungen für automatische GUI-Generierung:**
```c
typedef struct {
    const char *name;           // Parameter name
    const char *label;          // Display label
    const char *tooltip;        // Help tooltip
    const char *group;          // Tab name
    widget_type_t widget_type;  // SPINBOX_INT, SLIDER_FLOAT, CHECKBOX, etc.
    double min_val, max_val, default_val, step;
    const char *unit;           // "µs", "ms", "%"
    size_t offset;              // Offset in recovery_config_t
} param_widget_desc_t;
```

---

### 5. Hardware Providers (Qt-Klassen)

**Location:** `src/hardware_providers/`  
**Zeilen:** ~8.000

| Provider | Zeilen | Features |
|----------|--------|----------|
| **fc5025hardwareprovider** | 456 | Native USB, 16 Formate, Format-Detection |
| **xum1541hardwareprovider** | 421 | C64 USB, GCR, Nibble I/O |
| **greaseweazlehardwareprovider** | 348 | Flux Read/Write, Multi-Rev |
| **fluxenginehardwareprovider** | 201 | Flux Read/Write |
| **kryofluxhardwareprovider** | 210 | Stream Read |
| **applesaucehardwareprovider** | 82 | Apple II |
| **catweaselhardwareprovider** | 82 | Legacy |
| **scphardwareprovider** | 82 | SuperCard Pro |
| **hardwaremanager** | 181 | Provider Registry |

**FC5025 Unterstützte Formate:**
- Apple II DOS 3.2/3.3, ProDOS
- TRS-80 Model I/III/4
- CP/M 8" SSSD, Kaypro
- MS-DOS 360K/1.2M
- Atari 810/1050
- FM/MFM SD/DD/HD
- Raw Bitstream

---

### 6. WOZ2 Apple II Support

**Location:** `libflux_format/include/woz2.h`, `libflux_format/src/woz2.c`  
**Zeilen:** 278 + 450 = 728

**WOZ 2.0 Features:**
- Enhanced metadata support
- Improved timing accuracy
- Copy protection preservation
- Quarter-track support (0.25 increments)
- FLUX chunk support

**Chunks:**
- INFO (60 bytes): disk_type, creator, optimal_bit_timing
- TMAP (160 bytes): Quarter-track mapping
- TRKS: Track data mit bit_count
- META: Metadata (optional)
- WRIT: Write info (optional)

**API:**
```c
bool woz2_read(const char *filename, woz2_image_t *image);
bool woz2_write(const char *filename, const woz2_image_t *image);
bool woz2_add_track(woz2_image_t *image, uint8_t track_num, 
                    uint8_t quarter, const uint8_t *data, uint32_t bit_count);
bool woz2_from_woz1(const char *woz1, const char *woz2);
bool woz2_from_dsk(const char *dsk, const char *woz2, uint8_t disk_type);
```

---

### 7. Weak Bits Detection

**Location:** `libflux_core/include/weak_bits.h`, `libflux_core/src/weak_bits/weak_bits.c`  
**Zeilen:** 296 + 350 = 646

**Detection Algorithm:**
1. Compare each bit across all revolutions
2. If bit has different values → "weak"
3. Calculate variation percentage
4. Flag if exceeds threshold

**Integration:**
- X-Copy Error 8 (Verify error) detection
- UFM track metadata
- JSON export for archival

**API:**
```c
int weak_bits_detect(
    const uint8_t **tracks,      // Multiple revolutions
    size_t track_count,          // 3-10 recommended
    size_t track_length,
    const weak_bit_params_t *params,
    weak_bit_result_t *result_out
);

bool weak_bits_triggers_xcopy_error(
    const weak_bit_result_t *result,
    uint32_t threshold
);
```

---

### 8. Format-Module ZIPs (65 Stück!)

**Location:** `format_modules/`

**Kategorien:**
| Kategorie | Module |
|-----------|--------|
| **Commodore** | D64, D71, D81, D82, G64, GCRRAW, GCRDECODE |
| **Apple** | 2MG, NIB, PRODOS_PO_DO, WOZ |
| **Atari** | ATR, ATX, ATXDECODE, XFD |
| **PC** | IMG, FDI, IMD, TD0, TD0DECODE, XDF, HDM, DSK |
| **Amiga** | ADF, AMIGARAW |
| **Flux** | SCP, HFE, HFEV3, GWRAW, GWFLUX, KFS, FLUXDEC |
| **Emulator** | D88, DMK, 86F |
| **Tools** | UFTCLI, UFTBATCH, UFTPIPELINE, UFTREGTEST, UFTREPORT |

---

## 📁 Verzeichnisstruktur

```
UFT_MERGED/
├── include/uft/              # Core Headers
│   ├── uft_dd.h              # DD Module
│   ├── recovery_params.h     # GUI Parameters
│   ├── uft_iuniversaldrive.h # HAL (in libflux_core)
│   └── ...
├── libflux_core/             # Core Library (26.582 Zeilen)
│   ├── include/
│   │   ├── c64/              # C64 Protection Traits
│   │   ├── weak_bits.h
│   │   └── ...
│   └── src/
│       ├── c64/              # Trait Implementations
│       └── ...
├── libflux_format/           # Format Handlers (31.837 Zeilen)
│   ├── include/
│   │   ├── flux_format/      # All format headers
│   │   └── ...
│   └── src/
│       ├── autodetect/       # Format wrappers
│       └── ...
├── libflux_hw/               # Hardware Drivers (12.558 Zeilen)
│   ├── include/
│   └── src/
│       ├── fc5025/           # FC5025 native
│       ├── greaseweazle/
│       ├── kryoflux/
│       └── ...
├── src/                      # Qt GUI (8.085 Zeilen)
│   ├── hardware_providers/   # Qt Hardware Classes
│   ├── gui/                  # Widgets
│   └── ...
├── forms/                    # Qt UI Files
├── examples/                 # Example Code (7.115 Zeilen)
├── format_modules/           # 65 Format ZIPs
└── docs/                     # Documentation
```

---

## ✅ Empfehlungen zur Integration

### Sofort übernehmen:
1. **DD-Modul** - Essentiell für forensische Workflows
2. **C64 Protection Traits** - Einzigartig, nicht anderswo verfügbar
3. **IUniversalDrive HAL** - Architektur-kritisch
4. **Recovery Parameters** - GUI-Integration vorbereitet
5. **Weak Bits Detection** - Kopierschutz-Analyse

### Nach Review:
1. **Hardware Providers** - Qt-spezifisch, ggf. Anpassung nötig
2. **WOZ2** - Apple II spezifisch
3. **Format-Module ZIPs** - Selektiv entpacken

### Tests erforderlich:
- DD-Modul: Floppy Write auf echte Hardware
- C64 Traits: Bekannte geschützte Images
- HAL: Provider-Registrierung

---

## 📈 Statistik-Zusammenfassung

| Komponente | Dateien | Zeilen | Status |
|------------|---------|--------|--------|
| DD-Modul | 2 | 1.279 | ✅ Neu |
| C64 Protection | 38 | 3.500 | ✅ Neu |
| IUniversalDrive | 2 | 600 | ✅ Neu |
| Recovery Params | 2 | 700 | ✅ Neu |
| Hardware Providers | 22 | 2.000 | ✅ Neu |
| WOZ2 | 3 | 728 | ✅ Neu |
| Weak Bits | 2 | 646 | ✅ Neu |
| Format-Module | 65 ZIPs | ~30.000 | 📦 Archiviert |
| **TOTAL NEU** | | **~9.500** | |

---

*Report generiert: 2025-01-02*
