/**
 * @file uft_imd_data_modes.h
 * @brief ImageDisk (IMD) Data Mode Definitions
 * @version 1.0.0
 * 
 * Based on Dave Dunfield's ImageDisk specification.
 * Reference: http://www.classiccmp.org/dunfield/img/index.htm
 * 
 * Clean-room implementation based on public IMD specification.
 */

#ifndef UFT_IMD_DATA_MODES_H
#define UFT_IMD_DATA_MODES_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * IMD Data Mode Constants
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * IMD Mode values (as stored in track header)
 * 
 * The mode byte encodes both the data rate and FM/MFM encoding:
 *   Modes 0-2: FM encoding at 500k/300k/250k
 *   Modes 3-5: MFM encoding at 500k/300k/250k
 *   Mode 6: MFM at 1000k (ED disks, not in original IMD 1.18 spec)
 */
typedef enum {
    UFT_IMD_MODE_FM_500K    = 0,    /**< FM 500 kbps (8" SD) */
    UFT_IMD_MODE_FM_300K    = 1,    /**< FM 300 kbps (5.25" DD in HD drive) */
    UFT_IMD_MODE_FM_250K    = 2,    /**< FM 250 kbps (5.25" DD, 3.5" DD) */
    UFT_IMD_MODE_MFM_500K   = 3,    /**< MFM 500 kbps (5.25" HD, 3.5" HD, 8" DD) */
    UFT_IMD_MODE_MFM_300K   = 4,    /**< MFM 300 kbps (5.25" DD in HD drive) */
    UFT_IMD_MODE_MFM_250K   = 5,    /**< MFM 250 kbps (5.25" DD, 3.5" DD) */
    UFT_IMD_MODE_MFM_1000K  = 6,    /**< MFM 1000 kbps (3.5" ED) - extension */
    UFT_IMD_MODE_UNKNOWN    = 0xFF  /**< Unknown/invalid mode */
} uft_imd_mode_t;

/**
 * FDC data rate codes (matches PC FDC rate register)
 */
typedef enum {
    UFT_IMD_RATE_500K  = 0,   /**< 500 kbps */
    UFT_IMD_RATE_300K  = 1,   /**< 300 kbps */
    UFT_IMD_RATE_250K  = 2,   /**< 250 kbps */
    UFT_IMD_RATE_1000K = 3    /**< 1000 kbps (ED) */
} uft_imd_rate_t;

/**
 * IMD sector status codes (Sector Data Record type)
 */
typedef enum {
    UFT_IMD_SECTOR_UNAVAILABLE  = 0x00,  /**< Sector data unavailable */
    UFT_IMD_SECTOR_NORMAL       = 0x01,  /**< Normal data */
    UFT_IMD_SECTOR_COMPRESSED   = 0x02,  /**< Compressed (fill byte) */
    UFT_IMD_SECTOR_DELETED      = 0x03,  /**< Deleted data */
    UFT_IMD_SECTOR_DEL_COMPRESS = 0x04,  /**< Deleted + compressed */
    UFT_IMD_SECTOR_ERROR        = 0x05,  /**< Data with error */
    UFT_IMD_SECTOR_ERR_COMPRESS = 0x06,  /**< Error + compressed */
    UFT_IMD_SECTOR_DEL_ERROR    = 0x07,  /**< Deleted + error */
    UFT_IMD_SECTOR_DEL_ERR_COMP = 0x08   /**< Deleted + error + compressed */
} uft_imd_sector_status_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Data Mode Structure
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * Data mode descriptor
 */
typedef struct {
    uft_imd_mode_t  imd_mode;       /**< IMD mode value */
    const char     *name;           /**< Human-readable name */
    uft_imd_rate_t  rate;           /**< Data rate code */
    bool            is_fm;          /**< true=FM, false=MFM */
    uint32_t        bitrate_kbps;   /**< Actual bitrate in kbps */
    uint32_t        cell_ns;        /**< Nominal bit cell time in ns */
} uft_imd_data_mode_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Data Modes Table
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * Standard data modes table
 * 
 * Ordered by typical probe priority (MFM before FM, common rates first)
 */
static const uft_imd_data_mode_t UFT_IMD_DATA_MODES[] = {
    /* MFM modes - most common */
    { UFT_IMD_MODE_MFM_250K,  "MFM-250k",  UFT_IMD_RATE_250K,  false, 250,  4000 },
    { UFT_IMD_MODE_MFM_500K,  "MFM-500k",  UFT_IMD_RATE_500K,  false, 500,  2000 },
    { UFT_IMD_MODE_MFM_300K,  "MFM-300k",  UFT_IMD_RATE_300K,  false, 300,  3333 },
    { UFT_IMD_MODE_MFM_1000K, "MFM-1000k", UFT_IMD_RATE_1000K, false, 1000, 1000 },
    
    /* FM modes - legacy */
    { UFT_IMD_MODE_FM_250K,   "FM-250k",   UFT_IMD_RATE_250K,  true,  125,  8000 },
    { UFT_IMD_MODE_FM_500K,   "FM-500k",   UFT_IMD_RATE_500K,  true,  250,  4000 },
    { UFT_IMD_MODE_FM_300K,   "FM-300k",   UFT_IMD_RATE_300K,  true,  150,  6667 },
    
    /* Sentinel */
    { UFT_IMD_MODE_UNKNOWN, NULL, 0, false, 0, 0 }
};

#define UFT_IMD_DATA_MODE_COUNT 7

/* ═══════════════════════════════════════════════════════════════════════════════
 * Lookup Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * Look up data mode by IMD mode value
 * @param mode IMD mode byte from track header
 * @return Pointer to mode descriptor, or NULL if unknown
 */
static inline const uft_imd_data_mode_t *uft_imd_lookup_mode(uint8_t mode) {
    for (int i = 0; UFT_IMD_DATA_MODES[i].name != NULL; i++) {
        if (UFT_IMD_DATA_MODES[i].imd_mode == mode) {
            return &UFT_IMD_DATA_MODES[i];
        }
    }
    return NULL;
}

/**
 * Look up data mode by encoding and rate
 * @param is_fm true for FM, false for MFM
 * @param rate Data rate code
 * @return Pointer to mode descriptor, or NULL if unknown
 */
static inline const uft_imd_data_mode_t *uft_imd_lookup_by_encoding(
    bool is_fm, 
    uft_imd_rate_t rate
) {
    for (int i = 0; UFT_IMD_DATA_MODES[i].name != NULL; i++) {
        if (UFT_IMD_DATA_MODES[i].is_fm == is_fm && 
            UFT_IMD_DATA_MODES[i].rate == rate) {
            return &UFT_IMD_DATA_MODES[i];
        }
    }
    return NULL;
}

/**
 * Get IMD mode value from encoding and rate
 * @param is_fm true for FM, false for MFM
 * @param rate Data rate code
 * @return IMD mode value, or UFT_IMD_MODE_UNKNOWN if invalid
 */
static inline uft_imd_mode_t uft_imd_get_mode(bool is_fm, uft_imd_rate_t rate) {
    const uft_imd_data_mode_t *m = uft_imd_lookup_by_encoding(is_fm, rate);
    return m ? m->imd_mode : UFT_IMD_MODE_UNKNOWN;
}

/**
 * Check if sector status indicates compressed data
 * @param status Sector status byte
 * @return true if data is compressed (single fill byte)
 */
static inline bool uft_imd_is_compressed(uint8_t status) {
    return (status == UFT_IMD_SECTOR_COMPRESSED ||
            status == UFT_IMD_SECTOR_DEL_COMPRESS ||
            status == UFT_IMD_SECTOR_ERR_COMPRESS ||
            status == UFT_IMD_SECTOR_DEL_ERR_COMP);
}

/**
 * Check if sector status indicates deleted data
 * @param status Sector status byte
 * @return true if deleted address mark was used
 */
static inline bool uft_imd_is_deleted(uint8_t status) {
    return (status == UFT_IMD_SECTOR_DELETED ||
            status == UFT_IMD_SECTOR_DEL_COMPRESS ||
            status == UFT_IMD_SECTOR_DEL_ERROR ||
            status == UFT_IMD_SECTOR_DEL_ERR_COMP);
}

/**
 * Check if sector status indicates a CRC error
 * @param status Sector status byte
 * @return true if sector had CRC error during capture
 */
static inline bool uft_imd_has_error(uint8_t status) {
    return (status == UFT_IMD_SECTOR_ERROR ||
            status == UFT_IMD_SECTOR_ERR_COMPRESS ||
            status == UFT_IMD_SECTOR_DEL_ERROR ||
            status == UFT_IMD_SECTOR_DEL_ERR_COMP);
}

/**
 * Convert sector size code to bytes
 * @param code Size code (0-6)
 * @return Sector size in bytes (128, 256, 512, 1024, 2048, 4096, 8192)
 */
static inline uint32_t uft_imd_sector_bytes(uint8_t code) {
    if (code > 6) return 0;
    return 128U << code;
}

/**
 * Convert sector size in bytes to code
 * @param bytes Sector size in bytes
 * @return Size code (0-6), or 0xFF if invalid
 */
static inline uint8_t uft_imd_sector_code(uint32_t bytes) {
    switch (bytes) {
        case 128:  return 0;
        case 256:  return 1;
        case 512:  return 2;
        case 1024: return 3;
        case 2048: return 4;
        case 4096: return 5;
        case 8192: return 6;
        default:   return 0xFF;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Drive Type Associations
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * Typical drive type for data mode
 */
static inline const char *uft_imd_mode_drive_type(uft_imd_mode_t mode) {
    switch (mode) {
        case UFT_IMD_MODE_FM_500K:   return "8\" SD";
        case UFT_IMD_MODE_FM_300K:   return "5.25\" DD in HD";
        case UFT_IMD_MODE_FM_250K:   return "5.25\"/3.5\" DD";
        case UFT_IMD_MODE_MFM_500K:  return "5.25\"/3.5\" HD, 8\" DD";
        case UFT_IMD_MODE_MFM_300K:  return "5.25\" DD in HD";
        case UFT_IMD_MODE_MFM_250K:  return "5.25\"/3.5\" DD";
        case UFT_IMD_MODE_MFM_1000K: return "3.5\" ED";
        default: return "Unknown";
    }
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_IMD_DATA_MODES_H */
