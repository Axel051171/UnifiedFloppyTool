/*
 * uft_preset_atari_st.h - Atari ST Floppy Format Presets
 *
 * Part of UnifiedFloppyTool (UFT) v3.3.0
 * Based on MAME st_dsk.cpp (BSD-3-Clause, Olivier Galibert)
 *
 * Atari ST used standard PC-compatible 3.5" drives but with
 * different sector counts (9, 10, or 11 sectors per track).
 */

#ifndef UFT_PRESET_ATARI_ST_H
#define UFT_PRESET_ATARI_ST_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * Format IDs
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef enum uft_atari_st_format_id {
    /* Standard ST formats */
    UFT_ATARI_ST_SS_360K = 0,   /* 360K single sided 9 sectors */
    UFT_ATARI_ST_DS_720K,       /* 720K double sided 9 sectors */
    
    /* Extended formats (10 sectors) */
    UFT_ATARI_ST_SS_400K,       /* 400K single sided 10 sectors */
    UFT_ATARI_ST_DS_800K,       /* 800K double sided 10 sectors */
    
    /* High density formats (11 sectors) */
    UFT_ATARI_ST_SS_440K,       /* 440K single sided 11 sectors */
    UFT_ATARI_ST_DS_880K,       /* 880K double sided 11 sectors */
    
    /* Extended track counts */
    UFT_ATARI_ST_DS_720K_82T,   /* 720K 82 tracks */
    UFT_ATARI_ST_DS_800K_82T,   /* 800K 82 tracks */
    
    /* MSA compressed format */
    UFT_ATARI_ST_MSA,           /* MSA compressed image */
    
    /* STX (Pasti) format */
    UFT_ATARI_ST_STX,           /* STX copy-protected format */
    
    UFT_ATARI_ST_FORMAT_COUNT
} uft_atari_st_format_id_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Preset Structure
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct uft_atari_st_preset {
    uft_atari_st_format_id_t id;
    const char *name;
    const char *description;
    
    /* Geometry */
    uint8_t  cyls;
    uint8_t  heads;
    uint8_t  secs;
    uint16_t bps;               /* Bytes per sector (always 512) */
    
    /* Timing */
    uint16_t cell_size;         /* in ns (2000 for DD) */
    uint8_t  encoding;          /* 1=MFM */
    
    /* Image type */
    uint8_t  compressed;        /* 1 = MSA compressed */
    uint8_t  raw_format;        /* 1 = STX/Pasti */
    
    /* Extensions */
    const char *extensions;
} uft_atari_st_preset_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Preset Table
 * ═══════════════════════════════════════════════════════════════════════════ */

static const uft_atari_st_preset_t UFT_ATARI_ST_PRESETS[] = {
    /* Standard 9 sector formats */
    {
        .id = UFT_ATARI_ST_SS_360K,
        .name = "Atari ST SS 360K",
        .description = "360K Atari ST single sided 9 sectors",
        .cyls = 80, .heads = 1, .secs = 9, .bps = 512,
        .cell_size = 2000, .encoding = 1,
        .compressed = 0, .raw_format = 0,
        .extensions = ".st"
    },
    {
        .id = UFT_ATARI_ST_DS_720K,
        .name = "Atari ST DS 720K",
        .description = "720K Atari ST double sided 9 sectors",
        .cyls = 80, .heads = 2, .secs = 9, .bps = 512,
        .cell_size = 2000, .encoding = 1,
        .compressed = 0, .raw_format = 0,
        .extensions = ".st"
    },
    
    /* Extended 10 sector formats */
    {
        .id = UFT_ATARI_ST_SS_400K,
        .name = "Atari ST SS 400K",
        .description = "400K Atari ST single sided 10 sectors",
        .cyls = 80, .heads = 1, .secs = 10, .bps = 512,
        .cell_size = 2000, .encoding = 1,
        .compressed = 0, .raw_format = 0,
        .extensions = ".st"
    },
    {
        .id = UFT_ATARI_ST_DS_800K,
        .name = "Atari ST DS 800K",
        .description = "800K Atari ST double sided 10 sectors",
        .cyls = 80, .heads = 2, .secs = 10, .bps = 512,
        .cell_size = 2000, .encoding = 1,
        .compressed = 0, .raw_format = 0,
        .extensions = ".st"
    },
    
    /* High density 11 sector formats */
    {
        .id = UFT_ATARI_ST_SS_440K,
        .name = "Atari ST SS 440K",
        .description = "440K Atari ST single sided 11 sectors",
        .cyls = 80, .heads = 1, .secs = 11, .bps = 512,
        .cell_size = 2000, .encoding = 1,
        .compressed = 0, .raw_format = 0,
        .extensions = ".st"
    },
    {
        .id = UFT_ATARI_ST_DS_880K,
        .name = "Atari ST DS 880K",
        .description = "880K Atari ST double sided 11 sectors",
        .cyls = 80, .heads = 2, .secs = 11, .bps = 512,
        .cell_size = 2000, .encoding = 1,
        .compressed = 0, .raw_format = 0,
        .extensions = ".st"
    },
    
    /* Extended track counts (82 tracks) */
    {
        .id = UFT_ATARI_ST_DS_720K_82T,
        .name = "Atari ST DS 720K 82T",
        .description = "738K Atari ST 82 tracks 9 sectors",
        .cyls = 82, .heads = 2, .secs = 9, .bps = 512,
        .cell_size = 2000, .encoding = 1,
        .compressed = 0, .raw_format = 0,
        .extensions = ".st"
    },
    {
        .id = UFT_ATARI_ST_DS_800K_82T,
        .name = "Atari ST DS 800K 82T",
        .description = "820K Atari ST 82 tracks 10 sectors",
        .cyls = 82, .heads = 2, .secs = 10, .bps = 512,
        .cell_size = 2000, .encoding = 1,
        .compressed = 0, .raw_format = 0,
        .extensions = ".st"
    },
    
    /* MSA compressed format */
    {
        .id = UFT_ATARI_ST_MSA,
        .name = "Atari ST MSA",
        .description = "MSA compressed Atari ST image",
        .cyls = 80, .heads = 2, .secs = 9, .bps = 512,
        .cell_size = 2000, .encoding = 1,
        .compressed = 1, .raw_format = 0,
        .extensions = ".msa"
    },
    
    /* STX (Pasti) format */
    {
        .id = UFT_ATARI_ST_STX,
        .name = "Atari ST STX",
        .description = "STX/Pasti copy-protected format",
        .cyls = 80, .heads = 2, .secs = 0, .bps = 0,
        .cell_size = 2000, .encoding = 1,
        .compressed = 0, .raw_format = 1,
        .extensions = ".stx"
    },
};

/* ═══════════════════════════════════════════════════════════════════════════
 * MSA Header Structure
 * ═══════════════════════════════════════════════════════════════════════════ */

#define UFT_MSA_SIGNATURE       0x0E0F

typedef struct uft_msa_header {
    uint16_t signature;     /* 0x0E0F */
    uint16_t sectors;       /* Sectors per track (9-11) */
    uint16_t heads;         /* Number of sides (0 or 1) */
    uint16_t start_track;   /* Starting track */
    uint16_t end_track;     /* Ending track */
} uft_msa_header_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * API Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

static inline const uft_atari_st_preset_t *uft_atari_st_get_preset(uft_atari_st_format_id_t id) {
    if (id >= UFT_ATARI_ST_FORMAT_COUNT) return NULL;
    return &UFT_ATARI_ST_PRESETS[id];
}

static inline uint32_t uft_atari_st_disk_size(const uft_atari_st_preset_t *preset) {
    if (!preset || preset->secs == 0) return 0;
    return (uint32_t)preset->cyls * preset->heads * preset->secs * preset->bps;
}

/* Auto-detect ST format from file size */
static inline uft_atari_st_format_id_t uft_atari_st_detect_from_size(uint64_t size) {
    /* Check all standard sizes */
    for (int i = 0; i < UFT_ATARI_ST_FORMAT_COUNT; i++) {
        const uft_atari_st_preset_t *p = &UFT_ATARI_ST_PRESETS[i];
        if (p->secs == 0 || p->compressed) continue;
        uint32_t expected = (uint32_t)p->cyls * p->heads * p->secs * p->bps;
        if (size == expected) return p->id;
    }
    return UFT_ATARI_ST_FORMAT_COUNT; /* Not found */
}

/* Validate MSA header */
static inline int uft_msa_validate_header(const uft_msa_header_t *hdr) {
    if (!hdr) return 0;
    if (hdr->signature != UFT_MSA_SIGNATURE) return 0;
    if (hdr->sectors < 9 || hdr->sectors > 11) return 0;
    if (hdr->heads > 1) return 0;
    if (hdr->start_track > hdr->end_track) return 0;
    if (hdr->end_track >= 82) return 0;
    return 1;
}

#ifdef __cplusplus
}
#endif
#endif /* UFT_PRESET_ATARI_ST_H */
