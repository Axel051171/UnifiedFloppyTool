/*
 * uft_preset_commodore.h - Commodore Floppy Format Presets
 *
 * Part of UnifiedFloppyTool (UFT) v3.3.0
 * Based on MAME d64_dsk.cpp, d71_dsk.cpp, d80_dsk.cpp, d81_dsk.cpp,
 * d82_dsk.cpp, g64_dsk.cpp (BSD-3-Clause, Curt Coder)
 *
 * Covers all Commodore disk formats from VIC-1540 to 1581.
 * Uses proprietary GCR encoding (except D81 which uses MFM).
 */

#ifndef UFT_PRESET_COMMODORE_H
#define UFT_PRESET_COMMODORE_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * Format IDs
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef enum uft_commodore_format_id {
    /* D64 - 1541/1551/4040 (5.25" SS GCR) */
    UFT_CBM_D64_35T = 0,        /* 170K 35 tracks (standard) */
    UFT_CBM_D64_40T,            /* 196K 40 tracks */
    UFT_CBM_D64_42T,            /* 205K 42 tracks */
    UFT_CBM_D64_35T_ERR,        /* 35T with error info */
    UFT_CBM_D64_40T_ERR,        /* 40T with error info */
    
    /* D71 - 1571 (5.25" DS GCR) */
    UFT_CBM_D71,                /* 340K 70 tracks */
    UFT_CBM_D71_ERR,            /* 70T with error info */
    
    /* D80 - 8050 (5.25" SS GCR, 77 tracks) */
    UFT_CBM_D80,                /* 520K 77 tracks */
    
    /* D82 - 8250/SFD-1001 (5.25" DS GCR, 154 tracks) */
    UFT_CBM_D82,                /* 1040K 154 tracks */
    
    /* D81 - 1581 (3.5" DS MFM) */
    UFT_CBM_D81,                /* 800K 80 tracks MFM */
    
    /* G64 - GCR raw format */
    UFT_CBM_G64,                /* G64 raw GCR */
    UFT_CBM_G71,                /* G71 raw GCR (double sided) */
    
    /* P64 - Flux format */
    UFT_CBM_P64,                /* P64 flux format */
    
    /* NIB - Nibble format */
    UFT_CBM_NIB,                /* NIB raw nibble format */
    
    UFT_CBM_FORMAT_COUNT
} uft_commodore_format_id_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * D64 Zone Layout (variable sectors per track)
 * ═══════════════════════════════════════════════════════════════════════════ */

#define UFT_D64_ZONE_COUNT      4

typedef struct uft_d64_zone {
    uint8_t start_track;    /* First track in zone (1-based) */
    uint8_t end_track;      /* Last track in zone (1-based) */
    uint8_t sectors;        /* Sectors per track */
    uint16_t speed_zone;    /* Speed zone number (0-3) */
} uft_d64_zone_t;

static const uft_d64_zone_t UFT_D64_ZONES[UFT_D64_ZONE_COUNT] = {
    {  1, 17, 21, 3 },  /* Tracks 1-17:  21 sectors */
    { 18, 24, 19, 2 },  /* Tracks 18-24: 19 sectors */
    { 25, 30, 18, 1 },  /* Tracks 25-30: 18 sectors */
    { 31, 42, 17, 0 },  /* Tracks 31-42: 17 sectors (extended to 42) */
};

/* D64 total sectors per track count */
static inline uint8_t uft_d64_sectors_for_track(uint8_t track) {
    if (track >= 1 && track <= 17) return 21;
    if (track >= 18 && track <= 24) return 19;
    if (track >= 25 && track <= 30) return 18;
    if (track >= 31 && track <= 42) return 17;
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * D80/D82 Zone Layout (8050/8250)
 * ═══════════════════════════════════════════════════════════════════════════ */

#define UFT_D80_ZONE_COUNT      4

static const uft_d64_zone_t UFT_D80_ZONES[UFT_D80_ZONE_COUNT] = {
    {  1, 39, 29, 0 },  /* Tracks 1-39:  29 sectors */
    { 40, 53, 27, 1 },  /* Tracks 40-53: 27 sectors */
    { 54, 64, 25, 2 },  /* Tracks 54-64: 25 sectors */
    { 65, 77, 23, 3 },  /* Tracks 65-77: 23 sectors */
};

/* ═══════════════════════════════════════════════════════════════════════════
 * Preset Structure
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct uft_commodore_preset {
    uft_commodore_format_id_t id;
    const char *name;
    const char *description;
    
    /* Geometry */
    uint8_t  form_factor;       /* 5 = 5.25", 3 = 3.5" */
    uint8_t  cyls;              /* Number of tracks */
    uint8_t  heads;
    uint16_t total_sectors;     /* Total sectors on disk */
    uint16_t bps;               /* Bytes per sector */
    
    /* Encoding */
    uint8_t  encoding;          /* 0=GCR, 1=MFM */
    uint8_t  has_error_info;    /* 1 = has error bytes */
    uint8_t  raw_format;        /* 1 = G64/NIB/P64 */
    
    /* File sizes */
    uint32_t file_size;         /* Standard file size */
    uint32_t file_size_err;     /* File size with error info */
    
    /* Extensions */
    const char *extensions;
} uft_commodore_preset_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Preset Table
 * ═══════════════════════════════════════════════════════════════════════════ */

static const uft_commodore_preset_t UFT_CBM_PRESETS[] = {
    /* D64 - 1541 */
    {
        .id = UFT_CBM_D64_35T,
        .name = "C64 D64 35T",
        .description = "170K Commodore 1541 35 tracks",
        .form_factor = 5, .cyls = 35, .heads = 1,
        .total_sectors = 683, .bps = 256,
        .encoding = 0, .has_error_info = 0, .raw_format = 0,
        .file_size = 174848, .file_size_err = 175531,
        .extensions = ".d64"
    },
    {
        .id = UFT_CBM_D64_40T,
        .name = "C64 D64 40T",
        .description = "196K Commodore 1541 40 tracks",
        .form_factor = 5, .cyls = 40, .heads = 1,
        .total_sectors = 768, .bps = 256,
        .encoding = 0, .has_error_info = 0, .raw_format = 0,
        .file_size = 196608, .file_size_err = 197376,
        .extensions = ".d64"
    },
    {
        .id = UFT_CBM_D64_42T,
        .name = "C64 D64 42T",
        .description = "205K Commodore 1541 42 tracks",
        .form_factor = 5, .cyls = 42, .heads = 1,
        .total_sectors = 802, .bps = 256,
        .encoding = 0, .has_error_info = 0, .raw_format = 0,
        .file_size = 205312, .file_size_err = 206114,
        .extensions = ".d64"
    },
    {
        .id = UFT_CBM_D64_35T_ERR,
        .name = "C64 D64 35T+ERR",
        .description = "170K Commodore 1541 with error info",
        .form_factor = 5, .cyls = 35, .heads = 1,
        .total_sectors = 683, .bps = 256,
        .encoding = 0, .has_error_info = 1, .raw_format = 0,
        .file_size = 175531, .file_size_err = 175531,
        .extensions = ".d64"
    },
    {
        .id = UFT_CBM_D64_40T_ERR,
        .name = "C64 D64 40T+ERR",
        .description = "196K Commodore 1541 40T with error info",
        .form_factor = 5, .cyls = 40, .heads = 1,
        .total_sectors = 768, .bps = 256,
        .encoding = 0, .has_error_info = 1, .raw_format = 0,
        .file_size = 197376, .file_size_err = 197376,
        .extensions = ".d64"
    },
    
    /* D71 - 1571 */
    {
        .id = UFT_CBM_D71,
        .name = "C128 D71",
        .description = "340K Commodore 1571 double sided",
        .form_factor = 5, .cyls = 35, .heads = 2,
        .total_sectors = 1366, .bps = 256,
        .encoding = 0, .has_error_info = 0, .raw_format = 0,
        .file_size = 349696, .file_size_err = 351062,
        .extensions = ".d71"
    },
    {
        .id = UFT_CBM_D71_ERR,
        .name = "C128 D71+ERR",
        .description = "340K Commodore 1571 with error info",
        .form_factor = 5, .cyls = 35, .heads = 2,
        .total_sectors = 1366, .bps = 256,
        .encoding = 0, .has_error_info = 1, .raw_format = 0,
        .file_size = 351062, .file_size_err = 351062,
        .extensions = ".d71"
    },
    
    /* D80 - 8050 */
    {
        .id = UFT_CBM_D80,
        .name = "CBM D80",
        .description = "520K Commodore 8050",
        .form_factor = 5, .cyls = 77, .heads = 1,
        .total_sectors = 2083, .bps = 256,
        .encoding = 0, .has_error_info = 0, .raw_format = 0,
        .file_size = 533248, .file_size_err = 535331,
        .extensions = ".d80"
    },
    
    /* D82 - 8250/SFD-1001 */
    {
        .id = UFT_CBM_D82,
        .name = "CBM D82",
        .description = "1040K Commodore 8250/SFD-1001",
        .form_factor = 5, .cyls = 77, .heads = 2,
        .total_sectors = 4166, .bps = 256,
        .encoding = 0, .has_error_info = 0, .raw_format = 0,
        .file_size = 1066496, .file_size_err = 1070662,
        .extensions = ".d82"
    },
    
    /* D81 - 1581 (MFM!) */
    {
        .id = UFT_CBM_D81,
        .name = "C128 D81",
        .description = "800K Commodore 1581 (MFM)",
        .form_factor = 3, .cyls = 80, .heads = 2,
        .total_sectors = 3200, .bps = 256,
        .encoding = 1, .has_error_info = 0, .raw_format = 0,
        .file_size = 819200, .file_size_err = 822400,
        .extensions = ".d81"
    },
    
    /* G64 - Raw GCR */
    {
        .id = UFT_CBM_G64,
        .name = "C64 G64",
        .description = "G64 raw GCR format",
        .form_factor = 5, .cyls = 42, .heads = 1,
        .total_sectors = 0, .bps = 0,
        .encoding = 0, .has_error_info = 0, .raw_format = 1,
        .file_size = 0, .file_size_err = 0,
        .extensions = ".g64"
    },
    {
        .id = UFT_CBM_G71,
        .name = "C128 G71",
        .description = "G71 raw GCR format double sided",
        .form_factor = 5, .cyls = 42, .heads = 2,
        .total_sectors = 0, .bps = 0,
        .encoding = 0, .has_error_info = 0, .raw_format = 1,
        .file_size = 0, .file_size_err = 0,
        .extensions = ".g71"
    },
    
    /* P64 - Flux */
    {
        .id = UFT_CBM_P64,
        .name = "C64 P64",
        .description = "P64 flux format",
        .form_factor = 5, .cyls = 42, .heads = 1,
        .total_sectors = 0, .bps = 0,
        .encoding = 0, .has_error_info = 0, .raw_format = 1,
        .file_size = 0, .file_size_err = 0,
        .extensions = ".p64"
    },
    
    /* NIB - Nibble */
    {
        .id = UFT_CBM_NIB,
        .name = "C64 NIB",
        .description = "NIB raw nibble format",
        .form_factor = 5, .cyls = 42, .heads = 1,
        .total_sectors = 0, .bps = 0,
        .encoding = 0, .has_error_info = 0, .raw_format = 1,
        .file_size = 0, .file_size_err = 0,
        .extensions = ".nib;.nbz"
    },
};

/* ═══════════════════════════════════════════════════════════════════════════
 * GCR Tables
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Commodore GCR 4-to-5 encoding table */
static const uint8_t UFT_CBM_GCR_ENCODE[16] = {
    0x0A, 0x0B, 0x12, 0x13, 0x0E, 0x0F, 0x16, 0x17,
    0x09, 0x19, 0x1A, 0x1B, 0x0D, 0x1D, 0x1E, 0x15
};

/* Commodore GCR 5-to-4 decoding table */
static const uint8_t UFT_CBM_GCR_DECODE[32] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0x08, 0x00, 0x01, 0xFF, 0x0C, 0x04, 0x05,
    0xFF, 0xFF, 0x02, 0x03, 0xFF, 0x0F, 0x06, 0x07,
    0xFF, 0x09, 0x0A, 0x0B, 0xFF, 0x0D, 0x0E, 0xFF
};

/* ═══════════════════════════════════════════════════════════════════════════
 * API Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

static inline const uft_commodore_preset_t *uft_cbm_get_preset(uft_commodore_format_id_t id) {
    if (id >= UFT_CBM_FORMAT_COUNT) return NULL;
    return &UFT_CBM_PRESETS[id];
}

/* Auto-detect D64 format from file size */
static inline uft_commodore_format_id_t uft_d64_detect_from_size(uint64_t size) {
    switch (size) {
        case 174848: return UFT_CBM_D64_35T;
        case 175531: return UFT_CBM_D64_35T_ERR;
        case 196608: return UFT_CBM_D64_40T;
        case 197376: return UFT_CBM_D64_40T_ERR;
        case 205312: return UFT_CBM_D64_42T;
        case 349696: return UFT_CBM_D71;
        case 351062: return UFT_CBM_D71_ERR;
        case 533248: return UFT_CBM_D80;
        case 819200: return UFT_CBM_D81;
        case 1066496: return UFT_CBM_D82;
        default: return UFT_CBM_FORMAT_COUNT;
    }
}

#ifdef __cplusplus
}
#endif
#endif /* UFT_PRESET_COMMODORE_H */
