/**
 * @file uft_flashfloppy_formats.h
 * @brief FlashFloppy Format Definitions and Disk Types
 * @version 3.4.0
 *
 * Extracted from FlashFloppy by Keir Fraser (public domain)
 * Source: flashfloppy-master/src/image/img.c
 *
 * Contains:
 * - Standard IBM PC disk formats
 * - ADFS (Acorn) disk formats
 * - Akai sampler formats
 * - Commodore D81 formats
 * - DEC RX formats
 * - MSX formats
 * - PC-98 formats
 * - Ensoniq formats
 * - Various other format presets
 *
 * SPDX-License-Identifier: Unlicense
 */

#ifndef UFT_FLASHFLOPPY_FORMATS_H
#define UFT_FLASHFLOPPY_FORMATS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * FORMAT TYPE FLAGS
 *============================================================================*/

#define UFT_FF_LAYOUT_SEQUENTIAL        (1u << 0)
#define UFT_FF_LAYOUT_SIDES_SWAPPED     (1u << 1)
#define UFT_FF_LAYOUT_REVERSE_SIDE0     (1u << 2)
#define UFT_FF_LAYOUT_REVERSE_SIDE1     (1u << 3)

/*============================================================================
 * FORMAT ENTRY STRUCTURE
 *============================================================================*/

/**
 * @brief Disk format definition
 * 
 * Compact structure matching FlashFloppy's raw_type
 */
typedef struct uft_ff_format {
    uint8_t  nr_sectors;        /**< Sectors per track */
    uint8_t  nr_sides;          /**< Number of sides (1 or 2) */
    bool     has_iam;           /**< Has Index Address Mark */
    uint8_t  gap3;              /**< Gap 3 length */
    uint8_t  interleave;        /**< Sector interleave */
    uint8_t  sector_size;       /**< Sector size code (N: 128<<N bytes) */
    uint8_t  sector_base;       /**< First sector number */
    uint8_t  cskew;             /**< Cylinder skew */
    uint8_t  hskew;             /**< Head skew */
    uint8_t  nr_cyls;           /**< Number of cylinders (40 or 80) */
    uint16_t rpm;               /**< Rotation speed (300 or 360) */
    uint32_t total_size;        /**< Total capacity in bytes */
    const char *name;           /**< Format name */
} uft_ff_format_t;

/*============================================================================
 * STANDARD IBM PC FORMATS (from img_type[])
 *============================================================================*/

static const uft_ff_format_t UFT_FF_IBMPC_FORMATS[] = {
    /* 5.25" DD formats */
    { .nr_sectors = 8,  .nr_sides = 1, .has_iam = true, .gap3 = 84,
      .interleave = 1, .sector_size = 2, .sector_base = 1,
      .cskew = 0, .hskew = 0, .nr_cyls = 40, .rpm = 300,
      .total_size = 163840, .name = "PC 160K 5.25\" SSDD" },
    
    { .nr_sectors = 9,  .nr_sides = 1, .has_iam = true, .gap3 = 84,
      .interleave = 1, .sector_size = 2, .sector_base = 1,
      .cskew = 0, .hskew = 0, .nr_cyls = 40, .rpm = 300,
      .total_size = 184320, .name = "PC 180K 5.25\" SSDD" },
    
    { .nr_sectors = 10, .nr_sides = 1, .has_iam = true, .gap3 = 30,
      .interleave = 1, .sector_size = 2, .sector_base = 1,
      .cskew = 0, .hskew = 0, .nr_cyls = 40, .rpm = 300,
      .total_size = 204800, .name = "PC 200K 5.25\" SSDD" },
    
    { .nr_sectors = 8,  .nr_sides = 2, .has_iam = true, .gap3 = 84,
      .interleave = 1, .sector_size = 2, .sector_base = 1,
      .cskew = 0, .hskew = 0, .nr_cyls = 40, .rpm = 300,
      .total_size = 327680, .name = "PC 320K 5.25\" DSDD" },
    
    { .nr_sectors = 9,  .nr_sides = 2, .has_iam = true, .gap3 = 84,
      .interleave = 1, .sector_size = 2, .sector_base = 1,
      .cskew = 0, .hskew = 0, .nr_cyls = 40, .rpm = 300,
      .total_size = 368640, .name = "PC 360K 5.25\" DSDD" },
    
    { .nr_sectors = 10, .nr_sides = 2, .has_iam = true, .gap3 = 30,
      .interleave = 1, .sector_size = 2, .sector_base = 1,
      .cskew = 0, .hskew = 0, .nr_cyls = 40, .rpm = 300,
      .total_size = 409600, .name = "PC 400K 5.25\" DSDD" },
    
    /* 5.25" HD format */
    { .nr_sectors = 15, .nr_sides = 2, .has_iam = true, .gap3 = 84,
      .interleave = 1, .sector_size = 2, .sector_base = 1,
      .cskew = 0, .hskew = 0, .nr_cyls = 80, .rpm = 360,
      .total_size = 1228800, .name = "PC 1.2M 5.25\" DSHD" },
    
    /* 3.5" DD formats */
    { .nr_sectors = 9,  .nr_sides = 1, .has_iam = true, .gap3 = 84,
      .interleave = 1, .sector_size = 2, .sector_base = 1,
      .cskew = 0, .hskew = 0, .nr_cyls = 80, .rpm = 300,
      .total_size = 368640, .name = "PC 360K 3.5\" SSDD" },
    
    { .nr_sectors = 10, .nr_sides = 1, .has_iam = true, .gap3 = 30,
      .interleave = 1, .sector_size = 2, .sector_base = 1,
      .cskew = 0, .hskew = 0, .nr_cyls = 80, .rpm = 300,
      .total_size = 409600, .name = "PC 400K 3.5\" SSDD" },
    
    { .nr_sectors = 11, .nr_sides = 1, .has_iam = true, .gap3 = 3,
      .interleave = 2, .sector_size = 2, .sector_base = 1,
      .cskew = 0, .hskew = 0, .nr_cyls = 80, .rpm = 300,
      .total_size = 450560, .name = "PC 440K 3.5\" SSDD" },
    
    { .nr_sectors = 8,  .nr_sides = 2, .has_iam = true, .gap3 = 84,
      .interleave = 1, .sector_size = 2, .sector_base = 1,
      .cskew = 0, .hskew = 0, .nr_cyls = 80, .rpm = 300,
      .total_size = 655360, .name = "PC 640K 3.5\" DSDD" },
    
    { .nr_sectors = 9,  .nr_sides = 2, .has_iam = true, .gap3 = 84,
      .interleave = 1, .sector_size = 2, .sector_base = 1,
      .cskew = 0, .hskew = 0, .nr_cyls = 80, .rpm = 300,
      .total_size = 737280, .name = "PC 720K 3.5\" DSDD" },
    
    { .nr_sectors = 10, .nr_sides = 2, .has_iam = true, .gap3 = 30,
      .interleave = 1, .sector_size = 2, .sector_base = 1,
      .cskew = 0, .hskew = 0, .nr_cyls = 80, .rpm = 300,
      .total_size = 819200, .name = "PC 800K 3.5\" DSDD" },
    
    { .nr_sectors = 11, .nr_sides = 2, .has_iam = true, .gap3 = 3,
      .interleave = 2, .sector_size = 2, .sector_base = 1,
      .cskew = 0, .hskew = 0, .nr_cyls = 80, .rpm = 300,
      .total_size = 901120, .name = "PC 880K 3.5\" DSDD" },
    
    /* 3.5" HD formats */
    { .nr_sectors = 18, .nr_sides = 2, .has_iam = true, .gap3 = 108,
      .interleave = 1, .sector_size = 2, .sector_base = 1,
      .cskew = 0, .hskew = 0, .nr_cyls = 80, .rpm = 300,
      .total_size = 1474560, .name = "PC 1.44M 3.5\" DSHD" },
    
    { .nr_sectors = 19, .nr_sides = 2, .has_iam = true, .gap3 = 70,
      .interleave = 1, .sector_size = 2, .sector_base = 1,
      .cskew = 0, .hskew = 0, .nr_cyls = 80, .rpm = 300,
      .total_size = 1556480, .name = "PC 1.52M 3.5\" DSHD" },
    
    { .nr_sectors = 21, .nr_sides = 2, .has_iam = true, .gap3 = 12,
      .interleave = 2, .sector_size = 2, .sector_base = 1,
      .cskew = 3, .hskew = 0, .nr_cyls = 80, .rpm = 300,
      .total_size = 1720320, .name = "PC 1.68M DMF 3.5\" DSHD" },
    
    { .nr_sectors = 20, .nr_sides = 2, .has_iam = true, .gap3 = 40,
      .interleave = 1, .sector_size = 2, .sector_base = 1,
      .cskew = 0, .hskew = 0, .nr_cyls = 80, .rpm = 300,
      .total_size = 1638400, .name = "PC 1.6M 3.5\" DSHD" },
    
    /* 3.5" ED format */
    { .nr_sectors = 36, .nr_sides = 2, .has_iam = true, .gap3 = 84,
      .interleave = 1, .sector_size = 2, .sector_base = 1,
      .cskew = 0, .hskew = 0, .nr_cyls = 80, .rpm = 300,
      .total_size = 2949120, .name = "PC 2.88M 3.5\" DSED" },
    
    { 0 }  /* Terminator */
};

/*============================================================================
 * ACORN ADFS FORMATS (from adfs_type[])
 *============================================================================*/

static const uft_ff_format_t UFT_FF_ADFS_FORMATS[] = {
    /* ADFS D/E: 5 * 1kB */
    { .nr_sectors = 5,  .nr_sides = 2, .has_iam = true, .gap3 = 116,
      .interleave = 1, .sector_size = 3, .sector_base = 0,
      .cskew = 1, .hskew = 0, .nr_cyls = 80, .rpm = 300,
      .total_size = 819200, .name = "ADFS D/E 800K" },
    
    /* ADFS F: 10 * 1kB */
    { .nr_sectors = 10, .nr_sides = 2, .has_iam = true, .gap3 = 116,
      .interleave = 1, .sector_size = 3, .sector_base = 0,
      .cskew = 2, .hskew = 0, .nr_cyls = 80, .rpm = 300,
      .total_size = 1638400, .name = "ADFS F 1.6M" },
    
    /* ADFS L 640k */
    { .nr_sectors = 16, .nr_sides = 2, .has_iam = true, .gap3 = 57,
      .interleave = 1, .sector_size = 1, .sector_base = 0,
      .cskew = 0, .hskew = 0, .nr_cyls = 80, .rpm = 300,
      .total_size = 655360, .name = "ADFS L 640K" },
    
    /* ADFS M 320k */
    { .nr_sectors = 16, .nr_sides = 1, .has_iam = true, .gap3 = 57,
      .interleave = 1, .sector_size = 1, .sector_base = 0,
      .cskew = 0, .hskew = 0, .nr_cyls = 80, .rpm = 300,
      .total_size = 327680, .name = "ADFS M 320K" },
    
    /* ADFS S 160k */
    { .nr_sectors = 16, .nr_sides = 1, .has_iam = true, .gap3 = 57,
      .interleave = 1, .sector_size = 1, .sector_base = 0,
      .cskew = 0, .hskew = 0, .nr_cyls = 40, .rpm = 300,
      .total_size = 163840, .name = "ADFS S 160K" },
    
    { 0 }  /* Terminator */
};

/*============================================================================
 * AKAI SAMPLER FORMATS (from akai_type[])
 *============================================================================*/

static const uft_ff_format_t UFT_FF_AKAI_FORMATS[] = {
    /* Akai DD: 5*1kB sectors */
    { .nr_sectors = 5,  .nr_sides = 2, .has_iam = true, .gap3 = 116,
      .interleave = 1, .sector_size = 3, .sector_base = 1,
      .cskew = 2, .hskew = 0, .nr_cyls = 80, .rpm = 300,
      .total_size = 819200, .name = "Akai S900/S950 DD 800K" },
    
    /* Akai HD: 10*1kB sectors */
    { .nr_sectors = 10, .nr_sides = 2, .has_iam = true, .gap3 = 116,
      .interleave = 1, .sector_size = 3, .sector_base = 1,
      .cskew = 5, .hskew = 0, .nr_cyls = 80, .rpm = 300,
      .total_size = 1638400, .name = "Akai S1000/S3000 HD 1.6M" },
    
    { 0 }  /* Terminator */
};

/*============================================================================
 * COMMODORE D81 FORMATS (from d81_type[])
 *============================================================================*/

static const uft_ff_format_t UFT_FF_D81_FORMATS[] = {
    /* D81 800k */
    { .nr_sectors = 10, .nr_sides = 2, .has_iam = false, .gap3 = 30,
      .interleave = 1, .sector_size = 2, .sector_base = 1,
      .cskew = 0, .hskew = 0, .nr_cyls = 80, .rpm = 300,
      .total_size = 819200, .name = "Commodore D81 800K" },
    
    /* D81 HD 1.6M */
    { .nr_sectors = 10, .nr_sides = 2, .has_iam = false, .gap3 = 116,
      .interleave = 1, .sector_size = 3, .sector_base = 1,
      .cskew = 0, .hskew = 0, .nr_cyls = 80, .rpm = 300,
      .total_size = 1638400, .name = "Commodore D81 HD 1.6M" },
    
    /* D81 ED 3.2M */
    { .nr_sectors = 20, .nr_sides = 2, .has_iam = false, .gap3 = 116,
      .interleave = 1, .sector_size = 3, .sector_base = 1,
      .cskew = 0, .hskew = 0, .nr_cyls = 80, .rpm = 300,
      .total_size = 3276800, .name = "Commodore D81 ED 3.2M" },
    
    { 0 }  /* Terminator */
};

/*============================================================================
 * DEC FORMATS (from dec_type[])
 *============================================================================*/

static const uft_ff_format_t UFT_FF_DEC_FORMATS[] = {
    /* RX50 400k */
    { .nr_sectors = 10, .nr_sides = 1, .has_iam = true, .gap3 = 30,
      .interleave = 1, .sector_size = 2, .sector_base = 1,
      .cskew = 0, .hskew = 0, .nr_cyls = 80, .rpm = 300,
      .total_size = 409600, .name = "DEC RX50 400K" },
    
    { 0 }  /* Terminator */
};

/*============================================================================
 * ENSONIQ SAMPLER FORMATS (from ensoniq_type[])
 *============================================================================*/

static const uft_ff_format_t UFT_FF_ENSONIQ_FORMATS[] = {
    /* Ensoniq 800kB */
    { .nr_sectors = 10, .nr_sides = 2, .has_iam = true, .gap3 = 30,
      .interleave = 1, .sector_size = 2, .sector_base = 0,
      .cskew = 0, .hskew = 0, .nr_cyls = 80, .rpm = 300,
      .total_size = 819200, .name = "Ensoniq 800K" },
    
    /* Ensoniq 1.6MB */
    { .nr_sectors = 20, .nr_sides = 2, .has_iam = true, .gap3 = 40,
      .interleave = 1, .sector_size = 2, .sector_base = 0,
      .cskew = 0, .hskew = 0, .nr_cyls = 80, .rpm = 300,
      .total_size = 1638400, .name = "Ensoniq 1.6M" },
    
    { 0 }  /* Terminator */
};

/*============================================================================
 * MSX FORMATS (from msx_type[])
 *============================================================================*/

static const uft_ff_format_t UFT_FF_MSX_FORMATS[] = {
    /* MSX 320k */
    { .nr_sectors = 8,  .nr_sides = 1, .has_iam = true, .gap3 = 84,
      .interleave = 1, .sector_size = 2, .sector_base = 1,
      .cskew = 0, .hskew = 0, .nr_cyls = 80, .rpm = 300,
      .total_size = 327680, .name = "MSX 320K" },
    
    /* MSX 360k */
    { .nr_sectors = 9,  .nr_sides = 1, .has_iam = true, .gap3 = 84,
      .interleave = 1, .sector_size = 2, .sector_base = 1,
      .cskew = 0, .hskew = 0, .nr_cyls = 80, .rpm = 300,
      .total_size = 368640, .name = "MSX 360K" },
    
    { 0 }  /* Terminator */
};

/*============================================================================
 * PC-98 FORMATS (from pc98_type[])
 *============================================================================*/

static const uft_ff_format_t UFT_FF_PC98_FORMATS[] = {
    /* PC-98 HD 360RPM (1kB sectors) */
    { .nr_sectors = 8,  .nr_sides = 2, .has_iam = true, .gap3 = 116,
      .interleave = 1, .sector_size = 3, .sector_base = 1,
      .cskew = 0, .hskew = 0, .nr_cyls = 80, .rpm = 360,
      .total_size = 1310720, .name = "PC-98 1.25M HD 360RPM" },
    
    /* PC-98 DD 360RPM (512B sectors) */
    { .nr_sectors = 8,  .nr_sides = 2, .has_iam = true, .gap3 = 57,
      .interleave = 1, .sector_size = 2, .sector_base = 1,
      .cskew = 0, .hskew = 0, .nr_cyls = 80, .rpm = 360,
      .total_size = 655360, .name = "PC-98 640K DD 360RPM" },
    
    { 0 }  /* Terminator */
};

/*============================================================================
 * CASIO KEYBOARD FORMATS (from casio_type[])
 *============================================================================*/

static const uft_ff_format_t UFT_FF_CASIO_FORMATS[] = {
    /* Casio 1280k */
    { .nr_sectors = 8,  .nr_sides = 2, .has_iam = true, .gap3 = 116,
      .interleave = 3, .sector_size = 3, .sector_base = 1,
      .cskew = 0, .hskew = 0, .nr_cyls = 80, .rpm = 360,
      .total_size = 1310720, .name = "Casio 1280K" },
    
    { 0 }  /* Terminator */
};

/*============================================================================
 * FORMAT AUTO-DETECTION BY SIZE
 *============================================================================*/

/**
 * @brief Find format by file size
 * 
 * @param size File size in bytes
 * @param formats Array of format definitions (NULL-terminated)
 * @return Matching format or NULL
 */
static inline const uft_ff_format_t* uft_ff_find_format_by_size(
    uint32_t size, 
    const uft_ff_format_t *formats) 
{
    for (const uft_ff_format_t *f = formats; f->nr_sectors != 0; f++) {
        if (f->total_size == size) {
            return f;
        }
    }
    return NULL;
}

/**
 * @brief Calculate format size
 */
static inline uint32_t uft_ff_calc_format_size(const uft_ff_format_t *fmt) {
    return (uint32_t)fmt->nr_cyls * fmt->nr_sides * fmt->nr_sectors * 
           (128u << fmt->sector_size);
}

/**
 * @brief Get sector size in bytes from sector size code
 */
static inline uint32_t uft_ff_sector_size_bytes(uint8_t n) {
    return 128u << n;
}

/*============================================================================
 * GAP CALCULATION HELPERS
 *============================================================================*/

/**
 * @brief Default gap values by sector size and density
 */
typedef struct uft_ff_gap_defaults {
    uint8_t gap1;           /**< Post-index gap */
    uint8_t gap2;           /**< Post-ID gap */
    uint8_t gap3;           /**< Post-data gap */
    uint8_t gap4a;          /**< Pre-index gap */
} uft_ff_gap_defaults_t;

/* MFM default gaps by sector size */
static const uft_ff_gap_defaults_t UFT_FF_MFM_GAPS[] = {
    [0] = { .gap1 = 50, .gap2 = 22, .gap3 = 50,  .gap4a = 80 },  /* 128 bytes */
    [1] = { .gap1 = 50, .gap2 = 22, .gap3 = 54,  .gap4a = 80 },  /* 256 bytes */
    [2] = { .gap1 = 50, .gap2 = 22, .gap3 = 84,  .gap4a = 80 },  /* 512 bytes */
    [3] = { .gap1 = 50, .gap2 = 22, .gap3 = 116, .gap4a = 80 },  /* 1024 bytes */
};

/* FM default gaps by sector size */
static const uft_ff_gap_defaults_t UFT_FF_FM_GAPS[] = {
    [0] = { .gap1 = 26, .gap2 = 11, .gap3 = 27, .gap4a = 40 },  /* 128 bytes */
    [1] = { .gap1 = 26, .gap2 = 11, .gap3 = 42, .gap4a = 40 },  /* 256 bytes */
    [2] = { .gap1 = 26, .gap2 = 11, .gap3 = 58, .gap4a = 40 },  /* 512 bytes */
    [3] = { .gap1 = 26, .gap2 = 11, .gap3 = 138, .gap4a = 40 }, /* 1024 bytes */
};

/*============================================================================
 * DATA RATE CONSTANTS
 *============================================================================*/

/* Standard data rates in bits per second */
#define UFT_FF_DATARATE_FM_SD       125000   /* FM Single Density */
#define UFT_FF_DATARATE_FM_DD       250000   /* FM Double Density */
#define UFT_FF_DATARATE_MFM_DD      250000   /* MFM Double Density */
#define UFT_FF_DATARATE_MFM_HD      500000   /* MFM High Density */
#define UFT_FF_DATARATE_MFM_ED      1000000  /* MFM Extra Density */

/* Bitcell periods in nanoseconds */
#define UFT_FF_BITCELL_FM_SD        8000     /* 8µs */
#define UFT_FF_BITCELL_FM_DD        4000     /* 4µs */
#define UFT_FF_BITCELL_MFM_DD       4000     /* 4µs (2µs per flux) */
#define UFT_FF_BITCELL_MFM_HD       2000     /* 2µs (1µs per flux) */
#define UFT_FF_BITCELL_MFM_ED       1000     /* 1µs (0.5µs per flux) */

/*============================================================================
 * TRACK LENGTH CALCULATION
 *============================================================================*/

/**
 * @brief Calculate raw track length in bits
 * 
 * @param rpm Rotation speed
 * @param data_rate Data rate in bits/second
 * @return Track length in bits
 */
static inline uint32_t uft_ff_track_bits(uint16_t rpm, uint32_t data_rate) {
    /* bits = (60 / rpm) * data_rate */
    return (60ULL * data_rate) / rpm;
}

/**
 * @brief Calculate track capacity with overhead
 * 
 * @param fmt Format definition
 * @param is_fm True for FM encoding
 * @return Available data bytes per track
 */
static inline uint32_t uft_ff_track_capacity(const uft_ff_format_t *fmt,
                                              bool is_fm) {
    uint32_t sector_bytes = 128u << fmt->sector_size;
    uint32_t data_bytes = fmt->nr_sectors * sector_bytes;
    
    /* Add overhead: ID field, gaps, sync bytes */
    uint32_t overhead_per_sector = is_fm ? 31 : 62;  /* Approximate */
    uint32_t total_overhead = fmt->nr_sectors * overhead_per_sector;
    
    /* Gap4 fills remainder */
    return data_bytes + total_overhead;
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_FLASHFLOPPY_FORMATS_H */
