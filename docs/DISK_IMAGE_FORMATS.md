# UFT v19 - Floppy Disk Image Formats

## Overview

This extraction package provides comprehensive support for classic floppy disk image formats used in digital preservation. The code is derived from analysis of:

- **ImageDisk (IMD)** - Dave Dunfield's disk archival format
- **Teledisk (TD0)** - Sydex's proprietary format (reverse-engineered)
- **DMK** - David Keil's TRS-80 raw track format
- **FDC** - Low-level floppy disk controller interface

## Formats Supported

### 1. ImageDisk (IMD)

**File Extension:** `.IMD`

**Features:**
- Complete sector-level disk images
- Support for FM (SD) and MFM (DD/HD) encoding
- Multiple data rates: 250, 300, 500 kbps
- Variable sector sizes: 128 to 8192 bytes
- Compressed sectors (all-same-value optimization)
- Deleted data address marks
- Error flagging for bad sectors
- Human-readable ASCII header with timestamp
- Optional comment block

**File Structure:**
```
IMD v.vv: dd/mm/yyyy hh:mm:ss    (ASCII header)
Comment text...                   (optional, variable length)
0x1A                              (EOF marker)
[Track records...]
```

**Track Record:**
```
Mode       (1 byte)  - Data rate and density
Cylinder   (1 byte)  - Physical cylinder (0-255)
Head       (1 byte)  - Physical head + optional flags
NSectors   (1 byte)  - Sectors in track
SectorSize (1 byte)  - Size code (128 << code)
SectorMap  (N bytes) - Sector numbering
[CylMap]   (N bytes) - Optional cylinder map
[HeadMap]  (N bytes) - Optional head map
[Sector data records...]
```

**Mode Values:**
| Mode | Rate | Encoding |
|------|------|----------|
| 0 | 500K | FM |
| 1 | 300K | FM |
| 2 | 250K | FM |
| 3 | 500K | MFM |
| 4 | 300K | MFM |
| 5 | 250K | MFM |

### 2. Teledisk (TD0)

**File Extension:** `.TD0`

**Features:**
- Proprietary format from Sydex (now abandoned)
- Optional LZSS-Huffman compression ("Advanced Compression")
- Multi-volume support for large disks
- RLE sector data compression
- Comment block with timestamp
- Drive type and stepping information

**File Structure:**
```
Header (12 bytes)
[Comment block] (if present)
Track records...
0xFF (end marker)
```

**Compression:**
- Signature "TD" = Normal (uncompressed)
- Signature "td" = Advanced compression (LZSS-Huffman)

**LZSS Parameters:**
- Ring buffer: 4096 bytes
- Look-ahead: 60 bytes
- Threshold: 2
- Initialized with spaces (0x20)

**Sector Flags:**
| Bit | Meaning |
|-----|---------|
| 0x01 | Duplicated sector |
| 0x02 | CRC error |
| 0x04 | Deleted address mark |
| 0x10 | Not allocated (DOS mode) |
| 0x20 | No data field |
| 0x40 | No ID field |

### 3. DMK Format

**File Extension:** `.DMK`

**Features:**
- Raw track data including all address marks
- IDAM (ID Address Mark) pointer table per track
- Supports mixed FM/MFM tracks
- Preserves copy protection schemes
- Created for TRS-80 emulators

**File Structure:**
```
Header (16 bytes)
Track 0, Side 0:
  IDAM pointers (128 bytes)
  Raw track data
Track 0, Side 1:
  IDAM pointers (128 bytes)
  Raw track data
...
```

**Header:**
```c
struct dmk_header {
    uint8_t  write_protect;   // 0x00=R/W, 0xFF=RO
    uint8_t  tracks;          // Number of tracks
    uint16_t track_length;    // Bytes per track (LE)
    uint8_t  flags;           // Disk flags
    uint8_t  reserved[7];
    uint32_t native_flag;     // 0x12345678 if native
};
```

**Flags:**
| Bit | Meaning |
|-----|---------|
| 0x10 | Single-sided |
| 0x40 | Single-density (FM) |
| 0x80 | Ignore density |

### 4. FDC Controller Interface

**Features:**
- NEC µPD765 compatible register definitions
- Standard PC floppy controller I/O ports
- Drive timing parameters
- Gap length tables for all format combinations
- Pre-defined format parameters for common disk types

**Standard Formats Supported:**
- 5.25" DD: 160K, 180K, 320K, 360K
- 5.25" HD: 1.2MB
- 3.5" DD: 720K
- 3.5" HD: 1.44MB
- 3.5" ED: 2.88MB
- 8" SD/DD: Various CP/M formats
- Special: DMF (1.68MB), XDF, NEC PC-98

## Gap Length Tables

Gap lengths are critical for reliable read/write operations. The tables provide values for both read/write (gap3_rw) and format (gap3_fmt) operations.

### 8" FM Gaps
| Sector Size | Max Sectors | Gap R/W | Gap Fmt |
|-------------|-------------|---------|---------|
| 128 | 26 | 0x07 | 0x1B |
| 256 | 15 | 0x0E | 0x2A |
| 512 | 8 | 0x1B | 0x3A |
| 1024 | 4 | 0x47 | 0x8A |

### 5.25"/3.5" MFM Gaps
| Sector Size | Max Sectors | Gap R/W | Gap Fmt |
|-------------|-------------|---------|---------|
| 256 | 18 | 0x0A | 0x0C |
| 512 | 9 | 0x18 | 0x40 |
| 512 | 18 | 0x1B | 0x54 |
| 512 | 21 | 0x0C | 0x1C |
| 1024 | 4 | 0x8D | 0xF0 |

## Data Rates and Drive Types

### Data Rates
| Rate | Usage |
|------|-------|
| 250 kbps | DD drives (3.5", 5.25") |
| 300 kbps | DD on HD 5.25" (360 RPM) |
| 500 kbps | HD drives, 8" drives |
| 1000 kbps | ED drives (2.88MB) |

### Rotation Speeds
| Drive Type | Speed |
|------------|-------|
| 5.25" DD | 300 RPM |
| 5.25" HD | 360 RPM |
| 3.5" all | 300 RPM |
| 8" | 360 RPM |

## CRC-16 Algorithm

All formats use CRC-16-CCITT:
- Polynomial: 0x1021 (x^16 + x^12 + x^5 + 1)
- Initial value: 0xFFFF
- Bit order varies by format (DMK uses reversed)

## Usage Examples

### Reading an IMD file
```c
#include "uft_imd.h"

uft_imd_image_t img;
uft_imd_init(&img);

if (uft_imd_read("disk.imd", &img) == 0) {
    printf("Cylinders: %d\n", img.num_cylinders);
    printf("Heads: %d\n", img.num_heads);
    printf("Total sectors: %d\n", img.total_sectors);
    
    // Read sector
    uft_imd_track_t* track = uft_imd_get_track(&img, 0, 0);
    uint8_t buffer[512];
    uft_imd_read_sector(track, 1, buffer, sizeof(buffer));
}

uft_imd_free(&img);
```

### Reading a TD0 file
```c
#include "uft_td0.h"

uft_td0_image_t img;
uft_td0_init(&img);

if (uft_td0_read("disk.td0", &img) == 0) {
    printf("Compressed: %s\n", 
           img.advanced_compression ? "yes" : "no");
    printf("Tracks: %d\n", img.num_tracks);
}

uft_td0_free(&img);
```

### Getting Gap Lengths
```c
#include "uft_fdc.h"

uint8_t gap_rw, gap_fmt;

// Get gaps for 9 sectors of 512 bytes, MFM, 5.25"/3.5"
if (uft_fdc_get_gaps(true, false, 2, 9, &gap_rw, &gap_fmt)) {
    printf("Gap3 R/W: 0x%02X\n", gap_rw);
    printf("Gap3 Format: 0x%02X\n", gap_fmt);
}
```

## References

- Dave Dunfield's ImageDisk documentation
- Teledisk reverse-engineering notes (TD0NOTES.TXT)
- Linux kernel floppy driver (floppy.c)
- NEC µPD765 datasheet
- Intel 82077AA datasheet

## License

This extraction is based on publicly available format specifications and
reverse-engineering work. The IMD format specification is public domain
per Dave Dunfield's license. The TD0 format information is derived from
community reverse-engineering efforts.

## Version History

- v19.0 - Initial extraction from ImageDisk, Teledisk, DMK sources
