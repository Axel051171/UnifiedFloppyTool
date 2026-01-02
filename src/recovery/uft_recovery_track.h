/**
 * @file uft_recovery_track.h
 * @brief GOD MODE Track-Level Recovery
 * 
 * Track-Recovery:
 * - Index-Ignorieren / Index-Rekonstruktion
 * - Long-Track- / Short-Track-Erkennung
 * - Track-Length-Vergleich über Revs
 * - Track-Alignment über mehrere Reads
 * - Splice-Analyse
 * - Track-Timing-Profile
 * - Head-Misalignment-Erkennung
 * - Track-Duplikat-Analyse
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#ifndef UFT_RECOVERY_TRACK_H
#define UFT_RECOVERY_TRACK_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define UFT_TRACK_MAX_REVS          16      /**< Max revolutions per track */
#define UFT_TRACK_TIMING_POINTS     1024    /**< Timing profile resolution */

/* ============================================================================
 * Types
 * ============================================================================ */

/**
 * @brief Index pulse info
 */
typedef struct {
    uint32_t time_ns;           /**< Index pulse time */
    uint8_t  confidence;        /**< Index confidence */
    bool     is_reconstructed;  /**< Was reconstructed */
    bool     should_ignore;     /**< Should be ignored */
} uft_index_info_t;

/**
 * @brief Track length analysis
 */
typedef struct {
    uint32_t nominal_length;    /**< Nominal length (bits or ns) */
    uint32_t measured_length;   /**< Measured length */
    int32_t  deviation;         /**< Deviation from nominal */
    double   deviation_percent; /**< Deviation percentage */
    bool     is_long_track;     /**< Track is longer than normal */
    bool     is_short_track;    /**< Track is shorter than normal */
    bool     is_protection;     /**< Likely copy protection */
} uft_track_length_t;

/**
 * @brief Track alignment info
 */
typedef struct {
    size_t   rev_index;         /**< Revolution index */
    int32_t  offset_from_ref;   /**< Offset from reference (bits) */
    double   correlation;       /**< Correlation with reference */
    bool     is_aligned;        /**< Successfully aligned */
} uft_track_alignment_t;

/**
 * @brief Splice point (track wrap)
 */
typedef struct {
    uint32_t position_ns;       /**< Position in ns */
    size_t   bit_offset;        /**< Bit offset */
    uint8_t  quality;           /**< Splice quality 0-100 */
    bool     is_clean;          /**< Clean splice */
    bool     has_overlap;       /**< Has overlap region */
    uint32_t overlap_length;    /**< Overlap length if any */
} uft_splice_point_t;

/**
 * @brief Track timing profile
 */
typedef struct {
    double   timing[UFT_TRACK_TIMING_POINTS]; /**< Timing at each point */
    double   nominal_cell;      /**< Nominal cell width */
    double   min_cell;          /**< Minimum cell width */
    double   max_cell;          /**< Maximum cell width */
    double   variance;          /**< Overall variance */
    size_t   anomaly_count;     /**< Number of anomalies */
    size_t  *anomaly_positions; /**< Anomaly positions */
} uft_track_timing_t;

/**
 * @brief Head misalignment info
 */
typedef struct {
    double   offset_um;         /**< Offset in micrometers */
    uint8_t  severity;          /**< Severity 0-100 */
    bool     affects_read;      /**< Affects readability */
    bool     affects_adjacent;  /**< Affects adjacent tracks */
    int8_t   direction;         /**< Direction (+1/-1) */
} uft_head_misalignment_t;

/**
 * @brief Track duplicate analysis
 */
typedef struct {
    uint8_t  track_a;           /**< First track */
    uint8_t  track_b;           /**< Second track */
    double   similarity;        /**< Similarity 0-1 */
    bool     is_duplicate;      /**< Are duplicates */
    bool     is_partial;        /**< Partial duplicate */
    size_t   match_start;       /**< Match start offset */
    size_t   match_length;      /**< Match length */
} uft_track_duplicate_t;

/**
 * @brief Track recovery context
 */
typedef struct {
    /* Input */
    uint8_t  track;             /**< Track number */
    uint8_t  head;              /**< Head/side */
    
    /* Revolution data */
    const uint8_t **rev_data;   /**< Per-revolution data */
    size_t  *rev_lengths;       /**< Per-revolution lengths */
    size_t   rev_count;         /**< Number of revolutions */
    
    /* Index analysis */
    uft_index_info_t *indices;
    size_t   index_count;
    
    /* Length analysis */
    uft_track_length_t length;
    
    /* Alignment */
    uft_track_alignment_t *alignments;
    
    /* Splice */
    uft_splice_point_t splice;
    
    /* Timing */
    uft_track_timing_t timing;
    
    /* Head */
    uft_head_misalignment_t head_align;
    
    /* Output */
    uint8_t *recovered;
    size_t   recovered_len;
    uint8_t  confidence;
} uft_track_recovery_ctx_t;

/* ============================================================================
 * Index Handling
 * ============================================================================ */

/**
 * @brief Detect index pulses
 */
size_t uft_track_detect_indices(const uint8_t *track_data, size_t len,
                                uint32_t sample_rate,
                                uft_index_info_t **indices);

/**
 * @brief Validate index pulse timing
 */
bool uft_track_validate_index(const uft_index_info_t *index,
                              uint32_t expected_period_ns);

/**
 * @brief Reconstruct missing index
 * 
 * Rekonstruiert Index basierend auf Track-Struktur.
 * Markiert als "rekonstruiert".
 */
bool uft_track_reconstruct_index(const uint8_t *track_data, size_t len,
                                 uft_index_info_t *reconstructed);

/**
 * @brief Mark index for ignoring (bad pulse)
 */
void uft_track_ignore_index(uft_index_info_t *index, const char *reason);

/**
 * @brief Decode track ignoring index
 */
bool uft_track_decode_no_index(const uint8_t *track_data, size_t len,
                               uint8_t **decoded, size_t *decoded_len);

/* ============================================================================
 * Track Length Analysis
 * ============================================================================ */

/**
 * @brief Analyze track length
 */
void uft_track_analyze_length(const uint8_t *track_data, size_t len,
                              uint32_t expected_bits,
                              uft_track_length_t *result);

/**
 * @brief Compare track lengths across revolutions
 */
void uft_track_compare_rev_lengths(const size_t *lengths, size_t rev_count,
                                   double *variance, double *max_deviation);

/**
 * @brief Detect long track (copy protection)
 */
bool uft_track_is_long(const uft_track_length_t *length);

/**
 * @brief Detect short track
 */
bool uft_track_is_short(const uft_track_length_t *length);

/**
 * @brief Handle long track (don't truncate!)
 */
bool uft_track_handle_long(const uint8_t *track_data, size_t len,
                           uint8_t **output, size_t *output_len,
                           bool preserve_extra);

/* ============================================================================
 * Track Alignment
 * ============================================================================ */

/**
 * @brief Align multiple revolutions
 */
void uft_track_align_revolutions(const uint8_t **rev_data,
                                 const size_t *rev_lengths,
                                 size_t rev_count,
                                 uft_track_alignment_t *alignments);

/**
 * @brief Find best alignment offset
 */
int32_t uft_track_find_alignment(const uint8_t *ref, size_t ref_len,
                                 const uint8_t *target, size_t target_len,
                                 double *correlation);

/**
 * @brief Merge aligned revolutions
 */
bool uft_track_merge_aligned(const uint8_t **rev_data,
                             const size_t *rev_lengths,
                             const uft_track_alignment_t *alignments,
                             size_t rev_count,
                             uint8_t **merged, size_t *merged_len);

/* ============================================================================
 * Splice Analysis
 * ============================================================================ */

/**
 * @brief Detect splice point (track wrap)
 */
bool uft_track_detect_splice(const uint8_t *track_data, size_t len,
                             uft_splice_point_t *splice);

/**
 * @brief Analyze splice quality
 */
void uft_track_analyze_splice(const uint8_t *track_data, size_t len,
                              uft_splice_point_t *splice);

/**
 * @brief Handle splice overlap
 * 
 * Wenn Splice Überlappung hat, bestimme beste Daten.
 */
bool uft_track_handle_splice_overlap(const uint8_t *track_data, size_t len,
                                     const uft_splice_point_t *splice,
                                     uint8_t **output, size_t *output_len);

/* ============================================================================
 * Track Timing Profile
 * ============================================================================ */

/**
 * @brief Build timing profile
 */
void uft_track_build_timing_profile(const uint32_t *flux_times, size_t count,
                                    uft_track_timing_t *timing);

/**
 * @brief Detect timing anomalies
 */
size_t uft_track_detect_timing_anomalies(const uft_track_timing_t *timing,
                                         double threshold);

/**
 * @brief Compare timing profiles
 */
double uft_track_compare_timing(const uft_track_timing_t *t1,
                                const uft_track_timing_t *t2);

/**
 * @brief Normalize timing (as hypothesis, keep original!)
 */
bool uft_track_normalize_timing(const uint32_t *flux_times, size_t count,
                                const uft_track_timing_t *timing,
                                uint32_t **normalized, size_t *norm_count);

/* ============================================================================
 * Head Misalignment Detection
 * ============================================================================ */

/**
 * @brief Detect head misalignment
 */
void uft_track_detect_head_misalignment(const uint8_t *track_data, size_t len,
                                        uint8_t track_num,
                                        uft_head_misalignment_t *result);

/**
 * @brief Check adjacent track interference
 */
bool uft_track_check_adjacent_interference(const uint8_t *this_track,
                                           const uint8_t *adjacent_track,
                                           size_t len,
                                           double *interference_level);

/**
 * @brief Estimate optimal head position
 */
double uft_track_estimate_head_position(const uft_head_misalignment_t *align,
                                        uint8_t track_num);

/* ============================================================================
 * Track Duplicate Analysis
 * ============================================================================ */

/**
 * @brief Compare two tracks for duplication
 */
void uft_track_compare(const uint8_t *track_a, size_t len_a,
                       const uint8_t *track_b, size_t len_b,
                       uft_track_duplicate_t *result);

/**
 * @brief Find duplicated regions
 */
size_t uft_track_find_duplicates(const uint8_t *track_data, size_t len,
                                 uft_track_duplicate_t **duplicates);

/**
 * @brief Handle intentional duplicates (protection)
 */
bool uft_track_handle_intentional_duplicates(const uft_track_duplicate_t *dups,
                                             size_t dup_count,
                                             bool *is_protection);

/* ============================================================================
 * Full Track Recovery
 * ============================================================================ */

/**
 * @brief Create track recovery context
 */
uft_track_recovery_ctx_t* uft_track_recovery_create(uint8_t track, uint8_t head);

/**
 * @brief Add revolution to context
 */
bool uft_track_recovery_add_rev(uft_track_recovery_ctx_t *ctx,
                                const uint8_t *data, size_t len);

/**
 * @brief Run full track analysis
 */
void uft_track_recovery_analyze(uft_track_recovery_ctx_t *ctx);

/**
 * @brief Get recovered track
 */
bool uft_track_recovery_get_result(const uft_track_recovery_ctx_t *ctx,
                                   uint8_t **data, size_t *len,
                                   uint8_t *confidence);

/**
 * @brief Generate report
 */
char* uft_track_recovery_report(const uft_track_recovery_ctx_t *ctx);

/**
 * @brief Free context
 */
void uft_track_recovery_free(uft_track_recovery_ctx_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* UFT_RECOVERY_TRACK_H */
