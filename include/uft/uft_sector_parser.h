/*
 * uft_sector_parser.h - IBM FM/MFM Sector Parser
 *
 * Part of UnifiedFloppyTool (UFT) v3.3.0
 * Based on floppy_sector_parser by UFT Team
 *
 * Goals:
 *  - Parse IBM-style FM/MFM sector structures from demodulated byte stream
 *  - Strict bounds checks, no hidden heap allocations
 *  - Provide metadata for GUI display (timings/errors/CRC/status)
 *
 * Supported (IBM family):
 *  - ID Address Mark (IDAM):  0xFE
 *  - Data Address Mark (DAM): 0xFB (normal), 0xF8 (deleted)
 *  - CRC16-CCITT (IBM polynomial 0x1021, init 0xFFFF)
 *
 * License: MIT
 */
#ifndef UFT_SECTOR_PARSER_H
#define UFT_SECTOR_PARSER_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * Status Flags
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef enum uft_sector_status {
    UFT_SECTOR_OK                = 0u,
    UFT_SECTOR_CRC_ID_BAD        = 1u << 0,
    UFT_SECTOR_CRC_DATA_BAD      = 1u << 1,
    UFT_SECTOR_MISSING_DATA      = 1u << 2,
    UFT_SECTOR_DUPLICATE_ID      = 1u << 3,
    UFT_SECTOR_SIZE_MISMATCH     = 1u << 4,
    UFT_SECTOR_TRUNCATED         = 1u << 5,
    UFT_SECTOR_WEAK_SYNC         = 1u << 6,
    UFT_SECTOR_UNUSUAL_MARK      = 1u << 7,
} uft_sector_status_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Encoding Types
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef enum uft_sector_encoding {
    UFT_ENC_UNKNOWN = 0,
    UFT_ENC_MFM,
    UFT_ENC_FM,
} uft_sector_encoding_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Data Structures
 * ═══════════════════════════════════════════════════════════════════════════ */

/* ID fields (CHRN) */
typedef struct uft_sector_id {
    uint8_t cyl;    /* C - Cylinder */
    uint8_t head;   /* H - Head */
    uint8_t sec;    /* R - Record (sector number) */
    uint8_t size_n; /* N - Size code (2^N * 128 bytes) */
} uft_sector_id_t;

/* ID Address Mark Record */
typedef struct uft_id_record {
    uft_sector_id_t id;
    uint16_t crc_read;
    uint16_t crc_calc;
    size_t   offset;        /* offset of 0xFE */
    size_t   sync_offset;   /* offset of sync sequence start */
    uint32_t status;        /* uft_sector_status_t bits */
} uft_id_record_t;

/* Data Address Mark Record */
typedef struct uft_data_record {
    uint8_t  dam;           /* 0xFB or 0xF8 */
    uint16_t data_len;      /* bytes copied into 'data' */
    uint16_t expected_len;  /* derived from N */
    uint16_t crc_read;
    uint16_t crc_calc;
    size_t   offset;        /* offset of DAM */
    size_t   sync_offset;
    uint32_t status;        /* uft_sector_status_t bits */
} uft_data_record_t;

/* Complete Sector */
typedef struct uft_sector {
    uft_id_record_t   id_rec;
    uft_data_record_t data_rec;
    uint8_t *data;          /* caller-provided storage */
    uint16_t data_capacity; /* bytes available at data */
} uft_sector_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Configuration
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct uft_sector_config {
    uft_sector_encoding_t encoding;

    const uint8_t *mark_mask;   /* optional - marks special bytes */
    size_t mark_mask_len;

    uint16_t max_sectors;       /* max sectors to parse */
    uint16_t max_search_gap;    /* bytes after ID to search for data (0 = unlimited) */

    uint8_t require_mark_mask;  /* if 1, accept A1 sync only when mask confirms */
} uft_sector_config_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Result
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct uft_sector_result {
    uint16_t sectors_found;
    uint16_t sectors_with_data;
    uint16_t ids_found;
    uint16_t data_records_found;
    uint16_t duplicates;
    uint16_t warnings;
} uft_sector_result_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * API Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Calculate expected data length from size code N
 * @param size_n Size code (0-7)
 * @return Data length in bytes (128 << N), or 0 if invalid
 */
uint16_t uft_sector_length_from_n(uint8_t size_n);

/**
 * Calculate CRC16-CCITT (IBM floppy standard)
 * @param buf Input buffer
 * @param len Buffer length
 * @param init Initial CRC value (typically 0xFFFF)
 * @return Calculated CRC
 */
uint16_t uft_sector_crc16(const uint8_t *buf, size_t len, uint16_t init);

/**
 * Parse track data into sectors
 * @param cfg Configuration (encoding, limits, etc.)
 * @param stream Raw track byte stream
 * @param stream_len Length of stream
 * @param sectors Output array (caller allocates)
 * @param sectors_cap Capacity of sectors array
 * @param out Result statistics (can be NULL)
 * @return 0 on success, negative on error
 */
int uft_sector_parse_track(const uft_sector_config_t *cfg,
                           const uint8_t *stream, size_t stream_len,
                           uft_sector_t *sectors, size_t sectors_cap,
                           uft_sector_result_t *out);

/**
 * Get human-readable status string
 * @param status Status flags
 * @return Static string describing status
 */
const char *uft_sector_status_str(uint32_t status);

#ifdef __cplusplus
}
#endif
#endif /* UFT_SECTOR_PARSER_H */
