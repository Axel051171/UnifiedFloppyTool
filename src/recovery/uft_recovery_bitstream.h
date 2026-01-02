/**
 * @file uft_recovery_bitstream.h
 * @brief GOD MODE Bitstream-Level Recovery
 * 
 * Bitstream-Recovery:
 * - Bit-Slip-Korrektur
 * - Mehrere Decode-Hypothesen parallel
 * - Sync-Rekonstruktion
 * - Missing-Clock-Erkennung
 * - Region-basierte Re-Dekodierung
 * - Mixed-Encoding-Trennung
 * - Confidence-Score pro Bit/Region
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#ifndef UFT_RECOVERY_BITSTREAM_H
#define UFT_RECOVERY_BITSTREAM_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define UFT_MAX_DECODE_HYPOTHESES   8       /**< Max parallel decode attempts */
#define UFT_MAX_SYNC_PATTERNS       16      /**< Max sync patterns to try */
#define UFT_REGION_MIN_BITS         64      /**< Minimum bits per region */

/* ============================================================================
 * Encoding Types
 * ============================================================================ */

typedef enum {
    UFT_ENCODING_UNKNOWN = 0,
    UFT_ENCODING_FM,            /**< Frequency Modulation (SD) */
    UFT_ENCODING_MFM,           /**< Modified FM (DD/HD) */
    UFT_ENCODING_M2FM,          /**< DEC Modified MFM */
    UFT_ENCODING_GCR_C64,       /**< Commodore 64 GCR */
    UFT_ENCODING_GCR_APPLE,     /**< Apple II GCR */
    UFT_ENCODING_GCR_MAC,       /**< Macintosh GCR */
    UFT_ENCODING_GCR_VICTOR,    /**< Victor 9000 GCR */
    UFT_ENCODING_MIXED,         /**< Multiple encodings detected */
} uft_encoding_type_t;

/* ============================================================================
 * Types
 * ============================================================================ */

/**
 * @brief Bit with confidence
 */
typedef struct {
    uint8_t value;              /**< Bit value (0 or 1) */
    uint8_t confidence;         /**< Confidence 0-100 */
    uint8_t source;             /**< Source hypothesis index */
    uint16_t flags;             /**< Flags */
} uft_bit_t;

#define UFT_BIT_FLAG_SLIP       0x0001  /**< Bit slip detected */
#define UFT_BIT_FLAG_MISSING    0x0002  /**< Missing clock */
#define UFT_BIT_FLAG_INSERTED   0x0004  /**< Inserted bit */
#define UFT_BIT_FLAG_UNCERTAIN  0x0008  /**< Low confidence */
#define UFT_BIT_FLAG_SYNC       0x0010  /**< Part of sync pattern */

/**
 * @brief Bitstream region
 */
typedef struct {
    size_t start_bit;           /**< Start bit offset */
    size_t bit_count;           /**< Number of bits */
    uft_encoding_type_t encoding; /**< Detected encoding */
    uint8_t confidence;         /**< Region confidence */
    bool needs_redecode;        /**< Should be re-decoded */
    double clock_offset;        /**< Clock offset from nominal */
} uft_bitstream_region_t;

/**
 * @brief Sync pattern match
 */
typedef struct {
    size_t bit_offset;          /**< Offset in bitstream */
    uint32_t pattern;           /**< Matched pattern */
    uint8_t pattern_bits;       /**< Pattern bit count */
    uft_encoding_type_t encoding; /**< Encoding of sync */
    uint8_t confidence;         /**< Match confidence */
    bool is_standard;           /**< Standard sync for format */
} uft_sync_match_t;

/**
 * @brief Decode hypothesis
 */
typedef struct {
    uft_encoding_type_t encoding; /**< Encoding used */
    double clock_period;        /**< Clock period used */
    double phase_offset;        /**< Phase offset */
    
    uint8_t *decoded_data;      /**< Decoded bytes */
    size_t decoded_len;         /**< Decoded length */
    
    uft_bit_t *bits;            /**< Per-bit info */
    size_t bit_count;           /**< Total bits */
    
    uint32_t sync_matches;      /**< Number of sync matches */
    uint32_t crc_passes;        /**< Number of valid CRCs */
    uint32_t slip_corrections;  /**< Slip corrections made */
    
    double score;               /**< Overall score */
    bool is_best;               /**< Currently best */
} uft_decode_hypothesis_t;

/**
 * @brief Bit slip event
 */
typedef struct {
    size_t bit_offset;          /**< Where slip occurred */
    int8_t slip_amount;         /**< +1 = extra bit, -1 = missing bit */
    uint8_t confidence;         /**< Confidence in detection */
    bool corrected;             /**< Was corrected */
} uft_bit_slip_t;

/**
 * @brief Missing clock event
 */
typedef struct {
    size_t bit_offset;          /**< Where clock missing */
    uft_encoding_type_t encoding; /**< Expected encoding */
    bool reconstructed;         /**< Successfully reconstructed */
} uft_missing_clock_t;

/**
 * @brief Bitstream recovery context
 */
typedef struct {
    /* Input */
    const uint8_t *raw_bits;    /**< Raw bitstream */
    size_t raw_bit_count;       /**< Raw bit count */
    
    /* Region analysis */
    uft_bitstream_region_t *regions;
    size_t region_count;
    
    /* Sync detection */
    uft_sync_match_t *syncs;
    size_t sync_count;
    
    /* Hypotheses */
    uft_decode_hypothesis_t hypotheses[UFT_MAX_DECODE_HYPOTHESES];
    size_t hypothesis_count;
    
    /* Detected issues */
    uft_bit_slip_t *slips;
    size_t slip_count;
    uft_missing_clock_t *missing_clocks;
    size_t missing_clock_count;
    
    /* Options */
    bool auto_correct_slips;    /**< Automatically correct slips */
    bool try_all_encodings;     /**< Try all known encodings */
    uint8_t min_confidence;     /**< Minimum required confidence */
} uft_bitstream_recovery_ctx_t;

/* ============================================================================
 * Bit Slip Correction
 * ============================================================================ */

/**
 * @brief Detect bit slips in bitstream
 * 
 * Bit-Slip = Fehlendes oder zusätzliches Bit durch PLL-Drift.
 * 
 * @param bits          Bitstream
 * @param bit_count     Number of bits
 * @param encoding      Expected encoding
 * @param slips         Output: detected slips
 * @return Number of slips detected
 */
size_t uft_bitstream_detect_slips(const uint8_t *bits, size_t bit_count,
                                  uft_encoding_type_t encoding,
                                  uft_bit_slip_t **slips);

/**
 * @brief Correct detected bit slips
 * 
 * @param bits          Bitstream (modified in place)
 * @param bit_count     Input/Output: bit count
 * @param slips         Slips to correct
 * @param slip_count    Number of slips
 * @return Number of corrections made
 */
size_t uft_bitstream_correct_slips(uint8_t *bits, size_t *bit_count,
                                   const uft_bit_slip_t *slips, size_t slip_count);

/**
 * @brief Verify slip correction didn't break data
 */
bool uft_bitstream_verify_slip_correction(const uint8_t *original, size_t orig_count,
                                          const uint8_t *corrected, size_t corr_count,
                                          uft_encoding_type_t encoding);

/* ============================================================================
 * Parallel Decode Hypotheses
 * ============================================================================ */

/**
 * @brief Generate multiple decode hypotheses
 * 
 * Erzeugt mehrere parallele Dekodier-Versuche mit verschiedenen
 * Parametern. Keine wird als "richtig" angenommen.
 */
size_t uft_bitstream_generate_hypotheses(const uint8_t *bits, size_t bit_count,
                                         uft_decode_hypothesis_t *hypotheses,
                                         size_t max_hypotheses);

/**
 * @brief Score hypothesis based on sync/CRC matches
 */
void uft_bitstream_score_hypothesis(uft_decode_hypothesis_t *hyp,
                                    const uint32_t *expected_syncs, size_t sync_count);

/**
 * @brief Compare hypotheses and rank them
 */
void uft_bitstream_rank_hypotheses(uft_decode_hypothesis_t *hypotheses,
                                   size_t count);

/**
 * @brief Get best hypothesis (but don't discard others!)
 */
const uft_decode_hypothesis_t* uft_bitstream_get_best(
    const uft_decode_hypothesis_t *hypotheses, size_t count);

/**
 * @brief Merge best parts from multiple hypotheses
 */
bool uft_bitstream_merge_hypotheses(const uft_decode_hypothesis_t *hypotheses,
                                    size_t count,
                                    uint8_t **merged_data,
                                    uft_bit_t **merged_bits,
                                    size_t *merged_len);

/* ============================================================================
 * Sync Reconstruction
 * ============================================================================ */

/**
 * @brief Scan for sync patterns
 * 
 * Sucht nach bekannten Sync-Patterns auch in beschädigten Daten.
 */
size_t uft_bitstream_find_syncs(const uint8_t *bits, size_t bit_count,
                                uft_encoding_type_t encoding,
                                uft_sync_match_t **matches);

/**
 * @brief Reconstruct damaged sync pattern
 * 
 * Versucht beschädigte Sync-Patterns zu rekonstruieren.
 * Markiert als "rekonstruiert", nicht "original".
 */
bool uft_bitstream_reconstruct_sync(uint8_t *bits, size_t bit_count,
                                    size_t expected_offset,
                                    uint32_t expected_pattern,
                                    uft_sync_match_t *result);

/**
 * @brief Validate sync pattern spacing
 */
bool uft_bitstream_validate_sync_spacing(const uft_sync_match_t *syncs,
                                         size_t sync_count,
                                         size_t expected_spacing);

/* ============================================================================
 * Missing Clock Detection
 * ============================================================================ */

/**
 * @brief Detect missing clocks in MFM/FM data
 * 
 * Missing clocks = Stellen wo Clock-Bit fehlt.
 */
size_t uft_bitstream_detect_missing_clocks(const uint8_t *bits, size_t bit_count,
                                           uft_encoding_type_t encoding,
                                           uft_missing_clock_t **missing);

/**
 * @brief Attempt to reconstruct missing clocks
 */
size_t uft_bitstream_reconstruct_clocks(uint8_t *bits, size_t *bit_count,
                                        const uft_missing_clock_t *missing,
                                        size_t missing_count);

/* ============================================================================
 * Region-Based Re-Decoding
 * ============================================================================ */

/**
 * @brief Analyze bitstream into regions
 * 
 * Teilt Bitstream in Regionen mit konsistenten Eigenschaften.
 */
size_t uft_bitstream_analyze_regions(const uint8_t *bits, size_t bit_count,
                                     uft_bitstream_region_t **regions);

/**
 * @brief Re-decode specific region with different parameters
 */
bool uft_bitstream_redecode_region(const uint8_t *bits, size_t bit_count,
                                   uft_bitstream_region_t *region,
                                   uft_encoding_type_t try_encoding,
                                   double try_clock);

/**
 * @brief Get region confidence map
 */
void uft_bitstream_get_confidence_map(const uft_bitstream_region_t *regions,
                                      size_t region_count,
                                      uint8_t *confidence_map,
                                      size_t map_len);

/* ============================================================================
 * Mixed-Encoding Separation
 * ============================================================================ */

/**
 * @brief Detect encoding transitions
 * 
 * Findet Stellen wo Encoding wechselt (z.B. FM→MFM).
 */
size_t uft_bitstream_detect_encoding_changes(const uint8_t *bits, size_t bit_count,
                                             size_t **transition_offsets,
                                             uft_encoding_type_t **encodings);

/**
 * @brief Decode mixed-encoding track
 * 
 * Dekodiert Track mit mehreren Encodings korrekt.
 */
bool uft_bitstream_decode_mixed(const uint8_t *bits, size_t bit_count,
                                const uft_bitstream_region_t *regions,
                                size_t region_count,
                                uint8_t **decoded,
                                size_t *decoded_len);

/**
 * @brief Identify encoding for region
 */
uft_encoding_type_t uft_bitstream_identify_encoding(const uint8_t *bits,
                                                    size_t start, size_t count);

/* ============================================================================
 * Confidence Scoring
 * ============================================================================ */

/**
 * @brief Calculate per-bit confidence
 */
void uft_bitstream_calc_bit_confidence(const uint8_t *bits, size_t bit_count,
                                       const uft_decode_hypothesis_t *hypotheses,
                                       size_t hyp_count,
                                       uint8_t *confidence);

/**
 * @brief Calculate per-region confidence
 */
void uft_bitstream_calc_region_confidence(uft_bitstream_region_t *regions,
                                          size_t region_count,
                                          const uint8_t *bit_confidence);

/**
 * @brief Get overall bitstream confidence
 */
uint8_t uft_bitstream_overall_confidence(const uint8_t *confidence,
                                         size_t bit_count);

/* ============================================================================
 * Full Bitstream Recovery
 * ============================================================================ */

/**
 * @brief Create bitstream recovery context
 */
uft_bitstream_recovery_ctx_t* uft_bitstream_recovery_create(
    const uint8_t *bits, size_t bit_count);

/**
 * @brief Run full bitstream analysis
 */
void uft_bitstream_recovery_analyze(uft_bitstream_recovery_ctx_t *ctx);

/**
 * @brief Get best decoded result
 */
bool uft_bitstream_recovery_get_result(const uft_bitstream_recovery_ctx_t *ctx,
                                       uint8_t **data, size_t *len,
                                       uft_bit_t **bits, uint8_t *confidence);

/**
 * @brief Generate report
 */
char* uft_bitstream_recovery_report(const uft_bitstream_recovery_ctx_t *ctx);

/**
 * @brief Free context
 */
void uft_bitstream_recovery_free(uft_bitstream_recovery_ctx_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* UFT_RECOVERY_BITSTREAM_H */
