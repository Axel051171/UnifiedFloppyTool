# Transcopy (.tc) Format Specification

## Version
- Format Version: 1.0
- Document Version: 2026-01-06

## Overview

The Transcopy format is a track-based raw disk image format designed for 
preserving copy-protected floppy disks. It stores complete track data along
with metadata about the disk type, geometry, and per-track flags.

## File Structure

### Header (0x000 - 0x0FF)

| Offset | Size | Type | Description |
|--------|------|------|-------------|
| 0x000 | 2 | char[2] | Signature: "TC" (0x54, 0x43) |
| 0x002 | 32 | char[32] | Comment (null-padded ASCII) |
| 0x022 | 222 | - | Reserved (zero-filled) |

### Geometry Block (0x100 - 0x304)

| Offset | Size | Type | Description |
|--------|------|------|-------------|
| 0x100 | 1 | uint8_t | Disk Type (see Disk Types) |
| 0x101 | 1 | uint8_t | Track Start (0 or 1) |
| 0x102 | 1 | uint8_t | Track End (35, 40, 80, etc.) |
| 0x103 | 1 | uint8_t | Number of Sides (1 or 2) |
| 0x104 | 513 | - | Reserved |

### Tables (0x305 - 0x904)

#### Offset Table (0x305 - 0x504)
- 256 entries × 2 bytes = 512 bytes
- Little-endian uint16_t values
- Track index = track × sides + side
- Value = offset from data start (0x4000) in 256-byte units

#### Length Table (0x505 - 0x704)
- 256 entries × 2 bytes = 512 bytes
- Little-endian uint16_t values
- Track length in bytes

#### Flags Table (0x705 - 0x904)
- 256 entries × 2 bytes = 512 bytes
- Little-endian uint16_t values

**Flag Bits:**
| Bit | Mask | Description |
|-----|------|-------------|
| 0 | 0x0001 | Track present |
| 1 | 0x0002 | Copy protected |
| 2 | 0x0004 | Weak bits present |
| 3 | 0x0008 | CRC errors present |
| 4-15 | - | Reserved |

### Reserved (0x905 - 0x3FFF)

Zero-filled, reserved for future extensions.

### Track Data (0x4000+)

- Track data blocks, 256-byte aligned
- Each track starts at: 0x4000 + (offset[index] × 256)
- Track length from length table

## Disk Types

| Code | Name | Description | Encoding | Typical Track Length |
|------|------|-------------|----------|---------------------|
| 0x00 | Unknown | Unknown type | - | Varies |
| 0x02 | MFM_HD | 1.44MB HD | MFM | 12,500 bytes |
| 0x03 | MFM_DD_360 | 360KB DD | MFM | 6,250 bytes |
| 0x04 | APPLE_GCR | Apple II | GCR | 6,656 bytes |
| 0x05 | FM_SD | Single Density | FM | 3,125 bytes |
| 0x06 | C64_GCR | Commodore 1541 | GCR | 7,928 bytes (max) |
| 0x07 | MFM_DD | 720KB DD | MFM | 6,250 bytes |
| 0x08 | AMIGA_MFM | Amiga | MFM | 12,668 bytes |
| 0x0C | ATARI_FM | Atari 8-bit | FM | Varies |

## Encoding Types

- **MFM** (Modified Frequency Modulation): IBM PC, Amiga
- **GCR** (Group Coded Recording): Commodore, Apple II
- **FM** (Frequency Modulation): Early systems, Atari

## Variable Density Support

Some disk types (notably C64 GCR) have variable density with different
track lengths per zone:

**C64 1541 Speed Zones:**
| Zone | Tracks | Sectors | Bytes/Track |
|------|--------|---------|-------------|
| 0 | 1-17 | 21 | 7,928 |
| 1 | 18-24 | 19 | 7,170 |
| 2 | 25-30 | 18 | 6,792 |
| 3 | 31-35+ | 17 | 6,414 |

## Track Index Calculation

For double-sided disks:
```c
index = track * sides + side;
```

For single-sided disks:
```c
index = track;
```

## Example: Reading Track 18, Side 0 from Double-Sided Disk

1. Calculate index: `18 * 2 + 0 = 36`
2. Read offset from table: `offset = read_le16(data + 0x305 + 36*2)`
3. Read length from table: `length = read_le16(data + 0x505 + 36*2)`
4. Track data location: `track_data = data + 0x4000 + offset * 256`

## Compatibility Notes

- Maximum 256 tracks (index 0-255)
- Maximum track length: 65,535 bytes
- Maximum file size: ~16MB typical
- All multi-byte values are little-endian
