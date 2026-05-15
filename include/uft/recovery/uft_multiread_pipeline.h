#ifndef UFT_MULTIREAD_PIPELINE_H
#define UFT_MULTIREAD_PIPELINE_H
/**
 * @file uft_multiread_pipeline.h
 * @brief Unified Multi-Read Recovery Pipeline — public API.
 *
 * High-level pipeline combining multi-pass reading, majority voting
 * across reads, adaptive strategy, confidence scoring and weak-bit
 * detection — the "recover damaged media without losing the fact that
 * it was damaged" layer.
 *
 * MF-215: this header was extracted from src/recovery/uft_multiread_
 * pipeline.c, which previously defined its entire API inline in the .c
 * (no public surface — nothing outside the TU could call or test it).
 * Pure extraction: the type/constant/prototype declarations moved here
 * verbatim, the .c now #includes this header. No behaviour changed.
 *
 * Forensic note (DESIGN_PRINCIPLES "Kein Bit verloren"): the voting
 * here does NOT silently collapse divergent reads to a majority value
 * and discard the disagreement. When passes disagree at a byte the
 * majority value is emitted, but the divergence is preserved as
 * forensic metadata — weak_mask, has_weak_bits and a sub-100
 * confidence. A failed-CRC read can never outvote a CRC-verified one.
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Constants
 *============================================================================*/

/** Maximum read passes */
#define MULTIREAD_MAX_PASSES        16

/** Default number of passes */
#define MULTIREAD_DEFAULT_PASSES    5

/** Minimum confidence for success */
#define MULTIREAD_MIN_CONFIDENCE    75

/** Majority threshold (percentage) */
#define MULTIREAD_MAJORITY_PCT      66

/** Maximum track size */
#define MULTIREAD_MAX_TRACK_SIZE    32768

/** Maximum sectors per track */
#define MULTIREAD_MAX_SECTORS       32

/*============================================================================
 * Error Codes
 *============================================================================*/

typedef enum {
    MULTIREAD_OK = 0,
    MULTIREAD_ERR_NULL_PARAM,
    MULTIREAD_ERR_ALLOC,
    MULTIREAD_ERR_NO_DATA,
    MULTIREAD_ERR_READ_FAILED,
    MULTIREAD_ERR_INSUFFICIENT_PASSES,
    MULTIREAD_ERR_LOW_CONFIDENCE,
    MULTIREAD_ERR_CRC_FAILED,
    MULTIREAD_ERR_COUNT
} multiread_error_t;

/*============================================================================
 * Data Structures
 *============================================================================*/

/** @brief Single read pass data */
typedef struct {
    uint8_t    *data;               /**< Sector/track data */
    size_t      data_len;           /**< Data length */
    uint8_t     quality;            /**< Read quality (0-100) */
    bool        crc_ok;             /**< CRC passed */
    double      timing_variance;    /**< Timing variance */
} multiread_pass_t;

/** @brief Sector result from multi-read */
typedef struct {
    uint8_t     track;              /**< Track number */
    uint8_t     head;               /**< Head (side) */
    uint8_t     sector;             /**< Sector number */
    uint8_t    *data;               /**< Recovered data */
    size_t      data_len;           /**< Data length */
    uint8_t     confidence;         /**< Recovery confidence (0-100) */
    uint8_t     good_reads;         /**< Number of good reads */
    uint8_t     total_reads;        /**< Total read attempts */
    bool        recovered;          /**< Successfully recovered */
    bool        has_weak_bits;      /**< Contains weak/uncertain bits */
    uint8_t    *weak_mask;          /**< Weak bit mask (optional) */
} multiread_sector_t;

/** @brief Track result from multi-read */
typedef struct {
    uint8_t     track;              /**< Track number */
    uint8_t     head;               /**< Head (side) */
    multiread_sector_t sectors[MULTIREAD_MAX_SECTORS];
    uint8_t     sector_count;       /**< Number of sectors */
    uint8_t     good_sectors;       /**< Sectors with 100% confidence */
    uint8_t     recovered_sectors;  /**< Sectors recovered with voting */
    uint8_t     failed_sectors;     /**< Unrecoverable sectors */
    uint8_t     overall_confidence; /**< Track-level confidence */
} multiread_track_t;

/** @brief Pipeline configuration */
typedef struct {
    uint8_t     min_passes;         /**< Minimum read passes */
    uint8_t     max_passes;         /**< Maximum read passes */
    uint8_t     min_confidence;     /**< Minimum required confidence */
    uint8_t     majority_pct;       /**< Majority vote percentage */
    bool        adaptive_passes;    /**< Increase passes on failure */
    bool        detect_weak_bits;   /**< Enable weak bit detection */
    bool        generate_report;    /**< Generate detailed report */

    /* Callbacks */
    void       *user_data;
    int       (*read_callback)(void *user_data, uint8_t track, uint8_t head,
                               uint8_t *data, size_t *len);
    void      (*progress_callback)(void *user_data, uint8_t track, uint8_t head,
                                   uint8_t pass, uint8_t total_passes);
} multiread_config_t;

/** @brief Pipeline context */
typedef struct {
    multiread_config_t config;

    /* Pass data storage */
    multiread_pass_t passes[MULTIREAD_MAX_PASSES];
    uint8_t pass_count;

    /* Statistics */
    uint32_t total_reads;
    uint32_t successful_reads;
    uint32_t failed_reads;
    uint32_t bytes_recovered;
    double   avg_confidence;
} multiread_ctx_t;

/*============================================================================
 * Public API
 *============================================================================*/

/** @brief Human-readable error string for a multiread_error_t. */
const char *multiread_error_string(multiread_error_t err);

/** @brief Default pipeline configuration. */
multiread_config_t multiread_config_default(void);

/** @brief Create a pipeline context (NULL config → defaults). */
multiread_ctx_t *multiread_create(const multiread_config_t *config);

/** @brief Destroy a pipeline context and free its pass buffers. */
void multiread_destroy(multiread_ctx_t *ctx);

/** @brief Add one read pass (its data is copied into the context). */
multiread_error_t multiread_add_pass(multiread_ctx_t *ctx,
                                     const uint8_t *data, size_t len,
                                     uint8_t quality, bool crc_ok);

/**
 * @brief Vote across the added passes into @p output.
 *
 * Fills @p result including weak_mask / has_weak_bits — divergent reads
 * are preserved as forensic metadata, never silently flattened.
 */
multiread_error_t multiread_execute(multiread_ctx_t *ctx,
                                    uint8_t *output, size_t output_len,
                                    multiread_sector_t *result);

/** @brief High-level: read a track via the config's read_callback + vote. */
multiread_error_t multiread_track(multiread_ctx_t *ctx,
                                  uint8_t track, uint8_t head,
                                  multiread_track_t *result);

/** @brief Convenience: vote across a set of caller-owned buffers. */
multiread_error_t multiread_vote_buffers(const uint8_t **buffers,
                                         const size_t *lengths,
                                         uint8_t buffer_count,
                                         uint8_t *output, size_t output_len,
                                         uint8_t *confidence);

/** @brief Read out the running recovery statistics. */
void multiread_get_stats(const multiread_ctx_t *ctx,
                         uint32_t *total_reads, uint32_t *successful_reads,
                         uint32_t *bytes_recovered, double *avg_confidence);

/** @brief Generate a human-readable recovery report (caller frees). */
char *multiread_generate_report(const multiread_ctx_t *ctx,
                                const multiread_track_t *tracks,
                                uint8_t track_count);

#ifdef __cplusplus
}
#endif

#endif /* UFT_MULTIREAD_PIPELINE_H */
