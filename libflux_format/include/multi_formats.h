// SPDX-License-Identifier: MIT
/*
 * multi_formats.h - Multi-Platform Disk Formats
 * 
 * Flexible disk image formats that work across multiple platforms:
 * - FDI: Flexible Disk Image (PC/Atari/Amiga)
 * - ADF: Amiga Disk File
 * 
 * @version 2.8.8
 * @date 2024-12-26
 */

#ifndef MULTI_FORMATS_H
#define MULTI_FORMATS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================*
 * MULTI-PLATFORM FORMATS
 *============================================================================*/

/* FDI - Flexible Disk Image */
#include "uft_fdi.h"

/* ADF - Amiga Disk File */
#include "uft_adf.h"

/*============================================================================*
 * FORMAT DETECTION
 *============================================================================*/

/**
 * @brief Multi-platform disk format types
 */
typedef enum {
    MULTI_FORMAT_UNKNOWN = 0,
    MULTI_FORMAT_FDI,    /* Flexible Disk Image */
    MULTI_FORMAT_ADF     /* Amiga Disk File */
} multi_format_type_t;

/**
 * @brief Auto-detect multi-platform disk format from buffer
 * @param buffer File data buffer
 * @param size Buffer size in bytes
 * @return Detected format type
 */
static inline multi_format_type_t multi_detect_format(const uint8_t* buffer, size_t size)
{
    if (!buffer || size < 16) return MULTI_FORMAT_UNKNOWN;
    
    /* FDI: "FDI" signature */
    if (buffer[0] == 'F' && buffer[1] == 'D' && buffer[2] == 'I') {
        return MULTI_FORMAT_FDI;
    }
    
    /* ADF: Detect by size (DD: 901120, HD: 1802240) */
    uft_adf_geometry_t adf_geom;
    if (uft_adf_detect(buffer, size, &adf_geom)) {
        return MULTI_FORMAT_ADF;
    }
    
    return MULTI_FORMAT_UNKNOWN;
}

/**
 * @brief Get format name string
 * @param fmt Format type
 * @return Format name
 */
static inline const char* multi_format_name(multi_format_type_t fmt)
{
    switch (fmt) {
        case MULTI_FORMAT_FDI: return "FDI (Flexible Disk Image)";
        case MULTI_FORMAT_ADF: return "ADF (Amiga Disk File)";
        default: return "Unknown";
    }
}

/*============================================================================*
 * AMIGA STANDARD GEOMETRIES
 *============================================================================*/

/**
 * @brief Standard Amiga disk geometries
 */
typedef struct {
    const char* name;
    uint16_t cylinders;
    uint8_t heads;
    uint16_t spt;
    uint16_t sector_size;
    uint32_t total_bytes;
} amiga_geometry_t;

static const amiga_geometry_t AMIGA_GEOMETRIES[] = {
    { "DD (880KB)", 80, 2, 11, 512,  901120 },
    { "HD (1.76MB)", 80, 2, 22, 512, 1802240 },
    { NULL, 0, 0, 0, 0, 0 }
};

/*============================================================================*
 * FORMAT NOTES
 *============================================================================*/

/**
 * FDI (Flexible Disk Image):
 *   - Semi-raw container format
 *   - Used by various emulators/tools
 *   - Per-track data blocks
 *   - Optional timing/flags
 *   - Multi-platform (PC/Atari/Amiga)
 * 
 * ADF (Amiga Disk File):
 *   - Raw sector dump of Amiga floppy
 *   - DD: 880KB (80×2×11×512)
 *   - HD: 1.76MB (80×2×22×512) [rare]
 *   - No MFM sync words or weak bits
 *   - Simple, emulator-friendly format
 */

#ifdef __cplusplus
}
#endif

#endif /* MULTI_FORMATS_H */
