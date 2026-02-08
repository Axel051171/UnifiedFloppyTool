/**
 * @file uft_longtrack.h
 * @brief Longtrack Protection Collection Handler
 * 
 * Implements detection of various Amiga longtrack protection schemes.
 * Clean-room reimplementation for UFT.
 * 
 * Longtrack protections work by creating tracks that exceed the
 * normal ~105000 bits, making them impossible to copy on standard
 * hardware with fixed timing.
 * 
 * Supported variants:
 * - PROTEC (sync 0x4454, 107200+ bits)
 * - Protoscan (sync 0x41244124, 102400+ bits) - Lotus I/II
 * - Tiertex (sync 0x41244124, 99328-103680 bits) - Strider II
 * - Silmarils (sync 0xa144, 104128+ bits) - French publishers
 * - Infogrames (sync 0xa144, 104160+ bits) - Hostages
 * - Prolance (sync 0x8945, 109152+ bits) - B.A.T.
 * - APP (sync 0x924a, 110000+ bits) - Amiga Power Pack
 * - SevenCities (sync 0x9251/0x924a, 101500+ bits)
 * - SuperMethaneBros (GCR 0x99999999, 105500+ bits)
 * 
 * @version 1.0.0
 * @date 2026-01-04
 * @author UFT Team
 * 
 * SPDX-License-Identifier: MIT
 */

#ifndef UFT_LONGTRACK_H
#define UFT_LONGTRACK_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Constants
 *===========================================================================*/

/** Standard Amiga track length */
#define UFT_LONGTRACK_AMIGA_NORMAL      105000  /**< ~105000 bits standard */

/** Longtrack type count */
#define UFT_LONGTRACK_TYPE_COUNT        11

/** Maximum signature length */
#define UFT_LONGTRACK_MAX_SIG_LEN       16

/** Pattern detection window */
#define UFT_LONGTRACK_PATTERN_WINDOW    256

/*===========================================================================
 * Longtrack Type Enumeration
 *===========================================================================*/

/**
 * @brief Longtrack protection type
 */
typedef enum {
    UFT_LONGTRACK_UNKNOWN = 0,         /**< Not detected */
    UFT_LONGTRACK_PROTEC,              /**< PROTEC protection */
    UFT_LONGTRACK_PROTOSCAN,           /**< Protoscan (Lotus) */
    UFT_LONGTRACK_TIERTEX,             /**< Tiertex (Strider II) */
    UFT_LONGTRACK_SILMARILS,           /**< Silmarils (French) */
    UFT_LONGTRACK_INFOGRAMES,          /**< Infogrames (Hostages) */
    UFT_LONGTRACK_PROLANCE,            /**< Prolance (B.A.T.) */
    UFT_LONGTRACK_APP,                 /**< Amiga Power Pack */
    UFT_LONGTRACK_SEVENCITIES,         /**< Seven Cities of Gold */
    UFT_LONGTRACK_SUPERMETHANEBROS,    /**< Super Methane Brothers (GCR) */
    UFT_LONGTRACK_EMPTY,               /**< Empty longtrack */
    UFT_LONGTRACK_ZEROES               /**< All-zeros longtrack */
} uft_longtrack_type_t;

/*===========================================================================
 * Longtrack Definitions Table
 *===========================================================================*/

/**
 * @brief Longtrack type definition
 */
typedef struct {
    uft_longtrack_type_t type;
    const char          *name;
    uint32_t            sync_word;     /**< Primary sync marker */
    uint32_t            sync_word_alt; /**< Alternative sync (0 if none) */
    uint32_t            min_bits;      /**< Minimum track length */
    uint32_t            max_bits;      /**< Maximum track length (0 = unlimited) */
    uint8_t             pattern_byte;  /**< Fill pattern (0xFF = variable) */
    const char          *signature;    /**< Text signature (NULL if none) */
    uint8_t             sig_len;       /**< Signature length */
    bool                is_gcr;        /**< True if GCR encoded */
} uft_longtrack_def_t;

/**
 * @brief Longtrack definitions table
 */
static const uft_longtrack_def_t uft_longtrack_defs[UFT_LONGTRACK_TYPE_COUNT] = {
    /* PROTEC: Variable pattern (0x33 typical), many games */
    {
        .type = UFT_LONGTRACK_PROTEC,
        .name = "PROTEC",
        .sync_word = 0x4454,
        .sync_word_alt = 0,
        .min_bits = 107200,
        .max_bits = 0,
        .pattern_byte = 0x33,
        .signature = NULL,
        .sig_len = 0,
        .is_gcr = false
    },
    /* Protoscan: Lotus I/II and others */
    {
        .type = UFT_LONGTRACK_PROTOSCAN,
        .name = "Protoscan",
        .sync_word = 0x41244124,
        .sync_word_alt = 0,
        .min_bits = 102400,
        .max_bits = 0,
        .pattern_byte = 0x00,
        .signature = NULL,
        .sig_len = 0,
        .is_gcr = false
    },
    /* Tiertex: Strider II */
    {
        .type = UFT_LONGTRACK_TIERTEX,
        .name = "Tiertex",
        .sync_word = 0x41244124,
        .sync_word_alt = 0,
        .min_bits = 99328,
        .max_bits = 103680,
        .pattern_byte = 0x00,
        .signature = NULL,
        .sig_len = 0,
        .is_gcr = false
    },
    /* Silmarils: French publishers */
    {
        .type = UFT_LONGTRACK_SILMARILS,
        .name = "Silmarils",
        .sync_word = 0xA144,
        .sync_word_alt = 0,
        .min_bits = 104128,
        .max_bits = 0,
        .pattern_byte = 0x00,
        .signature = "ROD0",
        .sig_len = 4,
        .is_gcr = false
    },
    /* Infogrames: Hostages and others */
    {
        .type = UFT_LONGTRACK_INFOGRAMES,
        .name = "Infogrames",
        .sync_word = 0xA144,
        .sync_word_alt = 0,
        .min_bits = 104160,
        .max_bits = 0,
        .pattern_byte = 0x00,
        .signature = NULL,
        .sig_len = 0,
        .is_gcr = false
    },
    /* Prolance: B.A.T. */
    {
        .type = UFT_LONGTRACK_PROLANCE,
        .name = "Prolance",
        .sync_word = 0x8945,
        .sync_word_alt = 0,
        .min_bits = 109152,
        .max_bits = 0,
        .pattern_byte = 0x00,
        .signature = NULL,
        .sig_len = 0,
        .is_gcr = false
    },
    /* APP: Amiga Power Pack */
    {
        .type = UFT_LONGTRACK_APP,
        .name = "APP",
        .sync_word = 0x924A,
        .sync_word_alt = 0,
        .min_bits = 110000,
        .max_bits = 0,
        .pattern_byte = 0xDC,
        .signature = NULL,
        .sig_len = 0,
        .is_gcr = false
    },
    /* SevenCities: Seven Cities of Gold */
    {
        .type = UFT_LONGTRACK_SEVENCITIES,
        .name = "SevenCities",
        .sync_word = 0x9251,
        .sync_word_alt = 0x924A,
        .min_bits = 101500,
        .max_bits = 0,
        .pattern_byte = 0x00,
        .signature = NULL, /* Has 122-byte signature block */
        .sig_len = 122,
        .is_gcr = false
    },
    /* SuperMethaneBros: GCR encoded */
    {
        .type = UFT_LONGTRACK_SUPERMETHANEBROS,
        .name = "SuperMethaneBros",
        .sync_word = 0x99999999,
        .sync_word_alt = 0,
        .min_bits = 105500 / 2, /* GCR doubles effective density */
        .max_bits = 0,
        .pattern_byte = 0xFF,
        .signature = NULL,
        .sig_len = 0,
        .is_gcr = true
    },
    /* Empty longtrack */
    {
        .type = UFT_LONGTRACK_EMPTY,
        .name = "Empty",
        .sync_word = 0,
        .sync_word_alt = 0,
        .min_bits = UFT_LONGTRACK_AMIGA_NORMAL,
        .max_bits = 0,
        .pattern_byte = 0xFF,
        .signature = NULL,
        .sig_len = 0,
        .is_gcr = false
    },
    /* All-zeros longtrack */
    {
        .type = UFT_LONGTRACK_ZEROES,
        .name = "Zeroes",
        .sync_word = 0,
        .sync_word_alt = 0,
        .min_bits = UFT_LONGTRACK_AMIGA_NORMAL,
        .max_bits = 0,
        .pattern_byte = 0x00,
        .signature = NULL,
        .sig_len = 0,
        .is_gcr = false
    }
};

/*===========================================================================
 * Types
 *===========================================================================*/

/**
 * @brief Longtrack confidence level
 */
typedef enum {
    UFT_LONGTRACK_CONF_NONE = 0,
    UFT_LONGTRACK_CONF_POSSIBLE,       /**< Length only */
    UFT_LONGTRACK_CONF_LIKELY,         /**< Length + sync */
    UFT_LONGTRACK_CONF_CERTAIN         /**< Length + sync + signature/pattern */
} uft_longtrack_confidence_t;

/**
 * @brief Longtrack detection info
 */
typedef struct {
    uft_longtrack_type_t       type;
    const uft_longtrack_def_t *def;        /**< Pointer to definition */
    
    /* Sync analysis */
    uint32_t                   sync_word;      /**< Detected sync word */
    int32_t                    sync_offset;    /**< Bit position of sync */
    
    /* Track analysis */
    uint32_t                   min_track_bits; /**< Expected minimum */
    uint32_t                   actual_track_bits;
    float                      length_ratio;   /**< actual/normal ratio */
    
    /* Pattern analysis */
    uint8_t                    pattern_byte;   /**< Detected fill pattern */
    uint32_t                   pattern_start;  /**< Where pattern begins */
    uint32_t                   pattern_length; /**< Length of pattern region */
    float                      pattern_match;  /**< Percentage match (0-100) */
    
    /* Signature */
    bool                       signature_found;
    uint8_t                    signature[UFT_LONGTRACK_MAX_SIG_LEN];
    uint8_t                    signature_len;
} uft_longtrack_info_t;

/**
 * @brief Longtrack detection result
 */
typedef struct {
    /* Detection status */
    bool                       detected;
    uft_longtrack_confidence_t confidence;
    
    /* Primary detection */
    uft_longtrack_info_t       primary;
    
    /* Secondary candidates (if ambiguous) */
    uint8_t                    candidate_count;
    uft_longtrack_info_t       candidates[3];
    
    /* Track info */
    uint8_t                    track;
    uint8_t                    head;
    uint32_t                   track_bits;
    
    /* Raw statistics */
    uint8_t                    byte_histogram[256]; /**< Simplified histogram */
    uint8_t                    dominant_byte;
    float                      homogeneity;         /**< How uniform is fill? */
    
    /* Diagnostics */
    char                       info[256];
} uft_longtrack_result_t;

/*===========================================================================
 * Detection Functions
 *===========================================================================*/

/**
 * @brief Detect longtrack protection
 * 
 * Analyzes track for longtrack protection:
 * 1. Check if track exceeds normal length
 * 2. Search for characteristic sync markers
 * 3. Analyze fill pattern
 * 4. Look for signatures
 * 
 * @param track_data Raw track data
 * @param track_bits Number of bits in track
 * @param track Track number
 * @param head Head number
 * @param result Output: detection result
 * @return 0 on success
 */
int uft_longtrack_detect(const uint8_t *track_data,
                         uint32_t track_bits,
                         uint8_t track,
                         uint8_t head,
                         uft_longtrack_result_t *result);

/**
 * @brief Quick check if track is potentially longtrack
 * 
 * @param track_bits Track length in bits
 * @return true if track exceeds normal length
 */
static inline bool uft_longtrack_is_long(uint32_t track_bits) {
    return (track_bits > UFT_LONGTRACK_AMIGA_NORMAL + 500);
}

/**
 * @brief Detect specific longtrack type
 * 
 * @param track_data Raw track data
 * @param track_bits Track length
 * @param type Type to detect
 * @param info Output: detection info
 * @return true if type detected
 */
bool uft_longtrack_detect_type(const uint8_t *track_data,
                                uint32_t track_bits,
                                uft_longtrack_type_t type,
                                uft_longtrack_info_t *info);

/**
 * @brief Find sync word in track
 * 
 * @param track_data Raw track data
 * @param track_bits Track length
 * @param sync Sync word to find (16 or 32 bit)
 * @param is_32bit True if sync is 32-bit
 * @return Bit position, or -1 if not found
 */
int32_t uft_longtrack_find_sync(const uint8_t *track_data,
                                 uint32_t track_bits,
                                 uint32_t sync,
                                 bool is_32bit);

/**
 * @brief Analyze fill pattern
 * 
 * @param track_data Raw track data
 * @param track_bits Track length
 * @param start_bit Starting position
 * @param pattern Output: detected pattern byte
 * @param match Output: match percentage
 * @return Length of pattern region
 */
uint32_t uft_longtrack_analyze_pattern(const uint8_t *track_data,
                                        uint32_t track_bits,
                                        uint32_t start_bit,
                                        uint8_t *pattern,
                                        float *match);

/*===========================================================================
 * Specific Detectors
 *===========================================================================*/

/**
 * @brief Detect PROTEC longtrack
 */
bool uft_longtrack_detect_protec(const uint8_t *track_data,
                                  uint32_t track_bits,
                                  uft_longtrack_info_t *info);

/**
 * @brief Detect Protoscan longtrack (Lotus)
 */
bool uft_longtrack_detect_protoscan(const uint8_t *track_data,
                                     uint32_t track_bits,
                                     uft_longtrack_info_t *info);

/**
 * @brief Detect Tiertex longtrack (Strider II)
 */
bool uft_longtrack_detect_tiertex(const uint8_t *track_data,
                                   uint32_t track_bits,
                                   uft_longtrack_info_t *info);

/**
 * @brief Detect Silmarils longtrack
 */
bool uft_longtrack_detect_silmarils(const uint8_t *track_data,
                                     uint32_t track_bits,
                                     uft_longtrack_info_t *info);

/**
 * @brief Detect Infogrames longtrack
 */
bool uft_longtrack_detect_infogrames(const uint8_t *track_data,
                                      uint32_t track_bits,
                                      uft_longtrack_info_t *info);

/**
 * @brief Detect Prolance longtrack (B.A.T.)
 */
bool uft_longtrack_detect_prolance(const uint8_t *track_data,
                                    uint32_t track_bits,
                                    uft_longtrack_info_t *info);

/**
 * @brief Detect APP longtrack (Amiga Power Pack)
 */
bool uft_longtrack_detect_app(const uint8_t *track_data,
                               uint32_t track_bits,
                               uft_longtrack_info_t *info);

/**
 * @brief Detect Seven Cities of Gold longtrack
 */
bool uft_longtrack_detect_sevencities(const uint8_t *track_data,
                                       uint32_t track_bits,
                                       uft_longtrack_info_t *info);

/**
 * @brief Detect Super Methane Bros GCR longtrack
 */
bool uft_longtrack_detect_supermethanebros(const uint8_t *track_data,
                                            uint32_t track_bits,
                                            uft_longtrack_info_t *info);

/*===========================================================================
 * Reporting Functions
 *===========================================================================*/

/**
 * @brief Generate longtrack analysis report
 */
size_t uft_longtrack_report(const uft_longtrack_result_t *result,
                             char *buffer, size_t buffer_size);

/**
 * @brief Export longtrack analysis to JSON
 */
size_t uft_longtrack_export_json(const uft_longtrack_result_t *result,
                                  char *buffer, size_t buffer_size);

/**
 * @brief Get type name as string
 */
const char* uft_longtrack_type_name(uft_longtrack_type_t type);

/**
 * @brief Get confidence name as string
 */
const char* uft_longtrack_confidence_name(uft_longtrack_confidence_t conf);

/**
 * @brief Get definition for type
 */
const uft_longtrack_def_t* uft_longtrack_get_def(uft_longtrack_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* UFT_LONGTRACK_H */
