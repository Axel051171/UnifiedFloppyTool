/**
 * @file uft_score.h
 * @brief Unified Confidence Scoring System
 * @version 1.0.0
 * 
 * Consolidates all *_score_t structures from format parsers into
 * a single, reusable type system.
 * 
 * COUPLING FIX: Eliminates 55+ duplicate score type definitions
 * across src/formats/
 * 
 * Usage:
 *   uft_format_score_t score = uft_score_init();
 *   uft_score_add_match(&score, "magic", UFT_SCORE_WEIGHT_HIGH);
 *   uft_score_add_match(&score, "structure", UFT_SCORE_WEIGHT_MEDIUM);
 *   if (uft_score_is_valid(&score)) { ... }
 */

#ifndef UFT_SCORE_H
#define UFT_SCORE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Score Weights (Standard Values)
 * ═══════════════════════════════════════════════════════════════════════════════ */

#define UFT_SCORE_WEIGHT_NONE       0.0f
#define UFT_SCORE_WEIGHT_MINIMAL    0.05f
#define UFT_SCORE_WEIGHT_LOW        0.10f
#define UFT_SCORE_WEIGHT_MEDIUM     0.20f
#define UFT_SCORE_WEIGHT_HIGH       0.30f
#define UFT_SCORE_WEIGHT_CRITICAL   0.40f
#define UFT_SCORE_WEIGHT_MAGIC      0.50f   /* Magic bytes match */

/* ═══════════════════════════════════════════════════════════════════════════════
 * Score Thresholds
 * ═══════════════════════════════════════════════════════════════════════════════ */

#define UFT_SCORE_THRESHOLD_REJECT      0.20f   /* Below: definitely not this format */
#define UFT_SCORE_THRESHOLD_POSSIBLE    0.40f   /* Maybe this format */
#define UFT_SCORE_THRESHOLD_LIKELY      0.60f   /* Probably this format */
#define UFT_SCORE_THRESHOLD_CONFIDENT   0.80f   /* Confident match */
#define UFT_SCORE_THRESHOLD_CERTAIN     0.95f   /* Near-certain match */

/* ═══════════════════════════════════════════════════════════════════════════════
 * Score Types
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Match detail for audit trail
 */
typedef struct {
    const char *field;              /**< What was checked (e.g., "magic", "checksum") */
    float weight;                   /**< Weight contribution */
    bool matched;                   /**< Did it match? */
    const char *note;               /**< Optional note */
} uft_score_match_t;

/**
 * @brief Universal format detection score
 * 
 * Replaces all format-specific *_score_t structures:
 * - adf_score_t, d64_score_t, dsk_score_t, etc.
 */
typedef struct {
    float overall;                  /**< Combined score (0.0 - 1.0) */
    bool valid;                     /**< Is this a valid detection? */
    
    /* Format-specific fields (union for space efficiency) */
    union {
        struct {
            uint8_t type;           /**< Format subtype */
            uint8_t variant;        /**< Format variant */
        } generic;
        
        struct {
            bool has_bootblock;     /**< Amiga bootblock present */
            uint8_t fs_type;        /**< Filesystem type (OFS/FFS) */
        } amiga;
        
        struct {
            uint8_t dos_type;       /**< D64: DOS type */
            uint8_t tracks;         /**< Track count (35/40) */
            bool has_errors;        /**< Error bytes present */
        } c64;
        
        struct {
            uint8_t media_type;     /**< PC: media descriptor */
            uint16_t sectors;       /**< Sectors per track */
            uint8_t fat_type;       /**< FAT12/16/32 */
        } pc;
        
        struct {
            bool is_extended;       /**< Extended DSK? */
            uint8_t sides;          /**< Side count */
            uint8_t tracks;         /**< Track count */
        } cpc;
        
        struct {
            uint8_t type;           /**< TRD type */
            uint8_t tracks;         /**< Track count (40/80) */
            bool is_double;         /**< Double-sided? */
        } spectrum;
    } detail;
    
    /* Match audit trail */
    uft_score_match_t matches[8];   /**< What contributed to score */
    size_t match_count;             /**< Number of matches recorded */
    
    /* Metadata */
    const char *format_name;        /**< Detected format name */
    const char *format_ext;         /**< File extension */
    uint32_t format_id;             /**< UFT format ID */
} uft_format_score_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Score Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Initialize score structure
 */
static inline uft_format_score_t uft_score_init(void) {
    uft_format_score_t s = {0};
    s.overall = 0.0f;
    s.valid = false;
    s.match_count = 0;
    return s;
}

/**
 * @brief Add a match to the score
 * @param score Score structure
 * @param field Field name that was checked
 * @param weight Weight if matched
 * @param matched Whether it matched
 * @param note Optional note
 */
static inline void uft_score_add_match(
    uft_format_score_t *score,
    const char *field,
    float weight,
    bool matched,
    const char *note
) {
    if (!score) return;
    
    if (matched) {
        score->overall += weight;
    }
    
    if (score->match_count < 8) {
        score->matches[score->match_count].field = field;
        score->matches[score->match_count].weight = weight;
        score->matches[score->match_count].matched = matched;
        score->matches[score->match_count].note = note;
        score->match_count++;
    }
}

/**
 * @brief Check if score indicates valid format
 */
static inline bool uft_score_is_valid(const uft_format_score_t *score) {
    if (!score) return false;
    return score->valid && score->overall >= UFT_SCORE_THRESHOLD_POSSIBLE;
}

/**
 * @brief Check if score indicates confident match
 */
static inline bool uft_score_is_confident(const uft_format_score_t *score) {
    if (!score) return false;
    return score->valid && score->overall >= UFT_SCORE_THRESHOLD_CONFIDENT;
}

/**
 * @brief Finalize and validate score
 */
static inline void uft_score_finalize(uft_format_score_t *score) {
    if (!score) return;
    
    /* Clamp to [0, 1] */
    if (score->overall < 0.0f) score->overall = 0.0f;
    if (score->overall > 1.0f) score->overall = 1.0f;
    
    /* Set validity based on threshold */
    score->valid = (score->overall >= UFT_SCORE_THRESHOLD_POSSIBLE);
}

/**
 * @brief Convert score to confidence (0-10000)
 */
static inline uint16_t uft_score_to_confidence(const uft_format_score_t *score) {
    if (!score) return 0;
    return (uint16_t)(score->overall * 10000.0f);
}

/**
 * @brief Compare two scores (for sorting)
 * @return Positive if a > b, negative if a < b, zero if equal
 */
static inline int uft_score_compare(
    const uft_format_score_t *a,
    const uft_format_score_t *b
) {
    if (!a || !b) return 0;
    
    if (a->overall > b->overall) return 1;
    if (a->overall < b->overall) return -1;
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Format ID Constants (for format_id field)
 * ═══════════════════════════════════════════════════════════════════════════════ */

#define UFT_FORMAT_ID_UNKNOWN       0x0000

/* Amiga formats */
#define UFT_FORMAT_ID_ADF           0x0100
#define UFT_FORMAT_ID_ADZ           0x0101
#define UFT_FORMAT_ID_DMS           0x0102
#define UFT_FORMAT_ID_AXDF          0x01F0

/* C64 formats */
#define UFT_FORMAT_ID_D64           0x0200
#define UFT_FORMAT_ID_D71           0x0201
#define UFT_FORMAT_ID_D81           0x0202
#define UFT_FORMAT_ID_G64           0x0210
#define UFT_FORMAT_ID_DXDF          0x02F0

/* PC formats */
#define UFT_FORMAT_ID_IMG           0x0300
#define UFT_FORMAT_ID_IMA           0x0301
#define UFT_FORMAT_ID_IMZ           0x0302
#define UFT_FORMAT_ID_XDF_PC        0x0310
#define UFT_FORMAT_ID_PXDF          0x03F0

/* Atari ST formats */
#define UFT_FORMAT_ID_ST            0x0400
#define UFT_FORMAT_ID_MSA           0x0401
#define UFT_FORMAT_ID_TXDF          0x04F0

/* ZX Spectrum formats */
#define UFT_FORMAT_ID_TRD           0x0500
#define UFT_FORMAT_ID_SCL           0x0501
#define UFT_FORMAT_ID_DSK_CPC       0x0510
#define UFT_FORMAT_ID_ZXDF          0x05F0

/* Multi-format */
#define UFT_FORMAT_ID_MXDF          0xFF00

#ifdef __cplusplus
}
#endif

#endif /* UFT_SCORE_H */
