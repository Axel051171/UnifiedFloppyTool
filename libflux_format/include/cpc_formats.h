// SPDX-License-Identifier: MIT
/*
 * cpc_formats.h - Unified CPC/Spectrum Disk Formats
 * 
 * Complete Amstrad CPC and ZX Spectrum +3 disk image support:
 * - DSK: Standard CPC/Spectrum disk images
 * - Extended DSK: Variable sector sizes per track
 * 
 * @version 2.8.8
 * @date 2024-12-26
 */

#ifndef CPC_FORMATS_H
#define CPC_FORMATS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================*
 * CPC/SPECTRUM FORMATS
 *============================================================================*/

/* DSK - CPC/Spectrum Disk Images */
#include "uft_dsk.h"

/*============================================================================*
 * FORMAT DETECTION
 *============================================================================*/

/**
 * @brief CPC/Spectrum disk format types
 */
typedef enum {
    CPC_FORMAT_UNKNOWN = 0,
    CPC_FORMAT_DSK_STANDARD,  /* Standard DSK */
    CPC_FORMAT_DSK_EXTENDED   /* Extended DSK */
} cpc_format_type_t;

/**
 * @brief Auto-detect CPC/Spectrum disk format from buffer
 * @param buffer File data buffer
 * @param size Buffer size in bytes
 * @return Detected format type
 */
static inline cpc_format_type_t cpc_detect_format(const uint8_t* buffer, size_t size)
{
    if (!buffer || size < 256) return CPC_FORMAT_UNKNOWN;
    
    /* Extended DSK: "EXTENDED CPC DSK File\r\nDisk-Info\r\n" */
    if (size >= 34 && 
        buffer[0] == 'E' && buffer[1] == 'X' && buffer[2] == 'T') {
        return CPC_FORMAT_DSK_EXTENDED;
    }
    
    /* Standard DSK: "MV - CPCEMU Disk-File\r\nDisk-Info\r\n" */
    if (size >= 34 && 
        buffer[0] == 'M' && buffer[1] == 'V' && buffer[2] == ' ') {
        return CPC_FORMAT_DSK_STANDARD;
    }
    
    return CPC_FORMAT_UNKNOWN;
}

/**
 * @brief Get format name string
 * @param fmt Format type
 * @return Format name
 */
static inline const char* cpc_format_name(cpc_format_type_t fmt)
{
    switch (fmt) {
        case CPC_FORMAT_DSK_STANDARD: return "DSK (Standard CPC/Spectrum)";
        case CPC_FORMAT_DSK_EXTENDED: return "DSK (Extended CPC/Spectrum)";
        default: return "Unknown";
    }
}

/*============================================================================*
 * STANDARD CPC GEOMETRIES
 *============================================================================*/

/**
 * @brief Standard CPC disk geometries
 */
typedef struct {
    const char* name;
    uint16_t cylinders;
    uint8_t heads;
    uint16_t spt;
    uint16_t sector_size;
    uint32_t total_bytes;
} cpc_geometry_t;

static const cpc_geometry_t CPC_GEOMETRIES[] = {
    { "CPC Data (180KB)",    40, 1,  9, 512, 184320 },
    { "CPC System (180KB)",  40, 1,  9, 512, 184320 },
    { "CPC Data (720KB)",    80, 2,  9, 512, 737280 },
    { "Spectrum +3 (180KB)", 40, 1,  9, 512, 184320 },
    { "Spectrum +3 (720KB)", 80, 2,  9, 512, 737280 },
    { NULL, 0, 0, 0, 0, 0 }
};

/*============================================================================*
 * CPC FORMAT NOTES
 *============================================================================*/

/**
 * CPC DSK Format Notes:
 * 
 * STANDARD DSK:
 *   - Fixed track size
 *   - "MV - CPCEMU Disk-File\r\nDisk-Info\r\n" signature
 *   - Simple, uniform sectors
 * 
 * EXTENDED DSK:
 *   - Variable sector sizes per track
 *   - "EXTENDED CPC DSK File\r\nDisk-Info\r\n" signature
 *   - Per-track size table
 *   - More flexible for copy-protected disks
 * 
 * PLATFORMS:
 *   - Amstrad CPC (primary)
 *   - ZX Spectrum +3
 *   - Some other CP/M systems
 * 
 * EMULATORS:
 *   - CPCEMU
 *   - WinAPE
 *   - Arnold
 *   - RetroVirtualMachine
 */

#ifdef __cplusplus
}
#endif

#endif /* CPC_FORMATS_H */
