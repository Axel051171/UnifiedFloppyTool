# R50 MEGA-INTEGRATION: External Source Code Integration

## Overview

R50 integrates algorithms and implementations from 5 major floppy disk preservation projects:

| Source | License | Key Contributions |
|--------|---------|-------------------|
| HxCFloppyEmulator | GPL v2 | PLL, HFE v3, Flux Analysis |
| OpenCBM | GPL v2 | GCR 4b5b Codec |
| FluxRipper | - | Multi-Pass Statistics |
| disk-utilities | Public Domain | 170+ Amiga Protections |
| FlashFloppy | Public Domain | Gotek HFE Handler |

---

## 1. HxCFloppyEmulator Integration

### Source Files
- `libhxcfe/sources/stream_analyzer/fluxStreamAnalyzer.c` (4800+ lines)
- `libhxcfe/sources/loaders/hfe_loader/hfev3_*.c`

### UFT Integration
```
include/uft/flux/uft_hxc_pll_enhanced.h
src/flux/uft_hxc_pll_enhanced.c
```

### Key Functions

#### Automatic Bitrate Detection
```c
uint32_t uft_hxc_detectpeaks(uft_hxc_pll_t* pll, const uint32_t* histogram);
```
- Analyzes flux timing histogram
- Detects 250k/300k/500k bitrates
- Returns cell size in ticks

#### Core PLL Cell Timing
```c
int uft_hxc_get_cell_timing(uft_hxc_pll_t* pll, int current_pulsevalue,
                            bool* badpulse, int overlapval, int phasecorrection);
```
- Inter-band rejection for GCR and FM
- Phase correction with fast/slow ratios
- Victor 9K variable speed support

### Victor 9K Band Definitions
```c
static const int victor_9k_bands_def[] = {
    0,  1, 2142, 3, 3600, 5, 5200, 0,
    4,  1, 2492, 3, 3800, 5, 5312, 0,
    // ... 8 zones total
};
```

---

## 2. OpenCBM Integration

### Source Files
- `opencbm/lib/gcr_4b5b.c`
- `opencbm/libd64copy/gcr.c`

### UFT Integration
```
include/uft/codec/uft_opencbm_gcr.h
src/codec/uft_opencbm_gcr.c
```

### Key Functions

#### Low-Level GCR Conversion
```c
int uft_gcr_5_to_4_decode(const uint8_t* source, uint8_t* dest,
                           size_t source_len, size_t dest_len);

int uft_gcr_4_to_5_encode(const uint8_t* source, uint8_t* dest,
                           size_t source_len, size_t dest_len);
```

#### Block Operations
```c
int uft_gcr_decode_block(const uint8_t* gcr_data, uint8_t* decoded,
                          uint8_t* header_out);

int uft_gcr_encode_block(const uint8_t* data, uint8_t* gcr_out);
```

#### Sector Header
```c
int uft_gcr_decode_sector_header(const uint8_t* gcr_header, 
                                  uft_gcr_sector_header_t* header);
```

### GCR Lookup Tables
```c
// Decode: 5-bit GCR → 4-bit value (255 = invalid)
static const uint8_t gcr_decode_table[32] = {
    255, 255, 255, 255, 255, 255, 255, 255,
    255,   8,   0,   1, 255,  12,   4,   5,
    255, 255,   2,   3, 255,  15,   6,   7,
    255,   9,  10,  11, 255,  13,  14, 255
};

// Encode: 4-bit value → 5-bit GCR
static const uint8_t gcr_encode_table[16] = {
    10, 11, 18, 19, 14, 15, 22, 23,
     9, 25, 26, 27, 13, 29, 30, 21
};
```

---

## 3. FluxRipper Integration

### Source Files
- `soc/firmware/include/fluxstat_hal.h`
- `soc/firmware/src/fluxstat_hal.c`

### UFT Integration
```
include/uft/flux/uft_fluxstat.h
```

### Key Concepts

#### Multi-Pass Capture
- 2-64 passes for statistical analysis
- Per-pass metadata (flux count, index time)
- Correlation across revolutions

#### Bit Cell Classification
```c
typedef enum {
    UFT_BITCELL_STRONG_1    = 0,    // >= 90% confidence
    UFT_BITCELL_WEAK_1      = 1,    // 60-89% confidence
    UFT_BITCELL_STRONG_0    = 2,
    UFT_BITCELL_WEAK_0      = 3,
    UFT_BITCELL_AMBIGUOUS   = 4     // < 60% confidence
} uft_bitcell_class_t;
```

#### Sector Recovery
```c
typedef struct {
    uint8_t  data[4096];
    uint16_t size;
    bool     crc_ok;
    uint8_t  confidence_min;
    uint8_t  confidence_avg;
    uint8_t  weak_bit_count;
    uint8_t  corrected_count;    // CRC-guided corrections
} uft_fluxstat_sector_t;
```

---

## 4. disk-utilities Integration

### Source Files
- `libdisk/format/amiga/*.c` (170+ protection handlers)
- `libdisk/format/amiga/copylock.c`
- `libdisk/format/amiga/longtrack.c`

### UFT Integration
```
include/uft/protection/uft_amiga_protection_registry.h
```

### Protection Categories

#### Major Systems
- CopyLock (Rob Northen)
- SpeedLock
- RNC Dualformat/Triformat
- Gremlin Longtrack

#### Publisher-Specific
- Psygnosis A/B/C
- Thalion
- Factor 5
- Ubi Soft
- Rainbow Arts

#### Format-Based Detection
- Long Track (>6300 bytes)
- Variable Timing
- Weak Bits
- Duplicate Sync

### CopyLock LFSR
```c
// 23-bit LFSR with taps at positions 1 and 23
static inline uint32_t uft_copylock_lfsr_next(uint32_t x)
{
    return ((x << 1) & 0x7FFFFF) | (((x >> 22) ^ x) & 1);
}

// "Rob Northen Comp" signature in sector 6
static const uint8_t uft_copylock_signature[16] = {
    0x52, 0x6f, 0x62, 0x20, 0x4e, 0x6f, 0x72, 0x74,
    0x68, 0x65, 0x6e, 0x20, 0x43, 0x6f, 0x6d, 0x70
};
```

---

## 5. FlashFloppy Integration (Reference)

### Source Files
- `src/image/hfe.c`
- `src/floppy.c`

### Key Definitions
```c
// HFEv3 opcodes (bit order reversed)
enum {
    OP_Nop      = 0x0f,
    OP_Index    = 0x8f,
    OP_Bitrate  = 0x4f,
    OP_SkipBits = 0xcf,
    OP_Rand     = 0x2f
};
```

---

## Build Integration

### New Targets
```cmake
# Flux Analysis
add_library(uft_flux_enhanced
    src/flux/uft_hxc_pll_enhanced.c
)

# GCR Codec
add_library(uft_codec_gcr
    src/codec/uft_opencbm_gcr.c
)

# Link with math library
uft_link_math(uft_flux_enhanced)
uft_link_math(uft_codec_gcr)
```

---

## Usage Examples

### Automatic Bitrate Detection
```c
#include "uft/flux/uft_hxc_pll_enhanced.h"

uft_hxc_bitrate_t result = uft_hxc_detect_bitrate(
    flux_data, num_samples, 24000000);

if (result.detected) {
    printf("Bitrate: %u Hz\n", result.bitrate_hz);
    printf("Encoding: %s\n", 
           result.encoding == UFT_HXC_ENC_DD_250K ? "DD 250K" :
           result.encoding == UFT_HXC_ENC_DD_300K ? "DD 300K" : "HD");
}
```

### GCR Block Decoding
```c
#include "uft/codec/uft_opencbm_gcr.h"

uint8_t decoded[256];
int err = uft_gcr_decode_block(gcr_data, decoded, NULL);

if (err == UFT_GCR_OK) {
    // Process decoded sector data
}
```

### Multi-Pass Analysis
```c
#include "uft/flux/uft_fluxstat.h"

uft_fluxstat_ctx_t* ctx = uft_fluxstat_create();
uft_fluxstat_configure(ctx, &config);

for (int pass = 0; pass < 8; pass++) {
    uft_fluxstat_add_pass(ctx, flux_data[pass], count[pass], index_time[pass]);
}

uft_fluxstat_correlate(ctx);

uft_fluxstat_track_t track;
uft_fluxstat_recover_track(ctx, &track);

printf("Recovered: %d/%d sectors\n", 
       track.sectors_recovered, track.sector_count);
```

---

## License Compliance

| Component | License | Attribution Required |
|-----------|---------|---------------------|
| HxCFloppyEmulator | GPL v2 | Yes (in source) |
| OpenCBM | GPL v2 | Yes (in source) |
| FluxRipper | - | - |
| disk-utilities | Public Domain | No |
| FlashFloppy | Public Domain | No |

All integrated code includes original copyright notices and license references.
