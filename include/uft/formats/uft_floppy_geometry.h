/**
 * @file uft_floppy_geometry.h
 * @brief Floppy Disk Physical Geometry and Format Definitions
 * 
 * Based on Wikipedia "List of floppy disk formats" and related sources.
 * Comprehensive database of floppy disk physical characteristics.
 * 
 * Covers:
 * - 8-inch, 5.25-inch, 3.5-inch, and exotic formats
 * - Single/Double/High/Extended density
 * - All major platforms (IBM PC, Apple, Commodore, Atari, Amiga, etc.)
 */

#ifndef UFT_FLOPPY_GEOMETRY_H
#define UFT_FLOPPY_GEOMETRY_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Physical Media Sizes
 *===========================================================================*/

typedef enum {
    UFT_MEDIA_8_INCH        = 8,    /**< 8" floppy (200mm) */
    UFT_MEDIA_5_25_INCH     = 5,    /**< 5.25" floppy (133mm) */
    UFT_MEDIA_3_5_INCH      = 3,    /**< 3.5" floppy (90mm) */
    UFT_MEDIA_3_INCH        = 30,   /**< 3" floppy (Amstrad, etc.) */
    UFT_MEDIA_2_5_INCH      = 25,   /**< 2.5" floppy (Sharp) */
    UFT_MEDIA_2_INCH        = 20    /**< 2" floppy (Video Floppy) */
} uft_media_size_t;

/*===========================================================================
 * Recording Density Classifications
 *===========================================================================*/

typedef enum {
    UFT_DENSITY_SINGLE      = 1,    /**< SD - FM encoding, ~125 kbit/s */
    UFT_DENSITY_DOUBLE      = 2,    /**< DD - MFM encoding, ~250 kbit/s */
    UFT_DENSITY_QUAD        = 4,    /**< QD - 96 TPI, ~500 kbit/s */
    UFT_DENSITY_HIGH        = 8,    /**< HD - MFM, ~500 kbit/s */
    UFT_DENSITY_EXTENDED    = 16    /**< ED - Perpendicular, ~1000 kbit/s */
} uft_density_t;

/*===========================================================================
 * Data Encoding Methods
 *===========================================================================*/

typedef enum {
    UFT_ENCODING_FM         = 1,    /**< Frequency Modulation (Single Density) */
    UFT_ENCODING_MFM        = 2,    /**< Modified FM (Double Density+) */
    UFT_ENCODING_M2FM       = 3,    /**< Modified MFM (Intel ISIS, HP) */
    UFT_ENCODING_GCR        = 4,    /**< Group Coded Recording (Apple, C64) */
    UFT_ENCODING_GCR_APPLE  = 5,    /**< Apple-specific GCR variants */
    UFT_ENCODING_GCR_C64    = 6     /**< Commodore-specific GCR */
} uft_encoding_t;

/*===========================================================================
 * Sectoring Types
 *===========================================================================*/

typedef enum {
    UFT_SECTOR_SOFT         = 0,    /**< Software-defined sectors */
    UFT_SECTOR_HARD         = 1     /**< Hardware index holes */
} uft_sectoring_t;

/*===========================================================================
 * Rotation Speeds (RPM)
 *===========================================================================*/

#define UFT_RPM_300             300     /**< Standard 5.25"/3.5" DD/HD */
#define UFT_RPM_360             360     /**< 5.25" HD in AT drives */
#define UFT_RPM_394             394     /**< Mac 3.5" outer tracks */
#define UFT_RPM_590             590     /**< Mac 3.5" inner tracks */

/*===========================================================================
 * Data Rates (kbit/s)
 *===========================================================================*/

#define UFT_RATE_125            125     /**< FM Single Density */
#define UFT_RATE_250            250     /**< MFM Double Density */
#define UFT_RATE_300            300     /**< 5.25" HD in 360 RPM drive */
#define UFT_RATE_500            500     /**< MFM High Density */
#define UFT_RATE_1000           1000    /**< Extended Density */

/*===========================================================================
 * Tracks Per Inch (TPI)
 *===========================================================================*/

#define UFT_TPI_48              48      /**< Standard 5.25" DD */
#define UFT_TPI_96              96      /**< 5.25" QD/HD, 3.5" */
#define UFT_TPI_100             100     /**< Some 8" formats */
#define UFT_TPI_135             135     /**< 3.5" HD/ED */

/*===========================================================================
 * Sector Size Codes (IBM-style)
 *===========================================================================*/

#define UFT_SECSIZE_128         0       /**< 128 bytes */
#define UFT_SECSIZE_256         1       /**< 256 bytes */
#define UFT_SECSIZE_512         2       /**< 512 bytes */
#define UFT_SECSIZE_1024        3       /**< 1024 bytes */
#define UFT_SECSIZE_2048        4       /**< 2048 bytes */
#define UFT_SECSIZE_4096        5       /**< 4096 bytes */
#define UFT_SECSIZE_8192        6       /**< 8192 bytes */
#define UFT_SECSIZE_16384       7       /**< 16384 bytes */

/** Convert size code to bytes */
#define UFT_SECSIZE_TO_BYTES(code)  (128U << (code))

/** Convert bytes to size code */
static inline uint8_t uft_bytes_to_secsize(uint16_t bytes)
{
    uint8_t code = 0;
    while (bytes > 128 && code < 7) {
        bytes >>= 1;
        code++;
    }
    return code;
}

/*===========================================================================
 * Disk Geometry Structure
 *===========================================================================*/

/**
 * @brief Complete floppy disk geometry specification
 */
typedef struct {
    /* Physical characteristics */
    uft_media_size_t media_size;    /**< Physical media size */
    uft_density_t    density;       /**< Recording density */
    uft_encoding_t   encoding;      /**< Data encoding method */
    uft_sectoring_t  sectoring;     /**< Hard or soft sectored */
    
    /* Geometry */
    uint8_t  sides;                 /**< Number of sides (1 or 2) */
    uint8_t  tracks;                /**< Tracks per side */
    uint8_t  sectors;               /**< Sectors per track (0 = variable) */
    uint16_t sector_size;           /**< Bytes per sector */
    
    /* Timing */
    uint16_t rpm;                   /**< Rotation speed */
    uint16_t data_rate;             /**< Data rate in kbit/s */
    uint8_t  tpi;                   /**< Tracks per inch */
    
    /* Capacity */
    uint32_t raw_capacity;          /**< Raw unformatted capacity */
    uint32_t formatted_capacity;    /**< Formatted capacity */
    
    /* Platform info */
    const char *name;               /**< Format name */
    const char *platform;           /**< Platform/system name */
} uft_floppy_geometry_t;

/*===========================================================================
 * IBM PC Compatible Formats
 *===========================================================================*/

/** 5.25" 160KB (PC DOS 1.0) */
#define UFT_GEOM_PC_160K { \
    .media_size = UFT_MEDIA_5_25_INCH, .density = UFT_DENSITY_DOUBLE, \
    .encoding = UFT_ENCODING_MFM, .sectoring = UFT_SECTOR_SOFT, \
    .sides = 1, .tracks = 40, .sectors = 8, .sector_size = 512, \
    .rpm = 300, .data_rate = 250, .tpi = 48, \
    .raw_capacity = 250000, .formatted_capacity = 163840, \
    .name = "PC 160K", .platform = "IBM PC" }

/** 5.25" 180KB (PC DOS 2.0) */
#define UFT_GEOM_PC_180K { \
    .media_size = UFT_MEDIA_5_25_INCH, .density = UFT_DENSITY_DOUBLE, \
    .encoding = UFT_ENCODING_MFM, .sectoring = UFT_SECTOR_SOFT, \
    .sides = 1, .tracks = 40, .sectors = 9, .sector_size = 512, \
    .rpm = 300, .data_rate = 250, .tpi = 48, \
    .raw_capacity = 250000, .formatted_capacity = 184320, \
    .name = "PC 180K", .platform = "IBM PC" }

/** 5.25" 320KB (PC DOS 1.1) */
#define UFT_GEOM_PC_320K { \
    .media_size = UFT_MEDIA_5_25_INCH, .density = UFT_DENSITY_DOUBLE, \
    .encoding = UFT_ENCODING_MFM, .sectoring = UFT_SECTOR_SOFT, \
    .sides = 2, .tracks = 40, .sectors = 8, .sector_size = 512, \
    .rpm = 300, .data_rate = 250, .tpi = 48, \
    .raw_capacity = 500000, .formatted_capacity = 327680, \
    .name = "PC 320K", .platform = "IBM PC" }

/** 5.25" 360KB (PC DOS 2.0) */
#define UFT_GEOM_PC_360K { \
    .media_size = UFT_MEDIA_5_25_INCH, .density = UFT_DENSITY_DOUBLE, \
    .encoding = UFT_ENCODING_MFM, .sectoring = UFT_SECTOR_SOFT, \
    .sides = 2, .tracks = 40, .sectors = 9, .sector_size = 512, \
    .rpm = 300, .data_rate = 250, .tpi = 48, \
    .raw_capacity = 500000, .formatted_capacity = 368640, \
    .name = "PC 360K", .platform = "IBM PC" }

/** 5.25" 1.2MB (PC AT) */
#define UFT_GEOM_PC_1200K { \
    .media_size = UFT_MEDIA_5_25_INCH, .density = UFT_DENSITY_HIGH, \
    .encoding = UFT_ENCODING_MFM, .sectoring = UFT_SECTOR_SOFT, \
    .sides = 2, .tracks = 80, .sectors = 15, .sector_size = 512, \
    .rpm = 360, .data_rate = 500, .tpi = 96, \
    .raw_capacity = 1000000, .formatted_capacity = 1228800, \
    .name = "PC 1.2M", .platform = "IBM PC AT" }

/** 3.5" 720KB */
#define UFT_GEOM_PC_720K { \
    .media_size = UFT_MEDIA_3_5_INCH, .density = UFT_DENSITY_DOUBLE, \
    .encoding = UFT_ENCODING_MFM, .sectoring = UFT_SECTOR_SOFT, \
    .sides = 2, .tracks = 80, .sectors = 9, .sector_size = 512, \
    .rpm = 300, .data_rate = 250, .tpi = 135, \
    .raw_capacity = 500000, .formatted_capacity = 737280, \
    .name = "PC 720K", .platform = "IBM PC" }

/** 3.5" 1.44MB */
#define UFT_GEOM_PC_1440K { \
    .media_size = UFT_MEDIA_3_5_INCH, .density = UFT_DENSITY_HIGH, \
    .encoding = UFT_ENCODING_MFM, .sectoring = UFT_SECTOR_SOFT, \
    .sides = 2, .tracks = 80, .sectors = 18, .sector_size = 512, \
    .rpm = 300, .data_rate = 500, .tpi = 135, \
    .raw_capacity = 1000000, .formatted_capacity = 1474560, \
    .name = "PC 1.44M", .platform = "IBM PC" }

/** 3.5" 2.88MB (Extended Density) */
#define UFT_GEOM_PC_2880K { \
    .media_size = UFT_MEDIA_3_5_INCH, .density = UFT_DENSITY_EXTENDED, \
    .encoding = UFT_ENCODING_MFM, .sectoring = UFT_SECTOR_SOFT, \
    .sides = 2, .tracks = 80, .sectors = 36, .sector_size = 512, \
    .rpm = 300, .data_rate = 1000, .tpi = 135, \
    .raw_capacity = 2000000, .formatted_capacity = 2949120, \
    .name = "PC 2.88M", .platform = "IBM PC" }

/** DMF 1.68MB (Microsoft Distribution Media Format) */
#define UFT_GEOM_PC_DMF { \
    .media_size = UFT_MEDIA_3_5_INCH, .density = UFT_DENSITY_HIGH, \
    .encoding = UFT_ENCODING_MFM, .sectoring = UFT_SECTOR_SOFT, \
    .sides = 2, .tracks = 80, .sectors = 21, .sector_size = 512, \
    .rpm = 300, .data_rate = 500, .tpi = 135, \
    .raw_capacity = 1000000, .formatted_capacity = 1720320, \
    .name = "DMF 1.68M", .platform = "Microsoft" }

/*===========================================================================
 * 8-inch IBM Formats
 *===========================================================================*/

/** 8" Single Density (IBM 3740) */
#define UFT_GEOM_IBM_8_SD { \
    .media_size = UFT_MEDIA_8_INCH, .density = UFT_DENSITY_SINGLE, \
    .encoding = UFT_ENCODING_FM, .sectoring = UFT_SECTOR_SOFT, \
    .sides = 1, .tracks = 77, .sectors = 26, .sector_size = 128, \
    .rpm = 360, .data_rate = 125, .tpi = 48, \
    .raw_capacity = 400000, .formatted_capacity = 256256, \
    .name = "IBM 3740", .platform = "IBM" }

/** 8" Double Density (IBM System/34) */
#define UFT_GEOM_IBM_8_DD { \
    .media_size = UFT_MEDIA_8_INCH, .density = UFT_DENSITY_DOUBLE, \
    .encoding = UFT_ENCODING_MFM, .sectoring = UFT_SECTOR_SOFT, \
    .sides = 2, .tracks = 77, .sectors = 26, .sector_size = 256, \
    .rpm = 360, .data_rate = 250, .tpi = 48, \
    .raw_capacity = 1600000, .formatted_capacity = 1025024, \
    .name = "IBM System/34", .platform = "IBM" }

/*===========================================================================
 * Apple II Formats
 *===========================================================================*/

/** Apple II 5.25" (DOS 3.2, 13 sectors) */
#define UFT_GEOM_APPLE_II_DOS32 { \
    .media_size = UFT_MEDIA_5_25_INCH, .density = UFT_DENSITY_SINGLE, \
    .encoding = UFT_ENCODING_GCR_APPLE, .sectoring = UFT_SECTOR_SOFT, \
    .sides = 1, .tracks = 35, .sectors = 13, .sector_size = 256, \
    .rpm = 300, .data_rate = 250, .tpi = 48, \
    .raw_capacity = 250000, .formatted_capacity = 116480, \
    .name = "Apple DOS 3.2", .platform = "Apple II" }

/** Apple II 5.25" (DOS 3.3, 16 sectors) */
#define UFT_GEOM_APPLE_II_DOS33 { \
    .media_size = UFT_MEDIA_5_25_INCH, .density = UFT_DENSITY_SINGLE, \
    .encoding = UFT_ENCODING_GCR_APPLE, .sectoring = UFT_SECTOR_SOFT, \
    .sides = 1, .tracks = 35, .sectors = 16, .sector_size = 256, \
    .rpm = 300, .data_rate = 250, .tpi = 48, \
    .raw_capacity = 250000, .formatted_capacity = 143360, \
    .name = "Apple DOS 3.3", .platform = "Apple II" }

/** Apple IIgs 3.5" 800KB (ProDOS) */
#define UFT_GEOM_APPLE_IIGS_800K { \
    .media_size = UFT_MEDIA_3_5_INCH, .density = UFT_DENSITY_DOUBLE, \
    .encoding = UFT_ENCODING_GCR_APPLE, .sectoring = UFT_SECTOR_SOFT, \
    .sides = 2, .tracks = 80, .sectors = 0, .sector_size = 512, \
    .rpm = 394, .data_rate = 250, .tpi = 135, \
    .raw_capacity = 1000000, .formatted_capacity = 819200, \
    .name = "Apple 800K", .platform = "Apple IIgs/Mac" }

/*===========================================================================
 * Macintosh Formats
 *===========================================================================*/

/** Macintosh 3.5" 400KB (GCR, variable speed) */
#define UFT_GEOM_MAC_400K { \
    .media_size = UFT_MEDIA_3_5_INCH, .density = UFT_DENSITY_DOUBLE, \
    .encoding = UFT_ENCODING_GCR, .sectoring = UFT_SECTOR_SOFT, \
    .sides = 1, .tracks = 80, .sectors = 0, .sector_size = 512, \
    .rpm = 394, .data_rate = 250, .tpi = 135, \
    .raw_capacity = 500000, .formatted_capacity = 409600, \
    .name = "Mac 400K", .platform = "Macintosh" }

/** Macintosh 3.5" 800KB (GCR, variable speed) */
#define UFT_GEOM_MAC_800K { \
    .media_size = UFT_MEDIA_3_5_INCH, .density = UFT_DENSITY_DOUBLE, \
    .encoding = UFT_ENCODING_GCR, .sectoring = UFT_SECTOR_SOFT, \
    .sides = 2, .tracks = 80, .sectors = 0, .sector_size = 512, \
    .rpm = 394, .data_rate = 250, .tpi = 135, \
    .raw_capacity = 1000000, .formatted_capacity = 819200, \
    .name = "Mac 800K", .platform = "Macintosh" }

/** Macintosh 3.5" 1.44MB (MFM, PC-compatible) */
#define UFT_GEOM_MAC_1440K { \
    .media_size = UFT_MEDIA_3_5_INCH, .density = UFT_DENSITY_HIGH, \
    .encoding = UFT_ENCODING_MFM, .sectoring = UFT_SECTOR_SOFT, \
    .sides = 2, .tracks = 80, .sectors = 18, .sector_size = 512, \
    .rpm = 300, .data_rate = 500, .tpi = 135, \
    .raw_capacity = 1000000, .formatted_capacity = 1474560, \
    .name = "Mac 1.44M", .platform = "Macintosh" }

/*===========================================================================
 * Commodore Formats
 *===========================================================================*/

/** Commodore 1541 5.25" (GCR, zone bit recording) */
#define UFT_GEOM_C64_1541 { \
    .media_size = UFT_MEDIA_5_25_INCH, .density = UFT_DENSITY_SINGLE, \
    .encoding = UFT_ENCODING_GCR_C64, .sectoring = UFT_SECTOR_SOFT, \
    .sides = 1, .tracks = 35, .sectors = 0, .sector_size = 256, \
    .rpm = 300, .data_rate = 250, .tpi = 48, \
    .raw_capacity = 250000, .formatted_capacity = 174848, \
    .name = "C64 1541", .platform = "Commodore 64" }

/** Commodore 1571 5.25" (Double-sided 1541) */
#define UFT_GEOM_C128_1571 { \
    .media_size = UFT_MEDIA_5_25_INCH, .density = UFT_DENSITY_DOUBLE, \
    .encoding = UFT_ENCODING_GCR_C64, .sectoring = UFT_SECTOR_SOFT, \
    .sides = 2, .tracks = 35, .sectors = 0, .sector_size = 256, \
    .rpm = 300, .data_rate = 250, .tpi = 48, \
    .raw_capacity = 500000, .formatted_capacity = 349696, \
    .name = "C128 1571", .platform = "Commodore 128" }

/** Commodore 1581 3.5" 800KB (MFM) */
#define UFT_GEOM_C128_1581 { \
    .media_size = UFT_MEDIA_3_5_INCH, .density = UFT_DENSITY_DOUBLE, \
    .encoding = UFT_ENCODING_MFM, .sectoring = UFT_SECTOR_SOFT, \
    .sides = 2, .tracks = 80, .sectors = 10, .sector_size = 512, \
    .rpm = 300, .data_rate = 250, .tpi = 135, \
    .raw_capacity = 1000000, .formatted_capacity = 819200, \
    .name = "C128 1581", .platform = "Commodore 128" }

/*===========================================================================
 * Amiga Formats
 *===========================================================================*/

/** Amiga 3.5" 880KB (11 sectors, no gaps) */
#define UFT_GEOM_AMIGA_DD { \
    .media_size = UFT_MEDIA_3_5_INCH, .density = UFT_DENSITY_DOUBLE, \
    .encoding = UFT_ENCODING_MFM, .sectoring = UFT_SECTOR_SOFT, \
    .sides = 2, .tracks = 80, .sectors = 11, .sector_size = 512, \
    .rpm = 300, .data_rate = 250, .tpi = 135, \
    .raw_capacity = 1000000, .formatted_capacity = 901120, \
    .name = "Amiga DD", .platform = "Amiga" }

/** Amiga 3.5" 1.76MB (22 sectors HD) */
#define UFT_GEOM_AMIGA_HD { \
    .media_size = UFT_MEDIA_3_5_INCH, .density = UFT_DENSITY_HIGH, \
    .encoding = UFT_ENCODING_MFM, .sectoring = UFT_SECTOR_SOFT, \
    .sides = 2, .tracks = 80, .sectors = 22, .sector_size = 512, \
    .rpm = 300, .data_rate = 500, .tpi = 135, \
    .raw_capacity = 2000000, .formatted_capacity = 1802240, \
    .name = "Amiga HD", .platform = "Amiga" }

/*===========================================================================
 * Atari ST Formats
 *===========================================================================*/

/** Atari ST 3.5" 360KB (9 sectors SS) */
#define UFT_GEOM_ATARI_ST_SS { \
    .media_size = UFT_MEDIA_3_5_INCH, .density = UFT_DENSITY_DOUBLE, \
    .encoding = UFT_ENCODING_MFM, .sectoring = UFT_SECTOR_SOFT, \
    .sides = 1, .tracks = 80, .sectors = 9, .sector_size = 512, \
    .rpm = 300, .data_rate = 250, .tpi = 135, \
    .raw_capacity = 500000, .formatted_capacity = 368640, \
    .name = "Atari ST SS", .platform = "Atari ST" }

/** Atari ST 3.5" 720KB (9 sectors DS) */
#define UFT_GEOM_ATARI_ST_DS { \
    .media_size = UFT_MEDIA_3_5_INCH, .density = UFT_DENSITY_DOUBLE, \
    .encoding = UFT_ENCODING_MFM, .sectoring = UFT_SECTOR_SOFT, \
    .sides = 2, .tracks = 80, .sectors = 9, .sector_size = 512, \
    .rpm = 300, .data_rate = 250, .tpi = 135, \
    .raw_capacity = 1000000, .formatted_capacity = 737280, \
    .name = "Atari ST DS", .platform = "Atari ST" }

/*===========================================================================
 * Atari 8-bit Formats
 *===========================================================================*/

/** Atari 810 5.25" 90KB (18 sectors, 128 bytes) */
#define UFT_GEOM_ATARI_810 { \
    .media_size = UFT_MEDIA_5_25_INCH, .density = UFT_DENSITY_SINGLE, \
    .encoding = UFT_ENCODING_FM, .sectoring = UFT_SECTOR_SOFT, \
    .sides = 1, .tracks = 40, .sectors = 18, .sector_size = 128, \
    .rpm = 288, .data_rate = 125, .tpi = 48, \
    .raw_capacity = 125000, .formatted_capacity = 92160, \
    .name = "Atari 810", .platform = "Atari 8-bit" }

/** Atari 1050 5.25" 130KB (Enhanced Density) */
#define UFT_GEOM_ATARI_1050 { \
    .media_size = UFT_MEDIA_5_25_INCH, .density = UFT_DENSITY_SINGLE, \
    .encoding = UFT_ENCODING_FM, .sectoring = UFT_SECTOR_SOFT, \
    .sides = 1, .tracks = 40, .sectors = 26, .sector_size = 128, \
    .rpm = 288, .data_rate = 125, .tpi = 48, \
    .raw_capacity = 166000, .formatted_capacity = 133120, \
    .name = "Atari 1050", .platform = "Atari 8-bit" }

/*===========================================================================
 * BBC Micro Formats
 *===========================================================================*/

/** BBC Micro DFS 5.25" 100KB (SS/40) */
#define UFT_GEOM_BBC_SS40 { \
    .media_size = UFT_MEDIA_5_25_INCH, .density = UFT_DENSITY_SINGLE, \
    .encoding = UFT_ENCODING_FM, .sectoring = UFT_SECTOR_SOFT, \
    .sides = 1, .tracks = 40, .sectors = 10, .sector_size = 256, \
    .rpm = 300, .data_rate = 125, .tpi = 48, \
    .raw_capacity = 125000, .formatted_capacity = 102400, \
    .name = "BBC DFS SS/40", .platform = "BBC Micro" }

/** BBC Micro DFS 5.25" 200KB (SS/80) */
#define UFT_GEOM_BBC_SS80 { \
    .media_size = UFT_MEDIA_5_25_INCH, .density = UFT_DENSITY_DOUBLE, \
    .encoding = UFT_ENCODING_MFM, .sectoring = UFT_SECTOR_SOFT, \
    .sides = 1, .tracks = 80, .sectors = 10, .sector_size = 256, \
    .rpm = 300, .data_rate = 250, .tpi = 96, \
    .raw_capacity = 250000, .formatted_capacity = 204800, \
    .name = "BBC DFS SS/80", .platform = "BBC Micro" }

/*===========================================================================
 * NEC PC-98 Formats (Japanese)
 *===========================================================================*/

/** NEC PC-98 5.25" 1.25MB (8 sectors, 1024 bytes) */
#define UFT_GEOM_PC98_1232K { \
    .media_size = UFT_MEDIA_5_25_INCH, .density = UFT_DENSITY_HIGH, \
    .encoding = UFT_ENCODING_MFM, .sectoring = UFT_SECTOR_SOFT, \
    .sides = 2, .tracks = 77, .sectors = 8, .sector_size = 1024, \
    .rpm = 360, .data_rate = 500, .tpi = 96, \
    .raw_capacity = 1000000, .formatted_capacity = 1261568, \
    .name = "PC-98 1.25M", .platform = "NEC PC-98" }

/** NEC PC-98 3.5" 1.44MB (3-mode) */
#define UFT_GEOM_PC98_1440K { \
    .media_size = UFT_MEDIA_3_5_INCH, .density = UFT_DENSITY_HIGH, \
    .encoding = UFT_ENCODING_MFM, .sectoring = UFT_SECTOR_SOFT, \
    .sides = 2, .tracks = 80, .sectors = 18, .sector_size = 512, \
    .rpm = 300, .data_rate = 500, .tpi = 135, \
    .raw_capacity = 1000000, .formatted_capacity = 1474560, \
    .name = "PC-98 1.44M", .platform = "NEC PC-98" }

/*===========================================================================
 * Amstrad/Schneider Formats
 *===========================================================================*/

/** Amstrad CPC 3" 180KB (Data format) */
#define UFT_GEOM_AMSTRAD_DATA { \
    .media_size = UFT_MEDIA_3_INCH, .density = UFT_DENSITY_SINGLE, \
    .encoding = UFT_ENCODING_MFM, .sectoring = UFT_SECTOR_SOFT, \
    .sides = 1, .tracks = 40, .sectors = 9, .sector_size = 512, \
    .rpm = 300, .data_rate = 250, .tpi = 96, \
    .raw_capacity = 250000, .formatted_capacity = 184320, \
    .name = "Amstrad Data", .platform = "Amstrad CPC" }

/** Amstrad CPC 3" 180KB (System format) */
#define UFT_GEOM_AMSTRAD_SYSTEM { \
    .media_size = UFT_MEDIA_3_INCH, .density = UFT_DENSITY_SINGLE, \
    .encoding = UFT_ENCODING_MFM, .sectoring = UFT_SECTOR_SOFT, \
    .sides = 1, .tracks = 40, .sectors = 9, .sector_size = 512, \
    .rpm = 300, .data_rate = 250, .tpi = 96, \
    .raw_capacity = 250000, .formatted_capacity = 178688, \
    .name = "Amstrad System", .platform = "Amstrad CPC" }

/*===========================================================================
 * Helper Functions
 *===========================================================================*/

/**
 * @brief Calculate formatted capacity from geometry
 */
static inline uint32_t uft_geom_capacity(const uft_floppy_geometry_t *geom)
{
    if (!geom) return 0;
    return (uint32_t)geom->sides * geom->tracks * 
           geom->sectors * geom->sector_size;
}

/**
 * @brief Calculate track size in bytes
 */
static inline uint32_t uft_geom_track_size(const uft_floppy_geometry_t *geom)
{
    if (!geom) return 0;
    return (uint32_t)geom->sectors * geom->sector_size;
}

/**
 * @brief Get encoding name
 */
const char *uft_encoding_name(uft_encoding_t encoding);

/**
 * @brief Get density name
 */
const char *uft_density_name(uft_density_t density);

/**
 * @brief Find geometry by capacity and media size
 */
const uft_floppy_geometry_t *uft_find_geometry(uint32_t capacity, 
                                                uft_media_size_t media);

#ifdef __cplusplus
}
#endif

#endif /* UFT_FLOPPY_GEOMETRY_H */
