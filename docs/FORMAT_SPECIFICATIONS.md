# UFT Format Specification Reference

**Version:** 4.1.0  
**Date:** 2026-01-03  
**Formats Covered:** 115+

---

## Table of Contents

1. [Sector Image Formats](#sector-image-formats)
2. [Track Image Formats](#track-image-formats)
3. [Flux Image Formats](#flux-image-formats)
4. [Platform-Specific Formats](#platform-specific-formats)
5. [Filesystem Formats](#filesystem-formats)
6. [Format Detection](#format-detection)

---

## Sector Image Formats

### ADF - Amiga Disk File

| Property | Value |
|----------|-------|
| Extension | `.adf` |
| Magic | None (size-based detection) |
| Platforms | Amiga |
| Encoding | MFM |

**Geometry:**

| Type | Tracks | Heads | Sectors | Bytes/Sector | Total Size |
|------|--------|-------|---------|--------------|------------|
| DD | 80 | 2 | 11 | 512 | 901,120 bytes |
| HD | 80 | 2 | 22 | 512 | 1,802,240 bytes |

**Structure:**
```
Offset 0x000000: Track 0, Head 0, Sectors 0-10
Offset 0x001600: Track 0, Head 1, Sectors 0-10
Offset 0x002C00: Track 1, Head 0, Sectors 0-10
...
```

**Filesystem:** OFS (Old File System) or FFS (Fast File System)

---

### D64 - Commodore 64 Disk Image

| Property | Value |
|----------|-------|
| Extension | `.d64` |
| Magic | None (size-based) |
| Platforms | C64, C128, VIC-20 |
| Encoding | GCR |

**Sizes:**

| Variant | Tracks | Size | Error Info |
|---------|--------|------|------------|
| Standard | 35 | 174,848 bytes | No |
| With Errors | 35 | 175,531 bytes | 683 bytes |
| Extended | 40 | 196,608 bytes | No |
| Extended+Errors | 40 | 197,376 bytes | 768 bytes |

**Track Layout:**

| Tracks | Sectors | Bytes/Track |
|--------|---------|-------------|
| 1-17 | 21 | 5,376 |
| 18-24 | 19 | 4,864 |
| 25-30 | 18 | 4,608 |
| 31-35 | 17 | 4,352 |

**BAM Location:** Track 18, Sector 0

---

### IMG/IMA - Raw Sector Image

| Property | Value |
|----------|-------|
| Extensions | `.img`, `.ima`, `.dsk`, `.bin` |
| Magic | None |
| Platforms | IBM PC, DOS |
| Encoding | MFM |

**Common Geometries:**

| Format | Tracks | Heads | Sectors | Size |
|--------|--------|-------|---------|------|
| 360K | 40 | 2 | 9 | 368,640 |
| 720K | 80 | 2 | 9 | 737,280 |
| 1.2M | 80 | 2 | 15 | 1,228,800 |
| 1.44M | 80 | 2 | 18 | 1,474,560 |
| 2.88M | 80 | 2 | 36 | 2,949,120 |

---

### STX - Atari ST Extended

| Property | Value |
|----------|-------|
| Extension | `.stx` |
| Magic | `RSY\0` (0x52535900) |
| Platforms | Atari ST |
| Encoding | MFM |

**Header (16 bytes):**
```c
struct stx_header {
    char     magic[4];      // "RSY\0"
    uint16_t version;       // Format version
    uint16_t tool_version;  // Tool that created it
    uint16_t reserved;
    uint8_t  tracks;        // Number of tracks
    uint8_t  revision;
    uint32_t reserved2;
};
```

---

## Track Image Formats

### DMK - TRS-80 Disk Image

| Property | Value |
|----------|-------|
| Extension | `.dmk` |
| Magic | None (header-based) |
| Platforms | TRS-80, CoCo |
| Encoding | FM or MFM |

**Header (16 bytes):**
```c
struct dmk_header {
    uint8_t  write_protect; // 0xFF = protected
    uint8_t  tracks;        // Number of tracks
    uint16_t track_length;  // Bytes per track (little-endian)
    uint8_t  flags;         // Bit 4: single-sided, Bit 6: single-density
    uint8_t  reserved[7];
    uint32_t real_format;   // Optional: native format ID
};
```

**Track Structure:**
```
Offset 0x0000: IDAM pointer table (64 entries × 2 bytes)
Offset 0x0080: Raw track data
```

**IDAM Pointer Format:**
- Bit 15: Density (0=MFM, 1=FM)
- Bits 14-0: Offset to IDAM within track

---

### MFM - Raw MFM Track Data

| Property | Value |
|----------|-------|
| Extension | `.mfm` |
| Magic | `MFM1` or `MFM2` |
| Encoding | MFM |

**Header:**
```c
struct mfm_header {
    char     magic[4];      // "MFM1" or "MFM2"
    uint8_t  tracks;
    uint8_t  heads;
    uint16_t rpm;           // Rotation speed
    uint16_t bitrate;       // Bits per second / 1000
    uint16_t track_bytes;   // Bytes per track
};
```

---

### HFE - HxC Floppy Emulator

| Property | Value |
|----------|-------|
| Extension | `.hfe` |
| Magic | `HXCPICFE` |
| Platforms | Universal |
| Versions | v1, v2, v3 |

**Header (512 bytes):**
```c
struct hfe_header {
    char     signature[8];      // "HXCPICFE"
    uint8_t  format_revision;   // 0 = v1
    uint8_t  tracks;
    uint8_t  heads;
    uint8_t  track_encoding;    // See encoding table
    uint16_t bitrate;           // kbit/s
    uint16_t rpm;
    uint8_t  interface_mode;
    uint8_t  reserved;
    uint16_t track_list_offset; // Offset to track LUT
    uint8_t  write_allowed;
    uint8_t  single_step;
    uint8_t  track0s0_altencoding;
    uint8_t  track0s0_encoding;
    uint8_t  track0s1_altencoding;
    uint8_t  track0s1_encoding;
};
```

**Track Encoding Values:**
| Value | Encoding |
|-------|----------|
| 0x00 | ISO MFM |
| 0x01 | Amiga MFM |
| 0x02 | ISO FM |
| 0x03 | EMU FM |
| 0x04 | Unknown |

---

## Flux Image Formats

### SCP - SuperCard Pro

| Property | Value |
|----------|-------|
| Extension | `.scp` |
| Magic | `SCP` |
| Platforms | Universal |
| Data | Raw flux transitions |

**Header (16 bytes):**
```c
struct scp_header {
    char     magic[3];      // "SCP"
    uint8_t  version;
    uint8_t  disk_type;
    uint8_t  revolutions;
    uint8_t  start_track;
    uint8_t  end_track;
    uint8_t  flags;
    uint8_t  bit_cell_width;
    uint8_t  heads;
    uint8_t  resolution;
    uint32_t checksum;
};
```

**Disk Type Values:**
| Value | Type |
|-------|------|
| 0x00 | C64 |
| 0x10 | Amiga |
| 0x20 | Atari ST |
| 0x30 | Atari 800 |
| 0x40 | Apple II |
| 0x50 | Apple Mac |
| 0x60 | PC 360K |
| 0x70 | PC 720K |
| 0x80 | PC 1.2M |
| 0x90 | PC 1.44M |

**Track Data:**
- 4 bytes: Track offset
- Per revolution:
  - 4 bytes: Index time (25ns units)
  - 4 bytes: Track length (bytes)
  - 4 bytes: Data offset
  - N bytes: Flux data (16-bit values, 25ns units)

---

### A2R - Applesauce Flux

| Property | Value |
|----------|-------|
| Extension | `.a2r` |
| Magic | `A2R2` or `A2R3` |
| Platforms | Apple II |
| Data | Raw flux transitions |

**Chunk-Based Format:**
```
Header: "A2R2" + 0xFF + 0x0A + 0x0D + 0x0A
INFO chunk: Disk metadata
STRM chunk: Flux stream data
META chunk: Optional metadata
```

**INFO Chunk:**
```c
struct a2r_info {
    uint8_t  version;       // 2 or 3
    char     creator[32];   // Tool name
    uint8_t  disk_type;     // 1=5.25", 2=3.5"
    uint8_t  write_protected;
    uint8_t  synchronized;
    uint8_t  hard_sectored;
};
```

---

### KryoFlux Raw

| Property | Value |
|----------|-------|
| Extension | `.raw` |
| Platforms | Universal |
| Data | Raw flux + stream markers |

**Filename Pattern:** `trackXX.Y.raw`
- XX = track number (00-99)
- Y = head (0 or 1)

**Stream Format:**
| Byte | Meaning |
|------|---------|
| 0x00-0x07 | Flux value + 0x0E00 |
| 0x08 | NOP |
| 0x09 | 2-byte overflow follows |
| 0x0A | 3-byte overflow follows |
| 0x0B | Overflow16 follows |
| 0x0C | Flux value follows |
| 0x0D | OOB data follows |

---

## Platform-Specific Formats

### Apple II Formats

#### NIB - Nibble Format
- Raw GCR nibbles
- 35 tracks × 6656 bytes = 232,960 bytes
- No sync bytes stored

#### WOZ - Applesauce Format
| Version | Features |
|---------|----------|
| WOZ 1.0 | Basic nibble stream |
| WOZ 2.0 | Optimal bit timing |
| WOZ 2.1 | Flux data support |

**Chunks:** INFO, TMAP, TRKS, FLUX, WRIT, META

### Commodore Formats

| Format | Platform | Tracks | Description |
|--------|----------|--------|-------------|
| D64 | C64 | 35/40 | Sector image |
| G64 | C64 | 84 max | GCR track image |
| D71 | C128 | 70 | Double-sided D64 |
| D81 | C128/1581 | 80 | 3.5" MFM |
| T64 | C64 | - | Tape archive |
| TAP | C64 | - | Raw tape pulses |

### Atari 8-bit Formats

| Format | Extension | Description |
|--------|-----------|-------------|
| ATR | `.atr` | Standard sector image |
| XFD | `.xfd` | Raw sector dump |
| DCM | `.dcm` | DiskComm compressed |
| ATX | `.atx` | Extended with protection |
| PRO | `.pro` | APE Pro format |

**ATR Header (16 bytes):**
```c
struct atr_header {
    uint16_t magic;         // 0x0296
    uint16_t size_para;     // Size in 16-byte paragraphs (low)
    uint16_t sector_size;   // 128 or 256
    uint8_t  size_para_hi;  // Size paragraphs (high byte)
    uint32_t crc;           // Optional CRC
    uint32_t reserved;
    uint8_t  flags;
};
```

---

## Filesystem Formats

### FAT12

| Property | Value |
|----------|-------|
| Cluster Size | 512-4096 bytes |
| Max Clusters | 4084 |
| Max Volume | ~16 MB |

**Boot Sector (BPB):**
```c
struct fat12_bpb {
    uint8_t  jmp[3];           // Jump instruction
    char     oem[8];           // OEM name
    uint16_t bytes_per_sector; // Usually 512
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  fat_count;        // Usually 2
    uint16_t root_entries;     // 224 for 1.44M
    uint16_t total_sectors_16;
    uint8_t  media_type;       // 0xF0 for 1.44M
    uint16_t sectors_per_fat;
    uint16_t sectors_per_track;
    uint16_t heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
};
```

### Amiga OFS/FFS

| Property | OFS | FFS |
|----------|-----|-----|
| Block Size | 512 | 512 |
| Data per Block | 488 | 512 |
| Checksum | Yes | Yes |
| International | No | Optional |

**Block Types:**
| Type | Value | Description |
|------|-------|-------------|
| T_HEADER | 2 | File/Directory header |
| T_DATA | 8 | Data block |
| T_LIST | 16 | Extension block |

### BBC DFS

| Property | Value |
|----------|-------|
| Sector Size | 256 bytes |
| Max Files | 31 (62 Watford) |
| Catalog | Sectors 0-1 |

**Catalog Entry (8 bytes):**
```c
struct dfs_entry {
    char    filename[7];    // Space-padded
    char    directory;      // Usually '$'
    uint16_t load_addr_lo;
    uint16_t exec_addr_lo;
    uint16_t length_lo;
    uint8_t  start_sector;
    uint8_t  mixed;         // Bits for high addresses
};
```

---

## Format Detection

### Magic Bytes Table

| Format | Offset | Magic | Length |
|--------|--------|-------|--------|
| SCP | 0 | `SCP` | 3 |
| HFE | 0 | `HXCPICFE` | 8 |
| A2R | 0 | `A2R2`/`A2R3` | 4 |
| WOZ | 0 | `WOZ1`/`WOZ2` | 4 |
| STX | 0 | `RSY\0` | 4 |
| TD0 | 0 | `TD`/`td` | 2 |
| IMD | 0 | `IMD ` | 4 |
| ATR | 0 | `0x96 0x02` | 2 |
| IPF | 0 | `CAPS` | 4 |

### Size-Based Detection

| Size (bytes) | Likely Format |
|--------------|---------------|
| 174,848 | D64 (35 tracks) |
| 175,531 | D64 with errors |
| 196,608 | D64 (40 tracks) |
| 368,640 | PC 360K |
| 737,280 | PC 720K / Atari ST |
| 901,120 | Amiga DD |
| 1,228,800 | PC 1.2M |
| 1,474,560 | PC 1.44M |
| 1,802,240 | Amiga HD |

### Detection Algorithm

```c
int detect_format(const uint8_t *data, size_t size) {
    int scores[FORMAT_COUNT] = {0};
    
    // 1. Check magic bytes
    for (int f = 0; f < FORMAT_COUNT; f++) {
        if (check_magic(data, size, formats[f].magic)) {
            scores[f] += 50;
        }
    }
    
    // 2. Check size
    for (int f = 0; f < FORMAT_COUNT; f++) {
        if (size == formats[f].expected_size) {
            scores[f] += 30;
        }
    }
    
    // 3. Validate structure
    for (int f = 0; f < FORMAT_COUNT; f++) {
        if (scores[f] > 0) {
            scores[f] += validate_structure(data, size, f);
        }
    }
    
    // Return highest score
    return find_max_score(scores);
}
```

---

## Appendix: Encoding Reference

### MFM Encoding

| Data | Clock | MFM Pattern |
|------|-------|-------------|
| 1 | 0 | 01 |
| 0 after 1 | 0 | 00 |
| 0 after 0 | 1 | 10 |

**Cell Timing (HD):**
- Short: 4µs (two 1-bits)
- Medium: 6µs (1-0 or 0-1)
- Long: 8µs (0-0 with clock)

### GCR Encoding (Commodore)

**5-to-4 Table:**
| Nybble | GCR |
|--------|-----|
| 0x0 | 01010 |
| 0x1 | 01011 |
| 0x2 | 10010 |
| ... | ... |
| 0xF | 10101 |

### Apple GCR (6-and-2)

- 6 bits data → 8 bits disk
- 342 bytes per sector
- Checksum at end of sector

---

*Document generated: 2026-01-03 | UFT v4.1.0*
