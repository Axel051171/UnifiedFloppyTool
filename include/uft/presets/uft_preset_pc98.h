/*
 * uft_preset_pc98.h - NEC PC-98 Floppy Format Presets
 *
 * Part of UnifiedFloppyTool (UFT) v3.3.0
 * Based on MAME pc98_dsk.cpp (BSD-3-Clause, Angelo Salese)
 *
 * NEC PC-98 was the dominant Japanese personal computer platform from 1982-2000.
 * These formats support various disk sizes used by PC-98 systems.
 */

#ifndef UFT_PRESET_PC98_H
#define UFT_PRESET_PC98_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * Format IDs
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef enum uft_pc98_format_id {
    UFT_PC98_5_160K_SSDD = 0,   /* 160K 5.25" SS/DD */
    UFT_PC98_5_320K_DSDD,       /* 320K 5.25" DS/DD */
    UFT_PC98_5_180K_SSDD,       /* 180K 5.25" SS/DD 9sec */
    UFT_PC98_5_360K_DSDD,       /* 360K 5.25" DS/DD 9sec */
    UFT_PC98_5_400K_DSDD,       /* 400K 5.25" DS/DD 10sec */
    UFT_PC98_5_720K_DSQD,       /* 720K 5.25" DS/QD */
    UFT_PC98_5_1200K_DSHD,      /* 1.2M 5.25" DS/HD (standard) */
    UFT_PC98_3_720K_DSDD,       /* 720K 3.5" DS/DD */
    UFT_PC98_3_1200K_DSHD,      /* 1.2M 3.5" DS/HD (Japanese) */
    UFT_PC98_3_1440K_DSHD,      /* 1.44M 3.5" DS/HD */
    UFT_PC98_3_2880K_DSED,      /* 2.88M 3.5" DS/ED */
    UFT_PC98_5_1232K_DSHD,      /* 1232K 5.25" 8sec 1024bps */
    UFT_PC98_5_1024K_DSHD,      /* 1M 5.25" N88 BASIC 26sec 256bps */
    UFT_PC98_FORMAT_COUNT
} uft_pc98_format_id_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Preset Structure
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct uft_pc98_preset {
    uft_pc98_format_id_t id;
    const char *name;
    const char *description;
    
    /* Geometry */
    uint8_t  form_factor;   /* 5 = 5.25", 3 = 3.5" */
    uint8_t  cyls;
    uint8_t  heads;
    uint8_t  secs;
    uint16_t bps;           /* Bytes per sector */
    
    /* Timing */
    uint16_t cell_size;     /* in ns (1000=HD, 2000=DD) */
    uint8_t  encoding;      /* 1=MFM */
    
    /* Extensions */
    const char *extensions;
} uft_pc98_preset_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Preset Table
 * ═══════════════════════════════════════════════════════════════════════════ */

static const uft_pc98_preset_t UFT_PC98_PRESETS[] = {
    {
        .id = UFT_PC98_5_160K_SSDD,
        .name = "PC-98 5.25\" 160K",
        .description = "160K 5.25\" single sided double density",
        .form_factor = 5, .cyls = 40, .heads = 1, .secs = 8, .bps = 512,
        .cell_size = 2000, .encoding = 1,
        .extensions = ".dsk;.ima;.img"
    },
    {
        .id = UFT_PC98_5_320K_DSDD,
        .name = "PC-98 5.25\" 320K",
        .description = "320K 5.25\" double sided double density",
        .form_factor = 5, .cyls = 40, .heads = 2, .secs = 8, .bps = 512,
        .cell_size = 2000, .encoding = 1,
        .extensions = ".dsk;.ima;.img"
    },
    {
        .id = UFT_PC98_5_180K_SSDD,
        .name = "PC-98 5.25\" 180K",
        .description = "180K 5.25\" single sided 9 sectors",
        .form_factor = 5, .cyls = 40, .heads = 1, .secs = 9, .bps = 512,
        .cell_size = 2000, .encoding = 1,
        .extensions = ".dsk;.ima;.img"
    },
    {
        .id = UFT_PC98_5_360K_DSDD,
        .name = "PC-98 5.25\" 360K",
        .description = "360K 5.25\" double sided 9 sectors",
        .form_factor = 5, .cyls = 40, .heads = 2, .secs = 9, .bps = 512,
        .cell_size = 2000, .encoding = 1,
        .extensions = ".dsk;.ima;.img;.360"
    },
    {
        .id = UFT_PC98_5_400K_DSDD,
        .name = "PC-98 5.25\" 400K",
        .description = "400K 5.25\" double sided 10 sectors",
        .form_factor = 5, .cyls = 40, .heads = 2, .secs = 10, .bps = 512,
        .cell_size = 2000, .encoding = 1,
        .extensions = ".dsk;.ima;.img"
    },
    {
        .id = UFT_PC98_5_720K_DSQD,
        .name = "PC-98 5.25\" 720K",
        .description = "720K 5.25\" quad density",
        .form_factor = 5, .cyls = 80, .heads = 2, .secs = 9, .bps = 512,
        .cell_size = 2000, .encoding = 1,
        .extensions = ".dsk;.ima;.img"
    },
    {
        .id = UFT_PC98_5_1200K_DSHD,
        .name = "PC-98 5.25\" 1.2M",
        .description = "1.2MB 5.25\" high density (standard PC-98)",
        .form_factor = 5, .cyls = 80, .heads = 2, .secs = 15, .bps = 512,
        .cell_size = 1200, .encoding = 1,
        .extensions = ".dsk;.ima;.img;.hdm"
    },
    {
        .id = UFT_PC98_3_720K_DSDD,
        .name = "PC-98 3.5\" 720K",
        .description = "720K 3.5\" double density",
        .form_factor = 3, .cyls = 80, .heads = 2, .secs = 9, .bps = 512,
        .cell_size = 2000, .encoding = 1,
        .extensions = ".dsk;.ima;.img"
    },
    {
        .id = UFT_PC98_3_1200K_DSHD,
        .name = "PC-98 3.5\" 1.2M",
        .description = "1.2MB 3.5\" high density (Japanese variant)",
        .form_factor = 3, .cyls = 80, .heads = 2, .secs = 15, .bps = 512,
        .cell_size = 1200, .encoding = 1,
        .extensions = ".dsk;.ima;.img;.hdm"
    },
    {
        .id = UFT_PC98_3_1440K_DSHD,
        .name = "PC-98 3.5\" 1.44M",
        .description = "1.44MB 3.5\" high density",
        .form_factor = 3, .cyls = 80, .heads = 2, .secs = 18, .bps = 512,
        .cell_size = 1000, .encoding = 1,
        .extensions = ".dsk;.ima;.img"
    },
    {
        .id = UFT_PC98_3_2880K_DSED,
        .name = "PC-98 3.5\" 2.88M",
        .description = "2.88MB 3.5\" extended density",
        .form_factor = 3, .cyls = 80, .heads = 2, .secs = 36, .bps = 512,
        .cell_size = 500, .encoding = 1,
        .extensions = ".dsk;.ima;.img"
    },
    {
        .id = UFT_PC98_5_1232K_DSHD,
        .name = "PC-98 5.25\" 1232K",
        .description = "1232K 5.25\" HD 8 sectors 1024 bytes",
        .form_factor = 5, .cyls = 77, .heads = 2, .secs = 8, .bps = 1024,
        .cell_size = 1200, .encoding = 1,
        .extensions = ".dsk;.ima;.img;.hdm"
    },
    {
        .id = UFT_PC98_5_1024K_DSHD,
        .name = "PC-98 N88 BASIC 1M",
        .description = "1MB 5.25\" N88 BASIC 26 sectors 256 bytes",
        .form_factor = 5, .cyls = 77, .heads = 2, .secs = 26, .bps = 256,
        .cell_size = 1200, .encoding = 1,
        .extensions = ".dsk;.ima;.img"
    },
};

/* ═══════════════════════════════════════════════════════════════════════════
 * API Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

static inline const uft_pc98_preset_t *uft_pc98_get_preset(uft_pc98_format_id_t id) {
    if (id >= UFT_PC98_FORMAT_COUNT) return NULL;
    return &UFT_PC98_PRESETS[id];
}

static inline uint32_t uft_pc98_disk_size(const uft_pc98_preset_t *preset) {
    if (!preset) return 0;
    return (uint32_t)preset->cyls * preset->heads * preset->secs * preset->bps;
}

#ifdef __cplusplus
}
#endif
#endif /* UFT_PRESET_PC98_H */
