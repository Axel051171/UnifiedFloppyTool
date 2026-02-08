/*
 * uft_preset_msx.h - MSX Computer Floppy Format Presets
 *
 * Part of UnifiedFloppyTool (UFT) v3.3.0
 * Based on MAME msx_dsk.cpp (BSD-3-Clause, Olivier Galibert)
 *
 * MSX was a standardized home computer architecture from 1983.
 * Popular in Japan, Korea, Europe, and South America.
 */

#ifndef UFT_PRESET_MSX_H
#define UFT_PRESET_MSX_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * Format IDs
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef enum uft_msx_format_id {
    UFT_MSX_5_180K_SSDD = 0,    /* 180K 5.25" SS/DD */
    UFT_MSX_5_360K_DSDD,        /* 360K 5.25" DS/DD */
    UFT_MSX_3_360K_SSDD,        /* 360K 3.5" SS/DD */
    UFT_MSX_3_720K_DSDD,        /* 720K 3.5" DS/DD (standard) */
    UFT_MSX_3_737K_DSDD,        /* 737K 3.5" DS/DD 81 tracks */
    UFT_MSX_FORMAT_COUNT
} uft_msx_format_id_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Preset Structure
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct uft_msx_preset {
    uft_msx_format_id_t id;
    const char *name;
    const char *description;
    
    /* Geometry */
    uint8_t  form_factor;   /* 5 = 5.25", 3 = 3.5" */
    uint8_t  cyls;
    uint8_t  heads;
    uint8_t  secs;
    uint16_t bps;           /* Bytes per sector */
    
    /* Timing */
    uint16_t cell_size;     /* in ns */
    uint8_t  encoding;      /* 1=MFM */
    
    /* Gap sizes (from MAME) */
    uint8_t  gap1;          /* Post-index gap */
    uint8_t  gap2;          /* Post-ID gap */
    uint8_t  gap3;          /* Post-data gap */
    
    /* Extensions */
    const char *extensions;
} uft_msx_preset_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Preset Table
 * ═══════════════════════════════════════════════════════════════════════════ */

static const uft_msx_preset_t UFT_MSX_PRESETS[] = {
    {
        .id = UFT_MSX_5_180K_SSDD,
        .name = "MSX 5.25\" 180K",
        .description = "180K 5.25\" single sided double density",
        .form_factor = 5, .cyls = 40, .heads = 1, .secs = 9, .bps = 512,
        .cell_size = 2000, .encoding = 1,
        .gap1 = 80, .gap2 = 50, .gap3 = 80,
        .extensions = ".dsk"
    },
    {
        .id = UFT_MSX_5_360K_DSDD,
        .name = "MSX 5.25\" 360K",
        .description = "360K 5.25\" double sided double density",
        .form_factor = 5, .cyls = 40, .heads = 2, .secs = 9, .bps = 512,
        .cell_size = 2000, .encoding = 1,
        .gap1 = 80, .gap2 = 50, .gap3 = 80,
        .extensions = ".dsk"
    },
    {
        .id = UFT_MSX_3_360K_SSDD,
        .name = "MSX 3.5\" 360K",
        .description = "360K 3.5\" single sided double density",
        .form_factor = 3, .cyls = 80, .heads = 1, .secs = 9, .bps = 512,
        .cell_size = 2000, .encoding = 1,
        .gap1 = 26, .gap2 = 24, .gap3 = 80,
        .extensions = ".dsk"
    },
    {
        .id = UFT_MSX_3_720K_DSDD,
        .name = "MSX 3.5\" 720K",
        .description = "720K 3.5\" double sided double density (standard)",
        .form_factor = 3, .cyls = 80, .heads = 2, .secs = 9, .bps = 512,
        .cell_size = 2000, .encoding = 1,
        .gap1 = 26, .gap2 = 24, .gap3 = 80,
        .extensions = ".dsk"
    },
    {
        .id = UFT_MSX_3_737K_DSDD,
        .name = "MSX 3.5\" 737K",
        .description = "737K 3.5\" double sided 81 tracks",
        .form_factor = 3, .cyls = 81, .heads = 2, .secs = 9, .bps = 512,
        .cell_size = 2000, .encoding = 1,
        .gap1 = 26, .gap2 = 24, .gap3 = 80,
        .extensions = ".dsk"
    },
};

/* ═══════════════════════════════════════════════════════════════════════════
 * API Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

static inline const uft_msx_preset_t *uft_msx_get_preset(uft_msx_format_id_t id) {
    if (id >= UFT_MSX_FORMAT_COUNT) return NULL;
    return &UFT_MSX_PRESETS[id];
}

static inline uint32_t uft_msx_disk_size(const uft_msx_preset_t *preset) {
    if (!preset) return 0;
    return (uint32_t)preset->cyls * preset->heads * preset->secs * preset->bps;
}

/* Boot sector detection for MSX-DOS */
static inline int uft_msx_detect_format(const uint8_t *boot_sector) {
    if (!boot_sector) return -1;
    
    /* MSX-DOS boot sector signature */
    /* Byte 0: 0xEB or 0xE9 (jump instruction) */
    if (boot_sector[0] != 0xEB && boot_sector[0] != 0xE9) return -1;
    
    /* Check media descriptor (byte 21) */
    uint8_t media = boot_sector[21];
    switch (media) {
        case 0xF8: return UFT_MSX_3_720K_DSDD;  /* Fixed disk / 720K */
        case 0xF9: return UFT_MSX_3_720K_DSDD;  /* 720K */
        case 0xFC: return UFT_MSX_5_180K_SSDD;  /* 180K */
        case 0xFD: return UFT_MSX_5_360K_DSDD;  /* 360K */
        case 0xFE: return UFT_MSX_5_180K_SSDD;  /* 180K SS */
        case 0xFF: return UFT_MSX_5_360K_DSDD;  /* 360K DS */
        default: return -1;
    }
}

#ifdef __cplusplus
}
#endif
#endif /* UFT_PRESET_MSX_H */
