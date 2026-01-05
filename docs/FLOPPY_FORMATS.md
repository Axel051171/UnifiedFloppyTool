# UFT Extraction v18 - Floppy Disk Format Reference

## Overview

Comprehensive floppy disk format database extracted from Wikipedia "List of floppy disk formats" and related technical sources.

## Physical Media Types

### 8-inch Floppy Disks (1971-1980s)

| Format | Capacity | Encoding | Sides | Tracks | Sectors | Sector Size |
|--------|----------|----------|-------|--------|---------|-------------|
| IBM 3740 | 250 KB | FM | 1 | 77 | 26 | 128 |
| IBM System/34 | 1.0 MB | MFM | 2 | 77 | 26 | 256 |
| DEC RX01 | 256 KB | FM | 1 | 77 | 26 | 128 |
| DEC RX02 | 512 KB | M2FM | 1 | 77 | 26 | 256 |

### 5.25-inch Floppy Disks (1976-1990s)

| Platform | Format | Capacity | Encoding | Sides | Tracks | Sectors |
|----------|--------|----------|----------|-------|--------|---------|
| IBM PC | DD | 160K | MFM | 1 | 40 | 8×512 |
| IBM PC | DD | 180K | MFM | 1 | 40 | 9×512 |
| IBM PC | DD | 320K | MFM | 2 | 40 | 8×512 |
| IBM PC | DD | 360K | MFM | 2 | 40 | 9×512 |
| IBM PC AT | HD | 1.2M | MFM | 2 | 80 | 15×512 |
| Apple II | SS | 113K | GCR 5-3 | 1 | 35 | 13×256 |
| Apple II | SS | 140K | GCR 6-2 | 1 | 35 | 16×256 |
| C64 1541 | SS | 170K | GCR | 1 | 35 | Variable |
| Atari 810 | SD | 90K | FM | 1 | 40 | 18×128 |
| Atari 1050 | ED | 130K | FM | 1 | 40 | 26×128 |
| BBC Micro | DD | 200K | MFM | 1 | 80 | 10×256 |

### 3.5-inch Floppy Disks (1982-2000s)

| Platform | Format | Capacity | Encoding | Sides | Tracks | Sectors |
|----------|--------|----------|----------|-------|--------|---------|
| IBM PC | DD | 720K | MFM | 2 | 80 | 9×512 |
| IBM PC | HD | 1.44M | MFM | 2 | 80 | 18×512 |
| IBM PC | ED | 2.88M | MFM | 2 | 80 | 36×512 |
| Macintosh | DD | 400K | GCR | 1 | 80 | Variable |
| Macintosh | DD | 800K | GCR | 2 | 80 | Variable |
| Amiga | DD | 880K | MFM | 2 | 80 | 11×512 |
| Amiga | HD | 1.76M | MFM | 2 | 80 | 22×512 |
| Atari ST | DD | 720K | MFM | 2 | 80 | 9×512 |
| NEC PC-98 | HD | 1.25M | MFM | 2 | 77 | 8×1024 |

## Data Encoding Methods

### FM (Frequency Modulation) - Single Density

FM encodes data with one clock pulse before each data bit:

```
Cell:   [C][D][C][D][C][D][C][D]...
Clock:   1  ?  1  ?  1  ?  1  ?
Data:       D0    D1    D2    D3

Data rate: 125 kbit/s (62.5 kbit/s effective)
Cell time: 4 µs
```

### MFM (Modified Frequency Modulation) - Double Density

MFM removes redundant clocks - clock only between consecutive 0s:

```
Rule: Clock = 1 if (previous_data = 0 AND current_data = 0)

Example: Data 0xA5 = 10100101
Encoded: 0100 0100 1001 0100 0100 1001 = 0x4494

Data rate: 250 kbit/s
Cell time: 2 µs
```

### GCR (Group Coded Recording)

#### Apple 6-and-2 GCR

Encodes 6 data bits to 8 disk bits:
- MSB must be 1
- Max 2 consecutive 0 bits

```
64 valid bytes: 0x96-0xFF (with gaps)
Disk nibble: [1xxxxxx] where x has ≤2 consecutive 0s
```

#### Commodore GCR

Encodes 4 data bits to 5 disk bits:

| Nibble | GCR | Nibble | GCR |
|--------|-----|--------|-----|
| 0 | 01010 | 8 | 01001 |
| 1 | 01011 | 9 | 11001 |
| 2 | 10010 | A | 11010 |
| 3 | 10011 | B | 11011 |
| 4 | 01110 | C | 01101 |
| 5 | 01111 | D | 11101 |
| 6 | 10110 | E | 11110 |
| 7 | 10111 | F | 10101 |

## Zone Bit Recording

### Commodore 1541 Speed Zones

The 1541 uses variable sectors per track:

| Zone | Tracks | Sectors | Speed | Data Rate |
|------|--------|---------|-------|-----------|
| 1 | 1-17 | 21 | 3 | Fastest |
| 2 | 18-24 | 19 | 2 | |
| 3 | 25-30 | 18 | 1 | |
| 4 | 31-35 | 17 | 0 | Slowest |

Total: 683 sectors × 256 bytes = 174,848 bytes

### Macintosh Variable Speed

Mac 400K/800K use CLV with variable RPM:

| Tracks | Sectors | RPM |
|--------|---------|-----|
| 0-15 | 12 | 394 |
| 16-31 | 11 | 429 |
| 32-47 | 10 | 472 |
| 48-63 | 9 | 524 |
| 64-79 | 8 | 590 |

## Track Layout (MFM)

### Standard IBM PC Format

```
┌─────────────────────────────────────────────────────────────┐
│ Gap4a │ Sync │ IAM │ Gap1 │ Sector 1 │ ... │ Sector N │ Gap4b │
└─────────────────────────────────────────────────────────────┘

Gap4a:  80 bytes of 0x4E (pre-index)
Sync:   12 bytes of 0x00
IAM:    3× 0xC2 + 0xFC (index address mark)
Gap1:   50 bytes of 0x4E

Sector:
┌───────┬──────┬──────┬─────┬──────┬──────┬──────┬──────┬──────┐
│ Sync  │ IDAM │  ID  │ CRC │ Gap2 │ Sync │ DAM  │ Data │ CRC  │
│ 12×00 │ A1×3 │ CHSN │ 2B  │ 22×4E│ 12×00│ A1×3 │ 512B │ 2B   │
│       │ +FE  │      │     │      │      │ +FB  │      │      │
└───────┴──────┴──────┴─────┴──────┴──────┴──────┴──────┴──────┘

Gap3:   54 bytes of 0x4E (inter-sector)
Gap4b:  Fill to track end with 0x4E
```

### Sector ID Field (CHRN)

| Byte | Name | Description |
|------|------|-------------|
| C | Cylinder | Track number (0-based) |
| H | Head | Side number (0 or 1) |
| R | Record | Sector number (usually 1-based) |
| N | Number | Size code (0=128, 1=256, 2=512, 3=1024) |

## CRC Algorithms

### CRC-16-CCITT (IBM MFM)

```c
Polynomial: 0x1021 (x^16 + x^12 + x^5 + 1)
Initial:    0xFFFF
Input:      ID mark + CHRN (for header)
            Data mark + data (for data)
```

### Calculation

```c
uint16_t crc16(const uint8_t *data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000)
                crc = (crc << 1) ^ 0x1021;
            else
                crc <<= 1;
        }
    }
    return crc;
}
```

## Common Image File Sizes

| Size (bytes) | Format(s) |
|--------------|-----------|
| 92,160 | Atari SD (90K) |
| 102,400 | BBC DFS SS/40 (100K) |
| 133,120 | Atari ED (130K) |
| 143,360 | Apple DOS 3.3 (140K) |
| 163,840 | PC 160K |
| 174,848 | C64 1541 (170K) |
| 184,320 | PC 180K, Atari DD |
| 204,800 | BBC DFS SS/80 (200K) |
| 327,680 | PC 320K |
| 368,640 | PC 360K, Atari ST SS |
| 409,600 | Mac 400K, BBC DS/40 |
| 737,280 | PC 720K, Atari ST DS |
| 819,200 | Mac 800K, C128 1581 |
| 901,120 | Amiga DD (880K) |
| 1,228,800 | PC 1.2M |
| 1,474,560 | PC 1.44M |
| 1,720,320 | DMF (1.68M) |
| 1,802,240 | Amiga HD (1.76M) |
| 2,949,120 | PC 2.88M |

## Platform-Specific Notes

### IBM PC
- Sector numbering starts at 1
- FAT12 filesystem standard
- Boot sector at sector 1, track 0, head 0

### Amiga
- Sector numbering starts at 0
- No inter-sector gaps (track written as unit)
- OFS (Original) or FFS (Fast) filesystem
- Boot block at tracks 0-1

### Commodore 64
- GCR encoding with zone bit recording
- Track 18 reserved for directory
- BAM (Block Availability Map) at track 18, sector 0

### Apple II
- GCR encoding (5-and-3 or 6-and-2)
- VTOC at track 17, sector 0
- Catalog at track 17, sectors 15-1

### Atari 8-bit
- Sector numbering starts at 1
- VTOC at sector 360
- Directory at sectors 361-368
- DD boot sectors are 128 bytes (first 3 only)

## References

1. Wikipedia - List of floppy disk formats
2. MAME floppy documentation
3. HxC Floppy Emulator documentation
4. Archive.org File Format Wiki
5. Platform-specific technical manuals
