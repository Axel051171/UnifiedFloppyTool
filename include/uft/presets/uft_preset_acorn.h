/*
 * uft_preset_acorn.h - Acorn/BBC Micro Floppy Format Presets
 *
 * Part of UnifiedFloppyTool (UFT) v3.3.0
 * Based on MAME acorn_dsk.cpp (BSD-3-Clause)
 *
 * Covers BBC Micro, Acorn Electron, Archimedes, and related systems.
 * Supports DFS (Disc Filing System), ADFS, Opus DDOS, Cumana DFS.
 */

#ifndef UFT_PRESET_ACORN_H
#define UFT_PRESET_ACORN_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * Format IDs
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef enum uft_acorn_format_id {
    /* DFS - Disc Filing System (BBC Micro) */
    UFT_ACORN_DFS_SS40 = 0,     /* 100K SS 40T */
    UFT_ACORN_DFS_SS80,         /* 200K SS 80T */
    UFT_ACORN_DFS_DS40,         /* 200K DS 40T */
    UFT_ACORN_DFS_DS80,         /* 400K DS 80T */
    
    /* DSD - Double Sided Interleaved */
    UFT_ACORN_DSD_DS80,         /* 400K DS 80T interleaved */
    UFT_ACORN_DSD_DS40,         /* 200K DS 40T interleaved */
    
    /* Opus DDOS */
    UFT_ACORN_OPUS_SS40,        /* 180K SS 40T */
    UFT_ACORN_OPUS_SS80,        /* 360K SS 80T */
    UFT_ACORN_OPUS_DS40,        /* 360K DS 40T */
    UFT_ACORN_OPUS_DS80,        /* 720K DS 80T */
    
    /* ADFS - Advanced Disc Filing System */
    UFT_ACORN_ADFS_S,           /* 160K (S format) */
    UFT_ACORN_ADFS_M,           /* 320K (M format) */
    UFT_ACORN_ADFS_L,           /* 640K (L format) interleaved */
    UFT_ACORN_ADFS_D,           /* 800K (D/E format) */
    UFT_ACORN_ADFS_F,           /* 1600K (F format) HD */
    
    /* Acorn DOS (PC compatible) */
    UFT_ACORN_DOS_360K,         /* 360K PC compatible */
    UFT_ACORN_DOS_720K,         /* 720K PC compatible */
    UFT_ACORN_DOS_1440K,        /* 1.44M PC compatible */
    
    /* Cumana DFS */
    UFT_ACORN_CUMANA_SS40,      /* Cumana SS 40T */
    UFT_ACORN_CUMANA_DS80,      /* Cumana DS 80T */
    
    UFT_ACORN_FORMAT_COUNT
} uft_acorn_format_id_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Preset Structure
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct uft_acorn_preset {
    uft_acorn_format_id_t id;
    const char *name;
    const char *description;
    const char *fs_type;        /* DFS, ADFS, DDOS, DOS */
    
    /* Geometry */
    uint8_t  form_factor;       /* 5 = 5.25", 3 = 3.5" */
    uint8_t  cyls;
    uint8_t  heads;
    uint8_t  secs;
    uint16_t bps;               /* Bytes per sector */
    
    /* Timing */
    uint16_t cell_size;         /* in ns */
    uint8_t  encoding;          /* 0=FM, 1=MFM */
    
    /* Interleave */
    uint8_t  interleaved;       /* 1 = sides interleaved in file */
    
    /* Extensions */
    const char *extensions;
} uft_acorn_preset_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Preset Table
 * ═══════════════════════════════════════════════════════════════════════════ */

static const uft_acorn_preset_t UFT_ACORN_PRESETS[] = {
    /* DFS - Single Density FM */
    {
        .id = UFT_ACORN_DFS_SS40,
        .name = "BBC DFS SS 40T",
        .description = "100K BBC DFS single sided 40 track",
        .fs_type = "DFS",
        .form_factor = 5, .cyls = 40, .heads = 1, .secs = 10, .bps = 256,
        .cell_size = 4000, .encoding = 0,
        .interleaved = 0,
        .extensions = ".ssd"
    },
    {
        .id = UFT_ACORN_DFS_SS80,
        .name = "BBC DFS SS 80T",
        .description = "200K BBC DFS single sided 80 track",
        .fs_type = "DFS",
        .form_factor = 5, .cyls = 80, .heads = 1, .secs = 10, .bps = 256,
        .cell_size = 4000, .encoding = 0,
        .interleaved = 0,
        .extensions = ".ssd"
    },
    {
        .id = UFT_ACORN_DFS_DS40,
        .name = "BBC DFS DS 40T",
        .description = "200K BBC DFS double sided 40 track",
        .fs_type = "DFS",
        .form_factor = 5, .cyls = 40, .heads = 2, .secs = 10, .bps = 256,
        .cell_size = 4000, .encoding = 0,
        .interleaved = 0,
        .extensions = ".ssd;.dsd"
    },
    {
        .id = UFT_ACORN_DFS_DS80,
        .name = "BBC DFS DS 80T",
        .description = "400K BBC DFS double sided 80 track",
        .fs_type = "DFS",
        .form_factor = 5, .cyls = 80, .heads = 2, .secs = 10, .bps = 256,
        .cell_size = 4000, .encoding = 0,
        .interleaved = 0,
        .extensions = ".ssd;.dsd"
    },
    
    /* DSD - Interleaved */
    {
        .id = UFT_ACORN_DSD_DS80,
        .name = "BBC DSD DS 80T",
        .description = "400K BBC DSD interleaved 80 track",
        .fs_type = "DFS",
        .form_factor = 5, .cyls = 80, .heads = 2, .secs = 10, .bps = 256,
        .cell_size = 4000, .encoding = 0,
        .interleaved = 1,
        .extensions = ".dsd"
    },
    {
        .id = UFT_ACORN_DSD_DS40,
        .name = "BBC DSD DS 40T",
        .description = "200K BBC DSD interleaved 40 track",
        .fs_type = "DFS",
        .form_factor = 5, .cyls = 40, .heads = 2, .secs = 10, .bps = 256,
        .cell_size = 4000, .encoding = 0,
        .interleaved = 1,
        .extensions = ".dsd"
    },
    
    /* Opus DDOS - MFM Double Density */
    {
        .id = UFT_ACORN_OPUS_SS40,
        .name = "Opus DDOS SS 40T",
        .description = "180K Opus DDOS single sided 40 track",
        .fs_type = "DDOS",
        .form_factor = 5, .cyls = 40, .heads = 1, .secs = 18, .bps = 256,
        .cell_size = 2000, .encoding = 1,
        .interleaved = 0,
        .extensions = ".img"
    },
    {
        .id = UFT_ACORN_OPUS_SS80,
        .name = "Opus DDOS SS 80T",
        .description = "360K Opus DDOS single sided 80 track",
        .fs_type = "DDOS",
        .form_factor = 5, .cyls = 80, .heads = 1, .secs = 18, .bps = 256,
        .cell_size = 2000, .encoding = 1,
        .interleaved = 0,
        .extensions = ".img"
    },
    {
        .id = UFT_ACORN_OPUS_DS40,
        .name = "Opus DDOS DS 40T",
        .description = "360K Opus DDOS double sided 40 track",
        .fs_type = "DDOS",
        .form_factor = 5, .cyls = 40, .heads = 2, .secs = 18, .bps = 256,
        .cell_size = 2000, .encoding = 1,
        .interleaved = 0,
        .extensions = ".img"
    },
    {
        .id = UFT_ACORN_OPUS_DS80,
        .name = "Opus DDOS DS 80T",
        .description = "720K Opus DDOS double sided 80 track",
        .fs_type = "DDOS",
        .form_factor = 5, .cyls = 80, .heads = 2, .secs = 18, .bps = 256,
        .cell_size = 2000, .encoding = 1,
        .interleaved = 0,
        .extensions = ".img"
    },
    
    /* ADFS */
    {
        .id = UFT_ACORN_ADFS_S,
        .name = "ADFS S",
        .description = "160K ADFS S format",
        .fs_type = "ADFS",
        .form_factor = 5, .cyls = 40, .heads = 1, .secs = 16, .bps = 256,
        .cell_size = 2000, .encoding = 1,
        .interleaved = 0,
        .extensions = ".adf;.adl"
    },
    {
        .id = UFT_ACORN_ADFS_M,
        .name = "ADFS M",
        .description = "320K ADFS M format",
        .fs_type = "ADFS",
        .form_factor = 5, .cyls = 80, .heads = 1, .secs = 16, .bps = 256,
        .cell_size = 2000, .encoding = 1,
        .interleaved = 0,
        .extensions = ".adf;.adl"
    },
    {
        .id = UFT_ACORN_ADFS_L,
        .name = "ADFS L",
        .description = "640K ADFS L format (interleaved)",
        .fs_type = "ADFS",
        .form_factor = 5, .cyls = 80, .heads = 2, .secs = 16, .bps = 256,
        .cell_size = 2000, .encoding = 1,
        .interleaved = 1,
        .extensions = ".adf;.adl"
    },
    {
        .id = UFT_ACORN_ADFS_D,
        .name = "ADFS D/E",
        .description = "800K ADFS D/E format (Archimedes)",
        .fs_type = "ADFS",
        .form_factor = 3, .cyls = 80, .heads = 2, .secs = 5, .bps = 1024,
        .cell_size = 2000, .encoding = 1,
        .interleaved = 1,
        .extensions = ".adf;.adl"
    },
    {
        .id = UFT_ACORN_ADFS_F,
        .name = "ADFS F",
        .description = "1600K ADFS F format HD",
        .fs_type = "ADFS",
        .form_factor = 3, .cyls = 80, .heads = 2, .secs = 10, .bps = 1024,
        .cell_size = 1000, .encoding = 1,
        .interleaved = 1,
        .extensions = ".adf;.adl"
    },
    
    /* Acorn DOS (PC compatible) */
    {
        .id = UFT_ACORN_DOS_360K,
        .name = "Acorn DOS 360K",
        .description = "360K PC compatible",
        .fs_type = "DOS",
        .form_factor = 5, .cyls = 40, .heads = 2, .secs = 9, .bps = 512,
        .cell_size = 2000, .encoding = 1,
        .interleaved = 0,
        .extensions = ".img"
    },
    {
        .id = UFT_ACORN_DOS_720K,
        .name = "Acorn DOS 720K",
        .description = "720K PC compatible",
        .fs_type = "DOS",
        .form_factor = 3, .cyls = 80, .heads = 2, .secs = 9, .bps = 512,
        .cell_size = 2000, .encoding = 1,
        .interleaved = 0,
        .extensions = ".img"
    },
    {
        .id = UFT_ACORN_DOS_1440K,
        .name = "Acorn DOS 1.44M",
        .description = "1.44M PC compatible HD",
        .fs_type = "DOS",
        .form_factor = 3, .cyls = 80, .heads = 2, .secs = 18, .bps = 512,
        .cell_size = 1000, .encoding = 1,
        .interleaved = 0,
        .extensions = ".img"
    },
    
    /* Cumana DFS */
    {
        .id = UFT_ACORN_CUMANA_SS40,
        .name = "Cumana DFS SS 40T",
        .description = "100K Cumana DFS single sided",
        .fs_type = "DFS",
        .form_factor = 5, .cyls = 40, .heads = 1, .secs = 10, .bps = 256,
        .cell_size = 4000, .encoding = 0,
        .interleaved = 0,
        .extensions = ".ssd"
    },
    {
        .id = UFT_ACORN_CUMANA_DS80,
        .name = "Cumana DFS DS 80T",
        .description = "400K Cumana DFS double sided",
        .fs_type = "DFS",
        .form_factor = 5, .cyls = 80, .heads = 2, .secs = 10, .bps = 256,
        .cell_size = 4000, .encoding = 0,
        .interleaved = 0,
        .extensions = ".dsd"
    },
};

/* ═══════════════════════════════════════════════════════════════════════════
 * API Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

static inline const uft_acorn_preset_t *uft_acorn_get_preset(uft_acorn_format_id_t id) {
    if (id >= UFT_ACORN_FORMAT_COUNT) return NULL;
    return &UFT_ACORN_PRESETS[id];
}

static inline uint32_t uft_acorn_disk_size(const uft_acorn_preset_t *preset) {
    if (!preset) return 0;
    return (uint32_t)preset->cyls * preset->heads * preset->secs * preset->bps;
}

/* DFS catalogue detection */
static inline int uft_acorn_detect_dfs(const uint8_t *sector0, const uint8_t *sector1) {
    if (!sector0 || !sector1) return 0;
    
    /* DFS: Sector 1 byte 6-7 contains sector count (big-endian, 10 bits) */
    uint16_t sectors = ((sector1[6] & 0x03) << 8) | sector1[7];
    
    /* Valid sector counts: 400 (SS40), 800 (SS80/DS40), 1600 (DS80) */
    if (sectors == 400 || sectors == 800 || sectors == 1600) {
        return 1;
    }
    return 0;
}

#ifdef __cplusplus
}
#endif
#endif /* UFT_PRESET_ACORN_H */
