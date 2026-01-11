/**
 * @file uft_atr_format.h
 * @brief ATR format profile - Atari 8-bit disk image format
 * @author UFT Team
 * @date 2025
 * @version 1.0.0
 *
 * ATR is the standard disk image format for Atari 8-bit computers
 * (400/800/XL/XE series). It stores sector data with a 16-byte header
 * describing the disk geometry and size.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef UFT_ATR_FORMAT_H
#define UFT_ATR_FORMAT_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * ATR Format Constants
 * ═══════════════════════════════════════════════════════════════════════════ */

/** @brief ATR magic number (0x0296 little-endian) */
#define UFT_ATR_MAGIC               0x0296
#define UFT_ATR_MAGIC_LO            0x96
#define UFT_ATR_MAGIC_HI            0x02

/** @brief ATR header size */
#define UFT_ATR_HEADER_SIZE         16

/** @brief Standard Atari sector sizes */
#define UFT_ATR_SECTOR_SIZE_SD      128     /**< Single density */
#define UFT_ATR_SECTOR_SIZE_DD      256     /**< Double density */

/** @brief Standard disk sizes */
#define UFT_ATR_SIZE_SSSD           92160   /**< 90KB SS/SD */
#define UFT_ATR_SIZE_SSED           133120  /**< 130KB SS/ED */
#define UFT_ATR_SIZE_SSDD           183936  /**< 180KB SS/DD */
#define UFT_ATR_SIZE_DSDD           368256  /**< 360KB DS/DD */

/** @brief Sectors per track */
#define UFT_ATR_SECTORS_SD          18      /**< Single density */
#define UFT_ATR_SECTORS_ED          26      /**< Enhanced density */
#define UFT_ATR_SECTORS_DD          18      /**< Double density */

/* ═══════════════════════════════════════════════════════════════════════════
 * ATR Flags
 * ═══════════════════════════════════════════════════════════════════════════ */

#define UFT_ATR_FLAG_PROTECTED      0x01    /**< Write protected */

/* ═══════════════════════════════════════════════════════════════════════════
 * ATR Structures
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief ATR file header (16 bytes)
 */
#pragma pack(push, 1)
typedef struct {
    uint16_t magic;                 /**< Magic number (0x0296) */
    uint16_t paragraphs_lo;         /**< Size in paragraphs (low word) */
    uint16_t sector_size;           /**< Sector size (128 or 256) */
    uint8_t paragraphs_hi;          /**< Size in paragraphs (high byte) */
    uint8_t crc;                    /**< CRC (usually 0) */
    uint32_t unused;                /**< Unused */
    uint8_t flags;                  /**< Flags */
    uint8_t bad_sectors_lo;         /**< Bad sector count (low) */
    uint8_t bad_sectors_hi;         /**< Bad sector count (high) */
    uint8_t unused2;                /**< Unused */
} uft_atr_header_t;
#pragma pack(pop)

/**
 * @brief ATR disk type
 */
typedef enum {
    UFT_ATR_TYPE_UNKNOWN    = 0,
    UFT_ATR_TYPE_SSSD       = 1,    /**< Single-sided single-density */
    UFT_ATR_TYPE_SSED       = 2,    /**< Single-sided enhanced-density */
    UFT_ATR_TYPE_SSDD       = 3,    /**< Single-sided double-density */
    UFT_ATR_TYPE_DSDD       = 4     /**< Double-sided double-density */
} uft_atr_disk_type_t;

/**
 * @brief Parsed ATR information
 */
typedef struct {
    uint16_t sector_size;
    uint32_t image_size;            /**< Size in bytes (excluding header) */
    uint32_t sector_count;
    uint8_t tracks;
    uint8_t sides;
    uint8_t sectors_per_track;
    uft_atr_disk_type_t disk_type;
    bool write_protected;
    uint16_t bad_sectors;
} uft_atr_info_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Compile-time Verification
 * ═══════════════════════════════════════════════════════════════════════════ */

_Static_assert(sizeof(uft_atr_header_t) == 16, "ATR header must be 16 bytes");

/* ═══════════════════════════════════════════════════════════════════════════
 * Helper Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

static inline bool uft_atr_validate_signature(const uint8_t* data, size_t size) {
    if (!data || size < UFT_ATR_HEADER_SIZE) return false;
    return (data[0] == UFT_ATR_MAGIC_LO && data[1] == UFT_ATR_MAGIC_HI);
}

static inline uint32_t uft_atr_get_image_size(const uft_atr_header_t* hdr) {
    /* Size in 16-byte paragraphs */
    uint32_t paragraphs = hdr->paragraphs_lo | ((uint32_t)hdr->paragraphs_hi << 16);
    return paragraphs * 16;
}

static inline uft_atr_disk_type_t uft_atr_detect_type(uint32_t size, uint16_t sector_size) {
    if (sector_size == 128) {
        if (size <= UFT_ATR_SIZE_SSSD + 1024) return UFT_ATR_TYPE_SSSD;
        if (size <= UFT_ATR_SIZE_SSED + 1024) return UFT_ATR_TYPE_SSED;
    } else if (sector_size == 256) {
        if (size <= UFT_ATR_SIZE_SSDD + 1024) return UFT_ATR_TYPE_SSDD;
        if (size <= UFT_ATR_SIZE_DSDD + 1024) return UFT_ATR_TYPE_DSDD;
    }
    return UFT_ATR_TYPE_UNKNOWN;
}

static inline const char* uft_atr_type_name(uft_atr_disk_type_t type) {
    switch (type) {
        case UFT_ATR_TYPE_SSSD: return "SS/SD (90KB)";
        case UFT_ATR_TYPE_SSED: return "SS/ED (130KB)";
        case UFT_ATR_TYPE_SSDD: return "SS/DD (180KB)";
        case UFT_ATR_TYPE_DSDD: return "DS/DD (360KB)";
        default: return "Unknown";
    }
}

static inline int uft_atr_probe(const uint8_t* data, size_t size) {
    if (!uft_atr_validate_signature(data, size)) return 0;
    
    int score = 50;
    const uft_atr_header_t* hdr = (const uft_atr_header_t*)data;
    
    /* Valid sector size */
    if (hdr->sector_size == 128 || hdr->sector_size == 256) {
        score += 20;
    }
    
    /* Size matches file */
    uint32_t img_size = uft_atr_get_image_size(hdr);
    if (size == UFT_ATR_HEADER_SIZE + img_size) {
        score += 25;
    } else if (size >= UFT_ATR_HEADER_SIZE + img_size - 256) {
        score += 10;
    }
    
    /* Known disk type */
    if (uft_atr_detect_type(img_size, hdr->sector_size) != UFT_ATR_TYPE_UNKNOWN) {
        score += 5;
    }
    
    return (score > 100) ? 100 : score;
}

static inline bool uft_atr_parse(const uint8_t* data, size_t size, uft_atr_info_t* info) {
    if (!uft_atr_validate_signature(data, size) || !info) return false;
    
    const uft_atr_header_t* hdr = (const uft_atr_header_t*)data;
    memset(info, 0, sizeof(*info));
    
    info->sector_size = hdr->sector_size;
    info->image_size = uft_atr_get_image_size(hdr);
    info->write_protected = (hdr->flags & UFT_ATR_FLAG_PROTECTED) != 0;
    info->bad_sectors = hdr->bad_sectors_lo | (hdr->bad_sectors_hi << 8);
    
    /* Calculate sectors (first 3 sectors are always 128 bytes in DD) */
    if (info->sector_size == 256 && info->image_size > 384) {
        info->sector_count = 3 + (info->image_size - 384) / 256;
    } else {
        info->sector_count = info->image_size / info->sector_size;
    }
    
    info->disk_type = uft_atr_detect_type(info->image_size, info->sector_size);
    
    /* Set geometry based on type */
    switch (info->disk_type) {
        case UFT_ATR_TYPE_SSSD:
            info->tracks = 40; info->sides = 1; info->sectors_per_track = 18;
            break;
        case UFT_ATR_TYPE_SSED:
            info->tracks = 40; info->sides = 1; info->sectors_per_track = 26;
            break;
        case UFT_ATR_TYPE_SSDD:
            info->tracks = 40; info->sides = 1; info->sectors_per_track = 18;
            break;
        case UFT_ATR_TYPE_DSDD:
            info->tracks = 40; info->sides = 2; info->sectors_per_track = 18;
            break;
        default:
            break;
    }
    
    return true;
}

static inline void uft_atr_create_header(uft_atr_header_t* hdr, uint32_t size, 
                                         uint16_t sector_size) {
    if (!hdr) return;
    memset(hdr, 0, sizeof(*hdr));
    
    hdr->magic = UFT_ATR_MAGIC;
    hdr->sector_size = sector_size;
    
    uint32_t paragraphs = size / 16;
    hdr->paragraphs_lo = paragraphs & 0xFFFF;
    hdr->paragraphs_hi = (paragraphs >> 16) & 0xFF;
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_ATR_FORMAT_H */
