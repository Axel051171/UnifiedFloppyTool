# UFT-IR Format Specification v1.0

## Overview

The **UFT Intermediate Representation (UFT-IR)** format is the canonical hub format for raw track and flux data within the UnifiedFloppyTool ecosystem. It serves as the central interchange format between:

- **Hardware controllers**: Greaseweazle, FluxEngine, KryoFlux, FC5025, XUM1541, SuperCard Pro
- **Disk image formats**: HFE, WOZ, SCP, IPF, G64, NIB, DMK, etc.
- **Analysis pipelines**: Unified Decoder, format analyzers, protection detectors
- **Archive/cache storage**: Persistent caching of decoded track data

```
                    ┌─────────────────────────────────────────┐
                    │            UFT-IR HUB FORMAT            │
                    │                                         │
   ┌────────────┐   │   ┌─────────────────────────────┐      │   ┌────────────┐
   │Greaseweazle├───┼──►│     uft_ir_disk_t          │◄─────┼───┤ HFE Format │
   └────────────┘   │   │                             │      │   └────────────┘
   ┌────────────┐   │   │  ┌──────────────────────┐  │      │   ┌────────────┐
   │ FluxEngine ├───┼──►│  │   uft_ir_track_t    │  │◄─────┼───┤ WOZ Format │
   └────────────┘   │   │  │                      │  │      │   └────────────┘
   ┌────────────┐   │   │  │ ┌────────────────┐  │  │      │   ┌────────────┐
   │  KryoFlux  ├───┼──►│  │ │ uft_ir_rev_t  │  │  │◄─────┼───┤ SCP Format │
   └────────────┘   │   │  │ │  flux deltas   │  │  │      │   └────────────┘
   ┌────────────┐   │   │  │ │  confidence    │  │  │      │   ┌────────────┐
   │   FC5025   ├───┼──►│  │ │  statistics    │  │  │◄─────┼───┤ IPF Format │
   └────────────┘   │   │  │ └────────────────┘  │  │      │   └────────────┘
   ┌────────────┐   │   │  └──────────────────────┘  │      │
   │  XUM1541   ├───┼──►│                             │      │
   └────────────┘   │   └─────────────────────────────┘      │
                    │                                         │
                    │         ▼           ▼           ▼      │
                    │    ┌─────────┐ ┌─────────┐ ┌─────────┐ │
                    │    │ Decoder │ │ Analyzer│ │ Archive │ │
                    │    └─────────┘ └─────────┘ └─────────┘ │
                    └─────────────────────────────────────────┘
```

## Design Goals

1. **Lossless Preservation**: Store all flux timing data without loss
2. **Multi-Revolution Support**: Up to 16 revolutions per track for weak-bit detection and fusion
3. **Comprehensive Metadata**: Timing, quality metrics, forensic data
4. **Efficient Serialization**: Support for compression and incremental I/O
5. **Platform Independence**: Little-endian, packed structures, no alignment issues

---

## File Format Structure

### File Magic & Header

```
Offset  Size  Field              Description
──────────────────────────────────────────────────────────────
0x00    8     magic              "UFTIR\x00\x01\x00" (8 bytes)
0x08    4     version            0x00010000 (v1.0.0)
0x0C    4     header_size        Total header size
0x10    4     flags              File flags
0x14    1     compression        Compression type (0=none, 1=zlib, 2=lz4, 3=zstd)
0x15    3     reserved           Padding
0x18    8     uncompressed_size  Total uncompressed data size
0x20    8     compressed_size    Compressed data size (if applicable)
0x28    4     track_count        Number of tracks
0x2C    4     crc32              Header CRC32
```

### Geometry Block

```
Offset  Size  Field              Description
──────────────────────────────────────────────────────────────
0x00    1     cylinders          Total cylinders (1-84)
0x01    1     heads              Number of heads (1-2)
0x02    1     sectors_per_track  Sectors per track (if uniform)
0x03    1     sector_size_shift  Sector size = 128 << shift
0x04    4     total_sectors      Total sectors on disk
0x08    4     rpm                Drive RPM
0x0C    4     data_rate_kbps     Data rate in kbit/s
0x10    1     density            0=SD, 1=DD, 2=HD, 3=ED
0x11    1     interleave         Sector interleave
0x12    1     track_skew         Track-to-track skew
0x13    1     head_skew          Head-to-head skew
```

### Track Header

```
Offset  Size  Field              Description
──────────────────────────────────────────────────────────────
0x00    1     cylinder           Physical cylinder (0-83)
0x01    1     head               Head/side (0-1)
0x02    2     flags              Track flags (bitfield)
0x04    4     data_offset        Offset to track data
0x08    4     data_size          Size of track data (compressed)
0x0C    4     uncompressed_size  Uncompressed track size
0x10    1     revolution_count   Number of revolutions
0x11    1     encoding           Encoding type
0x12    1     quality            Quality assessment
0x13    1     reserved           Padding
0x14    4     crc32              Track data CRC32
```

---

## Data Types

### Encoding Types

| Value | Name             | Description                      |
|-------|------------------|----------------------------------|
| 0     | UNKNOWN          | Not detected/unknown             |
| 1     | FM               | FM (Single Density)              |
| 2     | MFM              | MFM (Double Density)             |
| 3     | M2FM             | Modified MFM                     |
| 4     | GCR_COMMODORE    | GCR Commodore (C64/1541)         |
| 5     | GCR_APPLE        | GCR Apple II 5.25"               |
| 6     | GCR_APPLE_35     | GCR Apple 3.5"                   |
| 7     | GCR_VICTOR       | GCR Victor 9000                  |
| 8     | AMIGA_MFM        | Amiga-style MFM                  |
| 9     | RLL              | RLL encoding                     |
| 10    | MIXED            | Mixed encodings                  |
| 255   | CUSTOM           | Custom/proprietary               |

### Quality Levels

| Value | Name       | Description                              |
|-------|------------|------------------------------------------|
| 0     | UNKNOWN    | Not assessed                             |
| 1     | PERFECT    | All sectors OK, no errors                |
| 2     | GOOD       | Minor issues, all data recovered         |
| 3     | DEGRADED   | Some sectors with corrections            |
| 4     | MARGINAL   | Significant issues, data uncertain       |
| 5     | BAD        | Major errors, incomplete recovery        |
| 6     | UNREADABLE | Cannot decode track                      |
| 7     | EMPTY      | No flux detected (unformatted)           |
| 8     | PROTECTED  | Copy protection detected                 |

### Track Flags

| Bit  | Name             | Description                          |
|------|------------------|--------------------------------------|
| 0    | INDEXED          | Index-aligned revolutions            |
| 1    | WEAK_BITS        | Contains weak/random bits            |
| 2    | PROTECTED        | Copy protection detected             |
| 3    | LONG_TRACK       | Longer than standard                 |
| 4    | SHORT_TRACK      | Shorter than standard                |
| 5    | DENSITY_VARIED   | Variable density zones               |
| 6    | HALF_TRACK       | Half-track position                  |
| 7    | QUARTER_TRACK    | Quarter-track position               |
| 8    | MULTI_REV_FUSED  | Multi-revolution fusion applied      |
| 9    | CRC_CORRECTED    | CRC corrections applied              |
| 10   | SYNTHESIZED      | Partially synthesized data           |
| 11   | INCOMPLETE       | Incomplete read                      |
| 12   | VERIFIED         | Verified against source              |

### Hardware Sources

| Value | Name           | Description                  |
|-------|----------------|------------------------------|
| 0     | UNKNOWN        | Unknown source               |
| 1     | GREASEWEAZLE   | Greaseweazle                 |
| 2     | FLUXENGINE     | FluxEngine                   |
| 3     | KRYOFLUX       | KryoFlux                     |
| 4     | FC5025         | FC5025                       |
| 5     | XUM1541        | XUM1541                      |
| 6     | SUPERCARD_PRO  | SuperCard Pro                |
| 7     | PAULINE        | Pauline                      |
| 8     | APPLESAUCE     | Applesauce                   |
| 100   | CONVERTED      | Converted from image file    |
| 101   | SYNTHETIC      | Synthetically generated      |
| 102   | EMULATOR       | From emulator                |

---

## Timing & Resolution

### Clock Reference

All timing values in UFT-IR are stored in **nanoseconds** by default. This provides sufficient resolution for all known floppy disk formats:

| Format        | Clock Period | UFT-IR Resolution |
|---------------|--------------|-------------------|
| FM (SD)       | ~4000 ns     | ±1 ns             |
| MFM (DD)      | ~2000 ns     | ±1 ns             |
| MFM (HD)      | ~1000 ns     | ±1 ns             |
| GCR (C64)     | ~3250 ns     | ±1 ns             |
| Victor 9000   | ~1667 ns     | ±1 ns             |

### Tick Conversion

When importing from hardware with different tick rates:

```c
// Greaseweazle F7: 72 MHz clock
uint32_t ns = (ticks * 1000000000ULL) / 72000000;

// KryoFlux: 24.027428 MHz (SCK)
uint32_t ns = (ticks * 1000000000ULL) / 24027428;

// FluxEngine: Variable, typically 12 MHz
uint32_t ns = (ticks * 1000000000ULL) / 12000000;
```

---

## Multi-Revolution Data

UFT-IR supports up to 16 revolutions per track, enabling:

1. **Weak Bit Detection**: Bits that vary between reads indicate analog weakness
2. **Fusion**: Combine best data from multiple revolutions
3. **Statistical Analysis**: Timing variance, signal quality metrics
4. **Copy Protection Analysis**: Some protections rely on weak bits

### Revolution Structure

Each revolution contains:

```c
typedef struct uft_ir_revolution {
    uint32_t    rev_index;           // Revolution number (0-based)
    uint32_t    flags;               // Revolution flags
    uint32_t    duration_ns;         // Total revolution time
    uint32_t    index_offset_ns;     // Offset from index pulse
    
    uft_ir_data_type_t data_type;    // Type of stored data
    uint32_t    flux_count;          // Number of flux transitions
    uint32_t    data_size;           // Size in bytes
    uint32_t*   flux_deltas;         // Flux delta times (ns)
    
    uint8_t*    flux_confidence;     // Optional per-transition confidence
    
    uft_ir_flux_stats_t stats;       // Computed statistics
    uint8_t     quality_score;       // Quality (0-100)
} uft_ir_revolution_t;
```

---

## Statistics & Analysis

### Flux Statistics

```c
typedef struct uft_ir_flux_stats {
    uint32_t    total_transitions;   // Total flux count
    uint32_t    min_delta_ns;        // Minimum delta
    uint32_t    max_delta_ns;        // Maximum delta
    uint32_t    mean_delta_ns;       // Mean delta
    uint32_t    stddev_delta_ns;     // Standard deviation
    uint32_t    clock_period_ns;     // Detected clock
    uint32_t    index_to_index_ns;   // Revolution time
    uint16_t    histogram_1us[64];   // 1µs histogram (0-63µs)
} uft_ir_flux_stats_t;
```

### Quality Scoring Algorithm

```
Quality Score = 100
             - (missing_sectors × 10)
             - (crc_errors × 15)
             - (weak_bits_flag × 5)
             - (incomplete_flag × 20)
             + (multi_rev_fusion × 5)
```

---

## Copy Protection Support

UFT-IR includes dedicated structures for tracking copy protection:

```c
typedef struct uft_ir_protection {
    uint32_t    scheme_id;           // Protection identifier
    uint32_t    location_bit;        // Bitstream location
    uint32_t    length_bits;         // Affected region
    uint8_t     severity;            // Impact level
    uint8_t     confirmed;           // Analysis confirmed
    uint16_t    signature_crc;       // Signature CRC
    char        name[32];            // e.g., "V-MAX!", "CopyLock"
} uft_ir_protection_t;
```

### Known Protection Scheme IDs

| ID      | Name           | Platform        |
|---------|----------------|-----------------|
| 0x0001  | V-MAX!         | C64             |
| 0x0002  | PirateSlayer   | C64             |
| 0x0003  | RapidLok       | C64             |
| 0x0004  | Vorpal         | C64             |
| 0x0101  | CopyLock       | Amiga           |
| 0x0102  | Speedlock      | Amiga           |
| 0x0103  | Zynaps         | Amiga           |
| 0x0201  | Prolok         | PC              |
| 0x0202  | Softguard      | PC              |

---

## Weak Bit Regions

```c
typedef struct uft_ir_weak_region {
    uint32_t    start_bit;           // Start position
    uint32_t    length_bits;         // Length in bits
    uint8_t     pattern;             // 0=random, 1=stuck0, 2=stuck1
    uint8_t     confidence;          // Detection confidence (0-255)
} uft_ir_weak_region_t;
```

---

## API Usage Examples

### Creating a Disk Image

```c
// Create disk: 80 cylinders, 2 heads
uft_ir_disk_t* disk = uft_ir_disk_create(80, 2);

// Set metadata
strcpy(disk->metadata.title, "Workbench 1.3");
strcpy(disk->metadata.platform, "Amiga");
disk->metadata.source_type = UFT_IR_SRC_GREASEWEAZLE;

// Create and add tracks
for (int cyl = 0; cyl < 80; cyl++) {
    for (int head = 0; head < 2; head++) {
        uft_ir_track_t* track = uft_ir_track_create(cyl, head);
        track->encoding = UFT_IR_ENC_AMIGA_MFM;
        
        // Add revolution data...
        uft_ir_revolution_t* rev = uft_ir_revolution_create(flux_count);
        uft_ir_revolution_set_flux(rev, flux_data, count, UFT_IR_DATA_FLUX_DELTA);
        uft_ir_track_add_revolution(track, rev);
        
        // Calculate statistics
        uft_ir_revolution_calc_stats(rev);
        uft_ir_calc_quality(track);
        
        uft_ir_disk_add_track(disk, track);
    }
}

// Save to file
uft_ir_disk_save(disk, "workbench13.uftir", UFT_IR_COMP_ZSTD);

// Cleanup
uft_ir_disk_free(disk);
```

### Loading and Analyzing

```c
uft_ir_disk_t* disk;
int ret = uft_ir_disk_load("workbench13.uftir", &disk);
if (ret != UFT_IR_OK) {
    fprintf(stderr, "Load failed: %s\n", uft_ir_strerror(ret));
    return;
}

// Print summary
char* summary = uft_ir_disk_summary(disk);
printf("%s", summary);
free(summary);

// Analyze specific track
uft_ir_track_t* track = uft_ir_disk_get_track(disk, 0, 0);
if (track && track->revolution_count > 0) {
    uint8_t confidence;
    uft_ir_encoding_t enc = uft_ir_detect_encoding(track->revolutions[0], &confidence);
    printf("Encoding: %d (confidence: %d%%)\n", enc, confidence);
}

// Export to JSON
char* json = uft_ir_disk_to_json(disk, false);
printf("%s", json);
free(json);

uft_ir_disk_free(disk);
```

---

## Version History

| Version | Date       | Changes                                    |
|---------|------------|--------------------------------------------|
| 1.0.0   | 2025       | Initial specification                      |

---

## License

UFT-IR format specification and reference implementation are released under the MIT License.

```
SPDX-License-Identifier: MIT
Copyright (c) 2025 UnifiedFloppyTool Project
```
