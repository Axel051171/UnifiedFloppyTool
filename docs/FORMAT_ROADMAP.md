# 💾 UFT FORMAT & PERFORMANCE ROADMAP

## 📊 AKTUELLE IMPLEMENTIERUNG

| Format | Status | Read | Write | Create | Flux |
|--------|--------|------|-------|--------|------|
| ADF    | ✅ 100% | ✅ | ✅ | ✅ | ❌ |
| IMG    | ✅ 100% | ✅ | ✅ | ✅ | ❌ |
| SCP    | ⚠️ 60%  | 🔶 | ❌ | ❌ | ✅ |

---

## 🎯 FORMATE ZUM HINZUFÜGEN (PRIORISIERT)

### TIER 1 - KRITISCH (Sprint 2-3)

| Format | Typ | Komplexität | Beschreibung |
|--------|-----|-------------|--------------|
| **D64** | Sektor | 🟡 Mittel | C64/1541 - GCR, variable Sektoren/Track |
| **G64** | GCR-Raw | 🟠 Hoch | C64 GCR mit Timing - Kopierschutz! |
| **HFE** | Flux | 🟡 Mittel | HxC Emulator - weit verbreitet |
| **STX** | Flux+Meta | 🔴 Sehr hoch | Atari ST Pasti - Kopierschutz |

### TIER 2 - WICHTIG (Sprint 4-5)

| Format | Typ | Komplexität | Beschreibung |
|--------|-----|-------------|--------------|
| **IPF** | Flux+Meta | 🔴 Sehr hoch | CAPS/SPS - Museum-Standard |
| **WOZ** | Flux | 🟡 Mittel | Apple II - modern, gut dokumentiert |
| **A2R** | Flux | 🟡 Mittel | Applesauce - Apple Preservation |
| **NIB** | Nibble | 🟢 Einfach | Apple Nibble - Legacy |
| **IMD** | Sektor+Meta | 🟡 Mittel | ImageDisk - mit Analyse-Daten |

### TIER 3 - NICE-TO-HAVE (Sprint 6+)

| Format | Typ | Komplexität | Beschreibung |
|--------|-----|-------------|--------------|
| **KFX** | Flux | 🟠 Hoch | Kryoflux Stream Files |
| **TD0** | Sektor+Kompr | 🟡 Mittel | Teledisk - LZW komprimiert |
| **DMK** | Track | 🟡 Mittel | TRS-80 Track Image |
| **FDI** | Sektor+Meta | 🟢 Einfach | Formatted Disk Image |
| **MSA** | Sektor+Kompr | 🟢 Einfach | Atari ST komprimiert |
| **ADZ** | Sektor+Kompr | 🟢 Einfach | ADF + gzip |
| **DMS** | Sektor+Kompr | 🟠 Hoch | Amiga DiskMasher - custom LZ |

---

## 🔧 FORMAT-DETAILS

### D64 (Commodore 64)
```
┌─────────────────────────────────────────────────────┐
│ VARIABLE SEKTOREN PRO TRACK!                        │
├─────────────────────────────────────────────────────┤
│ Tracks 1-17:   21 Sektoren (Außen, schneller)      │
│ Tracks 18-24:  19 Sektoren                          │
│ Tracks 25-30:  18 Sektoren                          │
│ Tracks 31-35:  17 Sektoren (Innen, langsamer)      │
├─────────────────────────────────────────────────────┤
│ Encoding: GCR (Group Code Recording)                │
│ Sektor-Size: 256 Bytes                              │
│ Total: 683 Sektoren = 174,848 Bytes                │
│ Mit Error-Info: 175,531 Bytes (+683 für Fehler)    │
└─────────────────────────────────────────────────────┘
```

**Implementierung:**
```c
// Sektoren pro Track für 1541
static const uint8_t d64_sectors_per_track[35] = {
    21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,  // 1-17
    19,19,19,19,19,19,19,                                  // 18-24
    18,18,18,18,18,18,                                     // 25-30
    17,17,17,17,17                                         // 31-35
};
```

### G64 (GCR Raw)
```
┌─────────────────────────────────────────────────────┐
│ ECHTE GCR-DATEN MIT TIMING!                         │
├─────────────────────────────────────────────────────┤
│ Signatur: "GCR-1541" (8 Bytes)                     │
│ Version: 0                                          │
│ Track Count: 84 (max)                               │
│ Max Track Size: variabel                            │
├─────────────────────────────────────────────────────┤
│ Pro Track:                                          │
│   - Offset Table (4 Bytes × Tracks)                │
│   - Speed Zone Table (4 Bytes × Tracks)            │
│   - Raw GCR Data (bis 7928 Bytes)                  │
└─────────────────────────────────────────────────────┘
```

### HFE (HxC Floppy Emulator)
```
┌─────────────────────────────────────────────────────┐
│ UNIVERSELLES EMULATOR-FORMAT                        │
├─────────────────────────────────────────────────────┤
│ Header: 512 Bytes                                   │
│   - Signatur: "HXCPICFE"                           │
│   - Revision, Tracks, Sides, Encoding              │
│   - Bitrate, RPM, Interface Mode                   │
├─────────────────────────────────────────────────────┤
│ Track LUT: Offset + Length für jeden Track         │
│ Track Data: MFM/FM Bitstream, 256-byte aligned     │
│ Interleaved: Side 0 Block, Side 1 Block, ...       │
└─────────────────────────────────────────────────────┘

Encoding Types:
  0 = IBM MFM DD/HD
  1 = Amiga MFM
  2 = IBM FM SD
  3 = EMU FM
  7 = Apple II (GCR 6&2)
```

### IPF (Interchangeable Preservation Format)
```
┌─────────────────────────────────────────────────────┐
│ MUSEUM-GRADE PRESERVATION                           │
├─────────────────────────────────────────────────────┤
│ Chunks: INFO, IMGE, DATA                           │
│ Speichert:                                          │
│   - Exakte Timing-Informationen                    │
│   - Weak Bits / Fuzzy Bits                         │
│   - Kopierschutz-Schemata                          │
│   - Multiple Revolutions                           │
├─────────────────────────────────────────────────────┤
│ CAPS Library erforderlich (closed-source Teil)     │
│ Open-Source: nur Lesen, kein Schreiben             │
└─────────────────────────────────────────────────────┘
```

---

## ⚡ PERFORMANCE-OPTIMIERUNGEN

### 1. BUILD-FLAGS (CMake)

```cmake
# Bereits vorhanden, aber erweitern:
option(UFT_ENABLE_SIMD      "Enable SIMD optimizations"    ON)
option(UFT_ENABLE_OPENMP    "Enable OpenMP parallelization" ON)
option(UFT_ENABLE_MMAP      "Enable memory-mapped I/O"     ON)
option(UFT_ENABLE_LTO       "Enable Link-Time Optimization" OFF)
option(UFT_ENABLE_PGO       "Enable Profile-Guided Opt"    OFF)

# NEU HINZUFÜGEN:
option(UFT_ENABLE_PREFETCH  "Enable data prefetching"      ON)
option(UFT_ENABLE_CACHE     "Enable track/sector caching"  ON)
option(UFT_CACHE_SIZE_MB    "Cache size in MB"             16)
option(UFT_ASYNC_IO         "Enable async I/O operations"  OFF)
option(UFT_BATCH_SIZE       "Batch size for bulk ops"      16)
```

### 2. SIMD FÜR MFM/GCR DEKODIERUNG

```c
// SSE2 MFM Dekodierung (8 Bytes parallel)
#ifdef UFT_SIMD_SSE2
static inline uint64_t mfm_decode_sse2(const uint8_t* src) {
    __m128i data = _mm_loadu_si128((const __m128i*)src);
    __m128i mask = _mm_set1_epi8(0x55);  // Clock bits
    __m128i clock = _mm_and_si128(data, mask);
    __m128i databits = _mm_andnot_si128(mask, data);
    // ... weitere Verarbeitung
    return _mm_extract_epi64(result, 0);
}
#endif

// AVX2 MFM Dekodierung (32 Bytes parallel!)
#ifdef UFT_SIMD_AVX2
static inline void mfm_decode_avx2(const uint8_t* src, uint8_t* dst, size_t len) {
    for (size_t i = 0; i < len; i += 32) {
        __m256i data = _mm256_loadu_si256((const __m256i*)(src + i));
        // ... 4x schneller als Byte-weise
    }
}
#endif
```

**Erwartete Speedups:**
| Operation | Scalar | SSE2 | AVX2 |
|-----------|--------|------|------|
| MFM Decode | 1x | 4x | 8x |
| GCR Decode | 1x | 3x | 6x |
| CRC16 | 1x | 2x | 4x |
| Bit-Suche | 1x | 8x | 16x |

### 3. MEMORY-MAPPED I/O

```c
typedef struct {
    void*   base;        // mmap base pointer
    size_t  size;        // file size
    int     fd;          // file descriptor
    bool    writable;    // MAP_SHARED vs MAP_PRIVATE
} uft_mmap_t;

// Vorteile:
// - Kein Kopieren: Kernel liefert direkt
// - Lazy Loading: Nur genutzte Pages geladen
// - OS Caching: Automatisch optimiert
// - Prefetching: madvise(MADV_SEQUENTIAL)
```

### 4. TRACK CACHE

```c
typedef struct {
    uft_track_t** tracks;       // LRU Cache
    size_t        capacity;     // Max Einträge
    size_t        count;        // Aktuelle Einträge
    size_t        hits;         // Cache Hits
    size_t        misses;       // Cache Misses
    pthread_mutex_t lock;       // Thread-Safety
} uft_track_cache_t;

// Cache-Strategie:
// - LRU (Least Recently Used) Eviction
// - Dirty-Flag für Write-Back
// - Prefetch nächster Track bei Sequential Access
```

### 5. ASYNC I/O (für GUI)

```c
typedef struct {
    uft_disk_t*     disk;
    int             cylinder;
    int             head;
    uft_track_t*    result;
    uft_error_t     error;
    bool            complete;
    pthread_cond_t  cond;
} uft_async_read_t;

// Non-blocking Track-Read:
uft_async_read_t* uft_track_read_async(uft_disk_t* disk, int cyl, int head);
bool uft_async_is_complete(uft_async_read_t* req);
uft_track_t* uft_async_get_result(uft_async_read_t* req);
```

### 6. BATCH OPERATIONS

```c
// Statt:
for (int t = 0; t < 160; t++) {
    track = uft_track_read(disk, t/2, t%2, NULL);  // 160 syscalls!
}

// Besser:
uft_track_t* tracks[160];
uft_error_t uft_tracks_read_batch(
    uft_disk_t* disk,
    int start_track,
    int count,
    uft_track_t** out_tracks  // Array für Ergebnisse
);
// 1 syscall + mmap = viel schneller!
```

### 7. PARALLELISIERUNG (OpenMP)

```c
// Analyse parallelisieren:
#pragma omp parallel for collapse(2) reduction(+:crc_errors)
for (int cyl = 0; cyl < geo.cylinders; cyl++) {
    for (int head = 0; head < geo.heads; head++) {
        uft_track_t* track = uft_track_read(disk, cyl, head, NULL);
        if (track) {
            for (size_t s = 0; s < track->sector_count; s++) {
                if (track->sectors[s].status & UFT_SECTOR_CRC_ERROR) {
                    crc_errors++;
                }
            }
            uft_track_free(track);
        }
    }
}
```

---

## 🔬 DECODER-OPTIMIERUNGEN

### PLL (Phase-Locked Loop)

```c
typedef struct {
    double  phase;           // Aktuelle Phase
    double  freq;            // Aktuelle Frequenz
    double  nominal_freq;    // Soll-Frequenz
    double  phase_adj;       // Phase Adjustment Rate (0.01-0.1)
    double  freq_adj;        // Frequency Adjustment Rate (0.001-0.01)
    int     window_bits;     // Bits für Fenster-Averaging
} uft_pll_config_t;

// Aggressive vs. Conservative PLL:
// - Aggressive: Schnelles Einrasten, aber instabil bei Jitter
// - Conservative: Langsam, aber stabil bei beschädigten Medien
```

### Lookup-Tables für GCR

```c
// GCR 5-zu-4 Decode Table (C64)
static const uint8_t gcr_decode_5to4[32] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // 00-07 invalid
    0xFF, 0x08, 0x00, 0x01, 0xFF, 0x0C, 0x04, 0x05,  // 08-0F
    0xFF, 0xFF, 0x02, 0x03, 0xFF, 0x0F, 0x06, 0x07,  // 10-17
    0xFF, 0x09, 0x0A, 0x0B, 0xFF, 0x0D, 0x0E, 0xFF   // 18-1F
};

// 10-Bit Lookup für schnellere Dekodierung:
static uint8_t gcr_decode_10bit[1024];  // 2 Nibbles auf einmal
```

---

## 📈 BENCHMARKS (ZIELE)

| Operation | Aktuell | Ziel (SIMD) | Ziel (mmap) |
|-----------|---------|-------------|-------------|
| ADF Read (880KB) | 15ms | 8ms | 3ms |
| SCP Decode (1 Track) | 50ms | 12ms | 10ms |
| Full Disk Analyze | 2.5s | 800ms | 400ms |
| Convert ADF→IMG | 200ms | 100ms | 50ms |
| CRC32 (1MB) | 5ms | 0.8ms | 0.6ms |

---

## 🗓️ IMPLEMENTATION SCHEDULE

### Sprint 2 (Diese Woche)
- [ ] D64 Format Plugin (Read/Write)
- [ ] SCP Plugin fertigstellen (wrap existing code)
- [ ] MFM Decoder Plugin
- [ ] Track Cache Implementation

### Sprint 3
- [ ] G64 Format Plugin
- [ ] GCR Decoder Plugin
- [ ] SIMD MFM Decode (SSE2)
- [ ] Memory-mapped I/O

### Sprint 4
- [ ] HFE Format Plugin
- [ ] SIMD GCR Decode
- [ ] Async I/O Framework
- [ ] Batch Operations

### Sprint 5
- [ ] IPF Reader (mit CAPS lib)
- [ ] WOZ/A2R Format Plugins
- [ ] AVX2 Optimierungen
- [ ] OpenMP Parallelisierung

---

## 🏗️ NEUE HEADER-STRUKTUREN

```c
// include/uft/uft_simd.h
#ifndef UFT_SIMD_H
#define UFT_SIMD_H

// Runtime CPU Feature Detection
typedef struct {
    bool has_sse2;
    bool has_sse41;
    bool has_avx2;
    bool has_avx512;
    bool has_neon;      // ARM
} uft_cpu_features_t;

void uft_detect_cpu_features(uft_cpu_features_t* features);

// SIMD-optimierte Funktionen
size_t uft_mfm_decode_fast(const uint8_t* src, uint8_t* dst, size_t len);
size_t uft_gcr_decode_fast(const uint8_t* src, uint8_t* dst, size_t len);
uint32_t uft_crc32_fast(const uint8_t* data, size_t len);

#endif
```

```c
// include/uft/uft_cache.h
#ifndef UFT_CACHE_H
#define UFT_CACHE_H

typedef struct uft_cache uft_cache_t;

uft_cache_t* uft_cache_create(size_t max_entries, size_t max_memory);
void uft_cache_destroy(uft_cache_t* cache);

uft_track_t* uft_cache_get(uft_cache_t* cache, int cyl, int head);
void uft_cache_put(uft_cache_t* cache, int cyl, int head, uft_track_t* track);
void uft_cache_invalidate(uft_cache_t* cache, int cyl, int head);
void uft_cache_flush(uft_cache_t* cache);

typedef struct {
    size_t hits;
    size_t misses;
    size_t evictions;
    size_t memory_used;
} uft_cache_stats_t;

void uft_cache_get_stats(uft_cache_t* cache, uft_cache_stats_t* stats);

#endif
```

---

*Erstellt: 2025-01-01 | Elite QA & Floppy-Disk Superexperte*

---

## 📋 STATUS UPDATE (Sprint 2 abgeschlossen)

### ✅ FERTIG

| Komponente | Zeilen | Status |
|------------|--------|--------|
| **D64 Plugin** | 810 | ✅ Variable Sektoren, BAM, Read/Write/Create |
| **Track-Cache** | 573 | ✅ LRU, O(1) Lookup, Stats |
| **SCP Plugin** | 628 | ✅ Registriert, Flux-Read |
| **MFM Decoder** | 474 | ✅ PLL, CRC, Sektor-Parsing |

### 🔜 NÄCHSTE SCHRITTE

5. **SIMD Framework** - SSE2/AVX2 für MFM/GCR Decode
6. **G64 Plugin** - Commodore GCR-Raw
7. **HFE Plugin** - HxC Emulator Format

---
*Update: 2025-01-01 | Sprint 2 Complete*
