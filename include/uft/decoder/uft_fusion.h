/**
 * @file uft_fusion.h
 * @brief Multi-Revolution Fusion API
 */

#ifndef UFT_DECODER_FUSION_H
#define UFT_DECODER_FUSION_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Fusion configuration
 */
typedef struct {
    double min_consensus;      /**< Minimum consensus threshold (0.0-1.0) */
    bool detect_weak_bits;     /**< Detect and mark weak bits */
    bool use_weighting;        /**< Use quality weighting */
    int max_revolutions;       /**< Maximum revolutions to process */
} uft_fusion_config_t;

/**
 * @brief Fusion result
 */
typedef struct {
    uint8_t* data;             /**< Fused data */
    size_t length;             /**< Data length */
    double* confidence;        /**< Per-bit confidence */
    uint8_t* weak_bits;        /**< Weak bit map */
    double average_confidence; /**< Average confidence */
    int weak_count;            /**< Number of weak bits */
} uft_fusion_result_t;

/**
 * @brief Fusion context (opaque)
 */
typedef struct uft_fusion_s uft_fusion_t;

/**
 * @brief Create fusion context with default config
 */
uft_fusion_t* uft_fusion_create(void);

/**
 * @brief Create fusion context with config
 */
uft_fusion_t* uft_fusion_create_with_config(const uft_fusion_config_t* config);

/**
 * @brief Destroy fusion context
 */
void uft_fusion_destroy(uft_fusion_t* fusion);

/**
 * @brief Add revolution data
 */
int uft_fusion_add_revolution(uft_fusion_t* fusion, const uint8_t* data, 
                              size_t length, double quality, uint32_t index_pos);

/**
 * @brief Process all revolutions
 */
int uft_fusion_process(uft_fusion_t* fusion);

/**
 * @brief Get fusion result
 */
int uft_fusion_get_result(uft_fusion_t* fusion, uft_fusion_result_t* result);

/**
 * @brief Get default config
 */
void uft_fusion_default_config(uft_fusion_config_t* config);

#ifdef __cplusplus
}
#endif

#endif /* UFT_DECODER_FUSION_H */
