/*
 * uft_floppy_sector_parser.h  —  "Superman"-grade floppy sector parser (C99)
 *
 * Goals:
 *  - Parse IBM-style FM/MFM sector structures from an already-demodulated byte stream.
 *  - Strict bounds checks, no hidden heap allocations.
 *  - Provide enough metadata for GUI display (timings/errors/CRC/status).
 *
 * Assumptions / scope:
 *  - Input is a "track byte stream" where special sync/address marks (A1/C2 with missing clock)
 *    may appear as literal 0xA1/0xC2 bytes. If your decoder provides a separate "mark mask",
 *    pass it to improve detection (recommended).
 *  - This is NOT a flux-to-bits decoder. Use it after FM/MFM decoding.
 *
 * Supported (IBM family):
 *  - ID Address Mark (IDAM):  0xFE
 *  - Data Address Mark (DAM): 0xFB (normal data), 0xF8 (deleted data)
 *  - CRC16-CCITT (IBM polynomial 0x1021, init 0xFFFF) as used on floppy records
 *
 * License: MIT
 */
#ifndef FLOPPY_SECTOR_PARSER_H
#define FLOPPY_SECTOR_PARSER_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum fps_status_flags {
    FPS_OK                     = 0u,
    FPS_WARN_CRC_ID_BAD         = 1u << 0,
    FPS_WARN_CRC_DATA_BAD       = 1u << 1,
    FPS_WARN_MISSING_DATA       = 1u << 2,
    FPS_WARN_DUPLICATE_ID       = 1u << 3,
    FPS_WARN_SIZE_MISMATCH      = 1u << 4,
    FPS_WARN_TRUNCATED_RECORD   = 1u << 5,
    FPS_WARN_WEAK_SYNC          = 1u << 6,
    FPS_WARN_UNUSUAL_MARK       = 1u << 7,
} fps_status_flags_t;

typedef enum fps_encoding {
    FPS_ENC_UNKNOWN = 0,
    FPS_ENC_MFM,
    FPS_ENC_FM,
} fps_encoding_t;

typedef struct fps_id_fields {
    uint8_t cyl;   /* C */
    uint8_t head;  /* H */
    uint8_t sec;   /* R */
    uint8_t sizeN; /* N (2^N * 128 bytes) */
} fps_id_fields_t;

typedef struct fps_id_record {
    fps_id_fields_t id;
    uint16_t crc_read;
    uint16_t crc_calc;
    size_t   offset;        /* offset of 0xFE */
    size_t   sync_offset;   /* offset of sync sequence start */
    uint32_t status;        /* fps_status_flags_t bits */
} fps_id_record_t;

typedef struct fps_data_record {
    uint8_t  dam;           /* 0xFB or 0xF8 */
    uint16_t data_len;      /* bytes copied into 'data' */
    uint16_t expected_len;  /* derived from N */
    uint16_t crc_read;
    uint16_t crc_calc;
    size_t   offset;        /* offset of DAM */
    size_t   sync_offset;
    uint32_t status;        /* fps_status_flags_t bits */
} fps_data_record_t;

typedef struct fps_sector {
    fps_id_record_t   idrec;
    fps_data_record_t datarec;
    uint8_t *data;          /* caller-provided storage */
    uint16_t data_capacity; /* bytes available at data */
} fps_sector_t;

typedef struct fps_config {
    fps_encoding_t encoding;

    const uint8_t *mark_mask; /* optional */
    size_t mark_mask_len;

    uint16_t max_sectors;
    uint16_t max_search_gap;   /* bytes after ID to search for data (0 = unlimited) */

    uint8_t require_mark_mask; /* if 1, accept A1 sync only when mask confirms it */
} fps_config_t;

typedef struct fps_result {
    uint16_t sectors_found;
    uint16_t sectors_with_data;
    uint16_t ids_found;
    uint16_t data_records_found;
    uint16_t duplicates;
    uint16_t warnings;
} fps_result_t;

uint16_t fps_expected_length_from_N(uint8_t sizeN);
uint16_t fps_crc16_ccitt(const uint8_t *buf, size_t len, uint16_t init);

int fps_parse_track(const fps_config_t *cfg,
                    const uint8_t *stream, size_t stream_len,
                    fps_sector_t *sectors, size_t sectors_cap,
                    fps_result_t *out);

#ifdef __cplusplus
}
#endif
#endif
