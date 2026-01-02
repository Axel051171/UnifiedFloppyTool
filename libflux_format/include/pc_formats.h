// SPDX-License-Identifier: MIT
/*
 * pc_formats.h - Unified PC/DOS Disk Formats
 * 
 * Complete PC disk image format support including:
 * - IMG: Raw PC disk images (360KB - 2.88MB)
 * - TD0: Teledisk compressed images (RLE + Huffman)
 * - IMD: ImageDisk format (CP/M preservation standard)
 * 
 * @version 2.8.7
 * @date 2024-12-26
 */

#ifndef PC_FORMATS_H
#define PC_FORMATS_H

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================*
 * PC/DOS FORMATS
 *============================================================================*/

/* IMG - Raw PC Disk Images */
#include "uft_img.h"

/* TD0 - Teledisk (Compressed PC Format) */
#include "uft_td0.h"

/* IMD - ImageDisk (CP/M Preservation Standard) */
#include "uft_imd.h"

/*============================================================================*
 * FORMAT DETECTION
 *============================================================================*/

/**
 * @brief PC disk format types
 */
typedef enum {
    PC_FORMAT_UNKNOWN = 0,
    PC_FORMAT_IMG,      /* Raw PC disk image */
    PC_FORMAT_TD0,      /* Teledisk compressed */
    PC_FORMAT_IMD       /* ImageDisk */
} pc_format_type_t;

/**
 * @brief Auto-detect PC disk format from buffer
 * @param buffer File data buffer
 * @param size Buffer size in bytes
 * @return Detected format type
 */
static inline pc_format_type_t pc_detect_format(const uint8_t* buffer, size_t size)
{
    if (!buffer || size < 16) return PC_FORMAT_UNKNOWN;
    
    /* TD0: "TD" or "td" signature */
    if ((buffer[0] == 'T' && buffer[1] == 'D') ||
        (buffer[0] == 't' && buffer[1] == 'd')) {
        if (uft_td0_detect(buffer, size)) {
            return PC_FORMAT_TD0;
        }
    }
    
    /* IMD: "IMD " ASCII signature + 0x1A terminator */
    if (buffer[0] == 'I' && buffer[1] == 'M' && 
        buffer[2] == 'D' && buffer[3] == ' ') {
        /* Look for 0x1A within first 2KB */
        for (size_t i = 4; i < (size < 2048 ? size : 2048); i++) {
            if (buffer[i] == 0x1A) {
                return PC_FORMAT_IMD;
            }
        }
    }
    
    /* IMG: Detect by size heuristics (no signature) */
    uft_img_geometry_t geom;
    if (uft_img_detect(buffer, size, &geom)) {
        return PC_FORMAT_IMG;
    }
    
    return PC_FORMAT_UNKNOWN;
}

/**
 * @brief Get format name string
 * @param fmt Format type
 * @return Format name
 */
static inline const char* pc_format_name(pc_format_type_t fmt)
{
    switch (fmt) {
        case PC_FORMAT_IMG: return "IMG (Raw PC Disk)";
        case PC_FORMAT_TD0: return "TD0 (Teledisk)";
        case PC_FORMAT_IMD: return "IMD (ImageDisk)";
        default: return "Unknown";
    }
}

/*============================================================================*
 * STANDARD PC GEOMETRIES
 *============================================================================*/

/**
 * @brief Standard PC disk geometries
 */
typedef struct {
    const char* name;
    uint16_t cylinders;
    uint8_t heads;
    uint16_t spt;
    uint16_t sector_size;
    uint32_t total_bytes;
} pc_geometry_t;

static const pc_geometry_t PC_GEOMETRIES[] = {
    { "5.25\" 360KB",  40, 2,  9, 512,  368640 },
    { "5.25\" 1.2MB",  80, 2, 15, 512, 1228800 },
    { "3.5\" 720KB",   80, 2,  9, 512,  737280 },
    { "3.5\" 1.44MB",  80, 2, 18, 512, 1474560 },
    { "3.5\" 2.88MB",  80, 2, 36, 512, 2949120 },
    { NULL, 0, 0, 0, 0, 0 }
};

#ifdef __cplusplus
}
#endif

#endif /* PC_FORMATS_H */
