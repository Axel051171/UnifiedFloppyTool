# BATCH 6: HxC Floppy Emulator - Complete Analysis

## Overview

**Project**: HxC Floppy Emulator by Jean-François DEL NERO
**Website**: http://hxc2001.free.fr
**License**: GPL v2
**Analyzed**: December 2025

The HxC Floppy Emulator is the most comprehensive floppy disk emulation library available, supporting over 100 image formats and multiple encoding schemes. This analysis extracts all relevant algorithms, data structures, and techniques for integration into UFT.

## Archive Statistics

| Archive | Files | Size | Purpose |
|---------|-------|------|---------|
| HxCFloppyEmulator-main | 1601 | Core library |
| HXCFE_file_selector-master | 344 | File selection UI |
| HXCFE_AmstradCPC_file_selector-master | 90 | CPC-specific selector |
| HXCFE_Amiga_copy_utility-master | 41 | Amiga tools |
| HXCFE_QuickDisk_Toolkit-master | 24 | QuickDisk support |

**Total**: 2100+ files analyzed

## Key Components

### 1. libhxcfe - Core Library Structure

```
libhxcfe/sources/
├── loaders/           # 103 format loaders!
├── tracks/            # Track processing
│   ├── encoding/      # MFM, FM, M2FM
│   └── track_formats/ # 17 track format implementations
├── stream_analyzer/   # Flux stream analysis with PLL
├── fs_manager/        # Filesystem support (FAT12, AmigaDOS, CP/M, FLEX)
└── thirdpartylibs/    # External dependencies
```

### 2. Supported Image Formats (103 loaders!)

#### Sector-based Formats
- ADF (Amiga), ADL, ADZ (compressed ADF)
- D64/D81 (Commodore)
- D88 (PC-88/98)
- DSK (CPC), SSD/DSD (BBC)
- IMD, IMG, IMZ
- JV1/JV3/JVC (TRS-80)
- MSA, ST (Atari ST)
- TRD (TR-DOS)

#### Flux Stream Formats
- HFE/HFEv3 (native HxC format)
- SCP (SuperCard Pro)
- KryoFlux Stream
- DiscFerret DFI
- IPF (SPS/CAPS)
- A2R (Applesauce)
- WOZ (Apple II)

#### Raw/Bitstream Formats
- MFM, FDI, FDX
- DMK (TRS-80)
- STX (Atari pasti)
- PRI (PCE)

#### Specialty Formats
- Apple II: DO, NIB, 2MG
- System24 (Sega arcade)
- Ensoniq Mirage
- Roland D-20
- Kurzweil K2000
- W-30 (Roland sampler)
- E-MU EMAX

### 3. Track Format Support

| Format | Encoding | Tracks | Sectors | Size | Platform |
|--------|----------|--------|---------|------|----------|
| ISO IBM SD | FM | 40/80 | 8-10 | 128-512 | PC/CP/M |
| ISO IBM DD | MFM | 80 | 9-18 | 512 | PC |
| Amiga DD | MFM | 80 | 11 | 512 | Amiga |
| C64 | GCR | 35-40 | 17-21 | 256 | C64/1541 |
| Apple II | GCR 6&2 | 35 | 16 | 256 | Apple II |
| Apple Mac | GCR | 80 | Variable | 512 | Macintosh |
| Victor 9000 | GCR | 80 | Variable | 512 | Victor |
| DEC RX02 | M2FM | 77 | 26 | 256 | PDP-11 |
| Heathkit | FM | 40 | 10 | 256 | H-89 |
| Northstar | MFM | 35 | 10 | 512 | Northstar |

### 4. PLL Implementation (fluxStreamAnalyzer.c)

The HxC PLL is a sophisticated adaptive clock recovery system:

```c
// Key parameters
#define DEFAULT_MAXPULSESKEW 25      // in per-256
#define DEFAULT_BLOCK_TIME 1000      // in µS
#define DEFAULT_SEARCHDEPTH 0.025

// PLL State Structure
typedef struct pll_stat {
    int32_t pump_charge;              // Current cell size (1 cell)
    int32_t phase;                    // Current window phase
    int32_t pll_max, pivot, pll_min;  // Limits
    int32_t last_error;               // Phase error
    int32_t lastpulsephase;           // Last pulse position
    int tick_freq;                    // Sample rate
    int pll_min_max_percent;          // ±10% default
    int fast_correction_ratio_n/d;    // 7/8 default
    int slow_correction_ratio_n/d;    // 7/8 default
    int inter_band_rejection;         // 0=off, 1=GCR, 2=FM
    int max_pll_error_ticks;          // Flakey threshold
    int band_mode;                    // Victor 9000 mode
    int bands_separators[16];         // Zone boundaries
} pll_stat;
```

**Key Features:**
- Phase correction with configurable ratio
- Asymmetric fast/slow correction
- Inter-band rejection for GCR/FM
- Variable speed support (Victor 9000)
- Flakey bit detection based on PLL error
- Overflow protection for long tracks

### 5. MFM Encoding (mfm_encoding.c)

HxC uses precomputed lookup tables for fast MFM encoding/decoding:

```c
// Byte to MFM: 256 entries, each byte → 16-bit MFM
unsigned short LUT_Byte2MFM[256];

// Clock mask for special address marks (A1 sync)
unsigned short LUT_Byte2MFMClkMask[256];

// Amiga odd/even bit extraction
unsigned char LUT_Byte2EvenBits[256];
unsigned char LUT_Byte2OddBits[256];

// Bit reversal for LSB-first formats
unsigned char LUT_ByteBitsInverter[256];
```

**Fast MFM Generator:**
```c
for(l=0; l<size; l++) {
    byte = track_data[l];
    mfm_code = LUT_Byte2MFM[byte] & lastbit;
    mfm_buffer[i++] = mfm_code >> 8;
    mfm_buffer[i++] = mfm_code & 0xFF;
    lastbit = ~(LUT_Byte2MFM[byte] << 15);
}
```

### 6. GCR Encoding

#### C64 GCR (4-to-5)
```c
static const unsigned char gcrencodingtable[16] = {
    0x0A, 0x0B, 0x12, 0x13,
    0x0E, 0x0F, 0x16, 0x17,
    0x09, 0x19, 0x1A, 0x1B,
    0x0D, 0x1D, 0x1E, 0x15
};
```

#### Apple II GCR (6-and-2)
```c
static const unsigned char byte_translation_SixAndTwo[64] = {
    0x96, 0x97, 0x9a, 0x9b, 0x9d, 0x9e, 0x9f, 0xa6,
    0xa7, 0xab, 0xac, 0xad, 0xae, 0xaf, 0xb2, 0xb3,
    // ... 64 entries total
};
```

### 7. HFE File Format (Native HxC)

```c
typedef struct {
    uint8_t  HEADERSIGNATURE[8];    // "HXCPICFE"
    uint8_t  formatrevision;
    uint8_t  number_of_track;
    uint8_t  number_of_side;
    uint8_t  track_encoding;
    uint16_t bitRate;               // kbit/s
    uint16_t floppyRPM;
    uint8_t  floppyinterfacemode;
    uint8_t  write_protected;
    uint16_t track_list_offset;     // In 512-byte blocks
    uint8_t  write_allowed;
    uint8_t  single_step;
    uint8_t  track0s0_altencoding;
    uint8_t  track0s0_encoding;
    uint8_t  track0s1_altencoding;
    uint8_t  track0s1_encoding;
} picfileformatheader;
```

**HFE Track Data:**
- Interleaved side 0/side 1 in 256-byte blocks
- LSB first bit ordering
- Variable bitrate via timing buffer (HFEv3)

### 8. SCP File Format (SuperCard Pro)

**Complete specification from scp_format.h:**

```c
// Header (16 bytes)
struct scp_header {
    uint8_t  sign[3];               // "SCP"
    uint8_t  version;               // (Ver<<4)|Rev
    uint8_t  disk_type;             // Manufacturer + subtype
    uint8_t  number_of_revolution;  // 1-5 typically
    uint8_t  start_track;           // 0-165
    uint8_t  end_track;             // 0-165
    uint8_t  flags;                 // See below
    uint8_t  bit_cell_width;        // 0=16 bits
    uint8_t  number_of_heads;       // 0=both, 1/2=single
    uint8_t  resolution;            // 0=25ns base
    uint32_t file_data_checksum;
};

// Flags
#define INDEX_MARK    0x01    // Flux starts at index
#define DISK_96TPI    0x02    // 96 TPI drive
#define DISK_360RPM   0x04    // 360 RPM drive
#define NORMALIZED    0x08    // Flux normalized
#define READWRITE     0x10    // Read/write image
#define FOOTER        0x20    // Has extension footer

// Track Data Header (per track)
struct scp_track_header {
    uint8_t  trk_sign[3];           // "TRK"
    uint8_t  track_number;
    // Followed by revolution entries:
    // index_time (32-bit, 25ns units)
    // track_length (32-bit, bitcells)
    // track_offset (32-bit, from TDH start)
};
```

**Disk Types:**
- 0x00: Commodore C64
- 0x04: Commodore Amiga
- 0x10: Atari FM SS
- 0x14: Atari ST SS
- 0x20: Apple II
- 0x24: Apple 400K
- 0x30: PC 360K
- 0x40: Tandy TRS-80
- 0x50: TI-99/4A
- 0x60: Roland
- 0x80: Other

### 9. KryoFlux Stream Format

```c
// OOB Header
#define OOB_SIGN 0x0D

struct s_oob_header {
    uint8_t  Sign;      // 0x0D
    uint8_t  Type;      // See below
    uint16_t Size;
};

// OOB Types
#define OOBTYPE_Stream_Read  0x01
#define OOBTYPE_Index        0x02
#define OOBTYPE_Stream_End   0x03
#define OOBTYPE_String       0x04
#define OOBTYPE_End          0x0D

// Clock frequency
#define DEFAULT_KF_MCLOCK  48054857.14  // ~48 MHz
#define DEFAULT_KF_SCLOCK  24027428.57  // ~24 MHz (41.619ns)

// Stream encoding
#define KF_STREAM_OP_NOP1     0x08
#define KF_STREAM_OP_NOP2     0x09
#define KF_STREAM_OP_NOP3     0x0A
#define KF_STREAM_OP_OVERFLOW 0x0B
#define KF_STREAM_OP_VALUE16  0x0C
#define KF_STREAM_OP_OOB      0x0D
```

### 10. CRC Implementation

HxC uses a 4-bit table CRC-16 for smaller memory footprint:

```c
void CRC16_Init(uint8_t *hi, uint8_t *lo, 
                uint8_t *table, uint16_t poly, uint16_t init)
{
    // Generate 16-entry table for 4-bit chunks
    for(int i = 0; i < 16; i++) {
        uint16_t te = GenCRC16TableEntry(i, 4, poly);
        table[i+16] = te >> 8;
        table[i] = te & 0xFF;
    }
    *hi = init >> 8;
    *lo = init & 0xFF;
}

void CRC16_Update(uint8_t *hi, uint8_t *lo, uint8_t val, uint8_t *table)
{
    CRC16_Update4Bits(hi, lo, val >> 4, table);
    CRC16_Update4Bits(hi, lo, val & 0x0F, table);
}
```

### 11. Victor 9000 Variable Speed Zones

```c
int victor_9k_bands_def[] = {
    // cyl, code1,time1, code2,time2, code3,time3, end
    0,   1,2142, 3,3600, 5,5200, 0,   // Zone 0
    4,   1,2492, 3,3800, 5,5312, 0,   // Zone 1
    16,  1,2550, 3,3966, 5,5552, 0,   // Zone 2
    27,  1,2723, 3,4225, 5,5852, 0,   // Zone 3
    38,  1,2950, 3,4500, 5,6450, 0,   // Zone 4
    48,  1,3150, 3,4836, 5,6800, 0,   // Zone 5
    60,  1,3400, 3,5250, 5,7500, 0,   // Zone 6
    71,  1,3800, 3,5600, 5,8000, 0,   // Zone 7
    -1,  0,   0, 0,   0, 0,   0, 0    // End
};
```

### 12. Filesystem Support

HxC includes full filesystem implementations:

| Filesystem | Read | Write | Features |
|------------|------|-------|----------|
| FAT12 | ✓ | ✓ | MS-DOS, Atari ST |
| AmigaDOS | ✓ | ✓ | OFS/FFS |
| CP/M | ✓ | ✓ | Multiple variants |
| FLEX | ✓ | ✓ | 6800/6809 systems |

### 13. QuickDisk Support

The QuickDisk toolkit supports:

- **MO5**: Thomson MO5 QD format
- **Akai**: Akai S612 sampler
- **Roland**: Roland S-series samplers

**MO5 Sector Interleave Table** (400 sectors):
```c
int qdsector[400] = {
    321, 33,225,129,322, 34,226,130,323, 35,227,131,...
    // Complex interleave pattern for optimal access
};
```

## Integration Recommendations for UFT

### Priority 1: PLL Unification
Adopt HxC's adaptive PLL with:
- Configurable correction ratios
- Inter-band rejection modes
- Flakey bit detection
- Variable speed support

### Priority 2: Format Loader Architecture
Implement HxC-style loader plugin system:
```c
typedef struct {
    int (*IsValidFile)(char *filename);
    int (*LoadFile)(char *filename, FLOPPY **floppy);
    int (*SaveFile)(char *filename, FLOPPY *floppy);
    int (*GetExtension)(char **ext_list);
} LOADER_PLUGIN;
```

### Priority 3: Lookup Tables
Use precomputed tables for:
- MFM encoding/decoding
- GCR encoding/decoding
- Bit reversal
- CRC calculation

### Priority 4: HFE Support
Add native HFE read/write for:
- Hardware emulator compatibility
- Preservation quality storage
- Variable bitrate tracks

### Priority 5: Flux Stream Processing
Implement unified flux stream handling for:
- SCP (SuperCard Pro)
- KryoFlux
- DiscFerret
- HFE stream mode

## Created Files

1. **uft_hxc_algorithms.h** (950+ lines)
   - Complete PLL implementation
   - All lookup tables
   - Format structures (HFE, SCP, KryoFlux)
   - CRC functions
   - GCR encoding tables
   - Victor 9000 speed zones
   - Gap3 configuration table

## References

- HxC Floppy Emulator: http://hxc2001.free.fr
- SCP Format Spec: Jim Drew / SuperCard Pro
- KryoFlux: Software Preservation Society
- Apple II DOS 3.3: Beneath Apple DOS
- Commodore 1541: 1541 User's Guide
