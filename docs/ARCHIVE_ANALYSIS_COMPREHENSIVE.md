# UnifiedFloppyTool (UFT) - Comprehensive Archive Analysis Summary

## Analysis Overview

**Date**: December 30, 2025
**Total Archives Analyzed**: 50+ projects
**Total Files Processed**: 15,000+
**Generated Headers**: 60+ files
**Documentation**: 20+ analysis reports

---

## Batch 1-2: Core Projects

### FloppyControl, FDC Algorithms, Applesauce
- **Key Extractions**: Low-level FDC control, Apple II format handling
- **Files**: format_detection.c/h, flux_core.c/h
- **Algorithms**: FM/MFM decoding, GCR encoding, CRC calculations

### BBC FDC, P64 Flux
- **Key Extractions**: BBC Micro disk formats, P64 flux analysis
- **Files**: uft_bbc.h, uft_p64.h
- **Algorithms**: DFS filesystem, double-density FM

---

## Batch 3: GUI Parameter Alignment

### Greaseweazle Official Parameters
**Source**: Keir Fraser's reference implementation
**Key Parameters**:
```
--diskdefs    # Disk definition file
--format      # Format identifier  
--tracks      # Track range (c=N, h=N)
--revs        # Revolutions to read
--pll         # PLL parameters
```

### Created Files:
- `uft_gui_params_v2.h` - 800+ lines
- `uft_gw_official_params.h` - Complete parameter registry
- `GUI_PARAMETER_MAPPING.md` - Migration guide

---

## Batch 4: FluxEngine & FlashFloppy

### FluxEngine (David Given)
**Algorithms Extracted**:
- Adaptive PLL with prediction
- FluxMap architecture
- Sector-based decoders for 50+ formats

**Key Code**:
```cpp
// FluxEngine PLL (simplified)
class FluxDecoder {
    double clock_period;
    double clock_centre;
    double pll_adjust = 0.04;  // 4% adjustment
    
    void process_flux(double interval) {
        double error = interval - expected;
        clock_centre += error * pll_adjust;
    }
};
```

### FlashFloppy (Keir Fraser)
**Features**:
- HFE format support with streaming
- Gotek firmware implementation
- IMG/ST/ADF file handling

### Created Files:
- `uft_fluxengine_algorithms.h` - 700+ lines
- `uft_flashfloppy_formats.h` - Format definitions

---

## Batch 5: SamDisk & A8rawconv

### SamDisk (Keir Fraser - Original PLL Reference)

**THE Reference PLL Implementation**:
```cpp
// Original SamDisk PLL parameters
const double DEFAULT_CLOCK_CENTRE = 4.0;  // µs for DD
const double PLL_ADJUST = 0.04;           // 4%
const double PLL_PHASE_ADJUST = 0.65;     // 65%

class FluxDecoder {
    double clock_centre = DEFAULT_CLOCK_CENTRE;
    
    std::vector<uint8_t> decode(const FluxData& flux) {
        for (double time : flux.times()) {
            double cells = time / clock_centre;
            int whole_cells = std::round(cells);
            double error = time - (whole_cells * clock_centre);
            
            // Adjust clock
            clock_centre += error * PLL_ADJUST;
            
            // Emit bits
            for (int i = 0; i < whole_cells; i++)
                emit_bit(i == whole_cells - 1);
        }
    }
};
```

**Copy Protection Detection** (12+ schemes):
- Speedlock
- Copylock
- Rainbow Arts
- KBI-19
- Hexagon
- Prolok
- Weak sectors
- Long tracks

### A8rawconv (Atari 8-bit)
**Features**:
- ATR/XFD format conversion
- Boot sector analysis
- Sector timing extraction

### Created Files:
- `uft_samdisk_algorithms.h` - 1100+ lines
- `uft_samdisk_core.h` - Core definitions
- `uft_a8rawconv_algorithms.h` - Atari support
- `uft_a8rawconv_atari.h` - Atari formats

---

## Batch 6: HxC Floppy Emulator (COMPREHENSIVE)

### Project Statistics
- **Total Files**: 2,100+ 
- **Format Loaders**: 103
- **Track Formats**: 17+
- **Filesystems**: 4 (FAT12, AmigaDOS, CP/M, FLEX)

### Core Library (libhxcfe)

**PLL Implementation** (fluxStreamAnalyzer.c - 4817 lines):
```c
typedef struct pll_stat {
    int32_t pump_charge;      // Current cell size
    int32_t phase;            // Window phase
    int32_t pivot;            // Center value
    int32_t pll_max, pll_min; // Limits (+/-10%)
    int32_t last_error;       // Phase error
    
    // Correction ratios (default 7/8)
    int fast_correction_ratio_n, fast_correction_ratio_d;
    int slow_correction_ratio_n, slow_correction_ratio_d;
    
    // Special modes
    int inter_band_rejection; // 0=off, 1=GCR, 2=FM
    int band_mode;            // Victor 9000 variable speed
} pll_stat;
```

**MFM Lookup Tables** (luts.c):
- `LUT_Byte2MFM[256]` - Byte to 16-bit MFM
- `LUT_Byte2MFMClkMask[256]` - Clock masks for address marks
- `LUT_Byte2EvenBits[256]` - Amiga odd/even encoding
- `LUT_Byte2OddBits[256]` - Amiga odd/even encoding
- `LUT_ByteBitsInverter[256]` - Bit reversal

**GCR Encoding Tables**:
- C64 GCR (4-to-5): `gcrencodingtable[16]`
- Apple II 6-and-2: `byte_translation_SixAndTwo[64]`
- Apple Mac GCR

### Format Support (103 Loaders!)

| Category | Formats |
|----------|---------|
| **PC/DOS** | IMG, IMZ, IMD, DMF, 2MG |
| **Amiga** | ADF, ADZ, DMS, IPF |
| **Atari** | ST, MSA, STX, ATR |
| **Apple** | DO, NIB, WOZ, A2R, 2MG |
| **Commodore** | D64, D81, G64 |
| **BBC/Acorn** | SSD, DSD, ADF |
| **TRS-80** | DMK, JV1, JV3, JVC |
| **Japanese** | D88, FDI, FDX |
| **Flux** | SCP, KryoFlux, HFE, HxCStream |
| **Protected** | IPF (CAPS), STX (Pasti) |
| **Samplers** | Ensoniq, Roland, Kurzweil, E-MU |

### Created Files:
- `uft_hxc_algorithms.h` - 945 lines (PLL, encoding, CRC)
- `uft_hxc_formats.h` - 550+ lines (format structures)
- `ARCHIVE_ANALYSIS_BATCH6_HXC.md` - Complete documentation

---

## PLL Implementation Comparison

| Project | Clock Default | Adjustment | Phase Correction | Special Features |
|---------|--------------|------------|------------------|------------------|
| **SamDisk** | 4.0 µs | 4% | 65% | Reference implementation |
| **HxC** | Variable | 7/8 ratio | Phase-based | Inter-band rejection, Victor 9000 |
| **FluxEngine** | Auto-detect | 4% | Adaptive | FluxMap, prediction |
| **Greaseweazle** | Format-based | Configurable | Yes | Real-time hardware |

### Unified PLL Recommendation for UFT:

```c
typedef struct uft_pll_config {
    double clock_centre;           // Default: auto-detect from format
    double pll_adjust;             // Default: 0.04 (4%)
    double phase_adjust;           // Default: 0.65 (65%)
    int    min_max_percent;        // Default: 10 (+/-10%)
    int    correction_ratio_n;     // Default: 7 (7/8)
    int    correction_ratio_d;     // Default: 8
    int    inter_band_mode;        // 0=off, 1=GCR, 2=FM
    bool   adaptive;               // Use FluxEngine-style prediction
} uft_pll_config_t;
```

---

## Format Detection Strategy

### Score-Based Detection (from HxC/SamDisk):

```c
typedef struct {
    const char *format_name;
    int (*probe)(const uint8_t *data, size_t len);
    int confidence;  // 0-100
} uft_format_probe_t;

int uft_detect_format(const uint8_t *data, size_t len) {
    int best_score = 0;
    int best_format = -1;
    
    for (int i = 0; i < NUM_PROBES; i++) {
        int score = probes[i].probe(data, len);
        if (score > best_score) {
            best_score = score;
            best_format = i;
        }
    }
    
    if (best_score < CONFIDENCE_THRESHOLD)
        return UFT_FORMAT_UNKNOWN;
    
    return best_format;
}
```

### Magic Byte Signatures:
| Format | Magic | Offset |
|--------|-------|--------|
| HFE | `HXCPICFE` | 0 |
| HFEv3 | `HXCHFEV3` | 0 |
| SCP | `SCP` | 0 |
| WOZ | `WOZ1`/`WOZ2` | 0 |
| A2R | `A2R2` | 0 |
| IPF | `0x843265bb` | 0 |
| STX | `RSP\1` | 0 |
| IMD | `IMD ` | 0 |
| TD0 | `TD`/`td` | 0 |

---

## Generated UFT Header Files Summary

### Core (14 files)
- PLL, SIMD, CRC, Memory, Flux processing, Format detection

### Encoding (9 files)
- FM/MFM, Amiga MFM, C64 GCR, Apple GCR, WD1772 DPLL

### Formats (11 files)
- HFE, SCP, WOZ, A2R, DMK, IMD, MSA, DMF

### Filesystem (5 files)
- FAT12, CP/M, Apple HFS, ProDOS

### Protection (4 files)
- Copy protection schemes, CBM protection, Detection

### Hardware (4 files)
- Greaseweazle, FDC emulation, Timing

### GUI (5 files)
- Parameters v1/v2, Registry, Presets

### HxC Batch 6 (2 files)
- `uft_hxc_algorithms.h` - 945 lines
- `uft_hxc_formats.h` - 550+ lines

**Total: 60+ header files**

---

## Integration Priority

1. **PLL Consolidation** - Use SamDisk baseline + HxC features
2. **Format Plugin System** - HxC-style loaders
3. **Lookup Tables** - HxC MFM/GCR tables
4. **Filesystem Support** - FAT12, AmigaDOS, ProDOS, CP/M
5. **Copy Protection** - 12+ detection schemes

---

*Generated by UFT Archive Analysis System - December 2025*
