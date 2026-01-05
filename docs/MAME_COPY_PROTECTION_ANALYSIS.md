# MAME Copy Protection Analysis Report

**Date:** 2026-01-04  
**Source:** mame-master.zip (src/lib/formats/)  
**Analysis for:** UFT v3.6.0.017

---

## Executive Summary

MAME's floppy format library contains **221 format implementations** with ~60,000 LOC total. Key copy protection mechanisms are well-documented and implemented, particularly in the core `flopimg.cpp` (2,382 LOC) and `ipf_dsk.cpp` (767 LOC) files.

**Most Valuable for UFT:**
1. IPF format - Complete weak bit and gap handling
2. 86f format - Weak bit + surface damage implementation
3. D64/G64 - Variable speed zone handling for C64
4. Core flopimg.h - Magnetic state definitions

---

## 1. Magnetic State Types (MG_*)

MAME uses a 4-bit magnetic state encoding in the upper bits of each cell:

```c
// flopimg.h:509-514
enum {
    MG_MASK   = 0xf0000000,
    MG_SHIFT  = 28,
    MG_F      = (0 << MG_SHIFT),  // Flux orientation change (normal)
    MG_N      = (1 << MG_SHIFT),  // Non-magnetized zone (neutral/weak)
    MG_D      = (2 << MG_SHIFT),  // Damaged zone (reads neutral, unwritable)
    MG_E      = (3 << MG_SHIFT)   // End of zone marker
};
```

**UFT Mapping:**
| MAME | UFT Equivalent | Usage |
|------|---------------|-------|
| MG_F | Normal flux transition | Standard data |
| MG_N | Weak bit marker | Copy protection (random reads) |
| MG_D | Damaged zone | Unrecoverable areas |
| MG_E | Zone end | Track structure |

---

## 2. IPF Format - Copy Protection Reference Implementation

The IPF (Interchangeable Preservation Format) by SPS/CAPS is the gold standard for protected disk preservation.

### 2.1 Track Structure (ipf_dsk.cpp:11-26)

```cpp
struct track_info {
    uint32_t cylinder, head, type;
    uint32_t sigtype, process;
    uint32_t size_bytes, size_cells;
    uint32_t index_bytes, index_cells;
    uint32_t datasize_cells, gapsize_cells;
    uint32_t block_count, weak_bits;  // ← Key for protection
    uint32_t data_size_bits;
    bool info_set;
    const uint8_t *data;
    uint32_t data_size;
};
```

### 2.2 Block Data Generation (ipf_dsk.cpp:510-549)

```cpp
// Block description opcodes:
// 0 = End of description
// 1 = Raw bytes (param = byte count)
// 2 = MFM-decoded data bytes
// 3 = MFM-decoded gap bytes
// 5 = Weak bytes (copy protection!) ← CRITICAL

switch(val & 0x1f) {
    case 5: // Weak bytes
        track_write_weak(tpos, 16*param);
        context = 0;
        break;
}
```

### 2.3 Weak Bit Implementation (ipf_dsk.cpp:504-508)

```cpp
void ipf_format::ipf_decode::track_write_weak(
    std::vector<uint32_t>::iterator &tpos, 
    uint32_t cells)
{
    for(uint32_t i=0; i != cells; i++)
        *tpos++ = floppy_image::MG_N;  // Non-magnetized = random read
}
```

### 2.4 Gap Types for Protection (ipf_dsk.cpp:705-718)

```cpp
bool generate_block_gap(uint32_t gap_type, ...) {
    switch(gap_type) {
        case 0: return generate_block_gap_0(...);  // Simple pattern
        case 1: return generate_block_gap_1(...);  // Pre-slice description
        case 2: return generate_block_gap_2(...);  // Post-slice description
        case 3: return generate_block_gap_3(...);  // Split description
    }
}
```

---

## 3. 86f Format - Weak Bit + Surface Damage

The 86f format provides both weak bits and surface damage encoding.

### 3.1 Header Flags (86f_dsk.cpp:30-54)

```cpp
enum {
    SURFACE_DESC = 1,      // Has surface damage info
    TYPE_MASK = 6,         // DD/HD/ED
    TWO_SIDES = 8,
    WRITE_PROTECT = 0x10,
    EXTRA_BC = 0x80,       // Extra bitcells (long tracks!)
    TOTAL_BC = 0x1000      // Total bitcell count mode
};
```

### 3.2 Weak Bit Generation with Damage (86f_dsk.cpp:91-120)

```cpp
void generate_track_from_bitstream_with_weak(
    int track, int head,
    const uint8_t *trackbuf,
    const uint8_t *weak,     // ← Weak bit mask
    int index_cell,
    int track_size,
    floppy_image &image)
{
    for(int i=index_cell; i != track_size; i++, j++) {
        int databit = trackbuf[i >> 3] & (0x80 >> (i & 7));
        int weakbit = weak ? weak[i >> 3] & (0x80 >> (i & 7)) : 0;
        
        if(weakbit && databit)
            dest.push_back(floppy_image::MG_D | time);  // Damaged
        else if(weakbit && !databit)
            dest.push_back(floppy_image::MG_N | time);  // Weak/neutral
        else if(databit)
            dest.push_back(floppy_image::MG_F | time);  // Normal flux
    }
}
```

---

## 4. C64 Variable Speed Zones

### 4.1 Speed Zone Definition (d64_dsk.cpp:60-86)

```cpp
const uint32_t d64_format::cell_size[] = {
    4000,  // Zone 0: 16MHz/16/4 (tracks 31-42)
    3750,  // Zone 1: 16MHz/15/4 (tracks 25-30)
    3500,  // Zone 2: 16MHz/14/4 (tracks 18-24)
    3250   // Zone 3: 16MHz/13/4 (tracks 1-17)
};

const int d64_format::speed_zone[] = {
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,  //  1-17
    2, 2, 2, 2, 2, 2, 2,                                 // 18-24
    1, 1, 1, 1, 1, 1,                                    // 25-30
    0, 0, 0, 0, 0,                                       // 31-35
    0, 0, 0, 0, 0,                                       // 36-40
    0, 0                                                 // 41-42
};
```

### 4.2 G64 Speed Zone Detection (g64_dsk.cpp:112-122)

```cpp
int g64_format::generate_bitstream(int track, int head, int speed_zone, 
    std::vector<bool> &trackbuf, const floppy_image &image)
{
    int cell_size = c1541_cell_size[speed_zone];
    trackbuf = generate_bitstream_from_track(track, head, cell_size, image);
    
    int actual_cell_size = 200000000L/trackbuf.size();
    
    // Allow tolerance of +- 10 us
    return ((actual_cell_size >= cell_size-10) && 
            (actual_cell_size <= cell_size+10)) ? speed_zone : -1;
}
```

---

## 5. CRC Types for Platform Detection

```cpp
// flopimg.h:427-428
enum { 
    CRC_NONE, 
    CRC_AMIGA,      // Odd/even split checksum
    CRC_CBM,        // XOR of original data (GCR5)
    CRC_CCITT,      // Standard x^16+x^12+x^5+1
    CRC_CCITT_FM,   // FM variant
    CRC_MACHEAD,    // Mac GCR6 header
    CRC_FCS,        // Compucolor FCS
    CRC_VICTOR_HDR, // Victor 9000 header
    CRC_VICTOR_DATA // Victor 9000 data
};
```

---

## 6. Key Algorithms to Extract

### 6.1 PLL (Phase-Locked Loop) - flopimg.cpp:1394-1506

The PLL implementation for flux-to-bitstream conversion:
- Period tracking with min/max bounds (0.75x - 1.25x)
- Phase adjustment based on transition timing
- Frequency adjustment for density variations

### 6.2 GCR5 Sector Extraction - flopimg.cpp:2237-2317

Complete C64 GCR5 decoding with:
- Sync mark detection (10 consecutive 1-bits)
- Header block parsing (0x08 marker)
- Data block extraction (0x07 marker)
- XOR checksum validation

### 6.3 GCR6 (Apple) - flopimg.cpp:2104-2234

Mac-style GCR6 with:
- Variable speed zones (track/16)
- Sector counts: 12-8 sectors depending on zone
- 3-into-4 encoding with checksum cascade

### 6.4 Amiga MFM - ami_dsk.cpp:105-162

Odd/even split MFM with:
- Sync word detection (0x44894489)
- Header checksum (10 longwords)
- Data checksum (256 longwords)

---

## 7. Integration Recommendations for UFT

### 7.1 Immediate Value (1-2 days each)

| Module | Source | Priority | Notes |
|--------|--------|----------|-------|
| Weak bit types | flopimg.h:509-514 | P0 | Add MG_N/MG_D to uft_raw_track_t |
| IPF weak handling | ipf_dsk.cpp:504-549 | P0 | Reference for weak bit generation |
| Speed zone tables | d64_dsk.cpp:60-86 | P1 | Already have similar in uft_gcr_tables |

### 7.2 Medium Term (3-5 days each)

| Module | Source | Priority | Notes |
|--------|--------|----------|-------|
| 86f loader | 86f_dsk.cpp | P1 | New format with weak+damage |
| PLL refinements | flopimg.cpp:1394-1506 | P1 | Compare with our PLL |
| Gap type handling | ipf_dsk.cpp:552-718 | P2 | Complex gap patterns |

### 7.3 Format-Specific Notes

**IPF:**
- Block-based structure with opcode descriptions
- Native weak bit support (opcode 5)
- Four gap types for different protection schemes

**86f:**
- Separate weak mask track
- Combined weak+damage encoding
- Long track support via EXTRA_BC flag

**G64:**
- Raw GCR bitstream preservation
- Per-track speed zone storage
- Variable speed zone detection algorithm

---

## 8. Code Quality Assessment

| Aspect | MAME | UFT |
|--------|------|-----|
| Error handling | Basic (return false) | UFT_ERR_* codes ✓ |
| Memory safety | Vector-based | Bounds-checked ✓ |
| Endianness | Helper functions | Explicit handling ✓ |
| Documentation | Good comments | Doxygen-ready ✓ |

**MAME Strengths:**
- Battle-tested implementations
- Comprehensive format coverage
- Clean separation of concerns

**UFT Advantages:**
- Stricter error handling
- Audit trail support
- Forensic-grade logging

---

## 9. Files to Study Further

1. **flopimg.cpp** (2,382 LOC) - Core bit manipulation
2. **ipf_dsk.cpp** (767 LOC) - Protection reference
3. **86f_dsk.cpp** (269 LOC) - Weak bit + damage
4. **d64_dsk.cpp** (326 LOC) - C64 variable speed
5. **g64_dsk.cpp** (210 LOC) - Raw GCR preservation

---

## 10. Conclusion

MAME provides excellent reference implementations for copy protection handling. The key takeaways for UFT:

1. **Adopt MG_N/MG_D encoding** for weak/damaged bits
2. **IPF block opcodes** as reference for track generation
3. **86f dual-mask approach** for weak+damage separation
4. **PLL tolerances** of 0.75x-1.25x are industry standard

The MAME code is well-structured but uses C++ patterns that would need C99 adaptation for UFT integration. Most algorithms can be directly ported with our existing UFT conventions.

---

*Generated by UFT MAME Analysis Pipeline*
