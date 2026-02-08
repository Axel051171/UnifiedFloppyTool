/**
 * @file uft_uft_sam_core.h
 * @brief SamDisk Core Algorithms for UnifiedFloppyTool
 * 
 * Extracted from SamDisk - the ORIGINAL reference implementation for:
 * - PLL (Phase-Locked Loop) flux decoding
 * - BitBuffer management
 * - FM/MFM encoding/decoding
 * - CRC-16-CCITT calculation
 * - Track building
 * 
 * 
 * @copyright Adaptation for UFT: 2025
 */

#ifndef UFT_UFT_SAM_CORE_H
#define UFT_UFT_SAM_CORE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * PLL CONSTANTS - THE ORIGINAL SAMDISK DEFAULTS
 *============================================================================*/

/**
 * @defgroup PLL_Defaults SamDisk PLL Default Parameters
 * @{
 */

/** Default PLL clock adjustment percentage (range 0-50) */
#define UFT_SD_PLL_ADJUST_DEFAULT    4

/** Default PLL phase percentage (range 0-90) */
#define UFT_SD_PLL_PHASE_DEFAULT     60

/** Maximum PLL adjustment */
#define UFT_SD_PLL_ADJUST_MAX        50

/** Maximum PLL phase */
#define UFT_SD_PLL_PHASE_MAX         90

/** Jitter simulation percentage for motor wobble */
#define UFT_SD_JITTER_PERCENT        2

/** Good bits required before reporting sync loss again */
#define UFT_SD_SYNC_LOSS_THRESHOLD   256

/** @} */

/*============================================================================
 * DATA RATE DEFINITIONS
 *============================================================================*/

/**
 * @defgroup DataRates Standard Data Rates
 * @{
 */

typedef enum {
    UFT_SD_DATARATE_UNKNOWN = 0,
    UFT_SD_DATARATE_250K    = 250000,   /**< DD (Double Density) */
    UFT_SD_DATARATE_300K    = 300000,   /**< DD in 360 RPM drive */
    UFT_SD_DATARATE_500K    = 500000,   /**< HD (High Density) */
    UFT_SD_DATARATE_1M      = 1000000   /**< ED (Extra Density) */
} uft_sd_datarate_t;

/** Convert data rate to bitcell width in nanoseconds */
static inline int uft_sd_bitcell_ns(uft_sd_datarate_t datarate) {
    switch (datarate) {
        case UFT_SD_DATARATE_250K: return 4000;   /* 4µs */
        case UFT_SD_DATARATE_300K: return 3333;   /* 3.333µs */
        case UFT_SD_DATARATE_500K: return 2000;   /* 2µs */
        case UFT_SD_DATARATE_1M:   return 1000;   /* 1µs */
        default: return 4000;
    }
}

/** @} */

/*============================================================================
 * ENCODING TYPES
 *============================================================================*/

/**
 * @defgroup Encodings Disk Encoding Types
 * @{
 */

typedef enum {
    UFT_SD_ENC_UNKNOWN = 0,
    UFT_SD_ENC_FM,          /**< Frequency Modulation (SD) */
    UFT_SD_ENC_MFM,         /**< Modified FM (DD/HD) */
    UFT_SD_ENC_RX02,        /**< DEC RX02 hybrid encoding */
    UFT_SD_ENC_AMIGA,       /**< Amiga MFM with odd/even interleaving */
    UFT_SD_ENC_GCR,         /**< Commodore 64 GCR */
    UFT_SD_ENC_APPLE,       /**< Apple II 6&2 GCR */
    UFT_SD_ENC_VICTOR,      /**< Victor 9000 GCR */
    UFT_SD_ENC_ACE,         /**< Jupiter Ace */
    UFT_SD_ENC_MX,          /**< DVK MX (Soviet RT-11) */
    UFT_SD_ENC_AGAT,        /**< Agat 840K MFM */
    UFT_SD_ENC_VISTA        /**< Vista format */
} uft_sd_encoding_t;

/** @} */

/*============================================================================
 * PLL STATE STRUCTURE - THE ORIGINAL SAMDISK PLL
 *============================================================================*/

/**
 * @brief SamDisk PLL State Structure
 * 
 * This is the ORIGINAL PLL implementation from SamDisk, which was derived
 */
typedef struct {
    /* Clock parameters */
    int clock;              /**< Current clock period (ns) */
    int clock_centre;       /**< Target/center clock period */
    int clock_min;          /**< Minimum allowed clock */
    int clock_max;          /**< Maximum allowed clock */
    
    /* Phase tracking */
    int flux;               /**< Accumulated flux time since last bit */
    int clocked_zeros;      /**< Consecutive zero bits clocked */
    int goodbits;           /**< Good bits since last sync loss */
    
    /* Configuration */
    int pll_adjust;         /**< Clock adjustment percentage (0-50) */
    int pll_phase;          /**< Phase retention percentage (0-90) */
    int flux_scale_percent; /**< Flux timing scale (100 = nominal) */
    
    /* Status flags */
    bool index;             /**< Index pulse detected */
    bool sync_lost;         /**< Sync loss detected */
} uft_sd_pll_t;

/*============================================================================
 * PLL FUNCTIONS - THE ORIGINAL SAMDISK ALGORITHM
 *============================================================================*/

/**
 * @brief Initialize PLL with default parameters
 * 
 * @param pll Pointer to PLL state structure
 * @param bitcell_ns Nominal bitcell width in nanoseconds
 */
static inline void uft_sd_pll_init(uft_sd_pll_t *pll, int bitcell_ns) {
    pll->clock = bitcell_ns;
    pll->clock_centre = bitcell_ns;
    pll->clock_min = bitcell_ns * (100 - UFT_SD_PLL_ADJUST_DEFAULT) / 100;
    pll->clock_max = bitcell_ns * (100 + UFT_SD_PLL_ADJUST_DEFAULT) / 100;
    
    pll->flux = 0;
    pll->clocked_zeros = 0;
    pll->goodbits = 0;
    
    pll->pll_adjust = UFT_SD_PLL_ADJUST_DEFAULT;
    pll->pll_phase = UFT_SD_PLL_PHASE_DEFAULT;
    pll->flux_scale_percent = 100;
    
    pll->index = false;
    pll->sync_lost = false;
}

/**
 * @brief Initialize PLL with custom parameters
 * 
 * @param pll Pointer to PLL state structure
 * @param bitcell_ns Nominal bitcell width in nanoseconds
 * @param pll_adjust Clock adjustment percentage (0-50)
 * @param pll_phase Phase retention percentage (0-90)
 * @param flux_scale Flux timing scale percentage
 */
static inline void uft_sd_pll_init_ex(uft_sd_pll_t *pll, int bitcell_ns,
                                       int pll_adjust, int pll_phase,
                                       int flux_scale) {
    pll->clock = bitcell_ns;
    pll->clock_centre = bitcell_ns;
    pll->clock_min = bitcell_ns * (100 - pll_adjust) / 100;
    pll->clock_max = bitcell_ns * (100 + pll_adjust) / 100;
    
    pll->flux = 0;
    pll->clocked_zeros = 0;
    pll->goodbits = 0;
    
    pll->pll_adjust = pll_adjust;
    pll->pll_phase = pll_phase;
    pll->flux_scale_percent = flux_scale;
    
    pll->index = false;
    pll->sync_lost = false;
}

/**
 * @brief Process a flux transition and extract the next bit
 * 
 * This is the ORIGINAL SamDisk PLL algorithm. The key features:
 * 
 * 1. Dual-mode clock adjustment:
 *    - In sync (clocked_zeros <= 3): Direct phase correction
 *    - Out of sync: Gradual return to center frequency
 * 
 * 2. Phase retention (authentic PLL behavior):
 *    - Does NOT snap timing window to each transition
 *    - Retains partial phase error for smoother tracking
 * 
 * 3. Sync loss detection:
 *    - Requires 256 good bits before reporting new sync loss
 *    - Prevents spurious warnings during legitimate sync patterns
 * 
 * @param pll Pointer to PLL state structure
 * @param flux_ns Flux transition time in nanoseconds
 * @return 0 for zero bit, 1 for one bit, -1 for end of data
 */
static inline int uft_sd_pll_process(uft_sd_pll_t *pll, int flux_ns) {
    /* Apply flux scaling if configured */
    if (pll->flux_scale_percent != 100) {
        flux_ns = flux_ns * pll->flux_scale_percent / 100;
    }
    
    /* Accumulate flux time */
    pll->flux += flux_ns;
    pll->clocked_zeros = 0;
    
    /* Wait until we have at least half a bitcell of flux */
    while (pll->flux < pll->clock / 2) {
        return -2;  /* Need more flux data */
    }
    
    /* Subtract one clock period */
    pll->flux -= pll->clock;
    
    /* Check if we have a one bit (flux in timing window) */
    if (pll->flux >= pll->clock / 2) {
        /* Zero bit - no transition in this window */
        pll->clocked_zeros++;
        pll->goodbits++;
        return 0;
    }
    
    /* One bit - transition detected */
    
    /* PLL: Adjust clock frequency according to phase mismatch */
    if (pll->clocked_zeros <= 3) {
        /* In sync: adjust base clock by percentage of phase mismatch */
        pll->clock += pll->flux * pll->pll_adjust / 100;
    } else {
        /* Out of sync: adjust base clock towards centre */
        pll->clock += (pll->clock_centre - pll->clock) * pll->pll_adjust / 100;
        
        /* Require 256 good bits before reporting another loss of sync */
        if (pll->goodbits >= UFT_SD_SYNC_LOSS_THRESHOLD) {
            pll->sync_lost = true;
        }
        pll->goodbits = 0;
    }
    
    /* Clamp the clock's adjustment range */
    if (pll->clock < pll->clock_min) pll->clock = pll->clock_min;
    if (pll->clock > pll->clock_max) pll->clock = pll->clock_max;
    
    /* Authentic PLL: Do not snap the timing window to each flux transition
     * Instead, retain a portion of the phase error for smoother tracking */
    pll->flux = pll->flux * (100 - pll->pll_phase) / 100;
    
    pll->goodbits++;
    return 1;
}

/**
 * @brief Check and clear index pulse flag
 * @param pll Pointer to PLL state
 * @return true if index was detected, false otherwise
 */
static inline bool uft_sd_pll_index(uft_sd_pll_t *pll) {
    bool ret = pll->index;
    pll->index = false;
    return ret;
}

/**
 * @brief Check and clear sync loss flag
 * @param pll Pointer to PLL state
 * @return true if sync was lost, false otherwise
 */
static inline bool uft_sd_pll_sync_lost(uft_sd_pll_t *pll) {
    bool ret = pll->sync_lost;
    pll->sync_lost = false;
    return ret;
}

/*============================================================================
 * CRC-16-CCITT IMPLEMENTATION
 *============================================================================*/

/**
 * @defgroup CRC16 CRC-16-CCITT Functions
 * @{
 */

/** CRC-16-CCITT polynomial */
#define UFT_SD_CRC16_POLY       0x1021

/** CRC-16-CCITT initial value */
#define UFT_SD_CRC16_INIT       0xFFFF

/** CRC after encoding 0xA1, 0xA1, 0xA1 (MFM sync) */
#define UFT_SD_CRC16_A1A1A1     0xCDB4

/**
 * @brief CRC-16-CCITT lookup table
 * Generated with polynomial 0x1021
 */
static const uint16_t uft_sd_crc16_table[256] = {
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50a5, 0x60c6, 0x70e7,
    0x8108, 0x9129, 0xa14a, 0xb16b, 0xc18c, 0xd1ad, 0xe1ce, 0xf1ef,
    0x1231, 0x0210, 0x3273, 0x2252, 0x52b5, 0x4294, 0x72f7, 0x62d6,
    0x9339, 0x8318, 0xb37b, 0xa35a, 0xd3bd, 0xc39c, 0xf3ff, 0xe3de,
    0x2462, 0x3443, 0x0420, 0x1401, 0x64e6, 0x74c7, 0x44a4, 0x5485,
    0xa56a, 0xb54b, 0x8528, 0x9509, 0xe5ee, 0xf5cf, 0xc5ac, 0xd58d,
    0x3653, 0x2672, 0x1611, 0x0630, 0x76d7, 0x66f6, 0x5695, 0x46b4,
    0xb75b, 0xa77a, 0x9719, 0x8738, 0xf7df, 0xe7fe, 0xd79d, 0xc7bc,
    0x48c4, 0x58e5, 0x6886, 0x78a7, 0x0840, 0x1861, 0x2802, 0x3823,
    0xc9cc, 0xd9ed, 0xe98e, 0xf9af, 0x8948, 0x9969, 0xa90a, 0xb92b,
    0x5af5, 0x4ad4, 0x7ab7, 0x6a96, 0x1a71, 0x0a50, 0x3a33, 0x2a12,
    0xdbfd, 0xcbdc, 0xfbbf, 0xeb9e, 0x9b79, 0x8b58, 0xbb3b, 0xab1a,
    0x6ca6, 0x7c87, 0x4ce4, 0x5cc5, 0x2c22, 0x3c03, 0x0c60, 0x1c41,
    0xedae, 0xfd8f, 0xcdec, 0xddcd, 0xad2a, 0xbd0b, 0x8d68, 0x9d49,
    0x7e97, 0x6eb6, 0x5ed5, 0x4ef4, 0x3e13, 0x2e32, 0x1e51, 0x0e70,
    0xff9f, 0xefbe, 0xdfdd, 0xcffc, 0xbf1b, 0xaf3a, 0x9f59, 0x8f78,
    0x9188, 0x81a9, 0xb1ca, 0xa1eb, 0xd10c, 0xc12d, 0xf14e, 0xe16f,
    0x1080, 0x00a1, 0x30c2, 0x20e3, 0x5004, 0x4025, 0x7046, 0x6067,
    0x83b9, 0x9398, 0xa3fb, 0xb3da, 0xc33d, 0xd31c, 0xe37f, 0xf35e,
    0x02b1, 0x1290, 0x22f3, 0x32d2, 0x4235, 0x5214, 0x6277, 0x7256,
    0xb5ea, 0xa5cb, 0x95a8, 0x8589, 0xf56e, 0xe54f, 0xd52c, 0xc50d,
    0x34e2, 0x24c3, 0x14a0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
    0xa7db, 0xb7fa, 0x8799, 0x97b8, 0xe75f, 0xf77e, 0xc71d, 0xd73c,
    0x26d3, 0x36f2, 0x0691, 0x16b0, 0x6657, 0x7676, 0x4615, 0x5634,
    0xd94c, 0xc96d, 0xf90e, 0xe92f, 0x99c8, 0x89e9, 0xb98a, 0xa9ab,
    0x5844, 0x4865, 0x7806, 0x6827, 0x18c0, 0x08e1, 0x3882, 0x28a3,
    0xcb7d, 0xdb5c, 0xeb3f, 0xfb1e, 0x8bf9, 0x9bd8, 0xabbb, 0xbb9a,
    0x4a75, 0x5a54, 0x6a37, 0x7a16, 0x0af1, 0x1ad0, 0x2ab3, 0x3a92,
    0xfd2e, 0xed0f, 0xdd6c, 0xcd4d, 0xbdaa, 0xad8b, 0x9de8, 0x8dc9,
    0x7c26, 0x6c07, 0x5c64, 0x4c45, 0x3ca2, 0x2c83, 0x1ce0, 0x0cc1,
    0xef1f, 0xff3e, 0xcf5d, 0xdf7c, 0xaf9b, 0xbfba, 0x8fd9, 0x9ff8,
    0x6e17, 0x7e36, 0x4e55, 0x5e74, 0x2e93, 0x3eb2, 0x0ed1, 0x1ef0
};

/**
 * @brief Calculate CRC-16-CCITT for a byte
 * @param crc Current CRC value
 * @param byte Byte to add
 * @return Updated CRC value
 */
static inline uint16_t uft_sd_crc16_byte(uint16_t crc, uint8_t byte) {
    return (crc << 8) ^ uft_sd_crc16_table[((crc >> 8) ^ byte) & 0xFF];
}

/**
 * @brief Calculate CRC-16-CCITT for a buffer
 * @param data Pointer to data buffer
 * @param len Length of data
 * @param init Initial CRC value (use UFT_SD_CRC16_INIT for normal, UFT_SD_CRC16_A1A1A1 after MFM sync)
 * @return Calculated CRC value
 */
static inline uint16_t uft_sd_crc16(const uint8_t *data, size_t len, uint16_t init) {
    uint16_t crc = init;
    while (len--) {
        crc = uft_sd_crc16_byte(crc, *data++);
    }
    return crc;
}

/** @} */

/*============================================================================
 * IBM FORMAT CONSTANTS
 *============================================================================*/

/**
 * @defgroup IBM_Constants IBM Format Constants
 * @{
 */

/* Address Mark Types */
#define UFT_SD_IBM_DAM_DELETED      0xF8    /**< Deleted Data Address Mark */
#define UFT_SD_IBM_DAM_DELETED_ALT  0xF9    /**< Alternate DDAM */
#define UFT_SD_IBM_DAM_ALT          0xFA    /**< Alternate DAM */
#define UFT_SD_IBM_DAM              0xFB    /**< Data Address Mark */
#define UFT_SD_IBM_IAM              0xFC    /**< Index Address Mark */
#define UFT_SD_IBM_DAM_RX02         0xFD    /**< RX02 DAM */
#define UFT_SD_IBM_IDAM             0xFE    /**< ID Address Mark */

/* Gap sizes */
#define UFT_SD_GAP2_MFM_ED          41      /**< Gap2 for MFM 1Mbps (ED) */
#define UFT_SD_GAP2_MFM_DDHD        22      /**< Gap2 for MFM DD/HD */
#define UFT_SD_GAP2_FM              11      /**< Gap2 for FM */
#define UFT_SD_MIN_GAP3             1       /**< Minimum Gap3 */
#define UFT_SD_MAX_GAP3             82      /**< Maximum Gap3 */

/* Track overhead in bytes */
#define UFT_SD_TRACK_OVERHEAD_MFM   146     /**< 80×4E + 12×00 + 4×IAM + 50×4E */
#define UFT_SD_TRACK_OVERHEAD_FM    73      /**< 40×00 + 6×00 + 1×IAM + 26×00 */

/* Sector overhead in bytes */
#define UFT_SD_SECTOR_OVERHEAD_MFM  62      /**< MFM sector overhead */
#define UFT_SD_SECTOR_OVERHEAD_FM   33      /**< FM sector overhead */

/* RPM timing */
#define UFT_SD_RPM_TIME_200         300000  /**< 200 RPM in µs */
#define UFT_SD_RPM_TIME_300         200000  /**< 300 RPM in µs */
#define UFT_SD_RPM_TIME_360         166667  /**< 360 RPM in µs */

/** @} */

/*============================================================================
 * FM ADDRESS MARK BIT PATTERNS (32-bit flux patterns)
 *============================================================================*/

/**
 * @defgroup FM_Patterns FM Address Mark Patterns
 * These are the 32-bit flux patterns for FM address marks
 * @{
 */

#define UFT_SD_FM_DDAM      0xAA222888UL    /**< F8/C7 Deleted DAM */
#define UFT_SD_FM_DDAM_ALT  0xAA22288AUL    /**< F9/C7 Alt DDAM */
#define UFT_SD_FM_DAM_ALT   0xAA2228A8UL    /**< FA/C7 Alt DAM */
#define UFT_SD_FM_DAM       0xAA2228AAUL    /**< FB/C7 DAM */
#define UFT_SD_FM_IAM       0xAA2A2A88UL    /**< FC/D7 IAM */
#define UFT_SD_FM_RX02_DAM  0xAA222A8AUL    /**< FD/C7 RX02 DAM */
#define UFT_SD_FM_IDAM      0xAA222AA8UL    /**< FE/C7 IDAM */

/** @} */

/*============================================================================
 * MFM SYNC PATTERNS
 *============================================================================*/

/**
 * @defgroup MFM_Patterns MFM Sync Patterns
 * @{
 */

/** MFM A1 sync with missing clock (16-bit) */
#define UFT_SD_MFM_SYNC_A1          0x4489

/** MFM sync mask for detecting A1 with potential missing clock */
#define UFT_SD_MFM_SYNC_MASK        0xFFDFFFDF

/** Full MFM sync pattern (A1 A1) */
#define UFT_SD_MFM_SYNC_A1A1        0x44894489UL

/** Amiga sync pattern */
#define UFT_SD_AMIGA_SYNC           0xAAAA44894489ULL

/** @} */

/*============================================================================
 * GCR DECODING TABLES (Commodore 64 / Victor 9000)
 *============================================================================*/

/**
 * @brief GCR 5-bit to 4-bit decoding table
 * Used for C64 and Victor 9000 formats
 * 0x00 indicates invalid encoding
 */
static const uint8_t uft_sd_gcr5_decode[32] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  /* 00-07 */
    0x00, 0x08, 0x00, 0x01, 0x00, 0x0C, 0x04, 0x05,  /* 08-0F */
    0x00, 0x00, 0x02, 0x03, 0x00, 0x0F, 0x06, 0x07,  /* 10-17 */
    0x00, 0x09, 0x0A, 0x0B, 0x00, 0x0D, 0x0E, 0x00   /* 18-1F */
};

/**
 * @brief GCR 4-bit to 5-bit encoding table
 */
static const uint8_t uft_sd_gcr5_encode[16] = {
    0x0A, 0x0B, 0x12, 0x13, 0x0E, 0x0F, 0x16, 0x17,
    0x09, 0x19, 0x1A, 0x1B, 0x0D, 0x1D, 0x1E, 0x15
};

/*============================================================================
 * APPLE II 6&2 GCR DECODING
 *============================================================================*/

/**
 * @brief Apple II 6&2 GCR decoding table
 * 128 (0x80) indicates invalid encoding
 */
static const uint8_t uft_sd_gcr62_decode[256] = {
    128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,
    128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,
    128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,
    128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,
    128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,
    128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,
    128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,
    128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,
    128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,
    128,128,128,128,128,128,  0,  1,128,128,  2,  3,128,  4,  5,  6,
    128,128,128,128,128,128,  7,  8,128,128,  8,  9, 10, 11, 12, 13,
    128,128, 14, 15, 16, 17, 18, 19,128, 20, 21, 22, 23, 24, 25, 26,
    128,128,128,128,128,128,128,128,128,128,128, 27,128, 28, 29, 30,
    128,128,128, 31,128,128, 32, 33,128, 34, 35, 36, 37, 38, 39, 40,
    128,128,128,128,128, 41, 42, 43,128, 44, 45, 46, 47, 48, 49, 50,
    128,128, 51, 52, 53, 54, 55, 56,128, 57, 58, 59, 60, 61, 62, 63
};

/*============================================================================
 * VICTOR 9000 VARIABLE SPEED ZONES
 *============================================================================*/

/**
 * @brief Get Victor 9000 bitcell width for cylinder
 * Victor 9000 uses variable speed zones (9 zones total)
 * @param cylinder Cylinder number (0-79)
 * @return Bitcell width in nanoseconds
 */
static inline int uft_sd_victor_bitcell_ns(int cylinder) {
    if (cylinder < 4)  return 1789;
    if (cylinder < 16) return 1896;
    if (cylinder < 27) return 2009;
    if (cylinder < 38) return 2130;
    if (cylinder < 49) return 2272;
    if (cylinder < 60) return 2428;
    if (cylinder < 71) return 2613;
    return 2847;
}

/*============================================================================
 * COPY PROTECTION DETECTION
 *============================================================================*/

/**
 * @defgroup CopyProtection Copy Protection Detection Functions
 * @{
 */

/** Copy protection types */
typedef enum {
    UFT_SD_PROT_NONE = 0,
    UFT_SD_PROT_KBI19,              /**< KBI-19 protection */
    UFT_SD_PROT_SYSTEM24,           /**< Sega System 24 */
    UFT_SD_PROT_SPEEDLOCK_SPECTRUM, /**< Spectrum Speedlock */
    UFT_SD_PROT_SPEEDLOCK_CPC,      /**< CPC Speedlock */
    UFT_SD_PROT_RAINBOW_ARTS,       /**< Rainbow Arts weak sector */
    UFT_SD_PROT_LOGO_PROF,          /**< Logo Prof protection */
    UFT_SD_PROT_OPERA_SOFT,         /**< Opera Soft protection */
    UFT_SD_PROT_8K_SECTOR,          /**< 8K sector protection */
    UFT_SD_PROT_PREHISTORIK,        /**< Prehistorik protection */
    UFT_SD_PROT_11_SECTOR,          /**< 11-sector track */
    UFT_SD_PROT_REUSSIR             /**< Reussir protection */
} uft_sd_protection_t;

/**
 * @brief Check for Speedlock signature
 * @param data Sector data (512 bytes)
 * @return true if Speedlock signature found
 */
static inline bool uft_sd_is_speedlock_signature(const uint8_t *data) {
    /* Check known positions for "SPEEDLOCK" */
    return (memcmp(data + 304, "SPEEDLOCK", 9) == 0 ||
            memcmp(data + 176, "SPEEDLOCK", 9) == 0);
}

/** @} */

/*============================================================================
 * UTILITY FUNCTIONS
 *============================================================================*/

/**
 * @brief Convert sector size to size code
 * @param size Sector size in bytes (128, 256, 512, 1024, etc.)
 * @return Size code (0-7), or 0xFF if invalid
 */
static inline uint8_t uft_sd_size_to_code(int size) {
    switch (size) {
        case 128:   return 0;
        case 256:   return 1;
        case 512:   return 2;
        case 1024:  return 3;
        case 2048:  return 4;
        case 4096:  return 5;
        case 8192:  return 6;
        case 16384: return 7;
        default:    return 0xFF;
    }
}

/**
 * @brief Convert size code to sector size
 * @param code Size code (0-7)
 * @return Sector size in bytes
 */
static inline int uft_sd_code_to_size(uint8_t code) {
    return 128 << (code & 7);
}

/**
 * @brief Calculate track capacity in bytes
 * @param rpm Drive RPM (300 or 360)
 * @param datarate Data rate
 * @param encoding Encoding type
 * @return Track capacity in bytes
 */
static inline int uft_sd_track_capacity(int rpm, uft_sd_datarate_t datarate,
                                         uft_sd_encoding_t encoding) {
    int usecs = (rpm == 360) ? UFT_SD_RPM_TIME_360 : UFT_SD_RPM_TIME_300;
    int bits = (datarate * usecs) / 1000000;
    
    /* FM uses twice as many raw bits per data bit */
    if (encoding == UFT_SD_ENC_FM) {
        bits /= 2;
    }
    
    return bits / 8;  /* Convert to bytes */
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_UFT_SAM_CORE_H */
