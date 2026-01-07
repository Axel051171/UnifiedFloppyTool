/**
 * @file uft_sync_detector.h
 * @brief Multi-layer sync pattern detection with validation
 * 
 * Fixes algorithmic weakness #2: False positive sync detection
 * 
 * Features:
 * - MFM missing clock pattern detection
 * - Timing-based validation
 * - Context analysis
 * - Confidence scoring
 */

#ifndef UFT_SYNC_DETECTOR_H
#define UFT_SYNC_DETECTOR_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* MFM sync patterns with missing clock */
#define UFT_SYNC_MFM_A1     0x4489  /* A1 with missing clock */
#define UFT_SYNC_MFM_C2     0x5224  /* C2 with missing clock */
#define UFT_SYNC_MFM_A1_DECAY 0x8912  /* Decayed A1 */

/* Minimum confidence to accept a sync */
#define UFT_SYNC_MIN_CONFIDENCE 70

/* Maximum candidates to track */
#define UFT_SYNC_MAX_CANDIDATES 16

/* Sync types */
typedef enum {
    UFT_SYNC_TYPE_NONE = 0,
    UFT_SYNC_TYPE_IDAM,     /* ID Address Mark (0xFE) */
    UFT_SYNC_TYPE_DAM,      /* Data Address Mark (0xFB) */
    UFT_SYNC_TYPE_DDAM,     /* Deleted Data AM (0xF8) */
    UFT_SYNC_TYPE_IAM,      /* Index Address Mark (0xFC) */
    UFT_SYNC_TYPE_UNKNOWN
} uft_sync_type_t;

/* Sync candidate */
typedef struct {
    size_t   bit_position;      /**< Bit position in track */
    uint16_t mfm_pattern;       /**< Raw MFM pattern found */
    uint8_t  mark_byte;         /**< Address mark byte (FE/FB/F8/FC) */
    uft_sync_type_t type;       /**< Sync type */
    uint8_t  confidence;        /**< 0-100 */
    bool     has_missing_clock; /**< True if valid missing clock */
    double   timing_score;      /**< Timing validation score */
    double   context_score;     /**< Context validation score */
} uft_sync_candidate_t;

/* Sync detector state */
typedef struct {
    /* Sliding window for bit stream */
    uint64_t bit_window;
    size_t   bit_count;
    size_t   current_bit_pos;
    
    /* Detected candidates */
    uft_sync_candidate_t candidates[UFT_SYNC_MAX_CANDIDATES];
    size_t candidate_count;
    
    /* Timing validation */
    size_t last_sync_pos;       /**< Position of last valid sync */
    double expected_gap;         /**< Expected bits between syncs */
    double gap_tolerance;        /**< Tolerance (0.1 = 10%) */
    
    /* Context window */
    uint8_t context_bytes[8];   /**< Recent decoded bytes */
    size_t  context_idx;
    
    /* Configuration */
    size_t min_sync_separation;  /**< Minimum bits between syncs */
    bool   strict_mode;          /**< Require all validations */
    
    /* Statistics */
    size_t total_candidates;
    size_t accepted_syncs;
    size_t rejected_syncs;
    
} uft_sync_detector_t;

/**
 * @brief Initialize sync detector
 */
void uft_sync_init(uft_sync_detector_t *det);

/**
 * @brief Configure timing parameters
 * @param det Detector instance
 * @param expected_gap Expected bits between syncs (e.g., 1000)
 * @param tolerance Gap tolerance (e.g., 0.2 for ±20%)
 */
void uft_sync_configure(uft_sync_detector_t *det,
                        double expected_gap,
                        double tolerance);

/**
 * @brief Reset detector state
 */
void uft_sync_reset(uft_sync_detector_t *det);

/**
 * @brief Feed a bit to the detector
 * @param det Detector instance
 * @param bit Bit value (0 or 1)
 * @param out_sync Output: detected sync (if any)
 * @return true if sync detected
 */
bool uft_sync_feed_bit(uft_sync_detector_t *det,
                       uint8_t bit,
                       uft_sync_candidate_t *out_sync);

/**
 * @brief Feed a byte to the detector (convenience function)
 * @param det Detector instance
 * @param byte Byte value
 * @param out_sync Output: detected sync (if any)
 * @return Number of syncs detected (0-8)
 */
int uft_sync_feed_byte(uft_sync_detector_t *det,
                       uint8_t byte,
                       uft_sync_candidate_t *out_syncs);

/**
 * @brief Feed raw MFM word
 * @param det Detector instance
 * @param mfm_word 16-bit MFM word
 * @param bit_pos Current bit position
 * @param out_sync Output: detected sync
 * @return true if sync detected
 */
bool uft_sync_feed_mfm(uft_sync_detector_t *det,
                       uint16_t mfm_word,
                       size_t bit_pos,
                       uft_sync_candidate_t *out_sync);

/**
 * @brief Add context byte for validation
 * @param det Detector instance
 * @param byte Decoded byte value
 */
void uft_sync_add_context(uft_sync_detector_t *det, uint8_t byte);

/**
 * @brief Get best candidate from current list
 * @param det Detector instance
 * @return Best candidate or NULL
 */
const uft_sync_candidate_t* uft_sync_get_best(const uft_sync_detector_t *det);

/**
 * @brief Clear candidate list
 */
void uft_sync_clear_candidates(uft_sync_detector_t *det);

/**
 * @brief Get sync type name
 */
const char* uft_sync_type_name(uft_sync_type_t type);

/**
 * @brief Print detector status
 */
void uft_sync_dump_status(const uft_sync_detector_t *det);

#ifdef __cplusplus
}
#endif

#endif /* UFT_SYNC_DETECTOR_H */
