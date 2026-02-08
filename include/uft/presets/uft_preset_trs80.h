/*
 * uft_preset_trs80.h - TRS-80 Floppy Format Presets
 *
 * Part of UnifiedFloppyTool (UFT) v3.3.0
 * Based on MAME trs80_dsk.cpp (BSD-3-Clause, Dirk Best)
 *
 * TRS-80 was Radio Shack's line of 8-bit microcomputers (1977-1991).
 * Supports JV1 and JV3 disk image formats.
 */

#ifndef UFT_PRESET_TRS80_H
#define UFT_PRESET_TRS80_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * Format IDs
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef enum uft_trs80_format_id {
    /* JV1 formats (simple sector dump) */
    UFT_TRS80_JV1_35T = 0,      /* JV1 35 tracks */
    UFT_TRS80_JV1_40T,          /* JV1 40 tracks */
    UFT_TRS80_JV1_80T,          /* JV1 80 tracks */
    
    /* JV3 formats (with sector headers) */
    UFT_TRS80_JV3_SSSD,         /* JV3 single sided single density */
    UFT_TRS80_JV3_SSDD,         /* JV3 single sided double density */
    UFT_TRS80_JV3_DSSD,         /* JV3 double sided single density */
    UFT_TRS80_JV3_DSDD,         /* JV3 double sided double density */
    
    /* DMK format */
    UFT_TRS80_DMK,              /* DMK raw track format */
    
    UFT_TRS80_FORMAT_COUNT
} uft_trs80_format_id_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * JV3 Flag Bits
 * ═══════════════════════════════════════════════════════════════════════════ */

#define UFT_JV3_SIZE_MASK       0x03  /* Sector size bits */
#define UFT_JV3_NON_IBM         0x04  /* Non-IBM sector size */
#define UFT_JV3_CRC_ERROR       0x08  /* Sector has CRC error */
#define UFT_JV3_SIDE_1          0x10  /* Side 1 (else side 0) */
#define UFT_JV3_DAM_MASK        0x60  /* DAM code bits */
#define UFT_JV3_DOUBLE_DENSITY  0x80  /* MFM (else FM) */

/* DAM codes for JV3 */
#define UFT_JV3_DAM_FB          0x00  /* Normal data */
#define UFT_JV3_DAM_FA_SD       0x20  /* Undefined (SD) / Invalid (DD) */
#define UFT_JV3_DAM_F9_SD       0x40  /* Undefined (SD only) */
#define UFT_JV3_DAM_F8          0x60  /* Deleted data (SD) / 0x20 (DD) */

/* ═══════════════════════════════════════════════════════════════════════════
 * Preset Structure
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct uft_trs80_preset {
    uft_trs80_format_id_t id;
    const char *name;
    const char *description;
    
    /* Geometry */
    uint8_t  cyls;
    uint8_t  heads;
    uint8_t  secs;
    uint16_t bps;           /* Bytes per sector */
    
    /* Timing */
    uint16_t cell_size;     /* in ns (4000=SD FM, 2000=DD MFM) */
    uint8_t  encoding;      /* 0=FM, 1=MFM */
    
    /* Format type */
    uint8_t  format_type;   /* 0=JV1, 1=JV3, 2=DMK */
    
    /* Extensions */
    const char *extensions;
} uft_trs80_preset_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Preset Table
 * ═══════════════════════════════════════════════════════════════════════════ */

static const uft_trs80_preset_t UFT_TRS80_PRESETS[] = {
    /* JV1 formats - Model I Level 2 BASIC */
    {
        .id = UFT_TRS80_JV1_35T,
        .name = "TRS-80 JV1 35T",
        .description = "JV1 35 tracks 10 sectors (Model I)",
        .cyls = 35, .heads = 1, .secs = 10, .bps = 256,
        .cell_size = 4000, .encoding = 0,
        .format_type = 0,
        .extensions = ".jv1;.dsk"
    },
    {
        .id = UFT_TRS80_JV1_40T,
        .name = "TRS-80 JV1 40T",
        .description = "JV1 40 tracks 10 sectors",
        .cyls = 40, .heads = 1, .secs = 10, .bps = 256,
        .cell_size = 4000, .encoding = 0,
        .format_type = 0,
        .extensions = ".jv1;.dsk"
    },
    {
        .id = UFT_TRS80_JV1_80T,
        .name = "TRS-80 JV1 80T",
        .description = "JV1 80 tracks 10 sectors",
        .cyls = 80, .heads = 1, .secs = 10, .bps = 256,
        .cell_size = 4000, .encoding = 0,
        .format_type = 0,
        .extensions = ".jv1;.dsk"
    },
    
    /* JV3 formats - Model III/4 */
    {
        .id = UFT_TRS80_JV3_SSSD,
        .name = "TRS-80 JV3 SS/SD",
        .description = "JV3 single sided single density",
        .cyls = 40, .heads = 1, .secs = 10, .bps = 256,
        .cell_size = 4000, .encoding = 0,
        .format_type = 1,
        .extensions = ".jv3;.dsk"
    },
    {
        .id = UFT_TRS80_JV3_SSDD,
        .name = "TRS-80 JV3 SS/DD",
        .description = "JV3 single sided double density",
        .cyls = 40, .heads = 1, .secs = 18, .bps = 256,
        .cell_size = 2000, .encoding = 1,
        .format_type = 1,
        .extensions = ".jv3;.dsk"
    },
    {
        .id = UFT_TRS80_JV3_DSSD,
        .name = "TRS-80 JV3 DS/SD",
        .description = "JV3 double sided single density",
        .cyls = 40, .heads = 2, .secs = 10, .bps = 256,
        .cell_size = 4000, .encoding = 0,
        .format_type = 1,
        .extensions = ".jv3;.dsk"
    },
    {
        .id = UFT_TRS80_JV3_DSDD,
        .name = "TRS-80 JV3 DS/DD",
        .description = "JV3 double sided double density",
        .cyls = 80, .heads = 2, .secs = 18, .bps = 256,
        .cell_size = 2000, .encoding = 1,
        .format_type = 1,
        .extensions = ".jv3;.dsk"
    },
    
    /* DMK format */
    {
        .id = UFT_TRS80_DMK,
        .name = "TRS-80 DMK",
        .description = "DMK raw track format",
        .cyls = 80, .heads = 2, .secs = 18, .bps = 256,
        .cell_size = 2000, .encoding = 1,
        .format_type = 2,
        .extensions = ".dmk"
    },
};

/* ═══════════════════════════════════════════════════════════════════════════
 * JV3 Header Structure
 * ═══════════════════════════════════════════════════════════════════════════ */

#define UFT_JV3_HEADER_SIZE     0x2200
#define UFT_JV3_MAX_SECTORS     2901

typedef struct uft_jv3_sector_desc {
    uint8_t track;      /* Track number (0xFF = unused) */
    uint8_t sector;     /* Sector number */
    uint8_t flags;      /* Flag byte (see UFT_JV3_* defines) */
} uft_jv3_sector_desc_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * API Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

static inline const uft_trs80_preset_t *uft_trs80_get_preset(uft_trs80_format_id_t id) {
    if (id >= UFT_TRS80_FORMAT_COUNT) return NULL;
    return &UFT_TRS80_PRESETS[id];
}

static inline uint32_t uft_trs80_disk_size(const uft_trs80_preset_t *preset) {
    if (!preset) return 0;
    return (uint32_t)preset->cyls * preset->heads * preset->secs * preset->bps;
}

/* Decode JV3 sector size from flags */
static inline uint16_t uft_jv3_sector_size(uint8_t flags, int used) {
    uint8_t size_code = flags & UFT_JV3_SIZE_MASK;
    if (used) {
        /* Used sector: XOR with 1 */
        switch (size_code ^ 1) {
            case 0: return 128;
            case 1: return 256;
            case 2: return 512;
            case 3: return 1024;
        }
    } else {
        /* Unused sector: XOR with 2 */
        switch (size_code ^ 2) {
            case 0: return 128;
            case 1: return 256;
            case 2: return 512;
            case 3: return 1024;
        }
    }
    return 256; /* Default */
}

#ifdef __cplusplus
}
#endif
#endif /* UFT_PRESET_TRS80_H */
