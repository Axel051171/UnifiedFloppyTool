/**
 * @file uft_protection_ext.h
 * @brief Extended Copy Protection Detection - Longtrack Variants
 * 
 * Extension to uft_protection.h with specific longtrack detection:
 * - PROTEC (multiple variants)
 * - Protoscan/Tiertex (Lotus I/II, Strider II)
 * - Silmarils/Lankhor (French publishers)
 * - Infogrames (Hostages, etc.)
 * - Prolance (B.A.T. by Ubisoft)
 * - APP (Amiga Power Pack)
 * - Seven Cities Of Gold
 * - Super Methane Bros (GCR)
 * 
 * Based on analysis of Disk-Utilities by Keir Fraser.
 * 
 * @copyright UFT Project
 */

#ifndef UFT_PROTECTION_EXT_H
#define UFT_PROTECTION_EXT_H

#include "uft_protection.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Longtrack Type Enumeration
 *============================================================================*/

/**
 * @brief Specific longtrack protection types
 */
typedef enum {
    UFT_LONGTRACK_UNKNOWN       = 0,
    
    /* PROTEC family */
    UFT_LONGTRACK_PROTEC        = 0x0001,   /**< Standard PROTEC: 0x4454 sync */
    
    /* Protoscan family */
    UFT_LONGTRACK_PROTOSCAN     = 0x0010,   /**< Lotus I/II: 0x41244124 sync */
    UFT_LONGTRACK_TIERTEX       = 0x0011,   /**< Strider II variant */
    
    /* French publisher protections */
    UFT_LONGTRACK_SILMARILS     = 0x0020,   /**< Silmarils/Lankhor: 0xa144 + "ROD0" */
    UFT_LONGTRACK_INFOGRAMES    = 0x0021,   /**< Hostages etc: 0xa144 */
    
    /* Ubisoft/Others */
    UFT_LONGTRACK_PROLANCE      = 0x0030,   /**< B.A.T.: 0x8945 sync */
    UFT_LONGTRACK_APP           = 0x0031,   /**< Amiga Power Pack: 0x924a */
    UFT_LONGTRACK_SEVENCITIES   = 0x0032,   /**< Seven Cities Of Gold: dual sync */
    
    /* Special */
    UFT_LONGTRACK_SUPERMETHANEBROS = 0x0040,/**< GCR track: 0x99999999 */
    UFT_LONGTRACK_EMPTY         = 0x0050,   /**< Empty long track */
    UFT_LONGTRACK_ZEROES        = 0x0051,   /**< All MFM zeroes */
    UFT_LONGTRACK_RNC_EMPTY     = 0x0052    /**< RNC dual-format empty */
} uft_longtrack_type_t;

/*============================================================================
 * Longtrack Sync Markers
 *============================================================================*/

/** PROTEC sync word */
#define UFT_SYNC_PROTEC             0x4454

/** Protoscan sync word (32-bit) */
#define UFT_SYNC_PROTOSCAN          0x41244124

/** Silmarils/Infogrames sync word */
#define UFT_SYNC_SILMARILS          0xa144

/** Silmarils signature */
#define UFT_SIG_SILMARILS           "ROD0"
#define UFT_SIG_SILMARILS_LEN       4

/** Prolance sync word */
#define UFT_SYNC_PROLANCE           0x8945

/** APP sync word */
#define UFT_SYNC_APP                0x924a

/** Seven Cities sync words */
#define UFT_SYNC_SEVENCITIES_1      0x9251
#define UFT_SYNC_SEVENCITIES_2      0x924a

/** Super Methane Bros GCR pattern */
#define UFT_PATTERN_SUPERMETHANEBROS 0x99999999

/*============================================================================
 * Longtrack Length Requirements
 *============================================================================*/

/** Minimum track lengths for each type */
#define UFT_MINBITS_PROTEC          107200
#define UFT_MINBITS_PROTOSCAN       102400
#define UFT_MINBITS_TIERTEX_MIN     99328
#define UFT_MINBITS_TIERTEX_MAX     103680
#define UFT_MINBITS_SILMARILS       104128
#define UFT_MINBITS_INFOGRAMES      104160
#define UFT_MINBITS_PROLANCE        109152
#define UFT_MINBITS_APP             110000
#define UFT_MINBITS_SEVENCITIES     101500
#define UFT_MINBITS_EMPTY           105000

/** Default track lengths for generation */
#define UFT_DEFBITS_PROTEC          110000
#define UFT_DEFBITS_PROTOSCAN       105500
#define UFT_DEFBITS_TIERTEX         100150
#define UFT_DEFBITS_SILMARILS       110000
#define UFT_DEFBITS_INFOGRAMES      105500
#define UFT_DEFBITS_PROLANCE        110000
#define UFT_DEFBITS_APP             111000
#define UFT_DEFBITS_SEVENCITIES     101500
#define UFT_DEFBITS_EMPTY           110000

/*============================================================================
 * Longtrack Detection Result
 *============================================================================*/

/**
 * @brief Extended longtrack detection result
 */
typedef struct {
    bool                 detected;          /**< Any longtrack found */
    uft_longtrack_type_t type;              /**< Specific type detected */
    float                confidence;        /**< Detection confidence 0.0-1.0 */
    
    /* Track measurements */
    uint32_t sync_word;                     /**< Found sync word */
    uint32_t track_bits;                    /**< Measured track length */
    uint32_t min_required;                  /**< Minimum for this type */
    uint16_t percent;                       /**< Percentage of standard */
    
    /* Pattern info */
    uint8_t  pattern_byte;                  /**< Detected pattern byte */
    uint32_t pattern_count;                 /**< Repetitions found */
    
    /* Signature */
    bool     signature_found;               /**< Signature present */
    char     signature[16];                 /**< Found signature */
    
    /* Extra data */
    uint8_t  extra_data[128];               /**< Extra protection data */
    uint8_t  extra_data_len;                /**< Extra data length */
    uint16_t crc;                           /**< CRC if applicable */
    
    /* Track position */
    uint32_t data_bitoff;                   /**< Bit offset of sync */
} uft_longtrack_ext_t;

/*============================================================================
 * Longtrack Definition Table Entry
 *============================================================================*/

/**
 * @brief Longtrack type definition
 */
typedef struct {
    uft_longtrack_type_t type;
    const char          *name;
    uint32_t             sync_word;
    uint8_t              sync_bits;         /**< 16 or 32 */
    uint32_t             min_bits;
    uint32_t             default_bits;
    uint8_t              pattern_byte;      /**< 0 = variable */
    const char          *signature;
    uint8_t              signature_len;
} uft_longtrack_def_t;

/** Longtrack definitions table */
extern const uft_longtrack_def_t uft_longtrack_defs[];

/** Number of longtrack definitions */
#define UFT_LONGTRACK_DEF_COUNT  12

/*============================================================================
 * Specific Longtrack Detection Functions
 *============================================================================*/

/**
 * @brief Detect any longtrack type
 * 
 * Tries all known longtrack types and returns best match.
 * 
 * @param[in]  track_data   Raw MFM track data
 * @param[in]  track_bits   Track length in bits
 * @param[in]  flux_times   Optional flux timing data
 * @param[in]  num_flux     Number of flux times
 * @param[out] result       Detection result
 * @return true if any longtrack detected
 */
bool uft_detect_longtrack_ext(
    const uint8_t *track_data,
    size_t track_bits,
    const uint32_t *flux_times,
    size_t num_flux,
    uft_longtrack_ext_t *result
);

/**
 * @brief Detect PROTEC longtrack
 * 
 * Sync: 0x4454, Pattern: variable (often 0x33), Min: 107,200 bits
 * Protection checks >= 6700 raw words between sync marks.
 * 
 * @param[in]  track_data   Raw MFM track data
 * @param[in]  track_bits   Track length in bits
 * @param[out] result       Detection result
 * @return true if detected
 */
bool uft_detect_longtrack_protec(
    const uint8_t *track_data,
    size_t track_bits,
    uft_longtrack_ext_t *result
);

/**
 * @brief Detect Protoscan longtrack (Lotus I/II)
 * 
 * Sync: 0x41244124 (32-bit), Pattern: MFM zeroes, Min: 102,400 bits
 * Protection checks >= 6400 raw words between sync marks.
 * 
 * @param[in]  track_data   Raw MFM track data
 * @param[in]  track_bits   Track length in bits
 * @param[out] result       Detection result
 * @return true if detected
 */
bool uft_detect_longtrack_protoscan(
    const uint8_t *track_data,
    size_t track_bits,
    uft_longtrack_ext_t *result
);

/**
 * @brief Detect Tiertex longtrack (Strider II)
 * 
 * Variant of Protoscan with different length check:
 * 99,328 <= bits <= 103,680 (6208 <= words <= 6480)
 * Actually ~100,150 bits (normal length!)
 * 
 * @param[in]  track_data   Raw MFM track data
 * @param[in]  track_bits   Track length in bits
 * @param[out] result       Detection result
 * @return true if detected
 */
bool uft_detect_longtrack_tiertex(
    const uint8_t *track_data,
    size_t track_bits,
    uft_longtrack_ext_t *result
);

/**
 * @brief Detect Silmarils longtrack
 * 
 * Sync: 0xa144, Signature: "ROD0", Pattern: MFM zeroes, Min: 104,128 bits
 * Used by French publishers Silmarils and Lankhor.
 * 
 * @param[in]  track_data   Raw MFM track data
 * @param[in]  track_bits   Track length in bits
 * @param[out] result       Detection result
 * @return true if detected
 */
bool uft_detect_longtrack_silmarils(
    const uint8_t *track_data,
    size_t track_bits,
    uft_longtrack_ext_t *result
);

/**
 * @brief Detect Infogrames longtrack
 * 
 * Sync: 0xa144, Pattern: MFM zeroes (>13020 0xaa bytes), Min: 104,160 bits
 * Used on Hostages, Jumping Jack Son, etc.
 * 
 * @param[in]  track_data   Raw MFM track data
 * @param[in]  track_bits   Track length in bits
 * @param[out] result       Detection result
 * @return true if detected
 */
bool uft_detect_longtrack_infogrames(
    const uint8_t *track_data,
    size_t track_bits,
    uft_longtrack_ext_t *result
);

/**
 * @brief Detect Prolance longtrack (B.A.T.)
 * 
 * Sync: 0x8945, Pattern: MFM zeroes (>= 3412 longwords), Min: 109,152 bits
 * PROTEC variant used by Ubisoft.
 * 
 * @param[in]  track_data   Raw MFM track data
 * @param[in]  track_bits   Track length in bits
 * @param[out] result       Detection result
 * @return true if detected
 */
bool uft_detect_longtrack_prolance(
    const uint8_t *track_data,
    size_t track_bits,
    uft_longtrack_ext_t *result
);

/**
 * @brief Detect APP longtrack (Amiga Power Pack)
 * 
 * Sync: 0x924a, Pattern: 0xdc (6600 times), Min: 110,000 bits
 * 
 * @param[in]  track_data   Raw MFM track data
 * @param[in]  track_bits   Track length in bits
 * @param[out] result       Detection result
 * @return true if detected
 */
bool uft_detect_longtrack_app(
    const uint8_t *track_data,
    size_t track_bits,
    uft_longtrack_ext_t *result
);

/**
 * @brief Detect Seven Cities Of Gold protection
 * 
 * Dual sync: 0x9251 and 0x924a, 122 bytes with CRC 0x010a
 * Track length ~101,500 bits.
 * 
 * @param[in]  track_data   Raw MFM track data
 * @param[in]  track_bits   Track length in bits
 * @param[out] result       Detection result
 * @return true if detected
 */
bool uft_detect_longtrack_sevencities(
    const uint8_t *track_data,
    size_t track_bits,
    uft_longtrack_ext_t *result
);

/**
 * @brief Detect Super Methane Bros GCR track
 * 
 * Encoding: GCR (4µs bit time), Pattern: 0x99999999
 * Track length: ~52,750 GCR bits
 * 
 * @param[in]  track_data   Raw track data
 * @param[in]  track_bits   Track length in bits
 * @param[in]  flux_times   Flux timing (required for GCR detection)
 * @param[in]  num_flux     Number of flux times
 * @param[out] result       Detection result
 * @return true if detected
 */
bool uft_detect_longtrack_supermethanebros(
    const uint8_t *track_data,
    size_t track_bits,
    const uint32_t *flux_times,
    size_t num_flux,
    uft_longtrack_ext_t *result
);

/**
 * @brief Detect empty/zeroes long track
 * 
 * @param[in]  track_data   Raw MFM track data
 * @param[in]  track_bits   Track length in bits
 * @param[out] result       Detection result
 * @return true if detected
 */
bool uft_detect_longtrack_empty(
    const uint8_t *track_data,
    size_t track_bits,
    uft_longtrack_ext_t *result
);

/*============================================================================
 * Longtrack Generation Functions
 *============================================================================*/

/**
 * @brief Generate longtrack of specified type
 * 
 * @param[in]  type         Longtrack type
 * @param[in]  params       Optional parameters (NULL for defaults)
 * @param[out] track_data   Output buffer (caller allocates)
 * @param[out] track_bits   Output track length
 * @return Bytes written, 0 on error
 */
size_t uft_generate_longtrack(
    uft_longtrack_type_t type,
    const uft_longtrack_ext_t *params,
    uint8_t *track_data,
    uint32_t *track_bits
);

/**
 * @brief Generate PROTEC longtrack
 */
size_t uft_generate_longtrack_protec(
    uint8_t pattern_byte,
    uint32_t total_bits,
    uint8_t *track_data
);

/**
 * @brief Generate Protoscan longtrack
 */
size_t uft_generate_longtrack_protoscan(
    uint32_t total_bits,
    uint8_t *track_data
);

/**
 * @brief Generate Silmarils longtrack
 */
size_t uft_generate_longtrack_silmarils(
    uint32_t total_bits,
    uint8_t *track_data
);

/*============================================================================
 * Utility Functions
 *============================================================================*/

/**
 * @brief Get longtrack type name
 */
const char* uft_longtrack_type_name(uft_longtrack_type_t type);

/**
 * @brief Get longtrack definition
 */
const uft_longtrack_def_t* uft_longtrack_get_def(uft_longtrack_type_t type);

/**
 * @brief Check if byte sequence repeats
 * 
 * @param[in]  data      MFM data
 * @param[in]  bits      Data length in bits
 * @param[in]  offset    Start offset in bits
 * @param[in]  byte      Expected byte value
 * @param[in]  count     Minimum repetitions
 * @return Actual repetition count, 0 if less than count
 */
uint32_t uft_check_pattern_sequence(
    const uint8_t *data,
    size_t bits,
    size_t offset,
    uint8_t byte,
    uint32_t count
);

/**
 * @brief Calculate CRC16-CCITT
 */
uint16_t uft_crc16_ccitt(const uint8_t *data, size_t len);

/**
 * @brief Print extended longtrack result
 */
void uft_longtrack_ext_print(const uft_longtrack_ext_t *result, bool verbose);

#ifdef __cplusplus
}
#endif

#endif /* UFT_PROTECTION_EXT_H */
