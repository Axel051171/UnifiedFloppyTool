/**
 * @file uft_dim_format.h
 * @brief DIM format profile - Japanese PC disk image format
 * @author UFT Team
 * @date 2025
 * @version 1.0.0
 *
 * DIM is a disk image format used primarily for Japanese PC systems
 * including PC-98, X68000, and FM-Towns. It stores sector data with
 * a simple header describing the disk geometry.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef UFT_DIM_FORMAT_H
#define UFT_DIM_FORMAT_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * DIM Format Constants
 * ═══════════════════════════════════════════════════════════════════════════ */

/** @brief DIM header size */
#define UFT_DIM_HEADER_SIZE         256

/** @brief DIM signature byte position */
#define UFT_DIM_SIGNATURE_POS       0xAB

/** @brief DIM media types */
typedef enum {
    UFT_DIM_MEDIA_2HD           = 0x00,     /**< 1.2MB 2HD */
    UFT_DIM_MEDIA_2HS           = 0x01,     /**< 1.44MB 2HS (rare) */
    UFT_DIM_MEDIA_2HC           = 0x02,     /**< 1.2MB 2HC (15 sectors) */
    UFT_DIM_MEDIA_2HDE          = 0x03,     /**< 1.2MB 2HDE */
    UFT_DIM_MEDIA_2HQ           = 0x09,     /**< 1.44MB 2HQ */
    UFT_DIM_MEDIA_2DD_8         = 0x11,     /**< 640KB 2DD (8 sectors) */
    UFT_DIM_MEDIA_2DD_9         = 0x19      /**< 720KB 2DD (9 sectors) */
} uft_dim_media_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * DIM Structures
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief DIM file header (256 bytes)
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t media_type;             /**< Media type code */
    uint8_t reserved1[0xAA];        /**< Reserved */
    uint8_t signature;              /**< 0x00 for valid DIM */
    uint8_t reserved2[0x54];        /**< Reserved to 256 bytes */
} uft_dim_header_t;
#pragma pack(pop)

/**
 * @brief DIM geometry info
 */
typedef struct {
    const char* name;
    uint8_t media_type;
    uint8_t cylinders;
    uint8_t heads;
    uint8_t sectors;
    uint16_t sector_size;
    uint32_t total_size;
} uft_dim_geometry_t;

/**
 * @brief Parsed DIM information
 */
typedef struct {
    uint8_t media_type;
    uint8_t cylinders;
    uint8_t heads;
    uint8_t sectors_per_track;
    uint16_t sector_size;
    uint32_t data_size;
    const char* media_name;
} uft_dim_info_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Standard Geometries
 * ═══════════════════════════════════════════════════════════════════════════ */

static const uft_dim_geometry_t UFT_DIM_GEOMETRIES[] = {
    { "2HD (1.2MB)",  UFT_DIM_MEDIA_2HD,    77, 2,  8, 1024, 1261568 },
    { "2HC (1.2MB)",  UFT_DIM_MEDIA_2HC,    80, 2, 15,  512, 1228800 },
    { "2HQ (1.44MB)", UFT_DIM_MEDIA_2HQ,    80, 2, 18,  512, 1474560 },
    { "2DD8 (640KB)", UFT_DIM_MEDIA_2DD_8,  80, 2,  8,  512,  655360 },
    { "2DD9 (720KB)", UFT_DIM_MEDIA_2DD_9,  80, 2,  9,  512,  737280 },
    { NULL, 0, 0, 0, 0, 0, 0 }
};

/* ═══════════════════════════════════════════════════════════════════════════
 * Helper Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

static inline const uft_dim_geometry_t* uft_dim_get_geometry(uint8_t media_type) {
    for (int i = 0; UFT_DIM_GEOMETRIES[i].name != NULL; i++) {
        if (UFT_DIM_GEOMETRIES[i].media_type == media_type) {
            return &UFT_DIM_GEOMETRIES[i];
        }
    }
    return NULL;
}

static inline const char* uft_dim_media_name(uint8_t media_type) {
    const uft_dim_geometry_t* geom = uft_dim_get_geometry(media_type);
    return geom ? geom->name : "Unknown";
}

static inline bool uft_dim_validate(const uint8_t* data, size_t size) {
    if (!data || size < UFT_DIM_HEADER_SIZE) return false;
    /* Signature byte at 0xAB should be 0x00 */
    return data[UFT_DIM_SIGNATURE_POS] == 0x00;
}

static inline int uft_dim_probe(const uint8_t* data, size_t size) {
    if (!data || size < UFT_DIM_HEADER_SIZE) return 0;
    
    int score = 0;
    
    /* Check signature byte */
    if (data[UFT_DIM_SIGNATURE_POS] == 0x00) score += 30;
    
    /* Check media type */
    uint8_t media = data[0];
    if (uft_dim_get_geometry(media) != NULL) {
        score += 40;
        
        /* Verify file size matches geometry */
        const uft_dim_geometry_t* geom = uft_dim_get_geometry(media);
        if (size == UFT_DIM_HEADER_SIZE + geom->total_size) {
            score += 30;
        }
    }
    
    return (score > 100) ? 100 : score;
}

static inline bool uft_dim_parse(const uint8_t* data, size_t size, uft_dim_info_t* info) {
    if (!uft_dim_validate(data, size) || !info) return false;
    
    memset(info, 0, sizeof(*info));
    info->media_type = data[0];
    
    const uft_dim_geometry_t* geom = uft_dim_get_geometry(info->media_type);
    if (geom) {
        info->cylinders = geom->cylinders;
        info->heads = geom->heads;
        info->sectors_per_track = geom->sectors;
        info->sector_size = geom->sector_size;
        info->data_size = geom->total_size;
        info->media_name = geom->name;
    }
    
    return true;
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_DIM_FORMAT_H */
