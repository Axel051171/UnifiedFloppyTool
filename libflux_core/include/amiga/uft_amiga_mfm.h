/*
 * uft_amiga_mfm.h
 *
 * Amiga MFM (Modified Frequency Modulation) decoder.
 * Based on concepts from Keir Fraser's disk-utilities (Public Domain).
 *
 * Amiga MFM Format:
 * - MFM encoding with 2µs bit cells (500 kbps)
 * - Sync word: 0x4489 (special MFM pattern with missing clock)
 * - 11 sectors per track (standard AmigaDOS)
 * - Track format: gaps + sync + header + data blocks
 *
 * Supported Formats:
 * - AmigaDOS (OFS/FFS, 11 sectors, 512 bytes)
 * - Extended AmigaDOS (custom sector sizes)
 * - Long tracks (12+ sectors)
 * - Various copy protections (Speedlock, Copylock, etc.)
 *
 * Build:
 *   cc -std=c11 -O2 -Wall -Wextra -pedantic -c uft_amiga_mfm.c
 */

#ifndef UFT_AMIGA_MFM_H
#define UFT_AMIGA_MFM_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * CONSTANTS
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Amiga MFM sync patterns */
#define UFT_AMIGA_SYNC_STD      0x4489      /* Standard AmigaDOS sync */
#define UFT_AMIGA_SYNC_ZOUT     0x4521      /* Z Out (track 1) */
#define UFT_AMIGA_SYNC_TURBO    0x4891      /* Turbo Outrun */
#define UFT_AMIGA_SYNC_FTANK    0x4A84      /* Future Tank */

/* Track parameters */
#define UFT_AMIGA_SECTORS_STD   11          /* Standard sectors per track */
#define UFT_AMIGA_SECTOR_SIZE   512         /* Standard sector size */
#define UFT_AMIGA_TRACK_LEN     12668       /* Raw MFM bytes per track */
#define UFT_AMIGA_BIT_CELL_NS   2000        /* 2µs bit cell */

/* Header structure sizes */
#define UFT_AMIGA_HDR_SIZE      4           /* format, track, sector, gap */
#define UFT_AMIGA_LABEL_SIZE    16          /* Sector label */

/* ═══════════════════════════════════════════════════════════════════════════
 * DATA STRUCTURES
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Amiga sector header (decoded).
 */
typedef struct uft_amiga_header {
    uint8_t     format;             /* Format type (0xFF = AmigaDOS) */
    uint8_t     track;              /* Track number */
    uint8_t     sector;             /* Sector number */
    uint8_t     sectors_to_gap;     /* Sectors until gap */
    uint8_t     label[16];          /* Sector label (OS recovery info) */
    uint32_t    header_checksum;    /* Header checksum */
    uint32_t    data_checksum;      /* Data checksum */
} uft_amiga_header_t;

/**
 * Amiga sector (decoded).
 */
typedef struct uft_amiga_sector {
    uft_amiga_header_t  header;     /* Sector header */
    uint8_t            *data;       /* Sector data (512 bytes typically) */
    size_t              data_size;  /* Actual data size */
    uint32_t            sync;       /* Sync word used */
    bool                header_ok;  /* Header checksum valid */
    bool                data_ok;    /* Data checksum valid */
    uint64_t            latency;    /* Read latency (for timing analysis) */
} uft_amiga_sector_t;

/**
 * Amiga track (decoded).
 */
typedef struct uft_amiga_track {
    uint8_t             track_num;          /* Track number (0-159) */
    uint8_t             side;               /* Side (0 or 1) */
    uint8_t             nr_sectors;         /* Number of sectors found */
    uint8_t             nr_valid;           /* Number of valid sectors */
    uint32_t            format_type;        /* Detected format type */
    uft_amiga_sector_t  sectors[22];        /* Sectors (max 22 for long tracks) */
    bool                has_long_track;     /* True if >11 sectors */
    bool                has_protection;     /* Protection detected */
    uint32_t            protection_id;      /* Protection type if detected */
} uft_amiga_track_t;

/**
 * Amiga disk info.
 */
typedef struct uft_amiga_disk {
    char                disk_name[32];      /* Disk name from root block */
    uint8_t             filesystem;         /* 0=OFS, 1=FFS, 2=OFS-INTL, etc. */
    uint8_t             nr_tracks;          /* Tracks per side */
    uint8_t             nr_sides;           /* Number of sides */
    uint8_t             nr_sectors;         /* Sectors per track */
    uint32_t            root_block;         /* Root block number */
    bool                bootable;           /* Has valid bootblock */
    uint32_t            bootblock_sum;      /* Bootblock checksum */
} uft_amiga_disk_t;

/**
 * Amiga format types.
 */
typedef enum uft_amiga_format {
    UFT_AMIGA_FMT_UNKNOWN       = 0,
    UFT_AMIGA_FMT_AMIGADOS      = 1,    /* Standard AmigaDOS */
    UFT_AMIGA_FMT_AMIGADOS_EXT  = 2,    /* Extended AmigaDOS */
    UFT_AMIGA_FMT_LONGTRACK     = 3,    /* Long track (>11 sectors) */
    UFT_AMIGA_FMT_COPYLOCK      = 4,    /* RNC Copylock */
    UFT_AMIGA_FMT_SPEEDLOCK     = 5,    /* Speedlock */
    UFT_AMIGA_FMT_PROTECTION    = 6,    /* Other protection */
    UFT_AMIGA_FMT_RAW           = 7     /* Raw/unknown format */
} uft_amiga_format_t;

/**
 * Filesystem types.
 */
typedef enum uft_amiga_fs {
    UFT_AMIGA_FS_OFS            = 0,    /* Original File System */
    UFT_AMIGA_FS_FFS            = 1,    /* Fast File System */
    UFT_AMIGA_FS_OFS_INTL       = 2,    /* OFS International */
    UFT_AMIGA_FS_FFS_INTL       = 3,    /* FFS International */
    UFT_AMIGA_FS_OFS_DC         = 4,    /* OFS with DirCache */
    UFT_AMIGA_FS_FFS_DC         = 5,    /* FFS with DirCache */
    UFT_AMIGA_FS_LNFS           = 6,    /* Long Names FS */
    UFT_AMIGA_FS_UNKNOWN        = 255
} uft_amiga_fs_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * MFM ENCODING/DECODING
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Encode data to Amiga MFM.
 *
 * Amiga MFM is odd-even interleaved:
 * - First, all odd bits of the data
 * - Then, all even bits of the data
 * - Clock bits inserted between data bits
 *
 * @param data       Input data
 * @param data_len   Input length in bytes
 * @param mfm_out    Output MFM buffer (2x input size needed)
 * @param mfm_size   Size of output buffer
 * @return           Number of MFM bytes written
 */
size_t uft_amiga_mfm_encode(const uint8_t *data, size_t data_len,
                            uint8_t *mfm_out, size_t mfm_size);

/**
 * Decode Amiga MFM to data.
 *
 * @param mfm        Input MFM data
 * @param mfm_len    Input length in bytes
 * @param data_out   Output data buffer (half of input size)
 * @param data_size  Size of output buffer
 * @return           Number of data bytes decoded
 */
size_t uft_amiga_mfm_decode(const uint8_t *mfm, size_t mfm_len,
                            uint8_t *data_out, size_t data_size);

/**
 * Decode Amiga MFM long word (32 bits from 64 MFM bits).
 *
 * @param mfm_odd    Odd bits (MFM encoded)
 * @param mfm_even   Even bits (MFM encoded)
 * @return           Decoded 32-bit value
 */
uint32_t uft_amiga_mfm_decode_long(uint32_t mfm_odd, uint32_t mfm_even);

/**
 * Encode a 32-bit value to Amiga MFM (odd-even).
 *
 * @param value      Value to encode
 * @param mfm_odd    Output: Odd bits
 * @param mfm_even   Output: Even bits
 */
void uft_amiga_mfm_encode_long(uint32_t value,
                               uint32_t *mfm_odd, uint32_t *mfm_even);

/* ═══════════════════════════════════════════════════════════════════════════
 * CHECKSUM CALCULATION
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Calculate Amiga MFM checksum.
 *
 * Checksum is XOR of all longwords.
 *
 * @param data       Data to checksum
 * @param len        Length in bytes (must be multiple of 4)
 * @return           Checksum value
 */
uint32_t uft_amiga_checksum(const uint8_t *data, size_t len);

/**
 * Calculate Amiga bootblock checksum.
 *
 * Special checksum for bootblock (sectors 0-1).
 *
 * @param bootblock  Bootblock data (1024 bytes)
 * @return           Checksum value
 */
uint32_t uft_amiga_bootblock_checksum(const uint8_t *bootblock);

/**
 * Verify bootblock checksum.
 *
 * @param bootblock  Bootblock data (1024 bytes)
 * @return           true if checksum valid
 */
bool uft_amiga_verify_bootblock(const uint8_t *bootblock);

/* ═══════════════════════════════════════════════════════════════════════════
 * SECTOR OPERATIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Find sync word in MFM stream.
 *
 * @param mfm        MFM data
 * @param len        Data length
 * @param sync       Sync pattern to find
 * @param start_pos  Start position for search
 * @return           Position of sync, or -1 if not found
 */
int64_t uft_amiga_find_sync(const uint8_t *mfm, size_t len,
                            uint16_t sync, size_t start_pos);

/**
 * Decode sector header from MFM.
 *
 * @param mfm        MFM data (starting after sync)
 * @param header     Output header structure
 * @return           true if header decoded successfully
 */
bool uft_amiga_decode_header(const uint8_t *mfm,
                             uft_amiga_header_t *header);

/**
 * Decode complete sector from MFM.
 *
 * @param mfm        MFM data (starting at sync)
 * @param mfm_len    Length of MFM data
 * @param sector     Output sector structure
 * @param data_buf   Buffer for sector data
 * @param data_size  Size of data buffer
 * @return           true if sector decoded successfully
 */
bool uft_amiga_decode_sector(const uint8_t *mfm, size_t mfm_len,
                             uft_amiga_sector_t *sector,
                             uint8_t *data_buf, size_t data_size);

/**
 * Encode sector to MFM.
 *
 * @param sector     Sector to encode
 * @param mfm_out    Output MFM buffer
 * @param mfm_size   Size of output buffer
 * @return           Number of MFM bytes written
 */
size_t uft_amiga_encode_sector(const uft_amiga_sector_t *sector,
                               uint8_t *mfm_out, size_t mfm_size);

/* ═══════════════════════════════════════════════════════════════════════════
 * TRACK OPERATIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Decode all sectors from track MFM data.
 *
 * @param mfm        Track MFM data
 * @param mfm_len    Length of MFM data
 * @param track      Output track structure
 * @param data_buf   Buffer for all sector data
 * @param data_size  Size of data buffer
 * @return           Number of sectors decoded
 */
int uft_amiga_decode_track(const uint8_t *mfm, size_t mfm_len,
                           uft_amiga_track_t *track,
                           uint8_t *data_buf, size_t data_size);

/**
 * Encode complete track to MFM.
 *
 * @param track      Track to encode
 * @param mfm_out    Output MFM buffer
 * @param mfm_size   Size of output buffer
 * @return           Number of MFM bytes written
 */
size_t uft_amiga_encode_track(const uft_amiga_track_t *track,
                              uint8_t *mfm_out, size_t mfm_size);

/**
 * Detect track format type.
 *
 * @param mfm        Track MFM data
 * @param mfm_len    Length of MFM data
 * @return           Detected format type
 */
uft_amiga_format_t uft_amiga_detect_format(const uint8_t *mfm, size_t mfm_len);

/* ═══════════════════════════════════════════════════════════════════════════
 * DISK OPERATIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Parse disk info from root block.
 *
 * @param root_data  Root block data (512 bytes)
 * @param disk       Output disk info structure
 * @return           true if parsed successfully
 */
bool uft_amiga_parse_rootblock(const uint8_t *root_data,
                               uft_amiga_disk_t *disk);

/**
 * Calculate block number from track/sector/side.
 *
 * @param track      Track number
 * @param sector     Sector number
 * @param side       Side (0 or 1)
 * @param sectors_per_track  Sectors per track (usually 11)
 * @return           Block number
 */
uint32_t uft_amiga_calc_block(int track, int sector, int side,
                              int sectors_per_track);

/**
 * Calculate track/sector/side from block number.
 *
 * @param block      Block number
 * @param sectors_per_track  Sectors per track
 * @param track_out  Output: Track number
 * @param sector_out Output: Sector number
 * @param side_out   Output: Side
 */
void uft_amiga_block_to_ts(uint32_t block, int sectors_per_track,
                           int *track_out, int *sector_out, int *side_out);

/* ═══════════════════════════════════════════════════════════════════════════
 * PROTECTION DETECTION
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Protection types.
 */
typedef enum uft_amiga_protection {
    UFT_AMIGA_PROT_NONE         = 0,
    UFT_AMIGA_PROT_COPYLOCK     = 1,    /* RNC Copylock */
    UFT_AMIGA_PROT_SPEEDLOCK    = 2,    /* Speedlock */
    UFT_AMIGA_PROT_TIERTEX      = 3,    /* Tiertex protection */
    UFT_AMIGA_PROT_RAINBIRD     = 4,    /* Rainbird protection */
    UFT_AMIGA_PROT_GREMLIN      = 5,    /* Gremlin protection */
    UFT_AMIGA_PROT_PSYGNOSIS    = 6,    /* Psygnosis protection */
    UFT_AMIGA_PROT_LONGTRACK    = 7,    /* Long track protection */
    UFT_AMIGA_PROT_UNKNOWN      = 255
} uft_amiga_protection_t;

/**
 * Protection detection result.
 */
typedef struct uft_amiga_prot_result {
    uft_amiga_protection_t  type;       /* Protection type */
    uint32_t                version;    /* Version/variant if known */
    int                     track;      /* Track where detected */
    uint32_t                key;        /* Key/signature if applicable */
    char                    name[32];   /* Human-readable name */
} uft_amiga_prot_result_t;

/**
 * Detect copy protection in track.
 *
 * @param mfm        Track MFM data
 * @param mfm_len    Length of MFM data
 * @param track_num  Track number
 * @param result     Output: Detection result
 * @return           true if protection detected
 */
bool uft_amiga_detect_protection(const uint8_t *mfm, size_t mfm_len,
                                 int track_num,
                                 uft_amiga_prot_result_t *result);

/**
 * Detect Copylock protection.
 *
 * @param mfm        Track MFM data
 * @param mfm_len    Length of data
 * @param key_out    Output: Copylock key if found
 * @return           true if Copylock detected
 */
bool uft_amiga_detect_copylock(const uint8_t *mfm, size_t mfm_len,
                               uint32_t *key_out);

/**
 * Detect Speedlock protection.
 *
 * @param mfm        Track MFM data
 * @param mfm_len    Length of data
 * @return           true if Speedlock detected
 */
bool uft_amiga_detect_speedlock(const uint8_t *mfm, size_t mfm_len);

/* ═══════════════════════════════════════════════════════════════════════════
 * UTILITY FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Get filesystem name string.
 *
 * @param fs         Filesystem type
 * @return           Static string with name
 */
const char *uft_amiga_fs_name(uft_amiga_fs_t fs);

/**
 * Get format type name string.
 *
 * @param format     Format type
 * @return           Static string with name
 */
const char *uft_amiga_format_name(uft_amiga_format_t format);

/**
 * Get protection type name string.
 *
 * @param prot       Protection type
 * @return           Static string with name
 */
const char *uft_amiga_protection_name(uft_amiga_protection_t prot);

/**
 * Convert ADF offset to track/sector.
 *
 * @param offset     Byte offset in ADF file
 * @param track_out  Output: Track number
 * @param sector_out Output: Sector number
 * @param side_out   Output: Side (0 or 1)
 */
void uft_amiga_adf_to_ts(size_t offset,
                         int *track_out, int *sector_out, int *side_out);

/**
 * Convert track/sector to ADF offset.
 *
 * @param track      Track number
 * @param sector     Sector number
 * @param side       Side (0 or 1)
 * @return           Byte offset in ADF file
 */
size_t uft_amiga_ts_to_adf(int track, int sector, int side);

#ifdef __cplusplus
}
#endif

#endif /* UFT_AMIGA_MFM_H */
