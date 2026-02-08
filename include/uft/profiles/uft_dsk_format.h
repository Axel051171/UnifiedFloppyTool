/**
 * @file uft_dsk_format.h
 * @brief DSK format profile - Generic CP/M and Apple II disk images
 * @author UFT Team
 * @date 2025
 * @version 1.0.0
 *
 * DSK is a generic sector-level disk image format used by multiple
 * platforms including CP/M systems, Apple II (DOS 3.3, ProDOS), and
 * various other 8-bit computers. It stores raw sector data without
 * any header, relying on file size and content to determine format.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef UFT_DSK_FORMAT_H
#define UFT_DSK_FORMAT_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * DSK Format Constants
 * ═══════════════════════════════════════════════════════════════════════════ */

/** @brief Common DSK sizes */
#define UFT_DSK_SIZE_APPLE_140K     143360      /**< Apple II DOS 3.3 */
#define UFT_DSK_SIZE_APPLE_160K     163840      /**< Apple II 16-sector prodos */
#define UFT_DSK_SIZE_CPM_180K       184320      /**< CP/M 180KB SSDD */
#define UFT_DSK_SIZE_CPM_200K       204800      /**< CP/M 200KB (Osborne, etc) */
#define UFT_DSK_SIZE_CPM_360K       368640      /**< CP/M 360KB DSDD */
#define UFT_DSK_SIZE_CPM_400K       409600      /**< CP/M 400KB */
#define UFT_DSK_SIZE_CPM_720K       737280      /**< CP/M 720KB */
#define UFT_DSK_SIZE_CPM_800K       819200      /**< CP/M 800KB */

/** @brief Sector sizes */
#define UFT_DSK_SECTOR_128          128
#define UFT_DSK_SECTOR_256          256
#define UFT_DSK_SECTOR_512          512
#define UFT_DSK_SECTOR_1024         1024

/* ═══════════════════════════════════════════════════════════════════════════
 * DSK Platform Types
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef enum {
    UFT_DSK_PLATFORM_UNKNOWN    = 0,
    UFT_DSK_PLATFORM_APPLE_DOS  = 1,    /**< Apple II DOS 3.3 */
    UFT_DSK_PLATFORM_APPLE_PRO  = 2,    /**< Apple II ProDOS */
    UFT_DSK_PLATFORM_CPM        = 3,    /**< Generic CP/M */
    UFT_DSK_PLATFORM_KAYPRO     = 4,    /**< Kaypro */
    UFT_DSK_PLATFORM_OSBORNE    = 5,    /**< Osborne */
    UFT_DSK_PLATFORM_MORROW     = 6,    /**< Morrow */
    UFT_DSK_PLATFORM_EPSON      = 7,    /**< Epson QX-10 */
    UFT_DSK_PLATFORM_XEROX      = 8     /**< Xerox 820 */
} uft_dsk_platform_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * DSK Geometry
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    const char* name;
    uft_dsk_platform_t platform;
    uint8_t tracks;
    uint8_t sides;
    uint8_t sectors;
    uint16_t sector_size;
    uint32_t total_size;
} uft_dsk_geometry_t;

/**
 * @brief Parsed DSK information
 */
typedef struct {
    uft_dsk_platform_t platform;
    const char* platform_name;
    uint8_t tracks;
    uint8_t sides;
    uint8_t sectors_per_track;
    uint16_t sector_size;
    uint32_t total_size;
    bool is_apple_dos;
    bool is_prodos;
    bool is_cpm;
} uft_dsk_info_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Standard Geometries
 * ═══════════════════════════════════════════════════════════════════════════ */

static const uft_dsk_geometry_t UFT_DSK_GEOMETRIES[] = {
    /* Apple II */
    { "Apple DOS 3.3 (140KB)", UFT_DSK_PLATFORM_APPLE_DOS, 35, 1, 16, 256, 143360 },
    { "Apple ProDOS (140KB)",  UFT_DSK_PLATFORM_APPLE_PRO, 35, 1, 16, 256, 143360 },
    
    /* CP/M - 8" */
    { "CP/M 8\" SSSD (250KB)", UFT_DSK_PLATFORM_CPM, 77, 1, 26, 128, 256256 },
    { "CP/M 8\" SSDD (500KB)", UFT_DSK_PLATFORM_CPM, 77, 1, 26, 256, 512512 },
    
    /* CP/M - 5.25" */
    { "Kaypro II (200KB)",     UFT_DSK_PLATFORM_KAYPRO,   40, 1, 10, 512, 204800 },
    { "Kaypro 4 (400KB)",      UFT_DSK_PLATFORM_KAYPRO,   40, 2, 10, 512, 409600 },
    { "Osborne (100KB)",       UFT_DSK_PLATFORM_OSBORNE,  40, 1,  5, 512, 102400 },
    { "Osborne DD (200KB)",    UFT_DSK_PLATFORM_OSBORNE,  40, 1, 10, 512, 204800 },
    { "Xerox 820 (250KB)",     UFT_DSK_PLATFORM_XEROX,    40, 1, 18, 128, 92160 },
    
    /* Generic CP/M sizes */
    { "CP/M 180KB SSDD",       UFT_DSK_PLATFORM_CPM, 40, 1,  9, 512, 184320 },
    { "CP/M 360KB DSDD",       UFT_DSK_PLATFORM_CPM, 40, 2,  9, 512, 368640 },
    { "CP/M 720KB",            UFT_DSK_PLATFORM_CPM, 80, 2,  9, 512, 737280 },
    
    { NULL, 0, 0, 0, 0, 0, 0 }
};

/* ═══════════════════════════════════════════════════════════════════════════
 * Helper Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Find geometry matching file size
 */
static inline const uft_dsk_geometry_t* uft_dsk_find_geometry(size_t size) {
    for (int i = 0; UFT_DSK_GEOMETRIES[i].name != NULL; i++) {
        if (UFT_DSK_GEOMETRIES[i].total_size == size) {
            return &UFT_DSK_GEOMETRIES[i];
        }
    }
    return NULL;
}

/**
 * @brief Platform name
 */
static inline const char* uft_dsk_platform_name(uft_dsk_platform_t platform) {
    switch (platform) {
        case UFT_DSK_PLATFORM_APPLE_DOS: return "Apple II DOS 3.3";
        case UFT_DSK_PLATFORM_APPLE_PRO: return "Apple II ProDOS";
        case UFT_DSK_PLATFORM_CPM:       return "CP/M";
        case UFT_DSK_PLATFORM_KAYPRO:    return "Kaypro";
        case UFT_DSK_PLATFORM_OSBORNE:   return "Osborne";
        case UFT_DSK_PLATFORM_MORROW:    return "Morrow";
        case UFT_DSK_PLATFORM_EPSON:     return "Epson";
        case UFT_DSK_PLATFORM_XEROX:     return "Xerox 820";
        default: return "Unknown";
    }
}

/**
 * @brief Check for Apple DOS 3.3 VTOC
 */
static inline bool uft_dsk_is_apple_dos(const uint8_t* data, size_t size) {
    if (!data || size < 143360) return false;
    
    /* VTOC at track 17, sector 0 (offset 0x11000) */
    size_t vtoc_offset = 17 * 16 * 256;
    if (vtoc_offset + 256 > size) return false;
    
    const uint8_t* vtoc = data + vtoc_offset;
    
    /* Check VTOC fields */
    if (vtoc[0x00] != 0x04) return false;  /* Catalog track */
    if (vtoc[0x01] != 0x00) return false;  /* Catalog sector start */
    if (vtoc[0x27] != 122) return false;   /* Max track/sector pairs */
    if (vtoc[0x34] != 35) return false;    /* Tracks per disk */
    if (vtoc[0x35] != 16) return false;    /* Sectors per track */
    
    return true;
}

/**
 * @brief Check for ProDOS volume
 */
static inline bool uft_dsk_is_prodos(const uint8_t* data, size_t size) {
    if (!data || size < 143360) return false;
    
    /* Volume directory at block 2 (offset 0x400) */
    const uint8_t* vol = data + 0x400;
    
    /* Check storage type (bits 4-7 should be 0xF for volume) */
    if ((vol[0x04] & 0xF0) != 0xF0) return false;
    
    /* Check volume name length */
    uint8_t name_len = vol[0x04] & 0x0F;
    if (name_len == 0 || name_len > 15) return false;
    
    return true;
}

/**
 * @brief Probe data for DSK format
 */
static inline int uft_dsk_probe(const uint8_t* data, size_t size) {
    if (!data || size < 1024) return 0;
    
    int score = 0;
    
    /* Check known sizes */
    const uft_dsk_geometry_t* geom = uft_dsk_find_geometry(size);
    if (geom) {
        score += 30;
    }
    
    /* Apple II detection */
    if (size == UFT_DSK_SIZE_APPLE_140K) {
        if (uft_dsk_is_apple_dos(data, size)) {
            score += 50;
        } else if (uft_dsk_is_prodos(data, size)) {
            score += 50;
        }
    }
    
    /* CP/M detection - check for boot sector signature */
    if (data[0] == 0xC3 || data[0] == 0xEB || data[0] == 0xE9) {
        score += 15;  /* Jump instruction */
    }
    
    return (score > 100) ? 100 : score;
}

/**
 * @brief Parse DSK file
 */
static inline bool uft_dsk_parse(const uint8_t* data, size_t size, uft_dsk_info_t* info) {
    if (!data || !info || size < 1024) return false;
    
    memset(info, 0, sizeof(*info));
    info->total_size = size;
    
    /* Check Apple II */
    if (size == UFT_DSK_SIZE_APPLE_140K) {
        if (uft_dsk_is_apple_dos(data, size)) {
            info->platform = UFT_DSK_PLATFORM_APPLE_DOS;
            info->is_apple_dos = true;
        } else if (uft_dsk_is_prodos(data, size)) {
            info->platform = UFT_DSK_PLATFORM_APPLE_PRO;
            info->is_prodos = true;
        }
        info->tracks = 35;
        info->sides = 1;
        info->sectors_per_track = 16;
        info->sector_size = 256;
    } else {
        /* Use geometry table */
        const uft_dsk_geometry_t* geom = uft_dsk_find_geometry(size);
        if (geom) {
            info->platform = geom->platform;
            info->tracks = geom->tracks;
            info->sides = geom->sides;
            info->sectors_per_track = geom->sectors;
            info->sector_size = geom->sector_size;
            info->is_cpm = (geom->platform >= UFT_DSK_PLATFORM_CPM);
        } else {
            /* Unknown - try to guess */
            info->platform = UFT_DSK_PLATFORM_UNKNOWN;
            info->sector_size = 512;
        }
    }
    
    info->platform_name = uft_dsk_platform_name(info->platform);
    
    return true;
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_DSK_FORMAT_H */
