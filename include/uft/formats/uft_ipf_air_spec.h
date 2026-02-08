/**
 * @file uft_ipf_air_spec.h
 * @brief IPF (Interchangeable Preservation Format) Specification
 * @version 1.0.0
 *
 * Complete format specification for IPF disk images.
 * IPF was created by SPS (Software Preservation Society, formerly CAPS).
 * Two encoder variants: CAPS (original) and SPS (extended).
 *
 * Reference: AIR by Jean Louis-Guerin (DrCoolzic)
 * Source: IPFStruct.cs, IPFReader.cs, IPFWriter.cs
 *
 * BYTE ORDER: Big-Endian throughout (network byte order)
 *
 * ╔══════════════════════════════════════════════════════════════════╗
 * ║                    IPF FILE LAYOUT                              ║
 * ╠══════════════════════════════════════════════════════════════════╣
 * ║  CAPS Record     (12 bytes) - File magic                       ║
 * ║  INFO Record     (96 bytes) - Disk metadata                    ║
 * ║  IMGE Record(s)  (80 bytes each) - Track metadata              ║
 * ║  DATA Record(s)  (28 bytes header + extra data) - Track data   ║
 * ║  CTEI Record     (optional) - CT analyzer extension info       ║
 * ║  CTEX Record(s)  (optional) - Per-track CT extension info      ║
 * ╚══════════════════════════════════════════════════════════════════╝
 *
 * All records share a common header format:
 *   [4 bytes] type      - ASCII identifier ("CAPS"/"INFO"/"IMGE"/"DATA"/"CTEI"/"CTEX")
 *   [4 bytes] length    - Total record length including header
 *   [4 bytes] crc       - CRC-32 of entire record (with CRC field zeroed)
 */

#ifndef UFT_IPF_AIR_SPEC_H
#define UFT_IPF_AIR_SPEC_H

/*============================================================================
 * RECORD HEADER (12 bytes, common to all records)
 *
 * Offset  Size  Field    Description
 * ------  ----  -------  ------------------------------------------
 * 0x00    4     type     Record type identifier (ASCII)
 * 0x04    4     length   Total record length in bytes
 * 0x08    4     crc      CRC-32 (computed with bytes 8-11 = 0x00)
 *============================================================================*/

/*============================================================================
 * CAPS RECORD (12 bytes, must be first record)
 *
 * File magic marker. Contains only the common header.
 * type = "CAPS" (0x43 0x41 0x50 0x53)
 * length = 12
 *============================================================================*/

/*============================================================================
 * INFO RECORD (96 bytes)
 *
 * Offset  Size  Field           Description
 * ------  ----  --------------- ------------------------------------------
 * 0x0C    4     mediaType       1 = Floppy Disk
 * 0x10    4     encoderType     1 = CAPS, 2 = SPS
 * 0x14    4     encoderRev      Encoder revision
 * 0x18    4     fileKey         Unique file identifier
 * 0x1C    4     fileRev         File revision
 * 0x20    4     origin          CRC of source content
 * 0x24    4     minTrack        First track (0)
 * 0x28    4     maxTrack        Last track (typically 83)
 * 0x2C    4     minSide         First side (0)
 * 0x30    4     maxSide         Last side (0 or 1)
 * 0x34    4     creationDate    YYYYMMDD format
 * 0x38    4     creationTime    HHMMSSmmm format
 * 0x3C    16    platforms[4]    Platform IDs (see below)
 * 0x4C    4     diskNumber      Disk number in set
 * 0x50    4     creatorId       Creator identifier
 * 0x54    12    reserved[3]     Unused
 *
 * Platform IDs:
 *   0 = Unknown, 1 = Amiga, 2 = Atari ST, 3 = PC
 *   4 = Amstrad CPC, 5 = Spectrum, 6 = Sam Coupe
 *   7 = Archimedes, 8 = C64, 9 = Atari 8-bit
 *============================================================================*/

/*============================================================================
 * IMGE RECORD (80 bytes) - One per track×side
 *
 * Offset  Size  Field           Description
 * ------  ----  --------------- ------------------------------------------
 * 0x0C    4     track           Track number (0-83)
 * 0x10    4     side            Side (0 or 1)
 * 0x14    4     density         Density/protection type (see below)
 * 0x18    4     signalType      1 = cell_2us (standard MFM)
 * 0x1C    4     trackBytes      Total track bytes
 * 0x20    4     startBytePos    Start position (bytes)
 * 0x24    4     startBitPos     Start position (bits)
 * 0x28    4     dataBits        Total data bits
 * 0x2C    4     gapBits         Total gap bits
 * 0x30    4     trackBits       dataBits + gapBits
 * 0x34    4     blockCount      Number of block descriptors
 * 0x38    4     encoder         Encoder process (0)
 * 0x3C    4     trackFlags      bit 0 = fuzzy bits present
 * 0x40    4     dataKey         Links to DATA record key
 * 0x44    12    reserved[3]     Unused
 *
 * Density Types:
 *   0 = Unknown, 1 = Noise, 2 = Auto
 *   3 = Copylock Amiga, 4 = Copylock Amiga New
 *   5 = Copylock ST, 6 = Speedlock Amiga
 *   7 = Speedlock Amiga Old, 8 = Adam Brierley
 *   9 = Adam Brierley Key
 *============================================================================*/

/*============================================================================
 * DATA RECORD (28 bytes header + extra data segment)
 *
 * Header (28 bytes):
 * Offset  Size  Field           Description
 * ------  ----  --------------- ------------------------------------------
 * 0x0C    4     length          Extra data segment length
 * 0x10    4     bitSize         length × 8
 * 0x14    4     dataCRC         CRC-32 of extra data segment
 * 0x18    4     key             Links to IMGE record dataKey
 *
 * Extra Data Segment layout:
 *   [blockCount × 32 bytes]  Block Descriptors
 *   [variable]               Gap element streams (SPS only)
 *   [variable]               Data element streams (SPS only)
 *============================================================================*/

/*============================================================================
 * BLOCK DESCRIPTOR (32 bytes, inside DATA extra segment)
 *
 * The block descriptor has a UNION layout:
 * Fields at offsets 8-15 are interpreted differently for CAPS vs SPS.
 *
 *                CAPS Mode              SPS Mode
 * Offset  Size   Field                  Field
 * ------  ----   -------------------    -------------------
 * 0x00    4      dataBits               dataBits
 * 0x04    4      gapBits                gapBits
 * 0x08    4      dataBytes              gapOffset
 * 0x0C    4      gapBytes               cellType (SignalType)
 * 0x10    4      encoderType            encoderType
 * 0x14    4      blockFlags             blockFlags
 * 0x18    4      gapDefaultValue        gapDefaultValue
 * 0x1C    4      dataOffset             dataOffset
 *
 * encoderType: 1 = MFM, 2 = RAW
 *
 * blockFlags:
 *   bit 0 (0x01) FwGap       - Forward gap stream present
 *   bit 1 (0x02) BwGap       - Backward gap stream present
 *   bit 2 (0x04) DataInBit   - Data sizes in bits (not bytes)
 *============================================================================*/

/*============================================================================
 * GAP ELEMENTS (SPS encoder only, variable length)
 *
 * Located at gapOffset within extra data segment.
 * Forward gap elements come first (if FwGap flag), then backward (if BwGap).
 * Each stream is terminated by a 0x00 byte.
 *
 * Gap Element Header Byte:
 *   bits [7:5] = size_width (number of size bytes to follow: 1, 2, or 3)
 *   bits [4:0] = type:
 *     1 = GapLength  - Gap fill with default value, size = bit count
 *     2 = SampleLength - Custom fill pattern, followed by sample data
 *
 * Layout:  [header_byte] [size_bytes × size_width] [sample_data if type=2]
 *
 * Example (GapLength 2048 bits):
 *   0x41 0x08 0x00   → size_width=2, type=1, size=2048
 *
 * Example (SampleLength 8 bits of value 0x4E):
 *   0x22 0x08 0x4E   → size_width=1, type=2, size=8, sample=0x4E
 *============================================================================*/

/*============================================================================
 * DATA ELEMENTS (SPS encoder only, variable length)
 *
 * Located at dataOffset within extra data segment.
 * Terminated by a 0x00 byte.
 *
 * Data Element Header Byte:
 *   bits [7:5] = size_width (1, 2, or 3 bytes)
 *   bits [4:0] = type:
 *     1 = Sync  - Sync pattern (e.g., 0xA1 sync bytes)
 *     2 = Data  - Normal data bytes
 *     3 = IGap  - Inter-sector gap data
 *     4 = Raw   - Raw MFM data (not decoded)
 *     5 = Fuzzy - Fuzzy/random bits (NO sample data follows)
 *
 * Layout:  [header_byte] [size_bytes × size_width] [data_bytes if type≠Fuzzy]
 *
 * When DataInBit flag is set, size values are in bits instead of bytes.
 *
 * Fuzzy elements have no data payload - they indicate regions where
 * the data varies between reads (copy protection).
 *============================================================================*/

/*============================================================================
 * CTEI RECORD (optional, CT analyzer extension)
 *
 * Contains CT (CopyTool) analyzer metadata:
 *   releaseCRC   - CRC of the original release
 *   analyzerRev  - CT analyzer version
 *   + 14 reserved uint32 fields
 *============================================================================*/

/*============================================================================
 * CTEX RECORD (optional, per-track CT extension)
 *
 * Per-track metadata from CT analyzer:
 *   track, side, density, formatId, fix, trackSize
 * Multiple CTEX records may exist (one per analyzed track).
 *============================================================================*/

/*============================================================================
 * CRC-32 COMPUTATION
 *
 * Standard ISO 3309 CRC-32 (polynomial 0xEDB88320).
 *
 * For record headers: bytes 8-11 (CRC field) are treated as 0x00.
 * For data segments: standard CRC-32 over raw bytes.
 *
 * The IPFWriter serializes IMGE records for ALL tracks first,
 * then DATA records for all tracks. IMGE and DATA are linked
 * by matching dataKey fields (auto-incremented from 1).
 *============================================================================*/

#endif /* UFT_IPF_AIR_SPEC_H */
