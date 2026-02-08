/*
 * uft_preset_apple.h - Apple II/III Floppy Format Presets
 *
 * Part of UnifiedFloppyTool (UFT) v3.3.0
 * Based on MAME ap2_dsk.cpp, ap_dsk35.cpp (BSD-3-Clause, Olivier Galibert, R. Belmont)
 *
 * Apple II used 5.25" disks with proprietary GCR encoding (not standard MFM).
 * Apple III and Macintosh used 3.5" disks with variable speed zones (GCR).
 */

#ifndef UFT_PRESET_APPLE_H
#define UFT_PRESET_APPLE_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * Format IDs
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef enum uft_apple_format_id {
    /* Apple II 5.25" */
    UFT_APPLE2_DOS32 = 0,       /* DOS 3.2 - 13 sectors */
    UFT_APPLE2_DOS33,           /* DOS 3.3 - 16 sectors */
    UFT_APPLE2_PRODOS,          /* ProDOS - 16 sectors */
    UFT_APPLE2_NIB,             /* NIB raw nibble format */
    UFT_APPLE2_EDD,             /* EDD copy-protected format */
    UFT_APPLE2_WOZ,             /* WOZ flux format */
    
    /* Apple 3.5" (Mac/Apple II GS) */
    UFT_APPLE_35_SS_400K,       /* 400K single sided */
    UFT_APPLE_35_DS_800K,       /* 800K double sided */
    UFT_APPLE_35_HD_1440K,      /* 1.44M high density (MFM) */
    
    UFT_APPLE_FORMAT_COUNT
} uft_apple_format_id_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Apple II GCR Constants
 * ═══════════════════════════════════════════════════════════════════════════ */

#define UFT_APPLE2_TRACK_COUNT      35
#define UFT_APPLE2_TRACK_COUNT_EXT  40      /* Extended 40-track disks */
#define UFT_APPLE2_SECTOR_SIZE      256
#define UFT_APPLE2_SECTORS_13       13      /* DOS 3.2 */
#define UFT_APPLE2_SECTORS_16       16      /* DOS 3.3 / ProDOS */
#define UFT_APPLE2_NIB_TRACK_SIZE   6656    /* Raw nibble track size */

/* Apple 3.5" variable speed zones */
#define UFT_APPLE35_ZONE_COUNT      5
static const uint8_t UFT_APPLE35_SECTORS_PER_ZONE[UFT_APPLE35_ZONE_COUNT] = {
    12, 11, 10, 9, 8  /* Sectors per track in each zone */
};

/* ═══════════════════════════════════════════════════════════════════════════
 * Preset Structure
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct uft_apple_preset {
    uft_apple_format_id_t id;
    const char *name;
    const char *description;
    
    /* Geometry */
    uint8_t  form_factor;       /* 5 = 5.25", 3 = 3.5" */
    uint8_t  cyls;
    uint8_t  heads;
    uint8_t  secs;              /* Sectors per track (or 0 for variable) */
    uint16_t bps;               /* Bytes per sector */
    
    /* Encoding */
    uint8_t  encoding;          /* 0=GCR, 1=MFM */
    uint8_t  gcr_mode;          /* 0=5-3 (13sec), 1=6-2 (16sec), 2=Sony */
    
    /* Image type */
    uint8_t  raw_format;        /* 0=sector, 1=nibble, 2=flux */
    
    /* Extensions */
    const char *extensions;
} uft_apple_preset_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Preset Table
 * ═══════════════════════════════════════════════════════════════════════════ */

static const uft_apple_preset_t UFT_APPLE_PRESETS[] = {
    /* Apple II 5.25" */
    {
        .id = UFT_APPLE2_DOS32,
        .name = "Apple II DOS 3.2",
        .description = "113K Apple II DOS 3.2 (13 sectors)",
        .form_factor = 5, .cyls = 35, .heads = 1, .secs = 13, .bps = 256,
        .encoding = 0, .gcr_mode = 0,
        .raw_format = 0,
        .extensions = ".d13;.dsk"
    },
    {
        .id = UFT_APPLE2_DOS33,
        .name = "Apple II DOS 3.3",
        .description = "140K Apple II DOS 3.3 (16 sectors)",
        .form_factor = 5, .cyls = 35, .heads = 1, .secs = 16, .bps = 256,
        .encoding = 0, .gcr_mode = 1,
        .raw_format = 0,
        .extensions = ".do;.dsk"
    },
    {
        .id = UFT_APPLE2_PRODOS,
        .name = "Apple II ProDOS",
        .description = "140K Apple II ProDOS (16 sectors)",
        .form_factor = 5, .cyls = 35, .heads = 1, .secs = 16, .bps = 256,
        .encoding = 0, .gcr_mode = 1,
        .raw_format = 0,
        .extensions = ".po;.dsk"
    },
    {
        .id = UFT_APPLE2_NIB,
        .name = "Apple II NIB",
        .description = "232K Apple II raw nibble format",
        .form_factor = 5, .cyls = 35, .heads = 1, .secs = 0, .bps = 0,
        .encoding = 0, .gcr_mode = 1,
        .raw_format = 1,
        .extensions = ".nib"
    },
    {
        .id = UFT_APPLE2_EDD,
        .name = "Apple II EDD",
        .description = "Apple II EDD copy-protected format",
        .form_factor = 5, .cyls = 35, .heads = 1, .secs = 0, .bps = 0,
        .encoding = 0, .gcr_mode = 1,
        .raw_format = 1,
        .extensions = ".edd"
    },
    {
        .id = UFT_APPLE2_WOZ,
        .name = "Apple II WOZ",
        .description = "Apple II WOZ flux format",
        .form_factor = 5, .cyls = 35, .heads = 1, .secs = 0, .bps = 0,
        .encoding = 0, .gcr_mode = 1,
        .raw_format = 2,
        .extensions = ".woz"
    },
    
    /* Apple 3.5" */
    {
        .id = UFT_APPLE_35_SS_400K,
        .name = "Apple 3.5\" 400K",
        .description = "400K Macintosh/Apple II GS single sided",
        .form_factor = 3, .cyls = 80, .heads = 1, .secs = 0, .bps = 512,
        .encoding = 0, .gcr_mode = 2,
        .raw_format = 0,
        .extensions = ".image;.dsk"
    },
    {
        .id = UFT_APPLE_35_DS_800K,
        .name = "Apple 3.5\" 800K",
        .description = "800K Macintosh/Apple II GS double sided",
        .form_factor = 3, .cyls = 80, .heads = 2, .secs = 0, .bps = 512,
        .encoding = 0, .gcr_mode = 2,
        .raw_format = 0,
        .extensions = ".image;.dsk"
    },
    {
        .id = UFT_APPLE_35_HD_1440K,
        .name = "Apple 3.5\" 1.44M",
        .description = "1.44M Macintosh high density (PC compatible)",
        .form_factor = 3, .cyls = 80, .heads = 2, .secs = 18, .bps = 512,
        .encoding = 1, .gcr_mode = 0,
        .raw_format = 0,
        .extensions = ".image;.dsk"
    },
};

/* ═══════════════════════════════════════════════════════════════════════════
 * GCR Tables
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Apple II 6-and-2 GCR encoding (DOS 3.3, ProDOS) */
static const uint8_t UFT_APPLE2_GCR62_ENCODE[64] = {
    0x96, 0x97, 0x9A, 0x9B, 0x9D, 0x9E, 0x9F, 0xA6,
    0xA7, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xB2, 0xB3,
    0xB4, 0xB5, 0xB6, 0xB7, 0xB9, 0xBA, 0xBB, 0xBC,
    0xBD, 0xBE, 0xBF, 0xCB, 0xCD, 0xCE, 0xCF, 0xD3,
    0xD6, 0xD7, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE,
    0xDF, 0xE5, 0xE6, 0xE7, 0xE9, 0xEA, 0xEB, 0xEC,
    0xED, 0xEE, 0xEF, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6,
    0xF7, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF
};

/* Apple II address field markers */
#define UFT_APPLE2_ADDR_PROLOG_1    0xD5
#define UFT_APPLE2_ADDR_PROLOG_2    0xAA
#define UFT_APPLE2_ADDR_PROLOG_3    0x96
#define UFT_APPLE2_DATA_PROLOG_1    0xD5
#define UFT_APPLE2_DATA_PROLOG_2    0xAA
#define UFT_APPLE2_DATA_PROLOG_3    0xAD
#define UFT_APPLE2_EPILOG_1         0xDE
#define UFT_APPLE2_EPILOG_2         0xAA
#define UFT_APPLE2_EPILOG_3         0xEB

/* ═══════════════════════════════════════════════════════════════════════════
 * API Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

static inline const uft_apple_preset_t *uft_apple_get_preset(uft_apple_format_id_t id) {
    if (id >= UFT_APPLE_FORMAT_COUNT) return NULL;
    return &UFT_APPLE_PRESETS[id];
}

static inline uint32_t uft_apple_disk_size(const uft_apple_preset_t *preset) {
    if (!preset) return 0;
    if (preset->secs == 0) {
        /* Variable sectors - calculate based on format */
        if (preset->id == UFT_APPLE2_NIB) return 35 * UFT_APPLE2_NIB_TRACK_SIZE;
        if (preset->id == UFT_APPLE_35_SS_400K) return 400 * 1024;
        if (preset->id == UFT_APPLE_35_DS_800K) return 800 * 1024;
        return 0;
    }
    return (uint32_t)preset->cyls * preset->heads * preset->secs * preset->bps;
}

/* DOS 3.3 vs ProDOS sector interleave */
static const uint8_t UFT_APPLE2_DOS33_INTERLEAVE[16] = {
    0, 7, 14, 6, 13, 5, 12, 4, 11, 3, 10, 2, 9, 1, 8, 15
};

static const uint8_t UFT_APPLE2_PRODOS_INTERLEAVE[16] = {
    0, 8, 1, 9, 2, 10, 3, 11, 4, 12, 5, 13, 6, 14, 7, 15
};

#ifdef __cplusplus
}
#endif
#endif /* UFT_PRESET_APPLE_H */
