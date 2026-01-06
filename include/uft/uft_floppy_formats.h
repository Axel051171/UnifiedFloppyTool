/**
 * @file uft_floppy_formats.h
 * @brief Floppy disk format definitions derived from Linux kernel
 * 
 * Source: linux/drivers/block/floppy.c
 * Authors: Linus Torvalds, Alain Knaff, Alan Cox
 * License: GPL-2.0
 * 
 * Contains 32 format definitions covering PC, Amiga, Atari ST, and
 * extended format disks.
 */

#ifndef UFT_FLOPPY_FORMATS_H
#define UFT_FLOPPY_FORMATS_H

#include <stdint.h>
#include <stdbool.h>

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Data Rate Constants
 *===========================================================================*/

/** Data rate encoding for FDC */
typedef enum {
    UFT_RATE_500K   = 0x00,  /**< 500 kbps (HD) */
    UFT_RATE_300K   = 0x01,  /**< 300 kbps (DD 300 RPM) */
    UFT_RATE_250K   = 0x02,  /**< 250 kbps (DD) */
    UFT_RATE_1000K  = 0x03,  /**< 1000 kbps (ED) */
    UFT_RATE_PERP   = 0x40,  /**< Perpendicular recording flag */
} uft_data_rate_t;

/*===========================================================================
 * Stretch Field Flags
 *===========================================================================*/

/** Stretch/option field bits */
typedef enum {
    UFT_STRETCH_DOUBLE   = 0x01,  /**< Double-step (360K in 1.2M drive) */
    UFT_STRETCH_C1581    = 0x02,  /**< Commodore 1581 mode */
    UFT_STRETCH_SECT_MASK = 0x3FC, /**< First sector number << 2 */
} uft_stretch_flags_t;

/** First sector number encoding */
#define UFT_FIRST_SECTOR(s) (((s) >> 2) ^ 1)
#define UFT_ENCODE_FIRST_SECTOR(n) ((((n) ^ 1) << 2) & 0x3FC)

/*===========================================================================
 * Floppy Format Structure
 *===========================================================================*/

/**
 * @brief Floppy disk format parameters
 * 
 * Based on Linux kernel struct floppy_struct
 */
typedef struct {
    const char *name;       /**< Format name (e.g., "H1440") */
    uint32_t size;          /**< Total sectors (512-byte equivalent) */
    uint8_t sect;           /**< Sectors per track */
    uint8_t head;           /**< Number of heads (1 or 2) */
    uint8_t track;          /**< Number of tracks */
    uint16_t stretch;       /**< Stretch/option flags */
    uint8_t gap1;           /**< Gap 1 size */
    uint8_t rate;           /**< Data rate + flags */
    uint8_t spec1;          /**< Spec1 (SRT<<4 | HUT) */
    uint8_t fmt_gap;        /**< Format gap (gap 2) size */
    uint16_t sector_size;   /**< Bytes per sector (128, 256, 512, 1024) */
} uft_floppy_format_t;

/*===========================================================================
 * Standard PC Formats
 *===========================================================================*/

/** 360KB PC 5.25" DD */
#define UFT_FMT_360K_PC { \
    .name = "d360", .size = 720, .sect = 9, .head = 2, .track = 40, \
    .stretch = 0, .gap1 = 0x2A, .rate = 0x02, .spec1 = 0xDF, .fmt_gap = 0x50, \
    .sector_size = 512 \
}

/** 1.2MB AT 5.25" HD */
#define UFT_FMT_1200K_AT { \
    .name = "h1200", .size = 2400, .sect = 15, .head = 2, .track = 80, \
    .stretch = 0, .gap1 = 0x1B, .rate = 0x00, .spec1 = 0xDF, .fmt_gap = 0x54, \
    .sector_size = 512 \
}

/** 720KB 3.5" DD */
#define UFT_FMT_720K { \
    .name = "D720", .size = 1440, .sect = 9, .head = 2, .track = 80, \
    .stretch = 0, .gap1 = 0x2A, .rate = 0x02, .spec1 = 0xDF, .fmt_gap = 0x50, \
    .sector_size = 512 \
}

/** 1.44MB 3.5" HD - Standard PC floppy */
#define UFT_FMT_1440K { \
    .name = "H1440", .size = 2880, .sect = 18, .head = 2, .track = 80, \
    .stretch = 0, .gap1 = 0x1B, .rate = 0x00, .spec1 = 0xCF, .fmt_gap = 0x6C, \
    .sector_size = 512 \
}

/** 2.88MB 3.5" ED */
#define UFT_FMT_2880K { \
    .name = "E2880", .size = 5760, .sect = 36, .head = 2, .track = 80, \
    .stretch = 0, .gap1 = 0x1B, .rate = 0x43, .spec1 = 0xAF, .fmt_gap = 0x54, \
    .sector_size = 512 \
}

/*===========================================================================
 * Non-PC Compatible Formats
 *===========================================================================*/

/** 880KB Amiga format (11 sectors/track) */
#define UFT_FMT_880K_AMIGA { \
    .name = "h880", .size = 1760, .sect = 11, .head = 2, .track = 80, \
    .stretch = 0, .gap1 = 0x1C, .rate = 0x09, .spec1 = 0xCF, .fmt_gap = 0x00, \
    .sector_size = 512 \
}

/** 800KB Atari ST / Macintosh format (10 sectors/track) */
#define UFT_FMT_800K_ST { \
    .name = "D800", .size = 1600, .sect = 10, .head = 2, .track = 80, \
    .stretch = 0, .gap1 = 0x25, .rate = 0x02, .spec1 = 0xDF, .fmt_gap = 0x2E, \
    .sector_size = 512 \
}

/*===========================================================================
 * Extended/High-Capacity Formats
 *===========================================================================*/

/** 1.68MB DMF format (21 sectors/track) */
#define UFT_FMT_1680K_DMF { \
    .name = "H1680", .size = 3360, .sect = 21, .head = 2, .track = 80, \
    .stretch = 0, .gap1 = 0x1C, .rate = 0x00, .spec1 = 0xCF, .fmt_gap = 0x0C, \
    .sector_size = 512 \
}

/** 1.72MB format (21 sectors, 82 tracks) */
#define UFT_FMT_1722K { \
    .name = "H1722", .size = 3444, .sect = 21, .head = 2, .track = 82, \
    .stretch = 0, .gap1 = 0x25, .rate = 0x00, .spec1 = 0xDF, .fmt_gap = 0x0C, \
    .sector_size = 512 \
}

/*===========================================================================
 * Complete Format Table
 *===========================================================================*/

static const uft_floppy_format_t uft_floppy_formats[] = {
    /* ID 0: No testing/autodetect */
    { NULL, 0, 0, 0, 0, 0, 0x00, 0x00, 0x00, 0x00, 512 },
    
    /* ID 1: 360KB PC 5.25" */
    { "d360", 720, 9, 2, 40, 0, 0x2A, 0x02, 0xDF, 0x50, 512 },
    
    /* ID 2: 1.2MB AT 5.25" */
    { "h1200", 2400, 15, 2, 80, 0, 0x1B, 0x00, 0xDF, 0x54, 512 },
    
    /* ID 3: 360KB SS 3.5" */
    { "D360", 720, 9, 1, 80, 0, 0x2A, 0x02, 0xDF, 0x50, 512 },
    
    /* ID 4: 720KB 3.5" DD */
    { "D720", 1440, 9, 2, 80, 0, 0x2A, 0x02, 0xDF, 0x50, 512 },
    
    /* ID 5: 360KB in 1.2M drive */
    { "h360", 720, 9, 2, 40, 1, 0x23, 0x01, 0xDF, 0x50, 512 },
    
    /* ID 6: 720KB AT */
    { "h720", 1440, 9, 2, 80, 0, 0x23, 0x01, 0xDF, 0x50, 512 },
    
    /* ID 7: 1.44MB 3.5" HD */
    { "H1440", 2880, 18, 2, 80, 0, 0x1B, 0x00, 0xCF, 0x6C, 512 },
    
    /* ID 8: 2.88MB 3.5" ED */
    { "E2880", 5760, 36, 2, 80, 0, 0x1B, 0x43, 0xAF, 0x54, 512 },
    
    /* ID 9: 3.12MB 3.5" */
    { "E3120", 6240, 39, 2, 80, 0, 0x1B, 0x43, 0xAF, 0x28, 512 },
    
    /* ID 10: 1.44MB 5.25" */
    { "h1440", 2880, 18, 2, 80, 0, 0x25, 0x00, 0xDF, 0x02, 512 },
    
    /* ID 11: 1.68MB DMF 3.5" */
    { "H1680", 3360, 21, 2, 80, 0, 0x1C, 0x00, 0xCF, 0x0C, 512 },
    
    /* ID 12: 410KB 5.25" */
    { "h410", 820, 10, 2, 41, 1, 0x25, 0x01, 0xDF, 0x2E, 512 },
    
    /* ID 13: 820KB 3.5" */
    { "H820", 1640, 10, 2, 82, 0, 0x25, 0x02, 0xDF, 0x2E, 512 },
    
    /* ID 14: 1.48MB 5.25" */
    { "h1476", 2952, 18, 2, 82, 0, 0x25, 0x00, 0xDF, 0x02, 512 },
    
    /* ID 15: 1.72MB 3.5" */
    { "H1722", 3444, 21, 2, 82, 0, 0x25, 0x00, 0xDF, 0x0C, 512 },
    
    /* ID 16: 420KB 5.25" */
    { "h420", 840, 10, 2, 42, 1, 0x25, 0x01, 0xDF, 0x2E, 512 },
    
    /* ID 17: 830KB 3.5" */
    { "H830", 1660, 10, 2, 83, 0, 0x25, 0x02, 0xDF, 0x2E, 512 },
    
    /* ID 18: 1.49MB 5.25" */
    { "h1494", 2988, 18, 2, 83, 0, 0x25, 0x00, 0xDF, 0x02, 512 },
    
    /* ID 19: 1.74MB 3.5" */
    { "H1743", 3486, 21, 2, 83, 0, 0x25, 0x00, 0xDF, 0x0C, 512 },
    
    /* ID 20: 880KB Amiga */
    { "h880", 1760, 11, 2, 80, 0, 0x1C, 0x09, 0xCF, 0x00, 512 },
    
    /* ID 21: 1.04MB 3.5" */
    { "D1040", 2080, 13, 2, 80, 0, 0x1C, 0x01, 0xCF, 0x00, 512 },
    
    /* ID 22: 1.12MB 3.5" */
    { "D1120", 2240, 14, 2, 80, 0, 0x1C, 0x19, 0xCF, 0x00, 512 },
    
    /* ID 23: 1.6MB 5.25" */
    { "h1600", 3200, 20, 2, 80, 0, 0x1C, 0x20, 0xCF, 0x2C, 512 },
    
    /* ID 24: 1.76MB 3.5" */
    { "H1760", 3520, 22, 2, 80, 0, 0x1C, 0x08, 0xCF, 0x2E, 512 },
    
    /* ID 25: 1.92MB 3.5" */
    { "H1920", 3840, 24, 2, 80, 0, 0x1C, 0x20, 0xCF, 0x00, 512 },
    
    /* ID 26: 3.20MB 3.5" */
    { "E3200", 6400, 40, 2, 80, 0, 0x25, 0x5B, 0xCF, 0x00, 512 },
    
    /* ID 27: 3.52MB 3.5" */
    { "E3520", 7040, 44, 2, 80, 0, 0x25, 0x5B, 0xCF, 0x00, 512 },
    
    /* ID 28: 3.84MB 3.5" */
    { "E3840", 7680, 48, 2, 80, 0, 0x25, 0x63, 0xCF, 0x00, 512 },
    
    /* ID 29: 1.84MB 3.5" */
    { "H1840", 3680, 23, 2, 80, 0, 0x1C, 0x10, 0xCF, 0x00, 512 },
    
    /* ID 30: 800KB Atari ST/Mac */
    { "D800", 1600, 10, 2, 80, 0, 0x25, 0x02, 0xDF, 0x2E, 512 },
    
    /* ID 31: 1.6MB 3.5" */
    { "H1600", 3200, 20, 2, 80, 0, 0x1C, 0x00, 0xCF, 0x2C, 512 },
    
    /* Terminator */
    { NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }
};

#define UFT_FLOPPY_FORMAT_COUNT 32

/*===========================================================================
 * Drive Type Structure
 *===========================================================================*/

/**
 * @brief Floppy drive parameters
 */
typedef struct {
    const char *name;       /**< Drive name (e.g., "1.44M") */
    uint8_t cmos;           /**< CMOS drive type (0-6) */
    uint32_t max_dtr;       /**< Maximum data rate (bps) */
    uint16_t hlt;           /**< Head load time (ms) */
    uint16_t hut;           /**< Head unload time (ms) */
    uint16_t srt;           /**< Step rate interval (us) */
    uint8_t rps;            /**< Rotations per second */
    uint8_t tracks;         /**< Maximum tracks */
} uft_drive_type_t;

static const uft_drive_type_t uft_drive_types[] = {
    { "unknown",      0,  500000, 16, 16, 8000, 5, 80 },
    { "360K PC",      1,  300000, 16, 16, 8000, 5, 40 },
    { "1.2M",         2,  500000, 16, 16, 6000, 6, 83 },
    { "720k",         3,  250000, 16, 16, 3000, 5, 83 },
    { "1.44M",        4,  500000, 16, 16, 4000, 5, 83 },
    { "2.88M AMI",    5, 1000000, 15,  8, 3000, 5, 83 },
    { "2.88M",        6, 1000000, 15,  8, 3000, 5, 83 },
    { NULL, 0, 0, 0, 0, 0, 0, 0 }
};

/*===========================================================================
 * Sector Size Encoding
 *===========================================================================*/

/**
 * @brief Convert sector size to FDC size code
 */
static inline uint8_t uft_sector_size_code(uint16_t size) {
    switch (size) {
        case 128:   return 0;
        case 256:   return 1;
        case 512:   return 2;
        case 1024:  return 3;
        case 2048:  return 4;
        case 4096:  return 5;
        case 8192:  return 6;
        case 16384: return 7;
        default:    return 2;  /* Default to 512 */
    }
}

/**
 * @brief Convert FDC size code to sector size
 */
static inline uint16_t uft_sector_size_from_code(uint8_t code) {
    return 128U << (code & 0x07);
}

/*===========================================================================
 * Format Detection
 *===========================================================================*/

/**
 * @brief Detect format by image size
 * @param size Image file size in bytes
 * @return Pointer to matching format or NULL
 */
const uft_floppy_format_t *uft_detect_format_by_size(size_t size);

/**
 * @brief Calculate expected image size for format
 */
static inline size_t uft_format_image_size(const uft_floppy_format_t *fmt) {
    return (size_t)fmt->size * fmt->sector_size;
}

/**
 * @brief Calculate track size in bytes
 */
static inline size_t uft_format_track_size(const uft_floppy_format_t *fmt) {
    return (size_t)fmt->sect * fmt->sector_size;
}

/**
 * @brief Calculate total format capacity in bytes
 */
static inline size_t uft_format_capacity(const uft_floppy_format_t *fmt) {
    return (size_t)fmt->sect * fmt->head * fmt->track * fmt->sector_size;
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_FLOPPY_FORMATS_H */
