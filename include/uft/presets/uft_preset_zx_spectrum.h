/*
 * uft_preset_zx_spectrum.h - ZX Spectrum Floppy Format Presets
 *
 * Part of UnifiedFloppyTool (UFT) v3.3.0
 *
 * Supported formats:
 *  - ZX Spectrum +3 DOS (SSSD/DSDD)
 *  - MGT +D (DSDD)
 *  - Opus Discovery (SSSD/DSDD)
 *  - Kempston (SSSD/DSDD)
 *  - Watford SPDOS (SSSD/DSDD)
 *  - Turbodrive (DSSD/DSDD)
 *  - Didaktik D80 (DSDD)
 *  - Timex FDD3000 (SSSD/DSDD)
 *  - Beta Disk (TR-DOS)
 */

#ifndef UFT_PRESET_ZX_SPECTRUM_H
#define UFT_PRESET_ZX_SPECTRUM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * Format IDs
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef enum uft_zx_format_id {
    UFT_ZX_PLUS3DOS_SSSD = 0,
    UFT_ZX_PLUS3DOS_DSDD,
    UFT_ZX_MGT_PLUSD_DSDD,
    UFT_ZX_OPUS_SSSD,
    UFT_ZX_OPUS_DSDD,
    UFT_ZX_KEMPSTON_SSSD,
    UFT_ZX_KEMPSTON_DSDD,
    UFT_ZX_WATFORD_SSSD,
    UFT_ZX_WATFORD_DSDD,
    UFT_ZX_TURBODRIVE_DSSD,
    UFT_ZX_TURBODRIVE_DSDD,
    UFT_ZX_DIDAKTIK_D80_DSDD,
    UFT_ZX_TIMEX_FDD3000_SSSD,
    UFT_ZX_TIMEX_FDD3000_DSDD,
    UFT_ZX_BETADISK_TRDOS,
    UFT_ZX_FORMAT_COUNT
} uft_zx_format_id_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Format Preset Structure
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct uft_zx_preset {
    uft_zx_format_id_t id;
    const char *name;
    const char *description;
    const char *gw_format;      /* Greaseweazle format string */
    
    /* Geometry */
    uint8_t  cyls;
    uint8_t  heads;
    uint8_t  secs;
    uint16_t bps;               /* Bytes per sector */
    
    /* Timing */
    uint16_t rate;              /* Data rate in kbit/s (250/300/500) */
    uint8_t  encoding;          /* 0=FM, 1=MFM */
    
    /* Interleave & Skew */
    uint8_t  interleave;
    uint8_t  cskew;             /* Cylinder skew */
    uint8_t  hskew;             /* Head skew */
    
    /* Flags */
    uint8_t  iam;               /* Index Address Mark present */
    uint8_t  id_start;          /* Starting sector ID (usually 0 or 1) */
    
    /* File extensions */
    const char *extensions;     /* e.g. ".dsk;.img" */
} uft_zx_preset_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Preset Table
 * ═══════════════════════════════════════════════════════════════════════════ */

static const uft_zx_preset_t UFT_ZX_PRESETS[] = {
    /* +3 DOS */
    {
        .id = UFT_ZX_PLUS3DOS_SSSD,
        .name = "ZX Spectrum +3 DOS SS/SD",
        .description = "Amstrad +3 DOS single-sided single-density",
        .gw_format = "zx.3dos.sssd",
        .cyls = 40, .heads = 1, .secs = 9, .bps = 512,
        .rate = 250, .encoding = 1,
        .interleave = 1, .cskew = 0, .hskew = 0,
        .iam = 1, .id_start = 1,
        .extensions = ".dsk"
    },
    {
        .id = UFT_ZX_PLUS3DOS_DSDD,
        .name = "ZX Spectrum +3 DOS DS/DD",
        .description = "Amstrad +3 DOS double-sided double-density",
        .gw_format = "zx.3dos.dsdd",
        .cyls = 80, .heads = 2, .secs = 9, .bps = 512,
        .rate = 250, .encoding = 1,
        .interleave = 1, .cskew = 0, .hskew = 0,
        .iam = 1, .id_start = 1,
        .extensions = ".dsk"
    },
    
    /* MGT +D */
    {
        .id = UFT_ZX_MGT_PLUSD_DSDD,
        .name = "MGT +D",
        .description = "Miles Gordon +D interface",
        .gw_format = "zx.plusd.ds80",
        .cyls = 80, .heads = 2, .secs = 10, .bps = 512,
        .rate = 250, .encoding = 1,
        .interleave = 1, .cskew = 0, .hskew = 0,
        .iam = 1, .id_start = 1,
        .extensions = ".mgt"
    },
    
    /* Opus Discovery */
    {
        .id = UFT_ZX_OPUS_SSSD,
        .name = "Opus Discovery SS/SD",
        .description = "Opus Discovery single-sided",
        .gw_format = "zx.opus.ss40",
        .cyls = 40, .heads = 1, .secs = 18, .bps = 256,
        .rate = 250, .encoding = 1,
        .interleave = 13, .cskew = 13, .hskew = 0,
        .iam = 0, .id_start = 0,
        .extensions = ".opd;.img"
    },
    {
        .id = UFT_ZX_OPUS_DSDD,
        .name = "Opus Discovery DS/DD",
        .description = "Opus Discovery double-sided",
        .gw_format = "zx.opus.ds80",
        .cyls = 80, .heads = 2, .secs = 18, .bps = 256,
        .rate = 250, .encoding = 1,
        .interleave = 13, .cskew = 13, .hskew = 0,
        .iam = 0, .id_start = 0,
        .extensions = ".opd;.img"
    },
    
    /* Kempston */
    {
        .id = UFT_ZX_KEMPSTON_SSSD,
        .name = "Kempston SS/SD",
        .description = "Kempston Disc Interface single-sided",
        .gw_format = "zx.kempston.sssd",
        .cyls = 40, .heads = 1, .secs = 10, .bps = 512,
        .rate = 250, .encoding = 1,
        .interleave = 1, .cskew = 0, .hskew = 0,
        .iam = 1, .id_start = 0,
        .extensions = ".dsk;.img"
    },
    {
        .id = UFT_ZX_KEMPSTON_DSDD,
        .name = "Kempston DS/DD",
        .description = "Kempston Disc Interface double-sided",
        .gw_format = "zx.kempston.dsdd",
        .cyls = 80, .heads = 2, .secs = 10, .bps = 512,
        .rate = 250, .encoding = 1,
        .interleave = 1, .cskew = 0, .hskew = 0,
        .iam = 1, .id_start = 0,
        .extensions = ".dsk;.img"
    },
    
    /* Watford SPDOS */
    {
        .id = UFT_ZX_WATFORD_SSSD,
        .name = "Watford SPDOS SS/SD",
        .description = "Watford Electronics SPDOS single-sided",
        .gw_format = "zx.watford.sssd",
        .cyls = 40, .heads = 1, .secs = 10, .bps = 512,
        .rate = 250, .encoding = 1,
        .interleave = 1, .cskew = 0, .hskew = 0,
        .iam = 1, .id_start = 0,
        .extensions = ".dsk"
    },
    {
        .id = UFT_ZX_WATFORD_DSDD,
        .name = "Watford SPDOS DS/DD",
        .description = "Watford Electronics SPDOS double-sided",
        .gw_format = "zx.watford.dsdd",
        .cyls = 80, .heads = 2, .secs = 10, .bps = 512,
        .rate = 250, .encoding = 1,
        .interleave = 1, .cskew = 0, .hskew = 0,
        .iam = 1, .id_start = 0,
        .extensions = ".dsk"
    },
    
    /* Turbodrive */
    {
        .id = UFT_ZX_TURBODRIVE_DSSD,
        .name = "Turbodrive DS/SD",
        .description = "Turbodrive 40-track double-sided",
        .gw_format = "zx.turbodrive.dssd",
        .cyls = 40, .heads = 2, .secs = 10, .bps = 512,
        .rate = 250, .encoding = 1,
        .interleave = 5, .cskew = 5, .hskew = 0,
        .iam = 1, .id_start = 1,
        .extensions = ".dsk"
    },
    {
        .id = UFT_ZX_TURBODRIVE_DSDD,
        .name = "Turbodrive DS/DD",
        .description = "Turbodrive 80-track double-sided",
        .gw_format = "zx.turbodrive.dsdd",
        .cyls = 80, .heads = 2, .secs = 10, .bps = 512,
        .rate = 250, .encoding = 1,
        .interleave = 5, .cskew = 5, .hskew = 0,
        .iam = 1, .id_start = 1,
        .extensions = ".dsk"
    },
    
    /* Didaktik D80 */
    {
        .id = UFT_ZX_DIDAKTIK_D80_DSDD,
        .name = "Didaktik D80",
        .description = "Didaktik D80 MDOS",
        .gw_format = "zx.d80.dsdd",
        .cyls = 80, .heads = 2, .secs = 9, .bps = 512,
        .rate = 250, .encoding = 1,
        .interleave = 1, .cskew = 0, .hskew = 0,
        .iam = 1, .id_start = 1,
        .extensions = ".dsk"
    },
    
    /* Timex FDD 3000 */
    {
        .id = UFT_ZX_TIMEX_FDD3000_SSSD,
        .name = "Timex FDD 3000 SS/SD",
        .description = "Timex FDD 3/3000 single-sided",
        .gw_format = "tmx.fdd3000.sssd",
        .cyls = 40, .heads = 1, .secs = 16, .bps = 256,
        .rate = 250, .encoding = 1,
        .interleave = 1, .cskew = 0, .hskew = 0,
        .iam = 1, .id_start = 0,
        .extensions = ".dsk"
    },
    {
        .id = UFT_ZX_TIMEX_FDD3000_DSDD,
        .name = "Timex FDD 3000 DS/DD",
        .description = "Timex FDD 3/3000 double-sided",
        .gw_format = "tmx.fdd3000.dsdd",
        .cyls = 80, .heads = 2, .secs = 16, .bps = 256,
        .rate = 250, .encoding = 1,
        .interleave = 1, .cskew = 0, .hskew = 0,
        .iam = 1, .id_start = 0,
        .extensions = ".dsk"
    },
    
    /* Beta Disk / TR-DOS */
    {
        .id = UFT_ZX_BETADISK_TRDOS,
        .name = "Beta Disk TR-DOS",
        .description = "Technology Research TR-DOS",
        .gw_format = "zx.trdos.ds80",
        .cyls = 80, .heads = 2, .secs = 16, .bps = 256,
        .rate = 250, .encoding = 1,
        .interleave = 1, .cskew = 0, .hskew = 0,
        .iam = 1, .id_start = 1,
        .extensions = ".trd;.scl"
    },
};

/* ═══════════════════════════════════════════════════════════════════════════
 * API Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Get preset by ID
 */
static inline const uft_zx_preset_t *uft_zx_get_preset(uft_zx_format_id_t id) {
    if (id >= UFT_ZX_FORMAT_COUNT) return NULL;
    return &UFT_ZX_PRESETS[id];
}

/**
 * Find preset by Greaseweazle format string
 */
static inline const uft_zx_preset_t *uft_zx_find_by_gw_format(const char *gw_format) {
    if (!gw_format) return NULL;
    for (int i = 0; i < UFT_ZX_FORMAT_COUNT; i++) {
        if (strcmp(UFT_ZX_PRESETS[i].gw_format, gw_format) == 0) {
            return &UFT_ZX_PRESETS[i];
        }
    }
    return NULL;
}

/**
 * Get total disk size in bytes
 */
static inline uint32_t uft_zx_disk_size(const uft_zx_preset_t *preset) {
    if (!preset) return 0;
    return (uint32_t)preset->cyls * preset->heads * preset->secs * preset->bps;
}

#ifdef __cplusplus
}
#endif
#endif /* UFT_PRESET_ZX_SPECTRUM_H */
