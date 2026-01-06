/**
 * @file uft_floppy_formats.h
 * @brief Floppy Disk Format Database
 * 
 * Complete database of floppy disk formats from Wikipedia
 * "List of floppy disk formats" and related sources.
 * 
 * Includes format definitions for:
 * - IBM PC and compatibles
 * - Apple II, Macintosh
 * - Commodore 64/128, Amiga
 * - Atari 8-bit, ST
 * - BBC Micro, Amstrad
 * - NEC PC-98
 * - MSX, TRS-80
 * - CP/M systems
 * - And many more...
 */

#ifndef UFT_FLOPPY_FORMATS_H
#define UFT_FLOPPY_FORMATS_H

#include "uft_floppy_geometry.h"
#include "uft_floppy_encoding.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Format Identification
 *===========================================================================*/

/**
 * @brief Floppy disk format identifier
 */
typedef enum {
    /* Unknown/Invalid */
    UFT_FMT_UNKNOWN         = 0,
    
    /* IBM PC Formats (1-99) */
    UFT_FMT_PC_160K         = 1,    /**< 5.25" SS/DD 8 sectors */
    UFT_FMT_PC_180K         = 2,    /**< 5.25" SS/DD 9 sectors */
    UFT_FMT_PC_320K         = 3,    /**< 5.25" DS/DD 8 sectors */
    UFT_FMT_PC_360K         = 4,    /**< 5.25" DS/DD 9 sectors */
    UFT_FMT_PC_720K         = 5,    /**< 3.5" DS/DD */
    UFT_FMT_PC_1200K        = 6,    /**< 5.25" DS/HD */
    UFT_FMT_PC_1440K        = 7,    /**< 3.5" DS/HD */
    UFT_FMT_PC_2880K        = 8,    /**< 3.5" DS/ED */
    UFT_FMT_PC_DMF_1680K    = 9,    /**< DMF 21 sectors */
    UFT_FMT_PC_DMF_1720K    = 10,   /**< DMF variant */
    UFT_FMT_PC_XDF_1840K    = 11,   /**< XDF format */
    
    /* IBM 8-inch (100-119) */
    UFT_FMT_IBM8_SSSD       = 100,  /**< 8" SS/SD (3740) */
    UFT_FMT_IBM8_SSDD       = 101,  /**< 8" SS/DD */
    UFT_FMT_IBM8_DSSD       = 102,  /**< 8" DS/SD */
    UFT_FMT_IBM8_DSDD       = 103,  /**< 8" DS/DD (System/34) */
    
    /* Apple II (120-139) */
    UFT_FMT_APPLE_DOS32     = 120,  /**< DOS 3.2 (13 sectors) */
    UFT_FMT_APPLE_DOS33     = 121,  /**< DOS 3.3 (16 sectors) */
    UFT_FMT_APPLE_PRODOS    = 122,  /**< ProDOS 5.25" */
    UFT_FMT_APPLE_35_400K   = 123,  /**< 3.5" 400K */
    UFT_FMT_APPLE_35_800K   = 124,  /**< 3.5" 800K */
    UFT_FMT_APPLE_35_1440K  = 125,  /**< 3.5" 1.44MB */
    
    /* Macintosh (140-159) */
    UFT_FMT_MAC_400K        = 140,  /**< Mac 400K GCR */
    UFT_FMT_MAC_800K        = 141,  /**< Mac 800K GCR */
    UFT_FMT_MAC_1440K       = 142,  /**< Mac 1.44MB MFM */
    
    /* Commodore 64/128 (160-179) */
    UFT_FMT_C64_1541        = 160,  /**< 1541 170K */
    UFT_FMT_C64_1541_40     = 161,  /**< 1541 40-track */
    UFT_FMT_C128_1571       = 162,  /**< 1571 340K */
    UFT_FMT_C128_1581       = 163,  /**< 1581 800K */
    UFT_FMT_C64_GEOS        = 164,  /**< GEOS format */
    
    /* Amiga (180-199) */
    UFT_FMT_AMIGA_DD        = 180,  /**< Amiga 880K DD */
    UFT_FMT_AMIGA_HD        = 181,  /**< Amiga 1.76MB HD */
    UFT_FMT_AMIGA_FFS       = 182,  /**< Amiga FFS */
    UFT_FMT_AMIGA_OFS       = 183,  /**< Amiga OFS */
    
    /* Atari 8-bit (200-219) */
    UFT_FMT_ATARI_810       = 200,  /**< 810 SD 90K */
    UFT_FMT_ATARI_1050      = 201,  /**< 1050 ED 130K */
    UFT_FMT_ATARI_1050_DD   = 202,  /**< 1050 DD 180K */
    UFT_FMT_ATARI_XF551_DD  = 203,  /**< XF551 DD 360K */
    
    /* Atari ST (220-239) */
    UFT_FMT_ATARI_ST_SS     = 220,  /**< ST SS 360K */
    UFT_FMT_ATARI_ST_DS     = 221,  /**< ST DS 720K */
    UFT_FMT_ATARI_ST_HD     = 222,  /**< ST HD 1.44MB */
    UFT_FMT_ATARI_ST_ED     = 223,  /**< ST ED (rare) */
    
    /* BBC Micro (240-259) */
    UFT_FMT_BBC_DFS_SS40    = 240,  /**< DFS SS/40 100K */
    UFT_FMT_BBC_DFS_SS80    = 241,  /**< DFS SS/80 200K */
    UFT_FMT_BBC_DFS_DS40    = 242,  /**< DFS DS/40 200K */
    UFT_FMT_BBC_DFS_DS80    = 243,  /**< DFS DS/80 400K */
    UFT_FMT_BBC_ADFS_S      = 244,  /**< ADFS S 160K */
    UFT_FMT_BBC_ADFS_M      = 245,  /**< ADFS M 320K */
    UFT_FMT_BBC_ADFS_L      = 246,  /**< ADFS L 640K */
    
    /* Amstrad CPC (260-279) */
    UFT_FMT_AMSTRAD_DATA    = 260,  /**< Data format 180K */
    UFT_FMT_AMSTRAD_SYSTEM  = 261,  /**< System format */
    UFT_FMT_AMSTRAD_PCW     = 262,  /**< PCW format */
    
    /* MSX (280-299) */
    UFT_FMT_MSX_360K        = 280,  /**< MSX 360K */
    UFT_FMT_MSX_720K        = 281,  /**< MSX 720K */
    
    /* NEC PC-98 (300-319) */
    UFT_FMT_PC98_640K       = 300,  /**< PC-98 640K (2DD) */
    UFT_FMT_PC98_1232K      = 301,  /**< PC-98 1.25MB (2HD) */
    UFT_FMT_PC98_1440K      = 302,  /**< PC-98 1.44MB */
    
    /* TRS-80 (320-339) */
    UFT_FMT_TRS80_SSSD      = 320,  /**< Model I SS/SD */
    UFT_FMT_TRS80_SSDD      = 321,  /**< Model I SS/DD */
    UFT_FMT_TRS80_DSDD      = 322,  /**< Model III/4 DS/DD */
    
    /* CP/M (340-359) */
    UFT_FMT_CPM_8_SSSD      = 340,  /**< CP/M 8" SS/SD */
    UFT_FMT_CPM_8_DSDD      = 341,  /**< CP/M 8" DS/DD */
    UFT_FMT_CPM_525_SSDD    = 342,  /**< CP/M 5.25" SS/DD */
    UFT_FMT_CPM_525_DSDD    = 343,  /**< CP/M 5.25" DS/DD */
    
    /* Osborne/Kaypro (360-379) */
    UFT_FMT_OSBORNE_SSSD    = 360,  /**< Osborne 1 */
    UFT_FMT_OSBORNE_SSDD    = 361,  /**< Osborne DD */
    UFT_FMT_KAYPRO_SSDD     = 362,  /**< Kaypro II */
    UFT_FMT_KAYPRO_DSDD     = 363,  /**< Kaypro 4 */
    
    /* Sinclair/Spectrum (380-399) */
    UFT_FMT_SPECTRUM_PLUS3  = 380,  /**< +3 173K */
    UFT_FMT_SPECTRUM_PLUS3B = 381,  /**< +3 203K (B side) */
    UFT_FMT_QL_DD           = 382,  /**< Sinclair QL */
    
    /* Sharp (400-419) */
    UFT_FMT_SHARP_X68000    = 400,  /**< X68000 */
    UFT_FMT_SHARP_MZ        = 401,  /**< MZ series */
    
    /* Japanese (420-439) */
    UFT_FMT_FM_TOWNS        = 420,  /**< FM Towns */
    UFT_FMT_PC88            = 421,  /**< NEC PC-88 */
    
    /* Victor/Sirius (440-449) */
    UFT_FMT_VICTOR_9000     = 440,  /**< Victor 9000 (GCR) */
    
    /* DEC (450-459) */
    UFT_FMT_DEC_RX01        = 450,  /**< RX01 8" */
    UFT_FMT_DEC_RX02        = 451,  /**< RX02 8" */
    UFT_FMT_DEC_RX50        = 452,  /**< RX50 5.25" */
    
    /* Other (500+) */
    UFT_FMT_SEGA_SF7000     = 500,  /**< Sega SF-7000 */
    UFT_FMT_TANDY_COCO      = 501,  /**< TRS-80 CoCo */
    UFT_FMT_HP_9114         = 502,  /**< HP 9114 */
    
    UFT_FMT_MAX             = 999
} uft_format_id_t;

/*===========================================================================
 * Format Descriptor Structure
 *===========================================================================*/

/**
 * @brief Complete format descriptor
 */
typedef struct {
    uft_format_id_t         id;         /**< Format identifier */
    uft_floppy_geometry_t   geometry;   /**< Physical geometry */
    
    /* Filesystem info */
    const char *filesystem;             /**< Default filesystem */
    uint16_t    dir_entries;            /**< Directory entries */
    uint16_t    reserved_tracks;        /**< Reserved system tracks */
    
    /* Interleave and skew */
    uint8_t     interleave;             /**< Sector interleave */
    uint8_t     head_skew;              /**< Head skew */
    uint8_t     track_skew;             /**< Track skew */
    
    /* First sector number */
    uint8_t     first_sector;           /**< First sector number (0 or 1) */
    
    /* Gap sizes */
    uint8_t     gap1;                   /**< Post-index gap */
    uint8_t     gap2;                   /**< ID to data gap */
    uint8_t     gap3;                   /**< Inter-sector gap */
} uft_format_desc_t;

/*===========================================================================
 * Format Database Access
 *===========================================================================*/

/**
 * @brief Get format descriptor by ID
 */
const uft_format_desc_t *uft_format_get(uft_format_id_t id);

/**
 * @brief Get format descriptor by name
 */
const uft_format_desc_t *uft_format_find_by_name(const char *name);

/**
 * @brief Get format by geometry match
 */
uft_format_id_t uft_format_detect(uint32_t size, uint8_t sides,
                                   uint8_t tracks, uint8_t sectors,
                                   uint16_t sector_size);

/**
 * @brief Get number of registered formats
 */
int uft_format_count(void);

/**
 * @brief Iterate all formats
 */
typedef void (*uft_format_callback_t)(const uft_format_desc_t *fmt, void *ctx);
void uft_format_foreach(uft_format_callback_t callback, void *ctx);

/*===========================================================================
 * Format Detection from Image
 *===========================================================================*/

/**
 * @brief Analyze disk image and detect format
 * @param data Image data
 * @param size Image size
 * @param detected Output: detected format ID
 * @param confidence Output: confidence level (0-100)
 * @return 0 on success
 */
int uft_format_analyze(const uint8_t *data, size_t size,
                       uft_format_id_t *detected, int *confidence);

/**
 * @brief Get all possible formats for given image size
 * @param size Image size in bytes
 * @param formats Output array of format IDs
 * @param max_formats Maximum entries in array
 * @return Number of matching formats
 */
int uft_format_candidates(size_t size, uft_format_id_t *formats,
                          int max_formats);

/*===========================================================================
 * Common Image Sizes
 *===========================================================================*/

#define UFT_SIZE_160K           163840      /**< PC 160KB */
#define UFT_SIZE_180K           184320      /**< PC 180KB */
#define UFT_SIZE_320K           327680      /**< PC 320KB */
#define UFT_SIZE_360K           368640      /**< PC 360KB */
#define UFT_SIZE_400K           409600      /**< Mac 400KB */
#define UFT_SIZE_720K           737280      /**< PC 720KB */
#define UFT_SIZE_800K           819200      /**< Mac/Amiga 800KB */
#define UFT_SIZE_880K           901120      /**< Amiga 880KB */
#define UFT_SIZE_1200K          1228800     /**< PC 1.2MB */
#define UFT_SIZE_1440K          1474560     /**< PC 1.44MB */
#define UFT_SIZE_1680K          1720320     /**< DMF 1.68MB */
#define UFT_SIZE_1760K          1802240     /**< Amiga 1.76MB */
#define UFT_SIZE_2880K          2949120     /**< PC 2.88MB */

/* Atari 8-bit */
#define UFT_SIZE_ATARI_SD       92160       /**< Atari SD 90KB */
#define UFT_SIZE_ATARI_ED       133120      /**< Atari ED 130KB */
#define UFT_SIZE_ATARI_DD       184320      /**< Atari DD 180KB */

/* Commodore */
#define UFT_SIZE_C64_1541       174848      /**< C64 1541 170KB */
#define UFT_SIZE_C128_1571      349696      /**< C128 1571 340KB */

/* BBC */
#define UFT_SIZE_BBC_SS40       102400      /**< BBC DFS 100KB */
#define UFT_SIZE_BBC_SS80       204800      /**< BBC DFS 200KB */
#define UFT_SIZE_BBC_DS80       409600      /**< BBC DFS 400KB */

/*===========================================================================
 * Sector Numbering Styles
 *===========================================================================*/

typedef enum {
    UFT_SECTOR_NUM_0        = 0,    /**< Sectors start at 0 */
    UFT_SECTOR_NUM_1        = 1,    /**< Sectors start at 1 */
    UFT_SECTOR_NUM_CUSTOM   = 2     /**< Custom numbering */
} uft_sector_numbering_t;

/*===========================================================================
 * Boot Sector Detection
 *===========================================================================*/

/**
 * @brief FAT boot sector signature
 */
#define UFT_FAT_BOOT_SIG        0xAA55

/**
 * @brief Check for FAT boot sector
 */
static inline bool uft_is_fat_boot(const uint8_t *data)
{
    /* Check for 0x55 0xAA at offset 510 */
    return (data[510] == 0x55 && data[511] == 0xAA);
}

/**
 * @brief Check for Amiga boot block
 */
static inline bool uft_is_amiga_boot(const uint8_t *data)
{
    /* Check for "DOS" signature */
    return (data[0] == 'D' && data[1] == 'O' && data[2] == 'S');
}

/**
 * @brief Check for Atari ST boot sector
 */
static inline bool uft_is_atari_st_boot(const uint8_t *data)
{
    /* Check BRA instruction at start */
    return (data[0] == 0x60);
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_FLOPPY_FORMATS_H */
