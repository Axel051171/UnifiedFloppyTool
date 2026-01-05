/**
 * @file uft_apple_gcr_fusion.h
 * @brief Apple GCR Multi-Revolution Fusion API
 * 
 * Combines multiple captures of the same Apple II or Macintosh track
 * to improve read reliability, detect weak bits, and handle
 * copy-protected disks.
 * 
 * @author UFT Team
 * @version 3.4.0
 * @date 2026-01-03
 * 
 * SPDX-License-Identifier: MIT
 */

#ifndef UFT_APPLE_GCR_FUSION_H
#define UFT_APPLE_GCR_FUSION_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Constants
 *============================================================================*/

/** Maximum revolutions to fuse */
#define GCR_FUSION_MAX_REVOLUTIONS  16

/** Fusion methods */
#define GCR_FUSION_METHOD_MAJORITY  0   /**< Simple majority vote */
#define GCR_FUSION_METHOD_WEIGHTED  1   /**< Timing-weighted fusion */
#define GCR_FUSION_METHOD_BEST      2   /**< Use lowest variance revolution */

/*============================================================================
 * Error Codes
 *============================================================================*/

typedef enum {
    GCR_FUSION_OK = 0,              /**< Success */
    GCR_FUSION_ERR_NULL_PARAM,      /**< Null parameter */
    GCR_FUSION_ERR_NO_REVOLUTIONS,  /**< No revolutions added */
    GCR_FUSION_ERR_ALLOC,           /**< Memory allocation failed */
    GCR_FUSION_ERR_ALIGNMENT,       /**< Cannot align revolutions */
    GCR_FUSION_ERR_SHORT_TRACK,     /**< Track too short */
    GCR_FUSION_ERR_COUNT
} gcr_fusion_error_t;

/*============================================================================
 * Opaque Context Type
 *============================================================================*/

typedef struct gcr_fusion_ctx_s gcr_fusion_ctx_t;

/*============================================================================
 * Lifecycle Functions
 *============================================================================*/

/**
 * @brief Create fusion context
 * @return New context or NULL on allocation failure
 */
gcr_fusion_ctx_t *gcr_fusion_create(void);

/**
 * @brief Destroy fusion context and free resources
 * @param ctx Context to destroy
 */
void gcr_fusion_destroy(gcr_fusion_ctx_t *ctx);

/*============================================================================
 * Configuration
 *============================================================================*/

/**
 * @brief Set fusion method
 * @param ctx Fusion context
 * @param method GCR_FUSION_METHOD_* constant
 */
void gcr_fusion_set_method(gcr_fusion_ctx_t *ctx, uint8_t method);

/*============================================================================
 * Revolution Management
 *============================================================================*/

/**
 * @brief Add revolution to fusion context
 * 
 * @param ctx Fusion context
 * @param nibbles Nibble data (caller retains ownership)
 * @param count Number of nibbles
 * @param timings Optional per-nibble timing in ns (caller retains ownership)
 * @return GCR_FUSION_OK on success
 */
gcr_fusion_error_t gcr_fusion_add_revolution(gcr_fusion_ctx_t *ctx,
                                              const uint8_t *nibbles,
                                              uint32_t count,
                                              const double *timings);

/*============================================================================
 * Fusion Execution
 *============================================================================*/

/**
 * @brief Execute fusion on added revolutions
 * 
 * Must have at least one revolution added.
 * 
 * @param ctx Fusion context with added revolutions
 * @return GCR_FUSION_OK on success
 */
gcr_fusion_error_t gcr_fusion_execute(gcr_fusion_ctx_t *ctx);

/*============================================================================
 * Result Retrieval
 *============================================================================*/

/**
 * @brief Get fused nibble data
 * 
 * Call after gcr_fusion_execute().
 * 
 * @param ctx Fusion context
 * @param nibbles Output buffer (caller allocates)
 * @param max_nibbles Buffer size
 * @param out_count Actual nibbles written
 * @return GCR_FUSION_OK on success
 */
gcr_fusion_error_t gcr_fusion_get_result(const gcr_fusion_ctx_t *ctx,
                                          uint8_t *nibbles,
                                          uint32_t max_nibbles,
                                          uint32_t *out_count);

/**
 * @brief Get weak bit mask
 * 
 * Each byte corresponds to one nibble: 1 = weak, 0 = confident.
 * 
 * @param ctx Fusion context
 * @param mask Output buffer (same size as nibbles)
 * @param max_size Buffer size
 * @param weak_count Number of weak nibbles found
 * @return GCR_FUSION_OK on success
 */
gcr_fusion_error_t gcr_fusion_get_weak_mask(const gcr_fusion_ctx_t *ctx,
                                             uint8_t *mask,
                                             uint32_t max_size,
                                             uint32_t *weak_count);

/**
 * @brief Get per-nibble confidence values
 * 
 * Each byte is confidence 0-100.
 * 
 * @param ctx Fusion context
 * @param confidence Output buffer (same size as nibbles)
 * @param max_size Buffer size
 * @return GCR_FUSION_OK on success
 */
gcr_fusion_error_t gcr_fusion_get_confidence(const gcr_fusion_ctx_t *ctx,
                                              uint8_t *confidence,
                                              uint32_t max_size);

/**
 * @brief Get fusion statistics
 * 
 * @param ctx Fusion context
 * @param weak_count Number of weak nibbles
 * @param unanimous_count Number of unanimously agreed nibbles
 * @param avg_confidence Average confidence (0-100)
 * @return GCR_FUSION_OK on success
 */
gcr_fusion_error_t gcr_fusion_get_stats(const gcr_fusion_ctx_t *ctx,
                                         uint32_t *weak_count,
                                         uint32_t *unanimous_count,
                                         double *avg_confidence);

/*============================================================================
 * Convenience Functions
 *============================================================================*/

/**
 * @brief Fuse multiple Apple II track captures (convenience wrapper)
 * 
 * @param nibbles Array of nibble buffers
 * @param counts Array of nibble counts
 * @param rev_count Number of revolutions
 * @param output Output buffer (caller allocates, use max of counts)
 * @param out_count Actual output count
 * @param weak_mask Optional weak bit mask (caller allocates, can be NULL)
 * @return GCR_FUSION_OK on success
 */
gcr_fusion_error_t gcr_fusion_apple_track(const uint8_t **nibbles,
                                           const uint32_t *counts,
                                           uint8_t rev_count,
                                           uint8_t *output,
                                           uint32_t *out_count,
                                           uint8_t *weak_mask);

/*============================================================================
 * Error Handling
 *============================================================================*/

/**
 * @brief Get error string
 * @param err Error code
 * @return Human-readable error string
 */
const char *gcr_fusion_error_string(gcr_fusion_error_t err);

/*============================================================================
 * Unit Tests
 *============================================================================*/

#ifdef UFT_UNIT_TESTS
void uft_apple_gcr_fusion_tests(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* UFT_APPLE_GCR_FUSION_H */
