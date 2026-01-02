# BATCH 5: SamDisk & a8rawconv Analysis

## Executive Summary

**SamDisk** is the **ORIGINAL REFERENCE IMPLEMENTATION** for the PLL (Phase-Locked Loop) algorithm used by FluxEngine and other flux-reading tools. The PLL code is credited to Keir Fraser's Disk-Utilities/libdisk.

**a8rawconv** is a specialized Atari 8-bit disk conversion utility by Avery Lee (Phaeron) that includes excellent FM/MFM parsing, write precompensation, and Apple II GCR encoding.

## Critical Discoveries

### 1. Original PLL Algorithm (SamDisk)

**Location:** `samdisk-main/src/FluxDecoder.cpp`

This is the **authoritative reference** for PLL-based flux decoding:

```cpp
// DEFAULT VALUES (FluxDecoder.h)
DEFAULT_PLL_ADJUST = 4     // Clock adjustment percentage
DEFAULT_PLL_PHASE = 60     // Phase adjustment percentage
MAX_PLL_ADJUST = 50
MAX_PLL_PHASE = 90

// CORE ALGORITHM
if (clocked_zeros <= 3) {
    // In sync: adjust by phase mismatch
    clock += flux * pll_adjust / 100;
} else {
    // Out of sync: drift toward center
    clock += (clock_centre - clock) * pll_adjust / 100;
    
    // Require 256 good bits before reporting sync loss
    if (goodbits >= 256) sync_lost = true;
    goodbits = 0;
}

// Clamp clock range
clock = clamp(clock, clock_min, clock_max);

// Authentic PLL: preserve phase information
flux = flux * (100 - pll_phase) / 100;
```

**Key Insight:** The `pll_phase` parameter (60% by default) controls how much residual flux time is preserved after detecting a 1-bit. This prevents the timing window from snapping exactly to each transition, which improves tracking accuracy.

### 2. PLL Parameter Comparison

| Parameter | SamDisk | Greaseweazle | FluxEngine |
|-----------|---------|--------------|------------|
| pll_adjust | 4% | 5% | 5% |
| pll_phase | 60% | 60% | 75% |
| sync_threshold | 3 zeros | 3 zeros | 3 zeros |
| goodbits_threshold | 256 | 64 | 256 |

**Recommendation for UFT:** Use SamDisk's defaults as the baseline since it's the original implementation.

### 3. Multi-Pass Flux Scanning Strategy

SamDisk uses an intelligent multi-pass approach:

```cpp
// Speed jitter simulation
flux_scales = { 100, 98, 102 };  // ±2%

// PLL sensitivity levels
pll_adjusts = { 2, 4, 8, 16 };

// Data rate candidates
datarates = { last_successful, 250K, 500K, 300K, 1M };

// Try combinations until error-free track found
for (datarate : datarates)
    for (pll_adjust : pll_adjusts)
        for (flux_scale : flux_scales)
            if (scan_track().has_good_data()) break;
```

### 4. Write Precompensation (a8rawconv)

**Location:** `a8rawconv/compensation.cpp`

Anti-peak-shift algorithm for high-density recordings:

```cpp
// Threshold based on track position and RPM
thresh = samples_per_rev / 30000.0 * (160 + min(track, 47)) / 240.0;

for (each transition) {
    delta1 = max(0, thresh - distance_to_previous);
    delta2 = max(0, thresh - distance_to_next);
    
    // Push transitions apart when too close
    correction = ((delta2 - delta1) * 5) / 12;
    correction = clamp(correction, -prev_dist/2, next_dist/2);
}
```

### 5. MFM Encoding with Precompensation (a8rawconv)

```cpp
// 4-bit expansion table for MFM
static const uint8_t expand4[16] = {
    0b00000000, 0b00000001, 0b00000100, 0b00000101,
    0b00010000, 0b00010001, 0b00010100, 0b00010101,
    ...
};

// MFM encoding with write precomp
if (adjacent_bits == 0x20000)       // Close to prior
    shift_backwards_1_16_bitcell();
else if (adjacent_bits == 0x2000)   // Close to next
    shift_forwards_1_16_bitcell();
else
    use_nominal_position();
```

### 6. Copy Protection Detection (SamDisk)

**Location:** `samdisk-main/src/SpecialFormat.cpp`

Detected protection schemes:
- **Speedlock** (Spectrum +3 and CPC)
- **Rainbow Arts** (CPC)
- **KBI-19** (19-sector protection)
- **Sega System 24** (arcade)
- **8K Sector** tracks
- **OperaSoft** protection
- **LogoProf** protection
- **Prehistorik** protection

### 7. Victor 9000 Variable Speed Zones

```cpp
// Zone-based bitcell timing (ns)
if (cyl < 4)  return 1789;
if (cyl < 16) return 1896;
if (cyl < 27) return 2009;
if (cyl < 38) return 2130;
if (cyl < 49) return 2272;
if (cyl < 60) return 2428;
if (cyl < 71) return 2613;
return 2847;
```

### 8. Format Definitions (SamDisk)

Comprehensive format database including:
- PC formats: 320K, 360K, 640K, 720K, 1.2M, 1.44M, 2.88M
- Amiga: DD (880K), HD (1.76M)
- Atari ST: 360K, 720K
- SAM Coupé: MGT 800K
- Commodore: D80, D81
- TR-DOS: 640K
- Thomson: TO-640K
- Acorn: ADFS formats

### 9. Interleave Calculation (a8rawconv)

```cpp
// Atari 8-bit interleave ratios
if (sector_size == 128) {
    interleave = (sector_count + 1) / 2;  // 9:1 for 18spt
} else if (sector_size == 256) {
    interleave = (sector_count * 15 + 17) / 18;  // 15:1
} else {
    interleave = 1;  // 512-byte: no interleave
}

// Track-to-track skew: ~8% (16.7ms rotation)
t0 = 0.08 * track;
```

### 10. Apple II GCR 6&2 Prenibbling (a8rawconv)

```cpp
// Step 1: Fragment bytes (encode 2-bit portions)
for (j = 0; j < 84; j++) {
    a = src[j] & 3;
    b = src[j + 86] & 3;
    c = src[j + 172] & 3;
    nibblebuf[j + 1] = ((v >> 1) & 0x15) + ((v << 1) & 0x2A);
}

// Step 2: Base bits 2-7
for (j = 0; j < 256; j++) {
    nibblebuf[j + 87] = src[j] >> 2;
}

// Step 3: Adjacent XOR and GCR encode
for (j = 0; j < 343; j++) {
    gcr[j] = GCR6_ENCODE[nibblebuf[j] ^ nibblebuf[j + 1]];
}
```

## Architecture Insights

### SamDisk Class Hierarchy

```
TrackData
├── FluxData (raw flux transitions)
├── BitBuffer (decoded bitstream)
└── Track (decoded sectors)

Decoder hierarchy:
FluxDecoder → BitBuffer → BitstreamDecoder → Track

Format types (src/types/):
├── scp.cpp - SuperCardPro flux
├── hfe.cpp - HxC floppy emulator
├── dsk.cpp - CPC/Spectrum disk images
├── adf.cpp - Amiga disk images
├── d88.cpp - Japanese PC formats
├── td0.cpp - Teledisk
├── imd.cpp - ImageDisk
└── ... (60+ formats total)
```

### a8rawconv Structure

```
Core components:
├── SectorParser - FM byte-by-byte parsing
├── SectorParserMFM - MFM byte-by-byte parsing
├── SectorEncoder - Flux generation
├── RawDisk - Physical track storage
└── DiskInfo - Logical disk structure

Supported formats:
├── ATR - Atari standard image
├── ATX - VAPI/Atari extended
├── XFD - Atari bootable
├── A2R - Apple II flux
└── VFD - Japanese PC
```

## Created Files

### 1. uft_samdisk_algorithms.h (450+ lines)

Core contents:
- **PLL Structure**: Complete state machine with SamDisk defaults
- **CRC16-CCITT**: Lookup table implementation with A1A1A1 constant
- **IBM PC Constants**: DAM codes, gap sizes, track overhead formulas
- **Victor 9000**: Variable speed zone calculations
- **GCR Tables**: C64 5-bit and Apple II 6&2 decoding
- **FM/MFM Patterns**: Address mark bit patterns
- **Amiga MFM**: Checksum calculation
- **Copy Protection**: Speedlock signature detection
- **Format Definitions**: PC720, PC1440, AmigaDOS, AtariST structures

### 2. uft_a8rawconv_algorithms.h (500+ lines)

Core contents:
- **CRC Functions**: Standard and inverted CRC-CCITT
- **GCR 6&2 Tables**: Complete encoding table
- **Prenibbling**: Full 256-byte to 343-byte conversion
- **Sector Encoder**: FM, MFM, GCR with precompensation
- **Write Precompensation**: Anti-peak-shift algorithm
- **Interleave Calculation**: Atari-specific ratios
- **Disk Structures**: Sector, track, raw track definitions
- **SCP Interface**: Status codes and checksum

## Integration Recommendations

### Priority 1: Adopt SamDisk PLL as Reference

Replace existing UFT PLL with SamDisk implementation:
```c
#define UFT_PLL_ADJUST      4       // SamDisk default
#define UFT_PLL_PHASE       60      // SamDisk default
#define UFT_SYNC_THRESHOLD  3       // Consecutive zeros
#define UFT_GOODBITS_MIN    256     // Before reporting sync loss
```

### Priority 2: Implement Multi-Pass Scanning

```c
typedef struct {
    int flux_scales[3];     // { 100, 98, 102 }
    int pll_adjusts[4];     // { 2, 4, 8, 16 }
    int datarates[5];       // { last, 250K, 500K, 300K, 1M }
} uft_scan_params_t;
```

### Priority 3: Add Write Precompensation

Enable for high-density formats and Mac 800K:
```c
void uft_apply_precomp(uint32_t *transitions, size_t count,
                        float samples_per_rev, int track);
```

### Priority 4: Copy Protection Framework

```c
typedef struct {
    const char *name;
    bool (*detect)(const uft_track_t *track);
    uft_track_t (*generate)(const uft_cylhead_t *ch, 
                            const uft_track_t *track);
} uft_protection_handler_t;

extern const uft_protection_handler_t UFT_PROTECTION_SPEEDLOCK;
extern const uft_protection_handler_t UFT_PROTECTION_KBI19;
// ...
```

### Priority 5: Unified Format Registry

```c
typedef struct {
    uint32_t id;
    const char *name;
    uft_format_params_t params;
    uft_pll_preset_t pll;
    bool (*probe)(const uint8_t *data, size_t size);
} uft_format_entry_t;
```

## Statistics

- **SamDisk**: 184 files, ~500KB source code
- **a8rawconv**: 52 files, ~200KB source code
- **Headers created**: 2 files, ~950 lines
- **Format support**: 60+ disk image formats
- **Protection schemes**: 12+ detected

## Key Takeaways

1. **SamDisk is THE reference** - FluxEngine's PLL is derived from it
2. **Phase preservation is critical** - The `pll_phase` parameter prevents timing window snapping
3. **Multi-pass scanning** improves recovery rates significantly
4. **Write precompensation** is essential for high-density write support
5. **Copy protection** requires weak bit generation via flux patterns
6. **Victor 9000** is a special case requiring zone-based timing

---

*Generated from Batch 5 analysis of SamDisk and a8rawconv archives*
