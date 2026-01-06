/*
 * uft_drive_params.h - Drive and Media Parameter Definitions
 *
 * Part of UnifiedFloppyTool (UFT) v3.3.0
 * Based on fdutils (GPL-2.0+) - Parameter extraction
 *
 * Defines drive types, media densities, form factors, and
 * sector interleave/skew calculations.
 */

#ifndef UFT_DRIVE_PARAMS_H
#define UFT_DRIVE_PARAMS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * Media Density Types
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef enum uft_density {
    UFT_DENS_UNKNOWN = 0,
    UFT_DENS_SD,        /* Single Density (FM, 125 kbps) */
    UFT_DENS_DD,        /* Double Density (MFM, 250 kbps) */
    UFT_DENS_QD,        /* Quad Density (MFM, 300 kbps) */
    UFT_DENS_HD,        /* High Density (MFM, 500 kbps) */
    UFT_DENS_ED,        /* Extended Density (MFM, 1000 kbps) */
} uft_density_t;

static inline const char *uft_density_name(uft_density_t d) {
    static const char *names[] = {
        "Unknown", "SD", "DD", "QD", "HD", "ED"
    };
    return (d <= UFT_DENS_ED) ? names[d] : "Invalid";
}

static inline uint32_t uft_density_bitrate(uft_density_t d) {
    static const uint32_t rates[] = {
        0, 125000, 250000, 300000, 500000, 1000000
    };
    return (d <= UFT_DENS_ED) ? rates[d] : 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Drive Form Factors
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef enum uft_form_factor {
    UFT_FF_UNKNOWN = 0,
    UFT_FF_8,           /* 8 inch */
    UFT_FF_525,         /* 5.25 inch */
    UFT_FF_35,          /* 3.5 inch */
} uft_form_factor_t;

static inline const char *uft_ff_name(uft_form_factor_t ff) {
    static const char *names[] = {
        "Unknown", "8\"", "5.25\"", "3.5\""
    };
    return (ff <= UFT_FF_35) ? names[ff] : "Invalid";
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Drive Type Definitions
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct uft_drive_type {
    const char *name;
    uft_form_factor_t form_factor;
    uft_density_t max_density;
    uint8_t  tpi;               /* Tracks per inch */
    uint16_t rpm;               /* Rotational speed */
    uint8_t  max_cyls;          /* Maximum cylinders */
    uint8_t  default_heads;     /* Default head count */
} uft_drive_type_t;

/* Standard PC drive types */
static const uft_drive_type_t UFT_DRIVE_TYPES[] = {
    /* 5.25" drives */
    { "5.25\" 360K DD",   UFT_FF_525, UFT_DENS_DD, 48, 300, 40, 2 },
    { "5.25\" 1.2M HD",   UFT_FF_525, UFT_DENS_HD, 96, 360, 80, 2 },
    
    /* 3.5" drives */
    { "3.5\" 720K DD",    UFT_FF_35,  UFT_DENS_DD, 135, 300, 80, 2 },
    { "3.5\" 1.44M HD",   UFT_FF_35,  UFT_DENS_HD, 135, 300, 80, 2 },
    { "3.5\" 2.88M ED",   UFT_FF_35,  UFT_DENS_ED, 135, 300, 80, 2 },
    
    /* 8" drives */
    { "8\" 250K SD",      UFT_FF_8,   UFT_DENS_SD, 48, 360, 77, 1 },
    { "8\" 500K DD",      UFT_FF_8,   UFT_DENS_DD, 48, 360, 77, 1 },
    { "8\" 1.2M DD DS",   UFT_FF_8,   UFT_DENS_DD, 48, 360, 77, 2 },
};

#define UFT_DRIVE_TYPE_COUNT (sizeof(UFT_DRIVE_TYPES) / sizeof(UFT_DRIVE_TYPES[0]))

/* ═══════════════════════════════════════════════════════════════════════════
 * Media Format Parameters
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct uft_media_params {
    /* Basic geometry */
    uint8_t  cylinders;
    uint8_t  heads;
    uint8_t  sectors;           /* Sectors per track (0 = variable) */
    uint16_t sector_size;       /* Bytes per sector */
    
    /* Encoding */
    uft_density_t density;
    uint8_t  fm_mode;           /* 0 = MFM, 1 = FM */
    
    /* Data rate */
    uint16_t data_rate;         /* kbps */
    uint16_t rpm;               /* Rotational speed */
    
    /* Sector numbering */
    uint8_t  first_sector;      /* First sector number (usually 1) */
    uint8_t  interleave;        /* Sector interleave factor */
    int8_t   skew;              /* Track-to-track skew */
    
    /* Flags */
    uint8_t  swap_sides;        /* Side 0/1 swapped */
    uint8_t  perpendicular;     /* Perpendicular recording (ED) */
    uint8_t  double_step;       /* 40T disk in 80T drive */
} uft_media_params_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Common Format Presets
 * ═══════════════════════════════════════════════════════════════════════════ */

static const uft_media_params_t UFT_MEDIA_PRESETS[] = {
    /* 5.25" formats */
    {  40, 1,  8, 512, UFT_DENS_DD, 0, 250, 300, 1, 1, 0, 0, 0, 0 },  /* 160K SS */
    {  40, 2,  8, 512, UFT_DENS_DD, 0, 250, 300, 1, 1, 0, 0, 0, 0 },  /* 320K DS */
    {  40, 1,  9, 512, UFT_DENS_DD, 0, 250, 300, 1, 1, 0, 0, 0, 0 },  /* 180K SS */
    {  40, 2,  9, 512, UFT_DENS_DD, 0, 250, 300, 1, 1, 0, 0, 0, 0 },  /* 360K DS */
    {  80, 2, 15, 512, UFT_DENS_HD, 0, 500, 360, 1, 1, 0, 0, 0, 0 },  /* 1.2M HD */
    
    /* 3.5" formats */
    {  80, 1,  9, 512, UFT_DENS_DD, 0, 250, 300, 1, 1, 0, 0, 0, 0 },  /* 360K SS */
    {  80, 2,  9, 512, UFT_DENS_DD, 0, 250, 300, 1, 1, 0, 0, 0, 0 },  /* 720K DS */
    {  80, 2, 18, 512, UFT_DENS_HD, 0, 500, 300, 1, 1, 0, 0, 0, 0 },  /* 1.44M HD */
    {  80, 2, 36, 512, UFT_DENS_ED, 0, 1000, 300, 1, 1, 0, 0, 0, 1 }, /* 2.88M ED */
    
    /* 8" formats */
    {  77, 1, 26, 128, UFT_DENS_SD, 1, 125, 360, 1, 1, 0, 0, 0, 0 },  /* 250K SD */
    {  77, 1, 26, 256, UFT_DENS_DD, 0, 250, 360, 1, 1, 0, 0, 0, 0 },  /* 500K DD */
    {  77, 2, 26, 256, UFT_DENS_DD, 0, 250, 360, 1, 1, 0, 0, 0, 0 },  /* 1M DD DS */
};

#define UFT_MEDIA_PRESET_COUNT (sizeof(UFT_MEDIA_PRESETS) / sizeof(UFT_MEDIA_PRESETS[0]))

/* ═══════════════════════════════════════════════════════════════════════════
 * Sector Interleave Calculation
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Calculate sector numbers with interleave
 * 
 * @param sectors       Number of sectors per track
 * @param interleave    Interleave factor (1 = none)
 * @param first_sector  First sector number
 * @param out_table     Output table (must hold 'sectors' entries)
 */
static inline void uft_calc_interleave(
    uint8_t sectors,
    uint8_t interleave,
    uint8_t first_sector,
    uint8_t *out_table)
{
    if (!out_table || sectors == 0) return;
    
    /* Initialize all as unassigned */
    for (uint8_t i = 0; i < sectors; i++) {
        out_table[i] = 0xFF;
    }
    
    uint8_t pos = 0;
    for (uint8_t sec = 0; sec < sectors; sec++) {
        /* Find next free position */
        while (out_table[pos] != 0xFF) {
            pos = (pos + 1) % sectors;
        }
        
        out_table[pos] = first_sector + sec;
        pos = (pos + interleave) % sectors;
    }
}

/**
 * Calculate track skew offset
 * 
 * @param track         Track number
 * @param head          Head number
 * @param track_skew    Track-to-track skew (sectors)
 * @param head_skew     Head-to-head skew (sectors)
 * @param sectors       Sectors per track
 * @return              Skew offset (sector number to start at)
 */
static inline uint8_t uft_calc_skew(
    uint8_t track,
    uint8_t head,
    int8_t track_skew,
    int8_t head_skew,
    uint8_t sectors)
{
    if (sectors == 0) return 0;
    
    int total_skew = (track * track_skew) + (head * head_skew);
    
    /* Handle negative skew */
    while (total_skew < 0) total_skew += sectors;
    
    return (uint8_t)(total_skew % sectors);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Raw Track Capacity Calculation
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Calculate raw track capacity in bytes
 * 
 * @param data_rate     Data rate in kbps
 * @param rpm           Rotational speed
 * @param fm_mode       0 = MFM (2 bits/cell), 1 = FM (1 bit/cell)
 * @return              Raw capacity in bytes
 */
static inline uint32_t uft_track_capacity(
    uint16_t data_rate,
    uint16_t rpm,
    uint8_t fm_mode)
{
    if (rpm == 0) return 0;
    
    /* bits per revolution = data_rate * 60 / rpm */
    uint32_t bits_per_rev = ((uint32_t)data_rate * 1000 * 60) / rpm;
    
    /* MFM uses 2 clock bits per data bit, FM uses 1 */
    uint32_t data_bits = bits_per_rev / (fm_mode ? 2 : 4);
    
    return data_bits / 8;
}

/**
 * Estimate format from disk size
 * 
 * @param size          Disk image size in bytes
 * @return              Pointer to matching preset, or NULL
 */
static inline const uft_media_params_t *uft_estimate_format(size_t size) {
    for (size_t i = 0; i < UFT_MEDIA_PRESET_COUNT; i++) {
        const uft_media_params_t *p = &UFT_MEDIA_PRESETS[i];
        size_t expected = (size_t)p->cylinders * p->heads * p->sectors * p->sector_size;
        if (size == expected) return p;
    }
    return NULL;
}

#ifdef __cplusplus
}
#endif
#endif /* UFT_DRIVE_PARAMS_H */
