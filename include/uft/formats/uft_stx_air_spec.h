/**
 * @file uft_stx_air_spec.h
 * @brief STX/Pasti Format Specification - derived from DrCoolzic AIR analysis
 * @version 1.0.0
 *
 * Complete format specification for Pasti/STX (.stx/.pasti) disk images.
 * Pasti was developed by Jorge Cwik (Pasti team) for Atari ST disk preservation.
 * This specification covers Version 3 with revisions 0 and 2.
 *
 * Reference: AIR (Atari Image Reader) by Jean Louis-Guerin (DrCoolzic)
 * Source: PastiStruct.cs, PastiRead.cs, PastiWrite.cs
 *
 * BYTE ORDER: Little-Endian throughout
 *
 * ╔══════════════════════════════════════════════════════════════════╗
 * ║                    STX FILE LAYOUT                              ║
 * ╠══════════════════════════════════════════════════════════════════╣
 * ║  File Header         (16 bytes)                                 ║
 * ║  Track Record 0                                                 ║
 * ║    Track Descriptor   (16 bytes)                                ║
 * ║    Sector Descriptors (16 bytes × sector_count) [if SECT_DESC]  ║
 * ║    Fuzzy Byte Mask    (variable) [if fuzzy_count > 0]           ║
 * ║    Track Image        (variable) [if TRK_IMAGE flag]            ║
 * ║    Sector Data        (128 << size_code per sector)             ║
 * ║    Timing Record      (variable) [if bit_width + revision 2]    ║
 * ║  Track Record 1                                                 ║
 * ║  ...                                                            ║
 * ║  Track Record N-1                                               ║
 * ╚══════════════════════════════════════════════════════════════════╝
 */

#ifndef UFT_STX_AIR_SPEC_H
#define UFT_STX_AIR_SPEC_H

/*============================================================================
 * FILE HEADER (16 bytes, offset 0)
 *
 * Offset  Size  Field         Description
 * ------  ----  -----------   ------------------------------------------
 * 0x00    4     magic         "RSY\0" (0x52 0x53 0x59 0x00)
 * 0x04    2     version       Always 3
 * 0x06    2     tool          Tool that created file:
 *                               0x01 = Atari imaging tool
 *                               0xCC = Discovery Cartridge
 *                               0x10 = Aufit program
 * 0x08    2     reserved1     Unused
 * 0x0A    1     track_count   Number of track records (0-166)
 * 0x0B    1     revision      0x00=old Pasti, 0x02=new with timing
 * 0x0C    4     reserved2     Unused
 *============================================================================*/

/*============================================================================
 * TRACK DESCRIPTOR (16 bytes, follows file header or previous track record)
 *
 * Offset  Size  Field         Description
 * ------  ----  -----------   ------------------------------------------
 * 0x00    4     record_size   Total bytes in this track record (incl. descriptor)
 * 0x04    4     fuzzy_count   Bytes in fuzzy bit mask (0 if none)
 * 0x08    2     sector_count  Number of sectors on track
 * 0x0A    2     flags         Track flags (see below)
 * 0x0C    2     track_length  MFM track length in bytes (~6250 for DD)
 * 0x0E    1     track_number  bit[6:0]=track (0-83), bit[7]=side (0/1)
 * 0x0F    1     track_type    Reserved (always 0)
 *
 * Track Flags:
 *   bit 0 (0x01) SECT_DESC  - Track has sector descriptors
 *   bit 5 (0x20) PROT       - Track contains protection (always set)
 *   bit 6 (0x40) TRK_IMAGE  - Track record contains track image data
 *   bit 7 (0x80) TRK_SYNC   - Track image has 2-byte sync offset header
 *
 * If SECT_DESC is clear: track is "standard" - only sector data follows,
 * with sequential sector numbering and 512-byte sectors.
 *============================================================================*/

/*============================================================================
 * SECTOR DESCRIPTOR (16 bytes each, follows track descriptor)
 * Present only if SECT_DESC flag is set.
 *
 * Offset  Size  Field         Description
 * ------  ----  -----------   ------------------------------------------
 * 0x00    4     data_offset   Offset from track_data_start to sector data
 * 0x04    2     bit_position  Position in track from index (in bits/16)
 * 0x06    2     read_time     Microseconds for FDC read (0 = standard 16384µs)
 * 0x08    1     id_track      ID field: track number
 * 0x09    1     id_side       ID field: head number
 * 0x0A    1     id_number     ID field: sector number
 * 0x0B    1     id_size       ID field: size code (0=128, 1=256, 2=512, 3=1024)
 * 0x0C    2     id_crc        Address field CRC
 * 0x0E    1     fdc_flags     FDC status + pseudo flags (see below)
 * 0x0F    1     reserved      Unused
 *
 * FDC Flags:
 *   bit 0 (0x01) BIT_WIDTH   - Intra-sector bit width variation (Macrodos/Speedlock)
 *   bit 3 (0x08) CRC_ERROR   - CRC error (data field if RNF=0, ID field if RNF=1)
 *   bit 4 (0x10) RNF         - Record Not Found (address only, no data field)
 *   bit 5 (0x20) REC_TYPE    - Deleted Data mark (DAM=0xF8 instead of 0xFB)
 *   bit 7 (0x80) FUZZY       - Sector has fuzzy/random bits
 *
 * Sector size = 128 << id_size  (128, 256, 512, or 1024 bytes)
 *============================================================================*/

/*============================================================================
 * FUZZY BYTE MASK (variable length, follows sector descriptors)
 * Present only if fuzzy_count > 0 in track descriptor.
 *
 * Contains one mask byte per sector data byte for all fuzzy sectors.
 * Sectors are concatenated in order. Each mask byte indicates which
 * bits in the corresponding data byte are "fuzzy" (unreliable):
 *   0x00 = all bits reliable
 *   0xFF = all bits fuzzy/random
 *   0xF0 = upper 4 bits fuzzy
 *
 * The fuzzy mask is distributed to sectors: each sector with the
 * FUZZY flag gets sector_size bytes from the mask, in order.
 *============================================================================*/

/*============================================================================
 * TRACK IMAGE (variable length, follows fuzzy mask)
 * Present only if TRK_IMAGE flag is set.
 *
 * Layout:
 *   [2 bytes] sync_offset   - Only if TRK_SYNC flag set (first sync position)
 *   [2 bytes] image_size    - Track image byte count
 *   [N bytes] track_data    - Raw MFM track data
 *   [0-1 bytes] padding     - Word-align
 *
 * The track image represents the complete MFM bit stream as read from disk.
 * Sector data may reference positions within this image.
 *
 * track_data_start is the byte position AFTER sector descriptors + fuzzy mask.
 * Sector data_offset is relative to track_data_start.
 *============================================================================*/

/*============================================================================
 * TIMING RECORD (variable length, follows sector data)
 * Present only if any sector has BIT_WIDTH flag AND revision is 2.
 *
 * Layout:
 *   [2 bytes] flags         - Timing flags (unused, always 0)
 *   [2 bytes] size          - Total timing record size including header
 *   [N × 2 bytes] values    - Big-endian 16-bit timing values
 *
 * One timing value per 16 bytes of sector data.
 * Values indicate bit cell width variation:
 *   127 = standard 2µs MFM bit cell
 *   <127 = shorter cells (faster rotation region)
 *   >127 = longer cells (slower rotation region)
 *
 * For revision 0 (no timing record in file), the Macrodos/Speedlock
 * timing table is simulated:
 *   Quarter 1 (0-25%):    127 (standard)
 *   Quarter 2 (25-50%):   133 (slow)
 *   Quarter 3 (50-75%):   121 (fast)
 *   Quarter 4 (75-100%):  127 (standard)
 *
 * This variable bit width creates sectors that are unreadable by
 * standard FDC timing, providing effective copy protection.
 *============================================================================*/

/*============================================================================
 * COPY PROTECTION SCHEMES DETECTABLE IN STX
 *
 * 1. MACRODOS / SPEEDLOCK (BIT_WIDTH flag)
 *    Intra-sector bit width variation. Detected by BIT_WIDTH flag on sectors.
 *    The FDC reads timing values that vary within a single sector.
 *
 * 2. FUZZY BITS (FUZZY flag)
 *    Random/unreliable bits in sector data. Each read returns different values.
 *    Used for fingerprinting: protection checks for "randomness" of specific bytes.
 *
 * 3. LONG TRACKS (track_length > 6250)
 *    Tracks longer than standard, requiring precise drive speed.
 *
 * 4. WEAK SECTORS (CRC_ERROR flag)
 *    Intentional CRC errors that prevent sector from being copied correctly.
 *
 * 5. ADDRESS FIELD ERRORS (RNF flag)
 *    Missing or corrupted sector headers. FDC returns Record Not Found.
 *
 * 6. DELETED DATA MARKS (REC_TYPE flag)
 *    Sectors marked with DAM 0xF8 instead of standard 0xFB.
 *============================================================================*/

#endif /* UFT_STX_AIR_SPEC_H */
