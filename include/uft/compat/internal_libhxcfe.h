/**
 * @file internal_libhxcfe.h
 * @brief Internal HxCFE types for UFT compatibility
 */

#ifndef INTERNAL_LIBHXCFE_H
#define INTERNAL_LIBHXCFE_H

#include "libhxcfe.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Internal Constants
 * ============================================================================ */

#define MAX_CACHE_SECTOR        512
#define DEFAULT_DD_BITRATE      250000
#define DEFAULT_HD_BITRATE      500000
#define DEFAULT_ED_BITRATE      1000000
#define DEFAULT_RPM             300

/* Gap sizes */
#define IBM_GAP3_DD             84
#define IBM_GAP3_HD             108
#define IBM_GAP4A               80
#define IBM_GAP1                50
#define IBM_GAP2                22

/* Sync patterns */
#define MFM_SYNC_PATTERN        0x4489  /* A1 with missing clock */
#define FM_SYNC_PATTERN         0xF57E
#define AMIGA_SYNC_PATTERN      0x4489

/* Address marks */
#define IBM_IDAM                0xFE    /* ID Address Mark */
#define IBM_DAM                 0xFB    /* Data Address Mark */
#define IBM_DDAM                0xF8    /* Deleted Data Address Mark */

/* ============================================================================
 * Track Building Helpers
 * ============================================================================ */

typedef struct {
    int32_t indexlen;
    int32_t indexpos;
    int32_t track_len;
    int32_t number_of_sector;
    uint8_t start_sector_id;
    uint8_t fill_byte;
    uint8_t gap3_size;
    int32_t interleave;
    int32_t skew;
    int32_t bitrate;
    int32_t rpm;
    int32_t encoding;
} track_generator_config_t;

/* ============================================================================
 * Sector Operations
 * ============================================================================ */

typedef struct {
    int32_t track;
    int32_t side;
    int32_t sector;
    int32_t sectorsize;
    uint8_t* buffer;
    int32_t buffer_size;
    int32_t use_crc;
    uint16_t crc;
    int32_t status;     /* 0=OK, -1=CRC error, -2=missing */
} sector_operation_t;

/* ============================================================================
 * Utility Macros
 * ============================================================================ */

#define HXCFE_GETBYTE(buf, idx)     ((buf)[(idx)])
#define HXCFE_SETBYTE(buf, idx, v)  ((buf)[(idx)] = (v))

#define HXCFE_GETWORD_BE(buf, idx)  \
    ((uint16_t)(((buf)[(idx)] << 8) | (buf)[(idx)+1]))
    
#define HXCFE_GETWORD_LE(buf, idx)  \
    ((uint16_t)(((buf)[(idx)+1] << 8) | (buf)[(idx)]))

#define HXCFE_GETDWORD_BE(buf, idx) \
    ((uint32_t)(((buf)[(idx)] << 24) | ((buf)[(idx)+1] << 16) | \
                ((buf)[(idx)+2] << 8) | (buf)[(idx)+3]))

#define HXCFE_GETDWORD_LE(buf, idx) \
    ((uint32_t)(((buf)[(idx)+3] << 24) | ((buf)[(idx)+2] << 16) | \
                ((buf)[(idx)+1] << 8) | (buf)[(idx)]))

/* ============================================================================
 * Debug/Logging
 * ============================================================================ */

#ifndef HXCFE_DEBUG
#define HXCFE_DEBUG 0
#endif

#if HXCFE_DEBUG
#include <stdio.h>
#define hxcfe_log(ctx, level, fmt, ...) \
    fprintf(stderr, "[HXCFE] " fmt "\n", ##__VA_ARGS__)
#else
#define hxcfe_log(ctx, level, fmt, ...) ((void)0)
#endif

#define HXCFE_LOG_INFO    0
#define HXCFE_LOG_WARNING 1
#define HXCFE_LOG_ERROR   2
#define HXCFE_LOG_DEBUG   3

#ifdef __cplusplus
}
#endif

#endif /* INTERNAL_LIBHXCFE_H */
