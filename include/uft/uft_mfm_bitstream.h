/**
 * @file uft_mfm_bitstream.h
 * @brief UnifiedFloppyTool - IBM MFM Bitstream Tools
 * @version 3.1.4.007
 *
 * Complete IBM MFM bitstream manipulation toolkit.
 * Supports encoding, decoding, pattern matching, and sector extraction.
 *
 * Sources analyzed:
 * - DiskImageTool IBM_MFM_Tools.vb
 */

#ifndef UFT_MFM_BITSTREAM_H
#define UFT_MFM_BITSTREAM_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * MFM Constants
 *============================================================================*/

#define UFT_MFM_BITS_PER_BYTE       16
#define UFT_MFM_SYNC_NULL_BYTES     12
#define UFT_MFM_SYNC_BYTES          3
#define UFT_MFM_GAP_BYTE            0x4E
#define UFT_MFM_IDAREA_BYTES        4
#define UFT_MFM_CRC_BYTES           2

/** MFM address marks */
typedef enum {
    UFT_MFM_AM_INDEX        = 0xFC,
    UFT_MFM_AM_ID           = 0xFE,
    UFT_MFM_AM_DATA         = 0xFB,
    UFT_MFM_AM_DELETED_DATA = 0xF8
} uft_mfm_address_mark_t;

/** MFM track formats */
typedef enum {
    UFT_MFM_FORMAT_UNKNOWN = 0,
    UFT_MFM_FORMAT_DD      = 1,
    UFT_MFM_FORMAT_HD      = 2,
    UFT_MFM_FORMAT_HD1200  = 3,
    UFT_MFM_FORMAT_ED      = 4
} uft_mfm_track_format_t;

/*============================================================================
 * MFM Sync Patterns
 *============================================================================*/

static const uint8_t uft_mfm_idam_sync[] = { 0x44, 0x89, 0x44, 0x89, 0x44, 0x89, 0x55, 0x54 };
static const uint8_t uft_mfm_dam_sync[]  = { 0x44, 0x89, 0x44, 0x89, 0x44, 0x89, 0x55, 0x45 };
static const uint8_t uft_mfm_sync[]      = { 0x44, 0x89, 0x44, 0x89, 0x44, 0x89 };

/*============================================================================
 * MFM Encode/Decode
 *============================================================================*/

static inline uint8_t uft_reverse_bits(uint8_t b)
{
    b = ((b & 0xF0) >> 4) | ((b & 0x0F) << 4);
    b = ((b & 0xCC) >> 2) | ((b & 0x33) << 2);
    b = ((b & 0xAA) >> 1) | ((b & 0x55) << 1);
    return b;
}

static inline size_t uft_mfm_bits_to_bytes(size_t bits) { return bits / 16; }
static inline size_t uft_mfm_bytes_to_bits(size_t bytes) { return bytes * 16; }

static inline size_t uft_mfm_encode_bytes(const uint8_t *data, size_t len,
                                           uint8_t *out, bool seed_bit)
{
    bool prev = seed_bit;
    size_t bits = 0;
    memset(out, 0, len * 2);
    
    for (size_t i = 0; i < len; i++) {
        for (int j = 7; j >= 0; j--) {
            bool d = (data[i] >> j) & 1;
            bool c = !d && !prev;
            if (c) out[bits/8] |= (1 << (7 - bits%8));
            bits++;
            if (d) out[bits/8] |= (1 << (7 - bits%8));
            bits++;
            prev = d;
        }
    }
    return bits;
}

static inline size_t uft_mfm_decode_bytes(const uint8_t *bits, size_t start,
                                           size_t num, uint8_t *out)
{
    for (size_t i = 0; i < num; i++) {
        uint8_t b = 0;
        for (int j = 0; j < 8; j++) {
            size_t pos = start + i*16 + j*2 + 1;
            if ((bits[pos/8] >> (7 - pos%8)) & 1) b |= (1 << (7-j));
        }
        out[i] = b;
    }
    return num;
}

/*============================================================================
 * CRC-16 CCITT
 *============================================================================*/

static inline uint16_t uft_mfm_crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= ((uint16_t)data[i]) << 8;
        for (int j = 0; j < 8; j++) {
            crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : crc << 1;
        }
    }
    return ((crc >> 8) & 0xFF) | ((crc & 0xFF) << 8);
}

/*============================================================================
 * Track Format Detection
 *============================================================================*/

static inline uft_mfm_track_format_t uft_mfm_get_format(size_t bits)
{
    size_t r = ((bits + 2500) / 5000) * 5000;
    if (r <= 135000) return UFT_MFM_FORMAT_DD;
    if (r >= 165000 && r <= 175000) return UFT_MFM_FORMAT_HD1200;
    if (r >= 195000 && r <= 205000) return UFT_MFM_FORMAT_HD;
    if (r >= 395000 && r <= 405000) return UFT_MFM_FORMAT_ED;
    return UFT_MFM_FORMAT_UNKNOWN;
}

static inline size_t uft_mfm_sector_size(uint8_t code)
{
    return (code > 7) ? 16384 : 128 << code;
}

/*============================================================================
 * MFM Sector Structure
 *============================================================================*/

typedef struct {
    uint8_t  cylinder, head, sector_id, size_code;
    uint16_t data_size;
    bool     idam_found, dam_found, id_crc_valid, data_crc_valid, deleted;
    size_t   idam_pos, dam_pos;
} uft_mfm_sector_t;

typedef struct {
    uft_mfm_track_format_t format;
    size_t bit_count;
    uft_mfm_sector_t *sectors;
    size_t sector_count;
    bool iam_found;
    size_t iam_pos;
} uft_mfm_track_t;

int uft_mfm_parse_track(const uint8_t *bits, size_t len, uft_mfm_track_t *track);
int uft_mfm_find_pattern(const uint8_t *bits, size_t len, const uint8_t *pat, size_t plen, size_t start);
void uft_mfm_free_track(uft_mfm_track_t *track);

#ifdef __cplusplus
}
#endif

#endif /* UFT_MFM_BITSTREAM_H */
