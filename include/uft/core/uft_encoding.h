/**
 * @file uft_encoding.h
 * @brief Unified Disk Encoding Types (P2-ARCH-003)
 * 
 * Central definition of all disk encoding types used across UFT.
 * Consolidates: uft_encoding_t, uft_ir_encoding_t, uft_track_encoding_t
 * 
 * @version 1.0.0
 * @date 2026-01-05
 */

#ifndef UFT_ENCODING_H
#define UFT_ENCODING_H

#include <stdint.h>
#include <stdbool.h>

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Master Encoding Enumeration
 *===========================================================================*/

/**
 * @brief Unified disk encoding type
 * 
 * This is the canonical encoding enumeration for all UFT subsystems.
 * Legacy enums (uft_encoding_t, uft_ir_encoding_t) should be phased out.
 */
typedef enum uft_disk_encoding {
    /* ═══ Unknown/Raw ═══ */
    UFT_DISK_ENC_UNKNOWN        = 0,    /**< Unknown encoding */
    UFT_DISK_ENC_RAW            = 1,    /**< Raw flux/bitstream, no decoding */
    
    /* ═══ FM (Single Density) ═══ */
    UFT_DISK_ENC_FM             = 10,   /**< FM (standard single density) */
    UFT_DISK_ENC_FM_IBM         = 11,   /**< FM IBM 3740 format */
    UFT_DISK_ENC_FM_INTEL       = 12,   /**< FM Intel MCS-80 */
    
    /* ═══ MFM (Double/High Density) ═══ */
    UFT_DISK_ENC_MFM            = 20,   /**< MFM (standard double density) */
    UFT_DISK_ENC_MFM_IBM        = 21,   /**< MFM IBM System/34 */
    UFT_DISK_ENC_MFM_HD         = 22,   /**< MFM High Density (1.44MB) */
    UFT_DISK_ENC_MFM_ED         = 23,   /**< MFM Extra Density (2.88MB) */
    
    /* ═══ Amiga MFM ═══ */
    UFT_DISK_ENC_MFM_AMIGA      = 30,   /**< Amiga MFM (odd/even split) */
    UFT_DISK_ENC_MFM_AMIGA_HD   = 31,   /**< Amiga HD MFM */
    
    /* ═══ M2FM ═══ */
    UFT_DISK_ENC_M2FM           = 40,   /**< M2FM (Intel iSBC) */
    UFT_DISK_ENC_M2FM_HP        = 41,   /**< M2FM HP 9895 */
    
    /* ═══ GCR Commodore ═══ */
    UFT_DISK_ENC_GCR_C64        = 50,   /**< Commodore 64/1541 GCR */
    UFT_DISK_ENC_GCR_C128       = 51,   /**< Commodore 128 GCR */
    UFT_DISK_ENC_GCR_VIC20      = 52,   /**< VIC-20 GCR */
    UFT_DISK_ENC_GCR_1571       = 53,   /**< Commodore 1571 GCR */
    UFT_DISK_ENC_GCR_1581       = 54,   /**< Commodore 1581 (MFM actually) */
    
    /* ═══ GCR Apple ═══ */
    UFT_DISK_ENC_GCR_APPLE_525  = 60,   /**< Apple II 5.25" (6+2 encoding) */
    UFT_DISK_ENC_GCR_APPLE_DOS  = 61,   /**< Apple DOS 3.2/3.3 */
    UFT_DISK_ENC_GCR_APPLE_PRO  = 62,   /**< Apple ProDOS */
    UFT_DISK_ENC_GCR_APPLE_35   = 63,   /**< Apple 3.5" GCR */
    UFT_DISK_ENC_GCR_MAC        = 64,   /**< Macintosh GCR (400K/800K) */
    UFT_DISK_ENC_GCR_MAC_HD     = 65,   /**< Macintosh HD (MFM actually) */
    
    /* ═══ GCR Other ═══ */
    UFT_DISK_ENC_GCR_VICTOR     = 70,   /**< Victor 9000 GCR */
    UFT_DISK_ENC_GCR_NORTHSTAR  = 71,   /**< NorthStar GCR */
    
    /* ═══ Japanese Formats ═══ */
    UFT_DISK_ENC_MFM_PC98       = 80,   /**< NEC PC-98 MFM */
    UFT_DISK_ENC_MFM_X68K       = 81,   /**< Sharp X68000 MFM */
    UFT_DISK_ENC_MFM_FM7        = 82,   /**< Fujitsu FM-7 MFM */
    UFT_DISK_ENC_MFM_MSX        = 83,   /**< MSX MFM */
    
    /* ═══ European Formats ═══ */
    UFT_DISK_ENC_MFM_AMSTRAD    = 90,   /**< Amstrad CPC MFM */
    UFT_DISK_ENC_MFM_SPECTRUM   = 91,   /**< ZX Spectrum +3 MFM */
    UFT_DISK_ENC_MFM_SAM        = 92,   /**< SAM Coupé MFM */
    UFT_DISK_ENC_FM_BBC         = 93,   /**< BBC Micro FM */
    UFT_DISK_ENC_MFM_BBC        = 94,   /**< BBC Micro MFM */
    UFT_DISK_ENC_FM_ACORN       = 95,   /**< Acorn DFS FM */
    UFT_DISK_ENC_MFM_ACORN      = 96,   /**< Acorn ADFS MFM */
    
    /* ═══ US Computer Formats ═══ */
    UFT_DISK_ENC_FM_TRS80       = 100,  /**< TRS-80 FM */
    UFT_DISK_ENC_MFM_TRS80      = 101,  /**< TRS-80 MFM */
    UFT_DISK_ENC_FM_ATARI8      = 102,  /**< Atari 8-bit FM */
    UFT_DISK_ENC_MFM_ATARI8     = 103,  /**< Atari 8-bit MFM */
    UFT_DISK_ENC_MFM_ATARIST    = 104,  /**< Atari ST MFM */
    
    /* ═══ Hard Sector ═══ */
    UFT_DISK_ENC_HARDSEC_5      = 110,  /**< 5-sector hard sectored */
    UFT_DISK_ENC_HARDSEC_10     = 111,  /**< 10-sector hard sectored */
    UFT_DISK_ENC_HARDSEC_16     = 112,  /**< 16-sector hard sectored */
    
    /* ═══ Special ═══ */
    UFT_DISK_ENC_FLUXSTREAM     = 250,  /**< Raw flux stream (SCP, A2R) */
    UFT_DISK_ENC_BITSTREAM      = 251,  /**< Decoded bitstream (HFE) */
    UFT_DISK_ENC_CUSTOM         = 255   /**< Custom/user-defined */
    
} uft_disk_encoding_t;

/*===========================================================================
 * Encoding Categories
 *===========================================================================*/

/**
 * @brief Encoding category (for grouping)
 */
typedef enum uft_encoding_category {
    UFT_ENCCAT_UNKNOWN  = 0,
    UFT_ENCCAT_FM       = 1,    /**< FM variants */
    UFT_ENCCAT_MFM      = 2,    /**< MFM variants */
    UFT_ENCCAT_M2FM     = 3,    /**< M2FM variants */
    UFT_ENCCAT_GCR      = 4,    /**< GCR variants */
    UFT_ENCCAT_HARDSEC  = 5,    /**< Hard-sectored */
    UFT_ENCCAT_RAW      = 6     /**< Raw/special */
} uft_encoding_category_t;

/*===========================================================================
 * Encoding Properties
 *===========================================================================*/

/**
 * @brief Encoding properties
 */
typedef struct uft_encoding_info {
    uft_disk_encoding_t encoding;   /**< Encoding type */
    uft_encoding_category_t category; /**< Category */
    const char *name;               /**< Short name */
    const char *description;        /**< Full description */
    
    /* Timing parameters (at 300 RPM) */
    uint32_t bitcell_ns;            /**< Nominal bitcell time (ns) */
    uint32_t data_rate_kbps;        /**< Data rate (kbps) */
    
    /* Characteristics */
    uint8_t  clock_bits;            /**< Clock bits per data bit */
    bool     variable_rate;         /**< Has variable bit rate zones */
    bool     soft_sector;           /**< Soft-sectored (vs hard) */
    uint8_t  sync_pattern_bits;     /**< Sync pattern length */
    
    /* Platform association */
    const char *platforms;          /**< Associated platforms */
} uft_encoding_info_t;

/*===========================================================================
 * Function Prototypes
 *===========================================================================*/

/**
 * @brief Get encoding info
 */
const uft_encoding_info_t* uft_encoding_get_info(uft_disk_encoding_t enc);

/**
 * @brief Get encoding name
 */
const char* uft_disk_encoding_name(uft_disk_encoding_t enc);

/**
 * @brief Get encoding category
 */
uft_encoding_category_t uft_encoding_category(uft_disk_encoding_t enc);

/**
 * @brief Get category name
 */
const char* uft_encoding_category_name(uft_encoding_category_t cat);

/**
 * @brief Check if encoding is FM-based
 */
static inline bool uft_encoding_is_fm(uft_disk_encoding_t enc) {
    return uft_encoding_category(enc) == UFT_ENCCAT_FM;
}

/**
 * @brief Check if encoding is MFM-based
 */
static inline bool uft_encoding_is_mfm(uft_disk_encoding_t enc) {
    return uft_encoding_category(enc) == UFT_ENCCAT_MFM;
}

/**
 * @brief Check if encoding is GCR-based
 */
static inline bool uft_encoding_is_gcr(uft_disk_encoding_t enc) {
    return uft_encoding_category(enc) == UFT_ENCCAT_GCR;
}

/**
 * @brief Get nominal bitcell time for encoding
 */
uint32_t uft_encoding_bitcell_ns(uft_disk_encoding_t enc);

/**
 * @brief Get data rate for encoding
 */
uint32_t uft_encoding_data_rate(uft_disk_encoding_t enc);

/*===========================================================================
 * Legacy Conversion Functions
 *===========================================================================*/

/**
 * @brief Convert from legacy uft_encoding_t
 */
uft_disk_encoding_t uft_encoding_from_decoder(int legacy_enc);

/**
 * @brief Convert to legacy uft_encoding_t
 */
int uft_encoding_to_decoder(uft_disk_encoding_t enc);

/**
 * @brief Convert from legacy uft_ir_encoding_t
 */
uft_disk_encoding_t uft_encoding_from_ir(int ir_enc);

/**
 * @brief Convert to legacy uft_ir_encoding_t
 */
int uft_encoding_to_ir(uft_disk_encoding_t enc);

/**
 * @brief Detect encoding from bitstream pattern
 */
uft_disk_encoding_t uft_encoding_detect(const uint8_t *bitstream, 
                                         size_t bits, 
                                         float *confidence);

#ifdef __cplusplus
}
#endif

#endif /* UFT_ENCODING_H */
