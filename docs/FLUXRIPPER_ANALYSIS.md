# FluxRipper Analysis Report für UnifiedFloppyTool

**Analysiert:** 2024-12-30
**Quelle:** FluxRipper-main.zip
**Fokus:** Algorithmen, Datenstrukturen, Ideen für UFT-Integration

---

## Executive Summary

FluxRipper ist ein FPGA-basiertes Floppy/HDD-Preservation-System mit extremem Fokus auf:
1. **Statistische Flux-Wiederherstellung (FluxStat)** - Besser als SpinRite's DynaStat
2. **Hardware-PLL im FPGA** - Digitale Signalverarbeitung auf Chip-Level
3. **Multi-Encoding-Support** - MFM, FM, GCR (Apple/CBM), M2FM, RLL
4. **Automatische Format-Erkennung** - Score-basiert mit Lock-Mechanismus

Das Projekt enthält **vollständige Verilog RTL-Module** die als Referenz für Software-Implementierungen dienen können.

---

## 1. FluxStat - Statistische Bit-Wiederherstellung

### Konzept (überlegen zu SpinRite DynaStat)

```
SpinRite DynaStat:       FluxStat:
────────────────────     ──────────────────────────
Multiple Sector Reads    Multiple Flux Captures
       ↓                        ↓
Bit-by-bit voting        Transition timing analysis
       ↓                        ↓
Best guess               Bit cell probability matrix
                               ↓
                         Per-bit confidence scoring
                               ↓
                         CRC-guided correction
```

### Vorteile von FluxStat:
- **Mehr Datenpunkte** - Rohe Flux-Übergänge, nicht nur dekodierte Bits
- **Timing-Information** - ±5ns Auflösung
- **Muster-Analyse** - Flux-Muster über mehrere Lesungen
- **Encoding-agnostisch** - Funktioniert mit jedem Format

### Datenstrukturen (C für UFT):

```c
// Multi-Pass Capture
typedef struct {
    uint32_t pass_count;           // Anzahl der Durchläufe (8-64)
    uint32_t flux_counts[64];      // Übergänge pro Durchlauf
    uint32_t *flux_times[64];      // Timestamp-Arrays
    uint32_t index_offset[64];     // Index-Puls Position
    uint32_t total_time[64];       // Gesamte Track-Zeit
} multipass_capture_t;

// Flux-Korrelation
typedef struct {
    uint64_t time_sum;             // Summe der Timestamps
    uint64_t time_sum_sq;          // Summe der Quadrate
    uint32_t hit_count;            // Treffer über Durchläufe
    uint32_t total_passes;         // Gesamte Durchläufe
} flux_correlation_t;

// Bit-Zell-Klassifikation
typedef enum {
    BITCELL_STRONG_1  = 0,
    BITCELL_WEAK_1    = 1,
    BITCELL_STRONG_0  = 2,
    BITCELL_WEAK_0    = 3,
    BITCELL_AMBIGUOUS = 4
} bitcell_class_t;

// Confidence-Levels
#define CONF_STRONG    90  // 90%+ = stark
#define CONF_WEAK      60  // 60-89% = schwach
#define CONF_AMBIGUOUS 50  // <60% = mehrdeutig
```

### Confidence-Berechnung (aus FluxRipper):

```c
uint8_t calculate_confidence(flux_correlation_t *corr) {
    float hit_ratio = (float)corr->hit_count / corr->total_passes;
    
    // Timing-Varianz berechnen
    float mean = (float)corr->time_sum / corr->hit_count;
    float variance = ((float)corr->time_sum_sq / corr->hit_count) - (mean * mean);
    float stddev = sqrtf(variance);
    
    // Confidence aus Hit-Ratio + Timing-Konsistenz
    float ratio_score = hit_ratio * 50.0f;           // 0-50 aus Hit-Ratio
    float timing_score = 50.0f * expf(-stddev / 50.0f);  // 0-50 aus Timing
    
    float confidence = ratio_score + timing_score;
    return (uint8_t)CLAMP(confidence, 0, 100);
}
```

---

## 2. MFM Decoder - Vollständige Implementation

### MFM Encoding-Regeln:
```
01 → Daten-Bit 1
00 → Daten-Bit 0 (vorheriges Bit war 1)
10 → Daten-Bit 0 (vorheriges Bit war 0)
11 → FEHLER (ungültige MFM-Sequenz)
```

### MFM Sync-Patterns:
```c
#define MFM_SYNC_A1  0x4489  // A1 mit fehlendem Clock (IDAM/DAM)
#define MFM_SYNC_C2  0x5224  // C2 mit fehlendem Clock (Index Mark)

// Address Mark Bytes (nach 3x A1 Sync)
#define AM_IDAM  0xFE  // ID Address Mark
#define AM_DAM   0xFB  // Data Address Mark
#define AM_DDAM  0xF8  // Deleted Data Address Mark
```

### LUT-basierter MFM-Decoder (für SIMD/Speed):

```c
// Extrahiere Daten-Bits aus MFM (Positionen 14,12,10,8,6,4,2,0)
static inline uint8_t mfm_decode_byte(uint16_t encoded) {
    uint8_t data = 0;
    data |= ((encoded >> 14) & 1) << 7;
    data |= ((encoded >> 12) & 1) << 6;
    data |= ((encoded >> 10) & 1) << 5;
    data |= ((encoded >> 8)  & 1) << 4;
    data |= ((encoded >> 6)  & 1) << 3;
    data |= ((encoded >> 4)  & 1) << 2;
    data |= ((encoded >> 2)  & 1) << 1;
    data |= ((encoded >> 0)  & 1) << 0;
    return data;
}

// Prüfe auf MFM-Fehler (11-Pattern)
static inline bool mfm_has_error(uint16_t encoded) {
    return ((encoded & 0xC000) == 0xC000) ||
           ((encoded & 0x3000) == 0x3000) ||
           ((encoded & 0x0C00) == 0x0C00) ||
           ((encoded & 0x0300) == 0x0300) ||
           ((encoded & 0x00C0) == 0x00C0) ||
           ((encoded & 0x0030) == 0x0030) ||
           ((encoded & 0x000C) == 0x000C) ||
           ((encoded & 0x0003) == 0x0003);
}
```

---

## 3. GCR Decoder - Apple II & Commodore

### Apple 6-bit GCR (DOS 3.3/ProDOS):

```c
// 6-bit → 8-bit Encoding Table
static const uint8_t gcr_apple6_encode[64] = {
    0x96, 0x97, 0x9A, 0x9B, 0x9D, 0x9E, 0x9F, 0xA6,
    0xA7, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xB2, 0xB3,
    0xB4, 0xB5, 0xB6, 0xB7, 0xB9, 0xBA, 0xBB, 0xBC,
    0xBD, 0xBE, 0xBF, 0xCB, 0xCD, 0xCE, 0xCF, 0xD3,
    0xD6, 0xD7, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE,
    0xDF, 0xE5, 0xE6, 0xE7, 0xE9, 0xEA, 0xEB, 0xEC,
    0xED, 0xEE, 0xEF, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6,
    0xF7, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF
};

// Apple II Sync/Prologue Markers
#define APPLE_ADDR_PROLOGUE  {0xD5, 0xAA, 0x96}  // Address field
#define APPLE_DATA_PROLOGUE  {0xD5, 0xAA, 0xAD}  // Data field
#define APPLE_EPILOGUE       {0xDE, 0xAA, 0xEB}  // Field end
```

### Commodore 4-bit GCR (C64/1541):

```c
// 4-bit → 5-bit Encoding Table
static const uint8_t gcr_cbm_encode[16] = {
    0x0A, 0x0B, 0x12, 0x13, 0x0E, 0x0F, 0x16, 0x17,
    0x09, 0x19, 0x1A, 0x1B, 0x0D, 0x1D, 0x1E, 0x15
};

// 5-bit → 4-bit Decode (mit Error-Flag in Bit 4)
static const uint8_t gcr_cbm_decode[32] = {
    0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,  // 0x00-0x07: Error
    0x10, 0x08, 0x00, 0x01, 0x10, 0x0C, 0x04, 0x05,  // 0x08-0x0F
    0x10, 0x10, 0x02, 0x03, 0x10, 0x0F, 0x06, 0x07,  // 0x10-0x17
    0x10, 0x09, 0x0A, 0x0B, 0x10, 0x0D, 0x0E, 0x10   // 0x18-0x1F
};
```

---

## 4. Encoding Auto-Detection

### Priority-basierter Detector:

```c
typedef enum {
    ENC_MFM       = 0,  // Standard PC Floppy
    ENC_FM        = 1,  // Legacy Single-Density
    ENC_GCR_CBM   = 2,  // Commodore 64/1541
    ENC_GCR_AP6   = 3,  // Apple II 6-bit (DOS 3.3)
    ENC_GCR_AP5   = 4,  // Apple II 5-bit (DOS 3.2)
    ENC_M2FM      = 5,  // DEC RX01/02, Intel MDS
    ENC_TANDY     = 6   // TRS-80 CoCo, Dragon
} encoding_type_t;

// Detection Priority (höchste zuerst):
// 1. GCR-Apple (D5 AA 96/AD) - Sehr distinktiv
// 2. GCR-CBM (10-byte sync von 0xFF) - Langer Sync-Run
// 3. M2FM (F77A pattern) - Einzigartig für DEC/Intel
// 4. Tandy FM - Anders als Standard-FM
// 5. MFM (A1 A1 A1 mit fehlendem Clock) - Am häufigsten
// 6. FM (fallback) - Einfaches Clock-Pattern

typedef struct {
    encoding_type_t detected;
    uint8_t consecutive_matches;
    uint8_t mismatch_count;
    bool locked;
    uint8_t match_count;
    uint8_t sync_history;  // Bit-Flags für gesehene Syncs
} encoding_detector_t;

#define LOCK_THRESHOLD   3   // Aufeinanderfolgende Matches zum Sperren
#define UNLOCK_THRESHOLD 10  // Mismatches zum Entsperren
```

---

## 5. Flux Histogram

### Echtzeit-Statistik:

```c
typedef struct {
    uint16_t bins[256];           // Histogram-Bins
    uint32_t total_count;         // Gesamte Flux-Übergänge
    uint16_t interval_min;        // Minimum-Intervall
    uint16_t interval_max;        // Maximum-Intervall
    uint8_t  peak_bin;            // Bin mit höchster Anzahl
    uint16_t peak_count;          // Anzahl im Peak-Bin
    uint16_t mean_interval;       // EMA des Intervalls
    uint32_t overflow_count;      // Überläufe
} flux_histogram_t;

// EMA-Update (alpha = 1/16)
void flux_histogram_update(flux_histogram_t *h, uint16_t interval) {
    h->total_count++;
    
    // Min/Max
    if (interval < h->interval_min) h->interval_min = interval;
    if (interval > h->interval_max) h->interval_max = interval;
    
    // Bin-Index (mit Shift für Auflösung)
    uint8_t bin = (interval >> 2);  // BIN_SHIFT = 2
    if (bin >= 255) {
        h->overflow_count++;
        bin = 255;
    }
    
    // Inkrement (saturierend)
    if (h->bins[bin] < 0xFFFF) {
        h->bins[bin]++;
        if (h->bins[bin] > h->peak_count) {
            h->peak_bin = bin;
            h->peak_count = h->bins[bin];
        }
    }
    
    // EMA: new_mean = mean + (interval - mean) / 16
    h->mean_interval = h->mean_interval - (h->mean_interval >> 4) + (interval >> 4);
}
```

---

## 6. Digital PLL Algorithmus

### Software-PLL für Bit-Clock Recovery:

```c
typedef struct {
    uint32_t phase_accum;      // NCO Phasen-Akkumulator
    uint16_t frequency;        // Basis-Frequenz
    int16_t  phase_error;      // Letzter Phasen-Fehler
    int32_t  integrator;       // Loop-Filter Integrator
    uint8_t  lock_count;       // Lock-Zähler
    bool     locked;           // PLL gesperrt?
    
    // Koeffizienten
    uint8_t  kp;               // Proportional-Gain
    uint8_t  ki;               // Integral-Gain
} digital_pll_t;

// PLL-Konstanten für verschiedene Datenraten
static const uint16_t PLL_FREQ_250K = 0x0666;  // 250 Kbps @ 200MHz
static const uint16_t PLL_FREQ_300K = 0x07AE;  // 300 Kbps @ 200MHz
static const uint16_t PLL_FREQ_500K = 0x0CCC;  // 500 Kbps @ 200MHz

void pll_process_edge(digital_pll_t *pll, uint32_t edge_time) {
    // Phasen-Fehler berechnen
    int16_t phase_error = (int16_t)(edge_time - (pll->phase_accum >> 16));
    
    // Loop-Filter (PI-Regler)
    int32_t prop = (int32_t)phase_error * pll->kp;
    pll->integrator += (int32_t)phase_error * pll->ki;
    
    // Frequenz-Korrektur
    int32_t freq_adj = (prop + pll->integrator) >> 8;
    pll->phase_accum += pll->frequency + freq_adj;
    
    // Lock-Detection
    if (abs(phase_error) < 100) {
        if (pll->lock_count < 255) pll->lock_count++;
        if (pll->lock_count > 32) pll->locked = true;
    } else {
        if (pll->lock_count > 0) pll->lock_count--;
        if (pll->lock_count < 8) pll->locked = false;
    }
    
    pll->phase_error = phase_error;
}
```

---

## 7. Native Flux-Format (.flux)

### Header-Struktur:

```c
#pragma pack(push, 1)
typedef struct {
    char     magic[4];           // "FLUX"
    uint16_t version;            // 0x0100 = v1.0
    uint16_t flags;
    uint16_t track_count;
    uint8_t  head_count;
    uint32_t sample_rate;        // Hz
    uint64_t creation_time;      // Unix epoch
    uint8_t  drive_fingerprint[16];  // MD5
    uint8_t  reserved[25];
} flux_header_t;

typedef struct {
    uint32_t offset;             // Vom Datei-Anfang
    uint32_t length;             // Komprimierte Größe
} flux_track_index_t;

typedef struct {
    uint8_t  revolution_count;
    uint8_t  quality_score;      // 0-100
    uint8_t  flags;
    uint8_t  encoding_detected;
} flux_track_header_t;
#pragma pack(pop)
```

### Delta-Encoding für Flux-Timing:

```c
// Variable-Length Encoding:
// 0xxxxxxx         = 0-127 (1 Byte)
// 10xxxxxx xxxxxxxx = 128-16383 (2 Bytes)
// 110xxxxx xxxxxxxx xxxxxxxx = 16384-2097151 (3 Bytes)
// 111xxxxx + 4-byte = 2097152+ (5 Bytes)

size_t encode_flux_delta(uint32_t delta, uint8_t *out) {
    if (delta < 128) {
        out[0] = (uint8_t)delta;
        return 1;
    } else if (delta < 16384) {
        out[0] = 0x80 | ((delta >> 8) & 0x3F);
        out[1] = delta & 0xFF;
        return 2;
    } else if (delta < 2097152) {
        out[0] = 0xC0 | ((delta >> 16) & 0x1F);
        out[1] = (delta >> 8) & 0xFF;
        out[2] = delta & 0xFF;
        return 3;
    } else {
        out[0] = 0xE0 | ((delta >> 24) & 0x1F);
        out[1] = (delta >> 24) & 0xFF;
        out[2] = (delta >> 16) & 0xFF;
        out[3] = (delta >> 8) & 0xFF;
        out[4] = delta & 0xFF;
        return 5;
    }
}
```

---

## 8. Image-Format-Support Matrix

### Priority 0 (Must Have):
| Format | Extension | Read | Write |
|--------|-----------|:----:|:-----:|
| Raw Sector | .img/.ima | ✅ | ✅ |
| KryoFlux | .raw | ✅ | ✅ |
| SuperCard Pro | .scp | ✅ | ✅ |
| IPF | .ipf | ✅ | ❌ |
| ADF | .adf | ✅ | ✅ |
| D64/G64 | .d64/.g64 | ✅ | ✅ |

### Priority 1 (Should Have):
| Format | Extension | Read | Write |
|--------|-----------|:----:|:-----:|
| HFE | .hfe | ✅ | ✅ |
| WOZ | .woz | ✅ | ✅ |
| DSK | .dsk | ✅ | ✅ |
| ST/MSA | .st/.msa | ✅ | ✅ |

---

## 9. Empfohlene UFT-Integrationen

### Sofort implementierbar:

1. **FluxStat Multi-Pass Capture**
   - Mehrere Lesungen pro Track
   - Statistische Bit-Analyse
   - Confidence-Scoring

2. **Flux Histogram**
   - Echtzeit-Statistik während Capture
   - Automatische Datenraten-Erkennung
   - Qualitäts-Bewertung

3. **Encoding Auto-Detection**
   - Score-basierter Detector
   - Lock-Mechanismus
   - Priority-Queue

4. **GCR Decoder (Apple/CBM)**
   - LUT-basiert für Speed
   - Sync-Detection
   - Error-Recovery

### Mittelfristig:

5. **Native .flux Format**
   - Delta-Encoding
   - Vollständige Metadaten
   - Random-Access

6. **Digital PLL**
   - Software-PLL für Clock-Recovery
   - Adaptive Bandbreite
   - Lock-Detection

### Langfristig:

7. **CRC-Guided Correction**
   - Brute-Force für <8 uncertain Bits
   - Kombinatorische Suche

8. **Weak-Bit Preservation**
   - Copy-Protection-Analyse
   - Timing-Anomalie-Detection

---

## 10. Code-Referenzen im FluxRipper-Projekt

| Feature | Datei | Relevanz |
|---------|-------|----------|
| MFM Decoder | `rtl/encoding/mfm_decoder.v` | **HOCH** |
| GCR Apple | `rtl/encoding/gcr_apple.v` | **HOCH** |
| GCR CBM | `rtl/encoding/gcr_cbm.v` | **HOCH** |
| Digital PLL | `rtl/data_separator/digital_pll.v` | **HOCH** |
| Encoding Detector | `rtl/encoding/encoding_detector.v` | **HOCH** |
| Flux Histogram | `rtl/recovery/flux_histogram.v` | **HOCH** |
| Multi-Pass Capture | `rtl/recovery/multipass_capture.v` | **HOCH** |
| FluxStat HAL | `soc/firmware/src/fluxstat_hal.c` | **HOCH** |
| FluxStat Doc | `docs/statistical-flux-recovery.md` | **HOCH** |
| Image Formats | `docs/image-formats.md` | MITTEL |

---

## Zusammenfassung

FluxRipper bietet eine Goldgrube an Algorithmen und Ideen:

1. **FluxStat übertrifft SpinRite** durch Zugang zu rohen Flux-Daten
2. **Vollständige Encoder/Decoder** für alle gängigen Formate
3. **Score-basierte Auto-Detection** ist robust und erweiterbar
4. **Flux Histogram** ermöglicht Echtzeit-Qualitätsanalyse
5. **Native .flux Format** ist modern und effizient

Diese Algorithmen können direkt in UFT's C-Codebase portiert werden!
