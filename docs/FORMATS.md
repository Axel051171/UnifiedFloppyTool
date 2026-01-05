# UFT Format Catalog

**Version:** 3.3.3  
**Last Updated:** 2026-01-03

Comprehensive catalog of all supported disk image and flux formats in UnifiedFloppyTool.

## Format Summary

| Category | Formats | Read | Write | Flux |
|----------|---------|------|-------|------|
| **Commodore** | 18 | ✓ | ✓ | ✓ |
| **Atari** | 8 | ✓ | ✓ | ✓ |
| **Apple** | 6 | ✓ | ✓ | ✓ |
| **Amiga** | 8 | ✓ | ✓ | ✓ |
| **IBM PC** | 12 | ✓ | ✓ | ✓ |
| **Flux Formats** | 13 | ✓ | P | - |
| **Amstrad/CPC** | 5 | ✓ | ✓ | ✓ |
| **BBC Micro** | 2 | ✓ | ✓ | - |
| **TRS-80** | 4 | ✓ | ✓ | - |
| **TI-99** | 3 | ✓ | ✓ | - |
| **PC-98** | 6 | ✓ | ✓ | - |
| **Misc** | 22 | ✓ | P | - |
| **Total** | **107** | | | |

Legend: ✓ = Full support, P = Partial support, - = Not applicable

---

## Commodore Formats (18)

### D64 - 1541 Disk Image
```
Extension:    .d64
Tracks:       35 (standard), 40 (extended)
Sides:        1
Encoding:     GCR
Sector Size:  256 bytes
Capacity:     170,752 bytes (35 tracks)
              192,256 bytes (40 tracks)
```

**Variants:**
- D64 Standard (683 sectors)
- D64 Extended (768 sectors)
- D64 with error info (+683/768 bytes)
- D64 40-track (SpeedDOS, Dolphin DOS)

**Detection:** File size: 174,848 / 175,531 / 196,608 / 197,376 bytes

### G64 - 1541 GCR Track Image
```
Extension:    .g64
Tracks:       35-42
Sides:        1
Encoding:     Raw GCR
Capacity:     Variable (typically 300-400 KB)
Features:     Preserves copy protection, timing
```

**Header:** `GCR-1541` (8 bytes)

### D71 - 1571 Disk Image
```
Extension:    .d71
Tracks:       35
Sides:        2
Encoding:     GCR
Capacity:     341,504 bytes
```

### D81 - 1581 Disk Image
```
Extension:    .d81
Tracks:       80
Sides:        2
Encoding:     MFM
Sector Size:  512 bytes
Capacity:     819,200 bytes
```

### D80 - 8050 Disk Image
```
Extension:    .d80
Tracks:       77
Sides:        1
Encoding:     GCR
Capacity:     533,248 bytes
```

### D82 - 8250 Disk Image
```
Extension:    .d82
Tracks:       77
Sides:        2
Encoding:     GCR
Capacity:     1,066,496 bytes
```

### NIB - Nibbler Raw Image
```
Extension:    .nib
Tracks:       35-42
Sides:        1
Encoding:     Raw GCR
Track Size:   8192 bytes (fixed)
Capacity:     ~330 KB
```

**Note:** Fixed-size tracks may truncate or pad data

### NBZ - Compressed NIB
```
Extension:    .nbz
Compression:  zlib
Base Format:  NIB
```

---

## Atari Formats (8)

### ATR - Atari 8-bit Disk Image
```
Extension:    .atr
Magic:        0x96 0x02 (little-endian: 0x0296)
Sector Size:  128 or 256 bytes
Densities:    Single (90 KB), Enhanced (130 KB), Double (180 KB)
```

**Variants:**
- ATR Single Density (720 sectors × 128 bytes)
- ATR Enhanced Density (1040 sectors × 128 bytes)
- ATR Double Density (720 sectors × 256 bytes)
- ATR High Density (1440 sectors × 256 bytes)

### XFD - Atari Raw Sector Image
```
Extension:    .xfd
No Header:    Raw sector data
Sector Size:  128 or 256 bytes
```

### DCM - DiskComm Image
```
Extension:    .dcm
Compression:  RLE-based
Features:     Multi-file archives
```

### PRO - APE Pro Image
```
Extension:    .pro
Features:     Preserves bad sectors, protection
```

### ATX - Atari Protected Image
```
Extension:    .atx
Magic:        AT8X
Features:     Full protection preservation
              Timing information
              Weak bits
```

### ST - Atari ST Sector Image
```
Extension:    .st
Tracks:       80
Sides:        1 or 2
Sectors:      9 (DD) or 10 (custom)
Sector Size:  512 bytes
Capacity:     360-800 KB
```

### STX - Pasti Protected Image
```
Extension:    .stx
Magic:        RSY\0
Features:     Track timing
              Fuzzy bits
              Protection schemes
              Sector timing
```

**Capabilities:**
- Read: Full
- Write: Partial (protection may not round-trip)
- Protection detection: Yes

### MSA - Magic Shadow Archiver
```
Extension:    .msa
Magic:        0x0E 0x0F
Compression:  RLE
Base:         ST format
```

---

## Apple Formats (6)

### DSK - Apple II DOS 3.3
```
Extension:    .dsk, .do
Tracks:       35
Sides:        1
Sectors:      16
Sector Size:  256 bytes
Encoding:     GCR (6-and-2)
Capacity:     143,360 bytes
```

### PO - Apple ProDOS Order
```
Extension:    .po
Same as:      DSK with different sector interleave
Sectors:      16 (ProDOS order)
```

### NIB - Apple Nibble Image
```
Extension:    .nib
Tracks:       35
Track Size:   6,656 bytes
Encoding:     Raw GCR
Capacity:     232,960 bytes
```

### 2MG - Universal Apple Image
```
Extension:    .2mg, .2img
Magic:        2IMG
Version:      1
```

**Variants (detected by format byte):**
- DOS Order (format = 0)
- ProDOS Order (format = 1)
- Nibble (format = 2)

**Header Structure:**
```c
struct _2mg_header {
    char     magic[4];      // "2IMG"
    char     creator[4];    // Creator ID
    uint16_t header_size;   // 64 bytes
    uint16_t version;       // 1
    uint32_t format;        // 0=DOS, 1=ProDOS, 2=NIB
    uint32_t flags;
    uint32_t blocks;        // ProDOS blocks
    uint32_t data_offset;
    uint32_t data_length;
    uint32_t comment_offset;
    uint32_t comment_length;
    uint32_t creator_offset;
    uint32_t creator_length;
    uint8_t  reserved[16];
};
```

### WOZ - Applesauce Flux Image
```
Extension:    .woz
Magic:        WOZ1 or WOZ2
Features:     Flux-level preservation
              Weak bits
              Cross-track sync
              Write protection
```

**WOZ 1.0:** 
- Quarter-track support
- Basic metadata

**WOZ 2.1:**
- Optimal bit timing
- Large tracks
- Boot sector tags
- Enhanced metadata

### HDV - Apple Hard Disk Image
```
Extension:    .hdv
Blocks:       Variable
Block Size:   512 bytes
Filesystem:   ProDOS
```

---

## Amiga Formats (8)

### ADF - Amiga Disk File
```
Extension:    .adf
Tracks:       80
Sides:        2
Sectors:      11 (DD) or 22 (HD)
Sector Size:  512 bytes
Encoding:     MFM (Amiga-specific sync)
```

**Variants:**
- ADF DD: 901,120 bytes (880 KB)
- ADF HD: 1,802,240 bytes (1760 KB)
- ADF Extended: Includes track checksums

**Filesystem Detection:**
- OFS: `DOS\0` at block 0
- FFS: `DOS\1` at block 0
- DirCache OFS: `DOS\2`
- DirCache FFS: `DOS\3`
- Int'l OFS: `DOS\4`
- Int'l FFS: `DOS\5`

### ADZ - Compressed ADF
```
Extension:    .adz
Compression:  gzip
Base Format:  ADF
```

### DMS - DiskMasher System
```
Extension:    .dms
Compression:  Multiple algorithms (RLE, Quick, Medium, Deep, Heavy)
Features:     Multi-disk archives
              Password protection
              File dates
```

### FDI - Formatted Disk Image
```
Extension:    .fdi
Tracks:       80
Features:     Cross-platform support
              Multiple disk types
```

### HDF - Hard Disk File
```
Extension:    .hdf
Block Size:   512 bytes
Filesystem:   OFS, FFS, PFS
Features:     RDB (Rigid Disk Block) support
```

### IPF - Interchangeable Preservation Format
```
Extension:    .ipf
Developer:    SPS (Software Preservation Society)
Features:     Complete flux preservation
              Copy protection
              Weak bits
              Density variations
```

**IPF Types:**
- CAPS (Classic Amiga Preservation Society)
- CTRaw (Raw captures)
- SPS mastered

### MFM - Raw MFM Stream
```
Extension:    .mfm
Encoding:     Raw MFM data
Features:     Tool-specific variants
```

### ARAW - AmigaOS Raw
```
Extension:    .raw
Track Data:   Raw track dumps
```

---

## IBM PC Formats (12)

### IMG - Raw Sector Image
```
Extension:    .img, .ima
Sector Size:  512 bytes
Encoding:     MFM
```

**Common Geometries:**

| Type | Tracks | Sides | Sectors | Capacity |
|------|--------|-------|---------|----------|
| 160K | 40 | 1 | 8 | 160 KB |
| 180K | 40 | 1 | 9 | 180 KB |
| 320K | 40 | 2 | 8 | 320 KB |
| 360K | 40 | 2 | 9 | 360 KB |
| 720K | 80 | 2 | 9 | 720 KB |
| 1.2M | 80 | 2 | 15 | 1200 KB |
| 1.44M | 80 | 2 | 18 | 1440 KB |
| 2.88M | 80 | 2 | 36 | 2880 KB |

### IMA - Raw Image (DOS)
```
Extension:    .ima
Same as:      IMG
```

### DSK - Generic Disk Image
```
Extension:    .dsk
Context:      Multiple platforms (detect by size/content)
```

### VFD - Virtual Floppy Disk
```
Extension:    .vfd
Same as:      IMG
Usage:        Windows Virtual PC
```

### FLP - Floppy Image
```
Extension:    .flp
Same as:      IMG
```

### DMF - Distribution Media Format
```
Extension:    .dmf
Tracks:       80
Sides:        2
Sectors:      21
Capacity:     1,680 KB (1.68 MB)
Usage:        Microsoft distributions
```

### XDF - Extended Density Format
```
Extension:    .xdf
Capacity:     1,840 KB (1.84 MB)
Features:     Custom sector layout
              Used by OS/2, PC DOS 7
```

### TD0 - Teledisk
```
Extension:    .td0
Magic:        TD (uncompressed) or td (compressed)
Features:     Multi-format support
              Comment storage
              CRC protection
```

### IMD - ImageDisk
```
Extension:    .imd
Magic:        IMD
Features:     Full geometry preservation
              Sector timing
              Deleted data marks
              Bad sector flags
```

### CQM - CopyQM
```
Extension:    .cqm
Compression:  RLE
Features:     Blind mode support
              Comment storage
```

### FDI - Floppy Disk Image (PC)
```
Extension:    .fdi
Multiple:     See Amiga FDI
```

### DMK - TRS-80/CoCo Disk Image
```
Extension:    .dmk
Tracks:       35-80
Features:     Single/double density
              IDAM tracking
              Raw MFM/FM data
```

**Header (16 bytes):**
```c
struct dmk_header {
    uint8_t  write_protect;
    uint8_t  tracks;
    uint16_t track_length;  // LE
    uint8_t  flags;
    uint8_t  reserved[11];
};
```

---

## Flux Formats (13)

### SCP - SuperCard Pro
```
Extension:    .scp
Magic:        SCP
Features:     Multi-revolution capture
              25ns resolution
              Index hole timing
              Checksum verification
```

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
    uint8_t  bit_cell_enc;
    uint8_t  heads;
    uint8_t  resolution;
    uint32_t checksum;
};
```

**Disk Types:**
- 0x00: Commodore 64
- 0x01: Commodore Amiga
- 0x02: Atari ST
- 0x03: Atari 800
- 0x04: Apple II
- 0x05: Apple IIgs
- 0x06: Apple Mac
- 0x07: PC 360K
- 0x08: PC 720K
- 0x09: PC 1.2M
- 0x0A: PC 1.44M
- 0x10: TRS-80
- 0x20: Other

### KF - KryoFlux Stream
```
Extension:    .raw (in folder)
Structure:    track00.0.raw, track00.1.raw, etc.
Resolution:   ~42ns (SCK/2)
Features:     Index hole
              OOB (out-of-band) markers
```

### HFE - HxC Floppy Emulator
```
Extension:    .hfe
Magic:        HXCPICFE
Version:      1, 2, 3
Features:     Track interleaving
              MFM/FM encoding
              Write support
```

**HFE v3 Features:**
- HDDD A2 variant
- HD support
- Enhanced metadata

### MFI - MAME Floppy Image
```
Extension:    .mfi
Magic:        MFI2
Features:     Cell-level data
              Variant storage
```

### A2R - Applesauce Raw
```
Extension:    .a2r
Magic:        A2R2
Features:     Raw flux timing
              Multi-capture
```

### EDD - Electronic Disk Drive
```
Extension:    .edd
Features:     Apple II protection
```

### ANA - Analyser
```
Extension:    .ana
Features:     FM Analyzer format
```

### FLX - Flux Image
```
Extension:    .flx
Features:     Generic flux container
```

### RAW - Raw Flux Data
```
Extension:    .raw
Context:      Multiple formats use .raw
```

### CTR - CAPS/SPS Raw
```
Extension:    .ctr
Magic:        CTRAW
Features:     IPF raw captures
```

### PRI - PASTI Raw Image
```
Extension:    .pri
Features:     Pasti raw data
```

### CT - Catweasel
```
Extension:    .ct
Features:     Catweasel controller format
```

### FDX - PC-FDC
```
Extension:    .fdx
Features:     PC floppy controller dumps
```

---

## Amstrad/CPC Formats (5)

### DSK - CPC Disk Image
```
Extension:    .dsk
Magic:        MV - CPC (standard)
              EXTENDED CPC DSK (extended)
```

### EDSK - Extended DSK
```
Extension:    .dsk
Magic:        EXTENDED CPC DSK File\r\nDisk-Info\r\n
Features:     Variable sector sizes
              Weak sectors
              Random data
              ST1/ST2 FDC status
```

**Track-Info Block:**
```c
struct edsk_track_info {
    char     magic[13];     // "Track-Info\r\n"
    uint8_t  unused[3];
    uint8_t  track;
    uint8_t  side;
    uint8_t  unused2[2];
    uint8_t  sector_size;   // log2(size)-7
    uint8_t  sector_count;
    uint8_t  gap3_length;
    uint8_t  filler_byte;
    // Sector info table follows
};
```

### CPT - Compacted DSK
```
Extension:    .cpt
Compression:  Yes
Base:         DSK
```

### RAW - CPC Raw
```
Extension:    .raw
Features:     Raw track dumps
```

### SNA - Snapshot (with disk)
```
Extension:    .sna
Features:     Machine state + disk
```

---

## BBC Micro Formats (2)

### SSD - Single-Sided Disk
```
Extension:    .ssd
Tracks:       40 or 80
Sides:        1
Sectors:      10
Sector Size:  256 bytes
Filesystem:   DFS
Capacity:     100-200 KB
```

### DSD - Double-Sided Disk
```
Extension:    .dsd
Tracks:       40 or 80
Sides:        2
Sectors:      10
Sector Size:  256 bytes
Capacity:     200-400 KB
```

**Note:** BBC uses FM encoding (single density)

---

## TRS-80 Formats (4)

### JV1 - Jeff Vavasour Format 1
```
Extension:    .jv1
Sectors:      10 per track
Sector Size:  256 bytes
Density:      Single
```

### JV3 - Jeff Vavasour Format 3
```
Extension:    .jv3
Features:     Mixed density
              Sector header info
              DAM tracking
```

### DMK - Disk Master format
```
Extension:    .dmk
See:          IBM PC DMK
```

### CAS - Cassette
```
Extension:    .cas
Features:     Tape image (not floppy)
```

---

## TI-99 Formats (3)

### DSK - TI Disk Image
```
Extension:    .dsk
Tracks:       40
Sides:        1 or 2
Sectors:      9
Sector Size:  256 bytes
```

**Detection:** Sector 0 contains disk name (10 chars) and sector count

### V9T9 - PC99/V9T9 Format
```
Extension:    .dsk
Same as:      TI DSK
Origin:       V9T9 emulator
```

### TIFILE - TI Files
```
Extension:    .tifiles, .tfi
Features:     Single file container
              TIFILES header
```

---

## PC-98 Formats (6)

### FDI - PC-98 Disk Image
```
Extension:    .fdi
Tracks:       77
Sides:        2
Sector Size:  256 or 1024 bytes
```

### HDM - Hard Disk Module
```
Extension:    .hdm
Capacity:     1232 KB (1.2 MB logical)
Sectors:      8 × 1024 bytes
```

### D88 - D88/D68/D98 Format
```
Extension:    .d88, .d68, .d98, .1dd
Magic:        None (header-based)
Features:     Multi-platform (FM, PC-88, PC-98, X1)
              Variable geometry
              Write protection flag
```

**Header (688 bytes):**
```c
struct d88_header {
    char     name[17];
    uint8_t  reserved[9];
    uint8_t  write_protect;
    uint8_t  media_type;
    uint32_t disk_size;
    uint32_t track_offsets[164];
};
```

### NFD - NFD Image
```
Extension:    .nfd
Features:     PC-98 specific
```

### FDD - FDD Image
```
Extension:    .fdd
Features:     Raw sector image
```

### XDF - PC-98 XDF
```
Extension:    .xdf
Capacity:     1.25 MB
```

---

## Miscellaneous Formats (22)

### SAP - Thomson Format
```
Extension:    .sap
Magic:        Various
Platforms:    Thomson TO7, MO5
```

### FD - Sharp X68000
```
Extension:    .fd
Tracks:       77
Capacity:     1232 KB
```

### TRD - TR-DOS Image
```
Extension:    .trd
Tracks:       80
Sides:        2
Sectors:      16
Sector Size:  256 bytes
Platform:     ZX Spectrum
```

### SCL - Sinclair SCL
```
Extension:    .scl
Features:     TR-DOS file archive
```

### OPD - Opus Discovery
```
Extension:    .opd
Platform:     ZX Spectrum
```

### MGT - Miles Gordon Technology
```
Extension:    .mgt
Platform:     SAM Coupé, +D
Capacity:     819,200 bytes
```

### SAD - SAM Disk
```
Extension:    .sad
Platform:     SAM Coupé
```

### DSC - Discovery Cartridge
```
Extension:    .dsc
Platform:     Vectrex
```

### HDI - Anex86 Hard Disk
```
Extension:    .hdi
Platform:     PC-98 emulators
```

### NHD - T98-Next Hard Disk
```
Extension:    .nhd
Platform:     PC-98 emulators
```

### PDI - Puyo Disk Image
```
Extension:    .pdi
Platform:     Various
```

### FLP - AmigaOS Floppy
```
Extension:    .flp
Same as:      ADF
```

### TD - Teledisk (old)
```
Extension:    .td
Same as:      TD0
```

### ORI - Original Image
```
Extension:    .ori
Features:     Copy-protected originals
```

### BIN - Binary Dump
```
Extension:    .bin
Features:     Raw binary (context-dependent)
```

### DDI - DiskDupe Image
```
Extension:    .ddi
Features:     Copy protection
```

### VDI - Virtual Disk Image
```
Extension:    .vdi
Usage:        VirtualBox
```

### VMDK - VMware Disk
```
Extension:    .vmdk
Usage:        VMware
```

### QCOW2 - QEMU Copy-On-Write
```
Extension:    .qcow2
Usage:        QEMU/KVM
```

### CUE/BIN - CD Image
```
Extension:    .cue, .bin
Features:     CD-ROM images (not floppy)
```

### ISO - ISO 9660
```
Extension:    .iso
Features:     CD-ROM filesystem
```

### CHD - MAME CHD
```
Extension:    .chd
Features:     Compressed Hard Disk
Platform:     MAME
```

---

## Format Detection

UFT uses a multi-stage detection system:

### Stage 1: Extension Mapping
```
.adf → Amiga ADF
.d64 → Commodore D64
.st  → Atari ST
.img → IBM IMG (size-based)
```

### Stage 2: Magic Byte Detection
```
"2IMG" at 0x00    → Apple 2MG
"SCP"  at 0x00    → SuperCard Pro
"RSY"  at 0x00    → Pasti STX
"HXCPICFE"        → HxC HFE
"MV - CPC"        → Amstrad DSK
"EXTENDED CPC"    → Amstrad EDSK
```

### Stage 3: Content Analysis
- Boot sector analysis (FAT, OFS, FFS, DOS)
- Geometry inference from file size
- Sector layout patterns
- Filesystem signatures

### Stage 4: Confidence Scoring
Each parser returns confidence 0-100:
- 100: Perfect match (magic + size + content)
- 75-99: Strong match (magic + size)
- 50-74: Probable match (size or extension)
- 25-49: Possible match (extension only)
- 0-24: Unlikely match

---

## API Usage

### Opening a Disk Image
```c
#include <uft/uft_format.h>

uft_image_t* image = uft_image_open("disk.adf", UFT_MODE_READ);
if (!image) {
    printf("Error: %s\n", uft_error_string(uft_get_last_error()));
    return;
}

uft_geometry_t geo;
uft_image_get_geometry(image, &geo);
printf("Format: %s\n", uft_format_name(geo.format));
printf("Tracks: %u, Sides: %u, Sectors: %u\n",
       geo.tracks, geo.sides, geo.sectors_per_track);

uft_image_close(image);
```

### Format Conversion
```c
uft_error_t err = uft_convert("input.scp", "output.adf",
                              UFT_CONV_DECODE_FLUX |
                              UFT_CONV_MULTI_REV);
if (err != UFT_ERR_OK) {
    printf("Conversion failed: %s\n", uft_error_string(err));
}
```

### Format Probing
```c
uft_format_info_t info;
int confidence = uft_probe_format("unknown.dsk", &info);

printf("Detected: %s (confidence: %d%%)\n",
       info.name, confidence);
printf("Capabilities: %s%s%s\n",
       (info.caps & UFT_CAP_READ) ? "R" : "",
       (info.caps & UFT_CAP_WRITE) ? "W" : "",
       (info.caps & UFT_CAP_FLUX) ? "F" : "");
```

---

## Version History

| Version | Changes |
|---------|---------|
| 3.3.3 | Added SCP multi-rev, STX timing, EDSK weak sectors |
| 3.3.2 | Added 2MG variants, WOZ 2.1 |
| 3.3.0 | Added HFE v3, DMK mixed |
| 3.2.0 | Added IPF CTRaw |
| 3.0.0 | Initial 107 format support |

---

## See Also

- [PARSER_GUIDE.md](PARSER_GUIDE.md) - Implementing new format parsers
- [ARCHITECTURE.md](ARCHITECTURE.md) - System architecture
- [API_REFERENCE.md](API_REFERENCE.md) - Complete API documentation
