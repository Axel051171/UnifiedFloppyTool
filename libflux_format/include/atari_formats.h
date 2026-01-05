// SPDX-License-Identifier: MIT
/*
 * atari_formats.h - Unified Atari 8-bit Disk Formats
 * 
 * Complete Atari 8-bit disk image format support including:
 * - ATR: Standard Atari disk images (90KB - 360KB+)
 * - ATX: Protected Atari disk images (flux-level, weak bits, timing)
 * 
 * a8rawconv Compatibility:
 * This module provides API compatibility with a8rawconv command-line
 * parameters for seamless integration with existing Atari workflows.
 * 
 * @version 2.8.7
 * @date 2024-12-26
 */

#ifndef ATARI_FORMATS_H
#define ATARI_FORMATS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================*
 * ATARI 8-BIT FORMATS
 *============================================================================*/

/* ATR - Standard Atari 8-bit Disk Images */
#include "uft_atr.h"

/* ATX - Protected Atari 8-bit Disk Images (Flux-Level) */
#include "uft_atx.h"

/* XFD - Atari 8-bit Raw Disk Images (a8rawconv RAW format!) */
#include "uft_xfd.h"

/*============================================================================*
 * FORMAT DETECTION
 *============================================================================*/

/**
 * @brief Atari disk format types
 */
typedef enum {
    ATARI_FORMAT_UNKNOWN = 0,
    ATARI_FORMAT_ATR,    /* Standard Atari disk image */
    ATARI_FORMAT_ATX,    /* Protected Atari disk image (flux-level) */
    ATARI_FORMAT_XFD     /* Atari raw disk image (a8rawconv RAW!) */
} atari_format_type_t;

/**
 * @brief Auto-detect Atari disk format from buffer
 * @param buffer File data buffer
 * @param size Buffer size in bytes
 * @return Detected format type
 */
static inline atari_format_type_t atari_detect_format(const uint8_t* buffer, size_t size)
{
    if (!buffer || size < 16) return ATARI_FORMAT_UNKNOWN;
    
    /* ATX: "ATX\0" signature */
    if (uft_atx_detect(buffer, size)) {
        return ATARI_FORMAT_ATX;
    }
    
    /* ATR: 0x0296 magic (little-endian) */
    if (size >= 16) {
        uint16_t magic = buffer[0] | (buffer[1] << 8);
        if (magic == 0x0296) {
            return ATARI_FORMAT_ATR;
        }
    }
    
    /* XFD: Raw format - detect by size (must be checked last!) */
    uft_xfd_geometry_t xfd_geom;
    if (uft_xfd_detect(buffer, size, &xfd_geom)) {
        return ATARI_FORMAT_XFD;
    }
    
    return ATARI_FORMAT_UNKNOWN;
}

/**
 * @brief Get format name string
 * @param fmt Format type
 * @return Format name
 */
static inline const char* atari_format_name(atari_format_type_t fmt)
{
    switch (fmt) {
        case ATARI_FORMAT_ATR: return "ATR (Standard Atari 8-bit)";
        case ATARI_FORMAT_ATX: return "ATX (Protected Atari 8-bit)";
        case ATARI_FORMAT_XFD: return "XFD (Atari 8-bit Raw / a8rawconv)";
        default: return "Unknown";
    }
}

/*============================================================================*
 * a8rawconv COMPATIBILITY
 *============================================================================*/

/**
 * @brief a8rawconv conversion modes
 * 
 * Compatible with a8rawconv command-line parameters:
 * - Standard mode: ATR <-> RAW conversion
 * - Protected mode: ATX <-> RAW conversion (LOSSY!)
 */
typedef enum {
    A8RAWCONV_MODE_ATR_TO_RAW = 0,  /* ATR → RAW (XFD) */
    A8RAWCONV_MODE_RAW_TO_ATR,      /* RAW (XFD) → ATR */
    A8RAWCONV_MODE_ATX_TO_RAW,      /* ATX → RAW (LOSSY!) */
    A8RAWCONV_MODE_ATR_TO_XFD,      /* ATR → XFD (same as ATR_TO_RAW) */
    A8RAWCONV_MODE_XFD_TO_ATR,      /* XFD → ATR (same as RAW_TO_ATR) */
    A8RAWCONV_MODE_ATR_INFO,        /* Display ATR information */
    A8RAWCONV_MODE_ATX_INFO,        /* Display ATX information */
    A8RAWCONV_MODE_XFD_INFO         /* Display XFD information */
} a8rawconv_mode_t;

/**
 * @brief a8rawconv compatible geometry
 * 
 * Matches a8rawconv density parameters:
 * - SD: Single density (90KB, 720 sectors, 128 bytes/sector)
 * - ED: Enhanced density (130KB, 1040 sectors, 128 bytes/sector)
 * - DD: Double density (180KB, 720 sectors, 256 bytes/sector)
 */
typedef struct {
    const char* name;
    uint16_t sectors;
    uint16_t sector_size;
    uint16_t boot_sectors;  /* First N sectors at 128 bytes (DD quirk) */
    uint32_t total_bytes;
} a8rawconv_geometry_t;

static const a8rawconv_geometry_t A8RAWCONV_GEOMETRIES[] = {
    { "SD",   720,  128, 0,  92160 },   /* Single Density */
    { "ED",  1040,  128, 0, 133120 },   /* Enhanced Density */
    { "DD",   720,  256, 3, 183936 },   /* Double Density (boot quirk!) */
    { "DD+", 1040,  256, 3, 266496 },   /* DD Enhanced */
    { NULL, 0, 0, 0, 0 }
};

/**
 * @brief Get a8rawconv geometry by name
 * @param name Geometry name (SD, ED, DD, DD+)
 * @return Geometry pointer or NULL
 */
static inline const a8rawconv_geometry_t* a8rawconv_get_geometry(const char* name)
{
    if (!name) return NULL;
    
    for (int i = 0; A8RAWCONV_GEOMETRIES[i].name; i++) {
        if (strcmp(A8RAWCONV_GEOMETRIES[i].name, name) == 0) {
            return &A8RAWCONV_GEOMETRIES[i];
        }
    }
    return NULL;
}

/**
 * @brief a8rawconv compatible conversion
 * @param mode Conversion mode
 * @param input_path Input file path
 * @param output_path Output file path
 * @param geom_name Geometry name (SD/ED/DD/DD+) for RAW→ATR
 * @return 0 on success, negative on error
 */
static inline int a8rawconv_convert(a8rawconv_mode_t mode,
                                    const char* input_path,
                                    const char* output_path,
                                    const char* geom_name)
{
    switch (mode) {
        case A8RAWCONV_MODE_ATR_TO_RAW: {
            uft_atr_ctx_t ctx;
            int ret = uft_atr_open(&ctx, input_path, false);
            if (ret != UFT_ATR_OK) return ret;
            
            ret = uft_atr_convert_to_raw(&ctx, output_path);
            uft_atr_close(&ctx);
            return ret;
        }
        
        case A8RAWCONV_MODE_RAW_TO_ATR: {
            /* Not implemented in basic API - requires geometry */
            /* User should use ATR builder functions directly */
            return -1;
        }
        
        case A8RAWCONV_MODE_ATX_TO_RAW: {
            uft_atx_ctx_t ctx;
            int ret = uft_atx_open(&ctx, input_path);
            if (ret != UFT_ATX_SUCCESS) return ret;
            
            /* WARNING: LOSSY conversion! */
            ret = uft_atx_to_raw(&ctx, output_path);
            uft_atx_close(&ctx);
            return ret;
        }
        
        case A8RAWCONV_MODE_ATR_TO_XFD:
            /* Same as ATR_TO_RAW - XFD is the raw format */
            return a8rawconv_convert(A8RAWCONV_MODE_ATR_TO_RAW, 
                                    input_path, output_path, geom_name);
        
        case A8RAWCONV_MODE_XFD_TO_ATR:
            /* Same as RAW_TO_ATR */
            return a8rawconv_convert(A8RAWCONV_MODE_RAW_TO_ATR,
                                    input_path, output_path, geom_name);
        
        default:
            return -1;
    }
}

/*============================================================================*
 * STANDARD ATARI GEOMETRIES
 *============================================================================*/

/**
 * @brief Standard Atari 8-bit disk geometries
 */
typedef struct {
    const char* name;
    uint16_t cylinders;
    uint8_t heads;
    uint16_t spt;
    uint16_t sector_size;
    uint32_t total_bytes;
} atari_geometry_t;

static const atari_geometry_t ATARI_GEOMETRIES[] = {
    { "SD (90KB)",    40, 1, 18, 128,  92160 },
    { "ED (130KB)",   40, 1, 26, 128, 133120 },
    { "DD (180KB)",   40, 2, 18, 256, 183936 },  /* Note: boot sectors 128! */
    { "DD+ (360KB)",  80, 2, 18, 256, 368640 },
    { "QD (720KB)",   80, 2, 36, 256, 737280 },
    { NULL, 0, 0, 0, 0, 0 }
};

/*============================================================================*
 * UTILITY FUNCTIONS
 *============================================================================*/

/**
 * @brief Check if ATR image has boot sector quirk
 * @param ctx ATR context
 * @return true if first 3 sectors are 128 bytes, nominal is 256
 */
static inline bool atari_atr_has_boot_quirk(const uft_atr_ctx_t* ctx)
{
    return ctx && ctx->has_short_boot;
}

/**
 * @brief Check if ATX image has protection
 * @param ctx ATX context
 * @return true if weak bits or timing information present
 */
static inline bool atari_atx_has_protection(const uft_atx_ctx_t* ctx)
{
    if (!ctx || !ctx->tracks) return false;
    
    for (size_t i = 0; i < ctx->track_count; i++) {
        for (uint8_t s = 0; s < ctx->tracks[i].nsec; s++) {
            if (ctx->tracks[i].sectors[s].meta.has_weak_bits ||
                ctx->tracks[i].sectors[s].meta.has_timing) {
                return true;
            }
        }
    }
    return false;
}

#ifdef __cplusplus
}
#endif

#endif /* ATARI_FORMATS_H */
