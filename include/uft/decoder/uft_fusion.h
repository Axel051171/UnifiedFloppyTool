/**
 * @file uft_fusion.h
 * @brief Multi-Revolution Fusion API
 * 
 * Combines multiple disk revolutions using confidence-weighted voting
 * to recover data from degraded or copy-protected media.
 * 
 * @version 2.0.0
 * @date 2025-01-08
 */

#ifndef UFT_DECODER_FUSION_H
#define UFT_DECODER_FUSION_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * TYPES
 *============================================================================*/

/**
 * @brief Fusion configuration
 */
typedef struct {
    int min_revolutions;       /**< Minimum revolutions needed */
    int max_revolutions;       /**< Maximum revolutions to process */
    double alignment_tolerance;/**< Alignment tolerance (0.0-1.0) */
    double consensus_threshold;/**< Consensus threshold (0.0-1.0) */
    bool detect_weak_bits;     /**< Detect and mark weak bits */
    bool use_quality_weights;  /**< Use quality weighting */
} uft_fusion_config_t;

/**
 * @brief Fusion result
 */
typedef struct {
    uint8_t* data;             /**< Fused data (caller frees) */
    size_t length;             /**< Data length in bytes */
    double* confidence;        /**< Per-byte confidence (caller frees) */
    uint8_t* weak_bits;        /**< Weak bit map (caller frees) */
    double average_confidence; /**< Average confidence */
    int weak_count;            /**< Number of weak bits */
} uft_fusion_result_t;

/**
 * @brief Fusion context (opaque)
 */
typedef struct uft_fusion_s uft_fusion_t;

/*============================================================================
 * LIFECYCLE
 *============================================================================*/

/**
 * @brief Create fusion context with default config
 */
uft_fusion_t* uft_fusion_create(const uft_fusion_config_t* config);

/**
 * @brief Create fusion context with config (alias)
 */
uft_fusion_t* uft_fusion_create_with_config(const uft_fusion_config_t* config);

/**
 * @brief Destroy fusion context
 */
void uft_fusion_destroy(uft_fusion_t* fusion);

/**
 * @brief Reset context for new track
 */
void uft_fusion_reset(uft_fusion_t* fusion);

/*============================================================================
 * REVOLUTION INPUT
 *============================================================================*/

/**
 * @brief Add revolution data
 * 
 * @param fusion Context
 * @param data Raw byte data
 * @param length Data length
 * @param quality Quality score (0.0-1.0)
 * @param index_pos Index pulse position (optional, 0 if unknown)
 * @return 0 on success
 */
int uft_fusion_add_revolution(
    uft_fusion_t* fusion,
    const uint8_t* data,
    size_t length,
    double quality,
    uint32_t index_pos);

/**
 * @brief Add revolution with confidence array
 */
int uft_fusion_add_revolution_with_confidence(
    uft_fusion_t* fusion,
    const uint8_t* data,
    size_t length,
    const double* confidence,
    uint32_t index_pos);

/*============================================================================
 * PROCESSING
 *============================================================================*/

/**
 * @brief Process all revolutions and store result internally
 * @param fusion Context
 * @param result Result structure to fill
 * @return 0 on success
 */
int uft_fusion_process(uft_fusion_t* fusion, uft_fusion_result_t* result);

/**
 * @brief Get fusion result
 */
int uft_fusion_get_result(uft_fusion_t* fusion, uft_fusion_result_t* result);

/*============================================================================
 * CONFIGURATION
 *============================================================================*/

/**
 * @brief Get default config
 */
void uft_fusion_default_config(uft_fusion_config_t* config);

/**
 * @brief Set configuration
 */
int uft_fusion_set_config(uft_fusion_t* fusion, const uft_fusion_config_t* config);

/*============================================================================
 * CLEANUP
 *============================================================================*/

/**
 * @brief Free fusion result
 */
void uft_fusion_result_free(uft_fusion_result_t* result);

#ifdef __cplusplus
}
#endif

#endif /* UFT_DECODER_FUSION_H */
