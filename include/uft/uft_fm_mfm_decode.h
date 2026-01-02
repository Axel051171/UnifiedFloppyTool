/**
 * @file uft_fm_mfm_decode.h
 * @brief FM and MFM Encoding/Decoding with Auto-Detection
 * 
 * Extracted from floppy8 project
 * Source: /home/claude/floppy8/floppy8-main/extract.c
 * 
 * Clean implementation of FM (single density) and MFM (double density)
 * encoding with histogram-based format auto-detection.
 */

#ifndef UFT_FM_MFM_DECODE_H
#define UFT_FM_MFM_DECODE_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * TIMING CONSTANTS (for 600MHz Teensy 4.1, divide by 16)
 *============================================================================*/

/* Base timing at 37.5 counts per microsecond */
#define UFT_COUNTS_PER_US       37.5f
#define UFT_TWO_US              75      /* 2µs in counts */
#define UFT_ONE_US              (UFT_TWO_US / 2)
#define UFT_THREE_US            (UFT_TWO_US + UFT_ONE_US)
#define UFT_FOUR_US             (UFT_TWO_US * 2)
#define UFT_FIVE_US             ((UFT_TWO_US * 2) + UFT_ONE_US)

#define UFT_HALF_US             (UFT_ONE_US / 2)
#define UFT_ONEP5_US            (UFT_ONE_US + UFT_HALF_US)
#define UFT_TWOP5_US            (UFT_TWO_US + UFT_HALF_US)
#define UFT_THREEP5_US          (UFT_THREE_US + UFT_HALF_US)
#define UFT_FOURP5_US           (UFT_FOUR_US + UFT_HALF_US)

/* Thresholds for classification */
#define UFT_FM_SPLIT            UFT_THREE_US        /* FM: <3µs = short, >=3µs = long */
#define UFT_MFM_SPLIT_LO        UFT_TWOP5_US        /* MFM: 2µs vs 3µs boundary */
#define UFT_MFM_SPLIT_HI        UFT_THREEP5_US      /* MFM: 3µs vs 4µs boundary */

#define UFT_MAX_US              6                    /* Maximum µs bucket */

/*============================================================================
 * TRACK FORMAT TYPES
 *============================================================================*/

typedef enum {
    UFT_FORMAT_UNKNOWN = 0,
    UFT_FORMAT_FM      = 1,     /* Single density (FM) */
    UFT_FORMAT_MFM     = 2      /* Double density (MFM) */
} uft_track_format_t;

/*============================================================================
 * DISK GEOMETRY LIMITS
 *============================================================================*/

#define UFT_MAX_TRACKS          85
#define UFT_MAX_SIDES           2
#define UFT_MAX_SECTORS         33      /* Sectors 0-32 */
#define UFT_MAX_SECTOR_SIZE     1024    /* Size codes 0-3 */
#define UFT_NUM_SIZES           4       /* 128, 256, 512, 1024 */

/*============================================================================
 * FM SPECIAL MARKS (Bit Patterns)
 *============================================================================*/

/**
 * FM marks include clock violations to distinguish from data.
 * Format: interleaved clock and data bits
 * 
 * Index Mark:   Data=0xFC, Clock=0xD7 -> 1,1,1,0,1,1,0,1,1,1,0,0
 * Address Mark: Data=0xFE, Clock=0xC7 -> 1,1,1,0,0,0,1,1,1,1,1,0
 * Data Mark:    Data=0xFB, Clock=0xC7 -> 1,1,1,0,0,0,1,0,1,1,1,1
 * Deleted Mark: Data=0xF8, Clock=0xC7 -> 1,1,1,0,0,0,1,0,0,0,1
 */

static const uint8_t UFT_FM_INDEX_MARK[] = { 1,1,1,0,1,1,0,1,1,1,0,0 };
static const uint8_t UFT_FM_ADDR_MARK[]  = { 1,1,1,0,0,0,1,1,1,1,1,0 };
static const uint8_t UFT_FM_DATA_MARK[]  = { 1,1,1,0,0,0,1,0,1,1,1,1 };
static const uint8_t UFT_FM_DELD_MARK[]  = { 1,1,1,0,0,0,1,0,0,0,1 };

#define UFT_FM_INDEX_MARK_LEN   12
#define UFT_FM_ADDR_MARK_LEN    12
#define UFT_FM_DATA_MARK_LEN    12
#define UFT_FM_DELD_MARK_LEN    11

/*============================================================================
 * MFM SPECIAL MARKS (Byte Patterns)
 *============================================================================*/

/**
 * MFM marks use sync bytes (A1 with missing clock)
 * followed by identification byte.
 */

static const uint8_t UFT_MFM_INDEX_MARK[] = { 0xC2, 0xC2, 0xC2, 0xFC };
static const uint8_t UFT_MFM_ADDR_MARK[]  = { 0xA1, 0xA1, 0xA1, 0xFE };
static const uint8_t UFT_MFM_DATA_MARK[]  = { 0xA1, 0xA1, 0xA1, 0xFB };
static const uint8_t UFT_MFM_DELD_MARK[]  = { 0xA1, 0xA1, 0xA1, 0xF8 };

#define UFT_MFM_MARK_LEN        4

/*============================================================================
 * CRC-16 CCITT (Polynomial: X^16 + X^12 + X^5 + 1)
 *============================================================================*/

/**
 * Calculate CRC-16 CCITT
 * 
 * Initial value: 0xFFFF
 * Valid sector: CRC of (mark + data + crc_bytes) == 0x0000
 * 
 * @param buf Data buffer
 * @param count Number of bytes
 * @return CRC-16 value
 */
static inline uint16_t uft_crc16(const uint8_t *buf, uint32_t count) {
    uint16_t crc = 0xFFFF;
    
    for (uint32_t i = 0; i < count; i++) {
        uint8_t x = (crc >> 8) ^ buf[i];
        x ^= x >> 4;
        crc = (crc << 8) ^ ((uint16_t)(x << 12)) 
                        ^ ((uint16_t)(x << 5)) 
                        ^ ((uint16_t)x);
    }
    return crc;
}

/**
 * Calculate and append CRC to buffer
 * 
 * @param buf Data buffer (must have 2 extra bytes for CRC)
 * @param count Number of data bytes (not including CRC)
 */
static inline void uft_crc16_append(uint8_t *buf, uint32_t count) {
    uint16_t crc = uft_crc16(buf, count);
    buf[count]     = (crc >> 8) & 0xFF;     /* High byte first */
    buf[count + 1] = crc & 0xFF;
}

/**
 * Verify CRC (returns true if valid)
 */
static inline bool uft_crc16_verify(const uint8_t *buf, uint32_t count_with_crc) {
    return uft_crc16(buf, count_with_crc) == 0x0000;
}

/*============================================================================
 * FORMAT AUTO-DETECTION
 *============================================================================*/

/**
 * Build histogram of sample timings
 * 
 * @param samples Array of sample values (flux intervals)
 * @param n Number of samples
 * @param one_us Value representing 1µs
 * @param histogram Output array [UFT_MAX_US]
 */
static inline void uft_build_histogram(const uint32_t *samples, uint32_t n,
                                        uint32_t one_us, uint32_t *histogram) {
    memset(histogram, 0, UFT_MAX_US * sizeof(uint32_t));
    
    for (uint32_t i = 0; i < n; i++) {
        uint32_t us = (samples[i] + one_us / 2) / one_us;
        if (us >= UFT_MAX_US) us = UFT_MAX_US - 1;
        histogram[us]++;
    }
}

/**
 * Determine track format (FM vs MFM) from histogram
 * 
 * FM has peaks at 2µs and 4µs only.
 * MFM has peaks at 2µs, 3µs, and 4µs.
 * If >5% of samples are at 3µs, it's MFM.
 * 
 * @param histogram Timing histogram [UFT_MAX_US]
 * @param total Total number of samples
 * @return Detected format
 */
static inline uft_track_format_t uft_detect_format(const uint32_t *histogram, 
                                                    uint32_t total) {
    if (total == 0) return UFT_FORMAT_UNKNOWN;
    
    /* Check percentage at 3µs bucket */
    uint32_t pct_3us = (histogram[3] * 100) / total;
    
    return (pct_3us > 5) ? UFT_FORMAT_MFM : UFT_FORMAT_FM;
}

/**
 * Auto-detect format from raw samples
 */
static inline uft_track_format_t uft_detect_format_samples(const uint32_t *samples,
                                                            uint32_t n,
                                                            uint32_t one_us) {
    uint32_t histogram[UFT_MAX_US];
    uft_build_histogram(samples, n, one_us, histogram);
    return uft_detect_format(histogram, n);
}

/*============================================================================
 * SAMPLE CLASSIFICATION
 *============================================================================*/

/**
 * Classify sample to microsecond bucket
 */
static inline uint32_t uft_sample_to_us(uint32_t sample, uint32_t one_us) {
    uint32_t us = (sample + one_us / 2) / one_us;
    return (us < UFT_MAX_US) ? us : UFT_MAX_US - 1;
}

/**
 * Classify FM sample to bit value
 * 
 * @param sample Sample value
 * @param split Threshold (typically 3µs)
 * @return 0 for short (2µs), 1 for long (4µs)
 */
static inline uint8_t uft_fm_classify(uint32_t sample, uint32_t split) {
    return (sample < split) ? 1 : 0;
}

/**
 * Classify MFM sample to bit count
 * 
 * @param sample Sample value
 * @param split_lo Low threshold (2.5µs)
 * @param split_hi High threshold (3.5µs)
 * @return 2 for 2µs, 3 for 3µs, 4 for 4µs
 */
static inline uint8_t uft_mfm_classify(uint32_t sample, 
                                        uint32_t split_lo, uint32_t split_hi) {
    if (sample < split_lo) return 2;
    if (sample < split_hi) return 3;
    return 4;
}

/*============================================================================
 * FM BYTE EXTRACTION
 *============================================================================*/

/**
 * Extract FM byte from decoded bit stream
 * 
 * FM encoding: clock bits interleaved with data bits
 * Pattern: C D C D C D C D (8 data bits, up to 16 stream bits)
 * Clock=1 before data=0, Clock=1 before data=1
 * 
 * @param buf Pointer to bit stream position (updated)
 * @return Decoded byte
 */
static inline uint8_t uft_fm_fetch_byte(uint8_t **buf) {
    uint8_t byte = 0;
    uint8_t *p = *buf;
    
    for (int i = 0; i < 8; i++) {
        byte <<= 1;
        byte |= *p;
        /* Skip clock bit if present (clock=1 followed by data) */
        if (p[0] == 1 && p[1] == 1)
            p += 2;     /* Clock + data */
        else
            p += 1;     /* Data only */
    }
    *buf = p;
    return byte;
}

/**
 * Extract multiple FM bytes
 */
static inline uint8_t* uft_fm_fetch_bytes(uint8_t *in, uint8_t *out, uint32_t count) {
    for (uint32_t i = 0; i < count; i++) {
        out[i] = uft_fm_fetch_byte(&in);
    }
    return in;
}

/*============================================================================
 * MFM BIT/BYTE EXTRACTION
 *============================================================================*/

/**
 * Extract single MFM data bit from bit pairs
 * 
 * MFM encoding: pairs of bits encode single data bit
 * 00 -> 0, 01 -> 1, 10 -> 0, 11 -> invalid
 * 
 * @param buf Two consecutive bits
 * @return Data bit (0 or 1)
 */
static inline uint8_t uft_mfm_fetch_bit(const uint8_t *buf) {
    uint8_t pair = (buf[0] << 1) | buf[1];
    
    switch (pair) {
        case 0: case 2: return 0;
        case 1: return 1;
        default: return 0;  /* Invalid - treat as 0 */
    }
}

/**
 * Extract MFM byte from decoded bit stream
 * 
 * @param buf Pointer to bit stream position (updated)
 * @return Decoded byte
 */
static inline uint8_t uft_mfm_fetch_byte(uint8_t **buf) {
    uint8_t byte = 0;
    uint8_t *p = *buf;
    
    for (int i = 0; i < 8; i++) {
        byte <<= 1;
        byte |= uft_mfm_fetch_bit(p);
        p += 2;
    }
    *buf = p;
    return byte;
}

/**
 * Extract multiple MFM bytes
 */
static inline uint8_t* uft_mfm_fetch_bytes(uint8_t *in, uint8_t *out, uint32_t count) {
    for (uint32_t i = 0; i < count; i++) {
        out[i] = uft_mfm_fetch_byte(&in);
    }
    return in;
}

/*============================================================================
 * MARK PATTERN MATCHING
 *============================================================================*/

/**
 * Check if bit stream matches pattern
 */
static inline bool uft_mark_match(const uint8_t *stream, const uint8_t *pattern, 
                                   uint32_t len) {
    for (uint32_t i = 0; i < len; i++) {
        if (stream[i] != pattern[i]) return false;
    }
    return true;
}

/*============================================================================
 * ADDRESS FIELD PARSING
 *============================================================================*/

/**
 * Parsed address field
 */
typedef struct {
    uint8_t track;
    uint8_t side;
    uint8_t sector;
    uint8_t size_code;      /* 0=128, 1=256, 2=512, 3=1024 */
    uint32_t sector_size;   /* Actual size in bytes */
    bool valid;             /* CRC check passed */
} uft_address_field_t;

/**
 * Parse FM address field
 * 
 * Format: FE TT SS ST SZ CRC16
 * 
 * @param buf Bit stream after address mark
 * @param addr Output address field
 * @return Bytes consumed from stream, 0 on error
 */
static inline uint32_t uft_fm_parse_address(uint8_t *buf, uft_address_field_t *addr) {
    uint8_t data[7];    /* Mark + CHRN + CRC */
    uint8_t *end;
    
    data[0] = 0xFE;     /* Address mark */
    end = uft_fm_fetch_bytes(buf, &data[1], 6);
    
    if (uft_crc16(data, 7) != 0) {
        addr->valid = false;
        return 0;
    }
    
    addr->track = data[1];
    addr->side = data[2];
    addr->sector = data[3];
    addr->size_code = data[4];
    addr->sector_size = 128U << data[4];
    addr->valid = true;
    
    return (uint32_t)(end - buf);
}

/**
 * Parse MFM address field
 * 
 * Format: A1 A1 A1 FE TT SS ST SZ CRC16
 * 
 * @param buf Byte stream (starts at first A1)
 * @param addr Output address field
 * @return Bytes consumed, 0 on error
 */
static inline uint32_t uft_mfm_parse_address(uint8_t *buf, uft_address_field_t *addr) {
    uint8_t data[10];   /* Sync + Mark + CHRN + CRC */
    uint8_t *end;
    
    end = uft_mfm_fetch_bytes(buf, data, 10);
    
    if (uft_crc16(data, 10) != 0) {
        addr->valid = false;
        return 0;
    }
    
    addr->track = data[4];
    addr->side = data[5];
    addr->sector = data[6];
    addr->size_code = data[7];
    addr->sector_size = 128U << data[7];
    addr->valid = true;
    
    return (uint32_t)(end - buf);
}

/*============================================================================
 * SECTOR DATA PARSING
 *============================================================================*/

/**
 * Parse FM data field
 * 
 * @param buf Bit stream after data mark
 * @param sector_size Expected sector size
 * @param data_out Output buffer (must be sector_size bytes)
 * @return Bytes consumed, 0 on error
 */
static inline uint32_t uft_fm_parse_data(uint8_t *buf, uint32_t sector_size,
                                          uint8_t *data_out, bool *deleted) {
    uint8_t *temp = (uint8_t*)malloc(1 + sector_size + 2);
    uint8_t *end;
    
    if (!temp) return 0;
    
    temp[0] = *deleted ? 0xF8 : 0xFB;   /* Data/deleted mark */
    end = uft_fm_fetch_bytes(buf, &temp[1], sector_size + 2);
    
    if (uft_crc16(temp, 1 + sector_size + 2) != 0) {
        free(temp);
        return 0;
    }
    
    memcpy(data_out, &temp[1], sector_size);
    free(temp);
    
    return (uint32_t)(end - buf);
}

/**
 * Parse MFM data field
 */
static inline uint32_t uft_mfm_parse_data(uint8_t *buf, uint32_t sector_size,
                                           uint8_t *data_out, bool *deleted) {
    uint8_t *temp = (uint8_t*)malloc(4 + sector_size + 2);
    uint8_t *end;
    
    if (!temp) return 0;
    
    end = uft_mfm_fetch_bytes(buf, temp, 4 + sector_size + 2);
    
    if (uft_crc16(temp, 4 + sector_size + 2) != 0) {
        free(temp);
        return 0;
    }
    
    *deleted = (temp[3] == 0xF8);
    memcpy(data_out, &temp[4], sector_size);
    free(temp);
    
    return (uint32_t)(end - buf);
}

/*============================================================================
 * SECTOR SIZE UTILITIES
 *============================================================================*/

/**
 * Validate sector size
 */
static inline bool uft_valid_sector_size(uint32_t size) {
    return (size == 128 || size == 256 || size == 512 || size == 1024);
}

/**
 * Convert size to character for display
 */
static inline char uft_size_to_char(uint32_t size) {
    switch (size) {
        case 0:    return '.';
        case 128:  return '1';
        case 256:  return '2';
        case 512:  return '3';
        case 1024: return '4';
        default:   return '?';
    }
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_FM_MFM_DECODE_H */
