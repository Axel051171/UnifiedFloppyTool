/**
 * @file uft_god_mode.h
 * @brief UFT God-Mode Algorithms - Advanced Flux Analysis
 * @version 3.8.0
 * 
 * "Bei uns geht kein Bit verloren" - UFT Preservation Philosophy
 * 
 * This header provides access to advanced algorithms for:
 * - Bayesian format detection with probabilistic scoring
 * - Viterbi decoding for GCR/MFM with error correction
 * - Kalman PLL for adaptive bit timing recovery
 * - Multi-revolution fusion for weak bit recovery
 * - CRC correction for damaged sectors
 * - Fuzzy sync detection for non-standard patterns
 */

#ifndef UFT_GOD_MODE_H
#define UFT_GOD_MODE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * BAYESIAN FORMAT DETECTION
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Format probability result
 */
typedef struct {
    int format_id;              /**< Format identifier */
    const char* format_name;    /**< Human-readable name */
    double probability;         /**< Probability 0.0-1.0 */
    double confidence;          /**< Confidence in detection */
    int evidence_count;         /**< Number of evidence items */
} uft_bayesian_result_t;

/**
 * @brief Bayesian detection configuration
 */
typedef struct {
    bool use_prior;             /**< Use prior probabilities */
    bool check_size;            /**< Check file size patterns */
    bool check_magic;           /**< Check magic bytes */
    bool check_structure;       /**< Check internal structure */
    int max_results;            /**< Maximum results to return */
} uft_bayesian_config_t;

/**
 * @brief Run Bayesian format detection
 * @param data Raw disk data
 * @param size Data size
 * @param config Detection configuration
 * @param results Output results array
 * @param max_results Maximum results
 * @return Number of results found
 */
int uft_bayesian_detect(const uint8_t* data, size_t size,
                        const uft_bayesian_config_t* config,
                        uft_bayesian_result_t* results, int max_results);

/**
 * @brief Get default Bayesian configuration
 */
void uft_bayesian_config_init(uft_bayesian_config_t* config);

/* ═══════════════════════════════════════════════════════════════════════════════
 * VITERBI GCR/MFM DECODER
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Viterbi decoder configuration
 */
typedef struct {
    int encoding;               /**< 0=MFM, 1=GCR-C64, 2=GCR-Apple */
    int constraint_length;      /**< Constraint length (default 7) */
    double error_threshold;     /**< Error threshold for correction */
    bool use_soft_decode;       /**< Use soft decision decoding */
    int max_corrections;        /**< Maximum corrections per block */
} uft_viterbi_config_t;

/**
 * @brief Viterbi decode result
 */
typedef struct {
    uint8_t* decoded_data;      /**< Decoded output */
    size_t decoded_size;        /**< Output size */
    int corrections_made;       /**< Number of corrections */
    double error_rate;          /**< Bit error rate */
    bool checksum_valid;        /**< Checksum validation */
} uft_viterbi_result_t;

/**
 * @brief Viterbi decode GCR/MFM data
 */
int uft_viterbi_decode(const uint8_t* raw_bits, size_t bit_count,
                       const uft_viterbi_config_t* config,
                       uft_viterbi_result_t* result);

/**
 * @brief Free Viterbi result resources
 */
void uft_viterbi_result_free(uft_viterbi_result_t* result);

/**
 * @brief Get default Viterbi configuration for encoding type
 */
void uft_viterbi_config_init(uft_viterbi_config_t* config, int encoding);

/* ═══════════════════════════════════════════════════════════════════════════════
 * KALMAN PLL - Adaptive Bit Timing Recovery
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Kalman PLL state
 */
typedef struct {
    double bit_period;          /**< Current bit period estimate */
    double period_variance;     /**< Period variance */
    double phase;               /**< Phase accumulator */
    double phase_variance;      /**< Phase variance */
    uint64_t total_bits;        /**< Total bits processed */
    double drift_rate;          /**< Estimated drift rate */
} uft_kalman_state_t;

/**
 * @brief Kalman PLL configuration
 */
typedef struct {
    double nominal_period;      /**< Nominal bit period (ns) */
    double process_noise;       /**< Process noise Q */
    double measurement_noise;   /**< Measurement noise R */
    double initial_variance;    /**< Initial variance P0 */
    bool adaptive_noise;        /**< Adaptive noise estimation */
} uft_kalman_config_t;

/**
 * @brief Initialize Kalman PLL
 */
void uft_kalman_init(uft_kalman_state_t* state, const uft_kalman_config_t* config);

/**
 * @brief Process flux transition
 * @return Decoded bit(s): -1=no bit, 0/1=single bit, 2+=multiple
 */
int uft_kalman_process(uft_kalman_state_t* state, double flux_time);

/**
 * @brief Get default Kalman configuration for encoding
 * @param encoding 0=MFM, 1=FM, 2=GCR-C64, 3=GCR-Apple
 */
void uft_kalman_config_init(uft_kalman_config_t* config, int encoding);

/* ═══════════════════════════════════════════════════════════════════════════════
 * MULTI-REVOLUTION FUSION
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Revolution data for fusion
 */
typedef struct {
    const uint8_t* bits;        /**< Bit data */
    size_t bit_count;           /**< Number of bits */
    const double* timing;       /**< Optional timing data */
    double quality;             /**< Quality metric 0-1 */
} uft_revolution_t;

/**
 * @brief Fusion result
 */
typedef struct {
    uint8_t* fused_bits;        /**< Fused bit data */
    size_t fused_count;         /**< Fused bit count */
    uint8_t* confidence_map;    /**< Per-bit confidence */
    int weak_bit_count;         /**< Detected weak bits */
    int recovered_count;        /**< Recovered bit count */
    double overall_quality;     /**< Overall quality */
} uft_fusion_result_t;

/**
 * @brief Fuse multiple revolutions
 * @param revs Array of revolution data
 * @param rev_count Number of revolutions
 * @param result Output fused result
 * @return 0 on success
 */
int uft_fusion_process(const uft_revolution_t* revs, int rev_count,
                       uft_fusion_result_t* result);

/**
 * @brief Free fusion result
 */
void uft_fusion_result_free(uft_fusion_result_t* result);

/* ═══════════════════════════════════════════════════════════════════════════════
 * CRC ERROR CORRECTION
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief CRC correction result
 */
typedef struct {
    bool corrected;             /**< True if corrected */
    int bit_position;           /**< Position of corrected bit */
    uint16_t original_crc;      /**< Original CRC */
    uint16_t computed_crc;      /**< Computed CRC */
} uft_crc_correction_t;

/**
 * @brief Attempt single-bit CRC correction
 * @param data Sector data
 * @param size Data size (including CRC)
 * @param crc_type 0=CRC-16-CCITT, 1=CRC-16-IBM
 * @param result Correction result
 * @return true if correction possible
 */
bool uft_crc_correct(uint8_t* data, size_t size, int crc_type,
                     uft_crc_correction_t* result);

/* ═══════════════════════════════════════════════════════════════════════════════
 * FUZZY SYNC DETECTION
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Sync pattern match
 */
typedef struct {
    size_t bit_position;        /**< Position in bitstream */
    int pattern_id;             /**< Matched pattern ID */
    double match_quality;       /**< Match quality 0-1 */
    int mismatches;             /**< Bit mismatches */
} uft_sync_match_t;

/**
 * @brief Find sync patterns with fuzzy matching
 * @param bits Bit data
 * @param bit_count Number of bits
 * @param pattern Sync pattern to find
 * @param pattern_bits Pattern length in bits
 * @param max_mismatches Maximum allowed mismatches
 * @param matches Output match array
 * @param max_matches Maximum matches
 * @return Number of matches found
 */
int uft_fuzzy_sync_find(const uint8_t* bits, size_t bit_count,
                        const uint8_t* pattern, int pattern_bits,
                        int max_mismatches,
                        uft_sync_match_t* matches, int max_matches);

/* ═══════════════════════════════════════════════════════════════════════════════
 * DECODER METRICS
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Decoder quality metrics
 */
typedef struct {
    double bit_error_rate;      /**< Estimated BER */
    double sync_quality;        /**< Sync pattern quality */
    double timing_jitter;       /**< Timing jitter (ns) */
    double signal_quality;      /**< Overall signal quality */
    int missing_sectors;        /**< Missing sector count */
    int bad_checksums;          /**< Bad checksum count */
    int recovered_sectors;      /**< Recovered sector count */
} uft_decoder_metrics_t;

/**
 * @brief Calculate decoder metrics for track
 */
void uft_calculate_metrics(const uint8_t* track_data, size_t track_len,
                          int encoding, uft_decoder_metrics_t* metrics);

/* ═══════════════════════════════════════════════════════════════════════════════
 * ENCODING CONSTANTS
 * ═══════════════════════════════════════════════════════════════════════════════ */

#define UFT_ENCODING_MFM        0   /**< MFM (IBM PC, Amiga) */
#define UFT_ENCODING_FM         1   /**< FM (single density) */
#define UFT_ENCODING_GCR_C64    2   /**< GCR Commodore */
#define UFT_ENCODING_GCR_APPLE  3   /**< GCR Apple II */

#ifdef __cplusplus
}
#endif

#endif /* UFT_GOD_MODE_H */
