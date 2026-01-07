/**
 * @file uft_constants.h
 * @brief Unified Constants - All magic numbers in one place
 * 
 * This header consolidates all magic numbers and constants used throughout UFT.
 * Using named constants improves readability, maintainability, and prevents
 * errors from inconsistent values.
 * 
 * Categories:
 * - Disk geometry (tracks, sectors, sides)
 * - Sector sizes
 * - Bitrates and timing
 * - Encoding patterns (MFM, FM, GCR)
 * - Format-specific constants
 * - Buffer sizes
 * 
 * @version 1.0
 * @date 2026-01-07
 */

#ifndef UFT_CONSTANTS_H
#define UFT_CONSTANTS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * DISK GEOMETRY
 *===========================================================================*/

/** @name Track/Cylinder Limits */
/**@{*/
#define UFT_MAX_CYLINDERS           84      /**< Max cylinders (typical 3.5") */
#define UFT_MAX_CYLINDERS_525       42      /**< Max cylinders (5.25") */
#define UFT_MAX_CYLINDERS_8         77      /**< Max cylinders (8") */
#define UFT_MAX_HEADS               2       /**< Max heads (sides) */
#define UFT_MAX_TRACKS              168     /**< Max tracks (84 cyl × 2 heads) */
#define UFT_MAX_TRACKS_EXTENDED     200     /**< Extended track support */
/**@}*/

/** @name Sectors Per Track */
/**@{*/
#define UFT_SPT_FM_SD               16      /**< FM Single Density */
#define UFT_SPT_MFM_DD              9       /**< MFM Double Density (PC) */
#define UFT_SPT_MFM_DD_AMIGA        11      /**< MFM DD Amiga */
#define UFT_SPT_MFM_HD              18      /**< MFM High Density (PC) */
#define UFT_SPT_MFM_HD_AMIGA        22      /**< MFM HD Amiga */
#define UFT_SPT_MFM_ED              36      /**< MFM Extended Density */
#define UFT_SPT_C64_ZONE1           21      /**< C64 Zone 1 (tracks 1-17) */
#define UFT_SPT_C64_ZONE2           19      /**< C64 Zone 2 (tracks 18-24) */
#define UFT_SPT_C64_ZONE3           18      /**< C64 Zone 3 (tracks 25-30) */
#define UFT_SPT_C64_ZONE4           17      /**< C64 Zone 4 (tracks 31-35) */
#define UFT_SPT_APPLE_DOS32         13      /**< Apple II DOS 3.2 */
#define UFT_SPT_APPLE_DOS33         16      /**< Apple II DOS 3.3 */
/**@}*/

/*===========================================================================
 * SECTOR SIZES
 *===========================================================================*/

/** @name Standard Sector Sizes */
/**@{*/
#define UFT_SECTOR_SIZE_128         128     /**< FM Single Density */
#define UFT_SECTOR_SIZE_256         256     /**< C64, Apple II, early PC */
#define UFT_SECTOR_SIZE_512         512     /**< Standard PC/Amiga */
#define UFT_SECTOR_SIZE_1024        1024    /**< Some PC formats */
#define UFT_SECTOR_SIZE_2048        2048    /**< CD-ROM style */
#define UFT_SECTOR_SIZE_4096        4096    /**< Large sectors */
#define UFT_SECTOR_SIZE_8192        8192    /**< Extra large */
#define UFT_SECTOR_SIZE_16384       16384   /**< Maximum standard */
/**@}*/

/** @name Sector Size Codes (IBM) */
/**@{*/
#define UFT_SIZE_CODE_128           0       /**< N=0: 128 bytes */
#define UFT_SIZE_CODE_256           1       /**< N=1: 256 bytes */
#define UFT_SIZE_CODE_512           2       /**< N=2: 512 bytes */
#define UFT_SIZE_CODE_1024          3       /**< N=3: 1024 bytes */
#define UFT_SIZE_CODE_2048          4       /**< N=4: 2048 bytes */
#define UFT_SIZE_CODE_4096          5       /**< N=5: 4096 bytes */
#define UFT_SIZE_CODE_8192          6       /**< N=6: 8192 bytes */
#define UFT_SIZE_CODE_16384         7       /**< N=7: 16384 bytes */
/**@}*/

/** Convert size code to bytes: 128 << code */
#define UFT_SIZE_CODE_TO_BYTES(code) (128U << (code))

/** Convert bytes to size code */
static inline uint8_t uft_bytes_to_size_code(uint16_t bytes) {
    switch (bytes) {
        case 128:   return 0;
        case 256:   return 1;
        case 512:   return 2;
        case 1024:  return 3;
        case 2048:  return 4;
        case 4096:  return 5;
        case 8192:  return 6;
        case 16384: return 7;
        default:    return 2; /* Default to 512 */
    }
}

/*===========================================================================
 * BITRATES AND TIMING
 *===========================================================================*/

/** @name Data Rates (bits per second) */
/**@{*/
#define UFT_RATE_FM_SD              125000  /**< FM Single Density */
#define UFT_RATE_FM_DD              250000  /**< FM Double Density */
#define UFT_RATE_MFM_DD             250000  /**< MFM Double Density */
#define UFT_RATE_MFM_DD_300RPM      300000  /**< MFM DD at 300 RPM */
#define UFT_RATE_MFM_HD             500000  /**< MFM High Density */
#define UFT_RATE_MFM_ED             1000000 /**< MFM Extended Density */
#define UFT_RATE_GCR_C64            250000  /**< C64 GCR (zone dependent) */
#define UFT_RATE_GCR_APPLE          250000  /**< Apple II GCR */
/**@}*/

/** @name Cell Times (nanoseconds) */
/**@{*/
#define UFT_CELL_FM_SD_NS           8000    /**< FM SD: 8µs */
#define UFT_CELL_FM_DD_NS           4000    /**< FM DD: 4µs */
#define UFT_CELL_MFM_DD_NS          2000    /**< MFM DD: 2µs */
#define UFT_CELL_MFM_HD_NS          1000    /**< MFM HD: 1µs */
#define UFT_CELL_MFM_ED_NS          500     /**< MFM ED: 500ns */
/**@}*/

/** @name C64 GCR Zone Bitrates */
/**@{*/
#define UFT_RATE_C64_ZONE1          307692  /**< Zone 1: tracks 1-17 */
#define UFT_RATE_C64_ZONE2          285714  /**< Zone 2: tracks 18-24 */
#define UFT_RATE_C64_ZONE3          266667  /**< Zone 3: tracks 25-30 */
#define UFT_RATE_C64_ZONE4          250000  /**< Zone 4: tracks 31-35 */
/**@}*/

/** @name Rotation Speeds (RPM) */
/**@{*/
#define UFT_RPM_300                 300     /**< 5.25" DD, 3.5" DD */
#define UFT_RPM_360                 360     /**< 5.25" HD */
#define UFT_RPM_C64                 300     /**< C64 1541 */
#define UFT_RPM_APPLE               300     /**< Apple II */
/**@}*/

/** @name Track Length (microseconds at given RPM) */
/**@{*/
#define UFT_TRACK_TIME_300RPM_US    200000  /**< 200ms at 300 RPM */
#define UFT_TRACK_TIME_360RPM_US    166667  /**< 166.67ms at 360 RPM */
/**@}*/

/*===========================================================================
 * ENCODING PATTERNS
 *===========================================================================*/

/** @name MFM Sync Patterns */
/**@{*/
#define UFT_MFM_SYNC_A1             0x4489  /**< A1 with missing clock bit */
#define UFT_MFM_SYNC_C2             0x5224  /**< C2 with missing clock bit */
#define UFT_MFM_SYNC_PATTERN        0x4489  /**< Standard MFM sync */
#define UFT_MFM_IAM_PATTERN         0x5224  /**< Index Address Mark sync */
/**@}*/

/** @name MFM Address Marks */
/**@{*/
#define UFT_MFM_MARK_IAM            0xFC    /**< Index Address Mark */
#define UFT_MFM_MARK_IDAM           0xFE    /**< ID Address Mark */
#define UFT_MFM_MARK_DAM            0xFB    /**< Data Address Mark */
#define UFT_MFM_MARK_DDAM           0xF8    /**< Deleted Data Address Mark */
/**@}*/

/** @name FM Address Marks (with clock patterns) */
/**@{*/
#define UFT_FM_MARK_IAM             0xFC    /**< Index: clock=D7, data=FC */
#define UFT_FM_MARK_IDAM            0xFE    /**< ID: clock=C7, data=FE */
#define UFT_FM_MARK_DAM             0xFB    /**< Data: clock=C7, data=FB */
#define UFT_FM_MARK_DDAM            0xF8    /**< Deleted: clock=C7, data=F8 */
#define UFT_FM_CLOCK_IAM            0xD7    /**< Clock for IAM */
#define UFT_FM_CLOCK_IDAM           0xC7    /**< Clock for IDAM/DAM/DDAM */
/**@}*/

/** @name FM Encoded Address Marks (16-bit with clock) */
/**@{*/
#define UFT_FM_ENC_IAM              0xF77A  /**< Encoded IAM */
#define UFT_FM_ENC_IDAM             0xF57E  /**< Encoded IDAM */
#define UFT_FM_ENC_DAM              0xF56F  /**< Encoded DAM */
#define UFT_FM_ENC_DDAM             0xF56A  /**< Encoded DDAM */
/**@}*/

/** @name GCR Sync Patterns */
/**@{*/
#define UFT_GCR_C64_SYNC            0xFF    /**< C64 sync byte (10 × 0xFF) */
#define UFT_GCR_C64_SYNC_COUNT      10      /**< Number of sync bytes */
#define UFT_GCR_APPLE_SYNC          0xFF    /**< Apple sync byte */
#define UFT_GCR_APPLE_SYNC_COUNT    5       /**< Minimum sync bytes */
/**@}*/

/** @name C64 GCR Block Markers */
/**@{*/
#define UFT_C64_HEADER_MARKER       0x08    /**< Header block ID */
#define UFT_C64_DATA_MARKER         0x07    /**< Data block ID */
/**@}*/

/** @name Apple II GCR Markers */
/**@{*/
#define UFT_APPLE_ADDR_PROLOGUE_1   0xD5    /**< Address prologue byte 1 */
#define UFT_APPLE_ADDR_PROLOGUE_2   0xAA    /**< Address prologue byte 2 */
#define UFT_APPLE_ADDR_PROLOGUE_3   0x96    /**< Address prologue byte 3 (DOS 3.3) */
#define UFT_APPLE_ADDR_PROLOGUE_3_32 0xB5   /**< Address prologue byte 3 (DOS 3.2) */
#define UFT_APPLE_DATA_PROLOGUE_1   0xD5    /**< Data prologue byte 1 */
#define UFT_APPLE_DATA_PROLOGUE_2   0xAA    /**< Data prologue byte 2 */
#define UFT_APPLE_DATA_PROLOGUE_3   0xAD    /**< Data prologue byte 3 */
#define UFT_APPLE_EPILOGUE_1        0xDE    /**< Epilogue byte 1 */
#define UFT_APPLE_EPILOGUE_2        0xAA    /**< Epilogue byte 2 */
#define UFT_APPLE_EPILOGUE_3        0xEB    /**< Epilogue byte 3 */
/**@}*/

/*===========================================================================
 * FORMAT-SPECIFIC CONSTANTS
 *===========================================================================*/

/** @name Amiga */
/**@{*/
#define UFT_AMIGA_TRACK_SIZE        11968   /**< Raw track size (bytes) */
#define UFT_AMIGA_SECTOR_SIZE       512     /**< Sector data size */
#define UFT_AMIGA_SECTORS_DD        11      /**< Sectors per track DD */
#define UFT_AMIGA_SECTORS_HD        22      /**< Sectors per track HD */
#define UFT_AMIGA_BOOTBLOCK_SIZE    1024    /**< Boot block size */
#define UFT_AMIGA_ROOTBLOCK_OFFSET  880     /**< Root block sector (DD) */
/**@}*/

/** @name Commodore 64/1541 */
/**@{*/
#define UFT_C64_TRACKS_STANDARD     35      /**< Standard track count */
#define UFT_C64_TRACKS_EXTENDED     40      /**< Extended track count */
#define UFT_C64_SECTORS_TOTAL       683     /**< Total sectors (35 tracks) */
#define UFT_C64_SECTORS_EXTENDED    768     /**< Total sectors (40 tracks) */
#define UFT_C64_SECTOR_SIZE         256     /**< Sector data size */
#define UFT_C64_BAM_TRACK           18      /**< BAM/Directory track */
#define UFT_C64_GCR_NIBBLE_SIZE     325     /**< GCR nibbles per sector */
/**@}*/

/** @name Apple II */
/**@{*/
#define UFT_APPLE_TRACKS            35      /**< Standard track count */
#define UFT_APPLE_SECTOR_SIZE       256     /**< Sector size */
#define UFT_APPLE_SECTORS_DOS32     13      /**< Sectors (DOS 3.2) */
#define UFT_APPLE_SECTORS_DOS33     16      /**< Sectors (DOS 3.3) */
#define UFT_APPLE_NIBBLE_TRACK_SIZE 6656    /**< NIB track size */
/**@}*/

/** @name Atari ST */
/**@{*/
#define UFT_ATARI_TRACKS_SS         80      /**< Single-sided tracks */
#define UFT_ATARI_TRACKS_DS         160     /**< Double-sided tracks */
#define UFT_ATARI_SECTORS_DD        9       /**< Sectors per track DD */
#define UFT_ATARI_SECTORS_HD        18      /**< Sectors per track HD */
#define UFT_ATARI_SECTOR_SIZE       512     /**< Sector size */
/**@}*/

/** @name PC/IBM */
/**@{*/
#define UFT_PC_TRACKS_DD            80      /**< DD tracks (40 cyl × 2) */
#define UFT_PC_TRACKS_HD            160     /**< HD tracks (80 cyl × 2) */
#define UFT_PC_SECTORS_DD           9       /**< Sectors/track 720K */
#define UFT_PC_SECTORS_HD           18      /**< Sectors/track 1.44M */
#define UFT_PC_SECTORS_ED           36      /**< Sectors/track 2.88M */
#define UFT_PC_SECTOR_SIZE          512     /**< Standard sector size */
/**@}*/

/*===========================================================================
 * BUFFER SIZES
 *===========================================================================*/

/** @name Track Buffers */
/**@{*/
#define UFT_TRACK_BUFFER_MIN        8192    /**< Minimum track buffer */
#define UFT_TRACK_BUFFER_DEFAULT    32768   /**< Default track buffer */
#define UFT_TRACK_BUFFER_MAX        131072  /**< Maximum track buffer */
/**@}*/

/** @name Flux Buffers */
/**@{*/
#define UFT_FLUX_BUFFER_MIN         65536   /**< Minimum flux buffer */
#define UFT_FLUX_BUFFER_DEFAULT     262144  /**< Default flux buffer */
#define UFT_FLUX_BUFFER_MAX         1048576 /**< Maximum flux buffer */
/**@}*/

/** @name String Buffers */
/**@{*/
#define UFT_PATH_MAX                4096    /**< Maximum path length */
#define UFT_NAME_MAX                256     /**< Maximum name length */
#define UFT_ERROR_MSG_MAX           512     /**< Maximum error message */
/**@}*/

/*===========================================================================
 * CRC CONSTANTS
 *===========================================================================*/

/** @name CRC Polynomials */
/**@{*/
#define UFT_CRC16_POLY_CCITT        0x1021  /**< CRC-16 CCITT polynomial */
#define UFT_CRC16_POLY_IBM          0x8005  /**< CRC-16 IBM polynomial */
#define UFT_CRC32_POLY              0xEDB88320 /**< CRC-32 (reflected) */
/**@}*/

/** @name CRC Initial Values */
/**@{*/
#define UFT_CRC16_INIT_FFFF         0xFFFF  /**< Standard CRC-16 init */
#define UFT_CRC16_INIT_MFM          0xCDB4  /**< CRC after A1 A1 A1 sync */
#define UFT_CRC32_INIT              0xFFFFFFFF /**< Standard CRC-32 init */
/**@}*/

/*===========================================================================
 * PROTECTION DETECTION
 *===========================================================================*/

/** @name Weak Bit Thresholds */
/**@{*/
#define UFT_WEAK_BIT_MIN_VARIANCE   10      /**< Minimum variance (%) */
#define UFT_WEAK_BIT_MIN_READS      3       /**< Minimum reads to detect */
#define UFT_WEAK_BIT_MAX_READS      8       /**< Maximum reads */
/**@}*/

/** @name DPM Thresholds (nanoseconds) */
/**@{*/
#define UFT_DPM_THRESHOLD_NS        500000  /**< Anomaly threshold (500µs) */
#define UFT_DPM_PRECISION_NS        1000    /**< Measurement precision */
/**@}*/

/*===========================================================================
 * FILE FORMAT MAGIC NUMBERS
 *===========================================================================*/

/** @name Format Signatures */
/**@{*/
#define UFT_MAGIC_ADF               0x444F53 /**< "DOS" (AmigaDOS) */
#define UFT_MAGIC_D64               0x00     /**< No magic (check size) */
#define UFT_MAGIC_G64               0x47435200 /**< "GCR\0" */
#define UFT_MAGIC_SCP               0x534350  /**< "SCP" */
#define UFT_MAGIC_IPF               0x43415053 /**< "CAPS" */
#define UFT_MAGIC_HFE               0x48584345 /**< "HXCE" or "HXCF" */
#define UFT_MAGIC_WOZ               0x574F5A  /**< "WOZ" */
#define UFT_MAGIC_NIB               0x00     /**< No magic (check size) */
#define UFT_MAGIC_STX               0x525359  /**< "RSY" */
#define UFT_MAGIC_TD0               0x5444    /**< "TD" */
#define UFT_MAGIC_IMD               0x494D44  /**< "IMD" */
/**@}*/

/** @name File Sizes (for format detection) */
/**@{*/
#define UFT_SIZE_D64_STANDARD       174848  /**< D64: 35 tracks */
#define UFT_SIZE_D64_EXTENDED       196608  /**< D64: 40 tracks */
#define UFT_SIZE_D64_ERROR          175531  /**< D64 with error map */
#define UFT_SIZE_ADF_DD             901120  /**< ADF: DD (880K) */
#define UFT_SIZE_ADF_HD             1802240 /**< ADF: HD (1760K) */
#define UFT_SIZE_NIB                232960  /**< NIB: 35 tracks × 6656 */
/**@}*/

#ifdef __cplusplus
}
#endif

#endif /* UFT_CONSTANTS_H */
