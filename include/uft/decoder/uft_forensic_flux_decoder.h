/**
 * @file uft_forensic_flux_decoder.h
 * @brief Forensic-Grade Multi-Stage Flux Decoder API
 * 
 * 6-Stage Pipeline:
 * 1. Pre-Analysis: Cell time estimation, anomaly detection
 * 2. PLL Decode: Adaptive clock recovery with confidence tracking
 * 3. Multi-Rev Fusion: Confidence-weighted bit voting
 * 4. Sector Recovery: Fuzzy sync detection, error tolerance
 * 5. Error Correction: CRC-based bit correction
 * 6. Verification: Final validation and audit
 * 
 * @version 2.0.0
 * @date 2025-01-08
 */

#ifndef UFT_FORENSIC_FLUX_DECODER_H
#define UFT_FORENSIC_FLUX_DECODER_H

#include "uft_decoder_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * CONFIGURATION
 *============================================================================*/

/**
 * @brief Forensic decoder configuration
 */
typedef struct {
    /* Pre-analysis */
    double min_cell_ratio;      /**< Minimum cell ratio (default: 0.5) */
    double max_cell_ratio;      /**< Maximum cell ratio (default: 2.5) */
    double expected_rpm;        /**< Expected RPM (0 = auto-detect) */
    
    /* PLL */
    double pll_bandwidth;       /**< PLL bandwidth (0.0-1.0, default: 0.05) */
    double pll_damping;         /**< PLL damping factor (default: 0.707) */
    double weak_threshold;      /**< Weak bit threshold (default: 0.3) */
    
    /* Multi-rev fusion */
    bool enable_fusion;         /**< Enable multi-revolution fusion */
    double fusion_min_consensus;/**< Minimum consensus (default: 0.6) */
    int max_revolutions;        /**< Maximum revolutions (default: 5) */
    
    /* Sector recovery */
    int sync_hamming_tolerance; /**< Hamming tolerance for sync (default: 2) */
    bool enable_correction;     /**< Enable error correction */
    int max_correction_bits;    /**< Maximum bits to correct (default: 2) */
    
    /* Output control */
    bool keep_raw_bits;         /**< Keep raw bit stream in result */
    bool keep_confidence;       /**< Keep per-bit confidence values */
    bool enable_audit;          /**< Enable audit logging */
} uft_ffd_config_t;

/*============================================================================
 * CONFIGURATION PRESETS
 *============================================================================*/

/**
 * @brief Get default configuration
 */
uft_ffd_config_t uft_ffd_config_default(void);

/**
 * @brief Get paranoid configuration (maximum recovery, slower)
 */
uft_ffd_config_t uft_ffd_config_paranoid(void);

/**
 * @brief Get fast configuration (speed over accuracy)
 */
uft_ffd_config_t uft_ffd_config_fast(void);

/*============================================================================
 * SESSION MANAGEMENT
 *============================================================================*/

/**
 * @brief Decoder session for tracking statistics
 */
typedef struct uft_ffd_session_s uft_ffd_session_t;

/**
 * @brief Create decoder session
 */
uft_ffd_session_t* uft_ffd_session_create(const uft_ffd_config_t* config);

/**
 * @brief Destroy decoder session
 */
void uft_ffd_session_destroy(uft_ffd_session_t* session);

/**
 * @brief Get session statistics
 */
typedef struct {
    int tracks_processed;
    int sectors_decoded;
    int sectors_recovered;
    int total_corrections;
    int weak_bits_found;
    double average_confidence;
} uft_ffd_stats_t;

int uft_ffd_get_stats(const uft_ffd_session_t* session, uft_ffd_stats_t* stats);

/*============================================================================
 * MAIN DECODE FUNCTIONS
 *============================================================================*/

/**
 * @brief Pre-analyze flux data
 * 
 * @param flux Flux timing array (in ticks)
 * @param count Number of flux transitions
 * @param sample_clock Sample clock frequency (Hz)
 * @param encoding Expected encoding (or UFT_ENCODING_UNKNOWN for auto)
 * @param result Pre-analysis result
 * @return 0 on success, negative on error
 */
int uft_ffd_preanalyze(
    const uint32_t* flux,
    size_t count,
    double sample_clock,
    uft_encoding_t encoding,
    uft_preanalysis_result_t* result);

/**
 * @brief Decode single revolution with PLL
 */
int uft_ffd_pll_decode(
    const uint32_t* flux,
    size_t count,
    double sample_clock,
    double cell_time_ns,
    const uft_ffd_config_t* config,
    uft_pll_decode_result_t* result);

/**
 * @brief Fuse multiple revolutions
 */
int uft_ffd_fuse(
    const uft_pll_decode_result_t* revolutions,
    int rev_count,
    const uft_ffd_config_t* config,
    uft_fusion_result_t* result);

/**
 * @brief Recover sectors from bit stream
 */
int uft_ffd_recover_sectors(
    const uint8_t* bits,
    size_t bit_count,
    const uft_conf_t* confidence,
    uft_encoding_t encoding,
    const uft_ffd_config_t* config,
    uft_track_decode_result_t* result);

/**
 * @brief High-level: Decode track from flux
 * 
 * Runs the complete 6-stage pipeline.
 * 
 * @param flux Flux timing array
 * @param count Number of flux transitions
 * @param sample_clock Sample clock frequency (Hz)
 * @param cylinder Cylinder number
 * @param head Head number
 * @param encoding Expected encoding (or UFT_ENCODING_UNKNOWN)
 * @param config Decoder configuration (NULL for default)
 * @param session Optional session for statistics
 * @param result Track decode result
 * @return 0 on success, negative on error
 */
int uft_ffd_decode_track(
    const uint32_t* flux,
    size_t count,
    double sample_clock,
    int cylinder,
    int head,
    uft_encoding_t encoding,
    const uft_ffd_config_t* config,
    uft_ffd_session_t* session,
    uft_track_decode_result_t* result);

/**
 * @brief Decode track from multiple revolutions
 */
int uft_ffd_decode_track_multi(
    const uint32_t** flux_revs,
    const size_t* counts,
    int rev_count,
    double sample_clock,
    int cylinder,
    int head,
    uft_encoding_t encoding,
    const uft_ffd_config_t* config,
    uft_ffd_session_t* session,
    uft_track_decode_result_t* result);

/*============================================================================
 * ERROR CORRECTION
 *============================================================================*/

/**
 * @brief Attempt to correct CRC errors in sector
 */
int uft_ffd_correct_sector(
    uft_sector_decode_result_t* sector,
    const uft_conf_t* confidence,
    int max_bits);

/*============================================================================
 * AUDIT LOG
 *============================================================================*/

/**
 * @brief Get audit log entry count
 */
int uft_ffd_audit_count(const uft_ffd_session_t* session);

/**
 * @brief Get audit log entry
 */
const char* uft_ffd_audit_get(const uft_ffd_session_t* session, int index);

/**
 * @brief Export audit log to file
 */
int uft_ffd_audit_export(const uft_ffd_session_t* session, const char* path);

#ifdef __cplusplus
}
#endif

#endif /* UFT_FORENSIC_FLUX_DECODER_H */
