/**
 * @file uft_recovery_pipeline.h
 * @brief Disk Recovery Pipeline API
 * 
 * 5-Stage Recovery Pipeline:
 * 1. Read: Capture flux/sector data from source
 * 2. Validate: Check CRCs, detect errors
 * 3. Repair: Apply error correction, weak bit resolution
 * 4. Rebuild: Reconstruct missing/damaged sectors
 * 5. Verify: Final validation and reporting
 * 
 * @version 1.0.0
 * @date 2025-01-08
 */

#ifndef UFT_RECOVERY_PIPELINE_H
#define UFT_RECOVERY_PIPELINE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * RECOVERY STAGES
 *============================================================================*/

typedef enum {
    UFT_REC_STAGE_NONE = 0,
    UFT_REC_STAGE_READ,         /**< Reading source data */
    UFT_REC_STAGE_VALIDATE,     /**< Validating CRCs */
    UFT_REC_STAGE_REPAIR,       /**< Applying corrections */
    UFT_REC_STAGE_REBUILD,      /**< Reconstructing data */
    UFT_REC_STAGE_VERIFY,       /**< Final verification */
    UFT_REC_STAGE_COMPLETE,     /**< Pipeline complete */
    UFT_REC_STAGE_FAILED,       /**< Pipeline failed */
} uft_rec_stage_t;

/*============================================================================
 * ERROR TYPES
 *============================================================================*/

typedef enum {
    UFT_REC_ERR_NONE = 0,
    UFT_REC_ERR_CRC,            /**< CRC mismatch */
    UFT_REC_ERR_MISSING,        /**< Sector not found */
    UFT_REC_ERR_WEAK,           /**< Weak bits */
    UFT_REC_ERR_HEADER,         /**< Header error */
    UFT_REC_ERR_SYNC,           /**< Sync not found */
    UFT_REC_ERR_FORMAT,         /**< Format error */
    UFT_REC_ERR_HARDWARE,       /**< Hardware error */
} uft_rec_error_t;

/*============================================================================
 * REPAIR METHODS
 *============================================================================*/

typedef enum {
    UFT_REPAIR_NONE = 0,
    UFT_REPAIR_CRC_FLIP,        /**< Single bit flip based on CRC */
    UFT_REPAIR_CONFIDENCE,      /**< Flip low-confidence bits */
    UFT_REPAIR_MULTI_REV,       /**< Use alternate revolution */
    UFT_REPAIR_INTERPOLATE,     /**< Interpolate from neighbors */
    UFT_REPAIR_PATTERN,         /**< Pattern-based reconstruction */
    UFT_REPAIR_ECC,             /**< External ECC if available */
} uft_repair_method_t;

/*============================================================================
 * SECTOR STATUS
 *============================================================================*/

typedef struct {
    int track;
    int side;
    int sector;
    
    uft_rec_error_t error;      /**< Error type */
    uft_repair_method_t repair; /**< Repair method used */
    bool recovered;             /**< true if sector was recovered */
    
    uint16_t crc_stored;
    uint16_t crc_calculated;
    
    double confidence;          /**< Recovery confidence (0.0-1.0) */
    int retries;                /**< Number of retries */
    int revisions_used;         /**< Revolutions used for fusion */
} uft_sector_status_t;

/*============================================================================
 * TRACK STATUS
 *============================================================================*/

typedef struct {
    int track;
    int side;
    
    int total_sectors;
    int good_sectors;
    int repaired_sectors;
    int failed_sectors;
    
    double quality_score;       /**< Overall track quality */
    double rotation_time_ms;
    
    uft_sector_status_t sectors[32];
} uft_track_status_t;

/*============================================================================
 * PIPELINE CONFIGURATION
 *============================================================================*/

typedef struct {
    /* Read settings */
    int max_revolutions;        /**< Max revolutions per track (1-20) */
    int max_retries;            /**< Max retries per sector */
    
    /* Repair settings */
    bool enable_crc_correction; /**< Try CRC-based correction */
    int max_crc_bits;           /**< Max bits to flip for CRC (1-3) */
    bool enable_weak_fill;      /**< Fill weak bits from other revs */
    bool enable_interpolation;  /**< Interpolate missing sectors */
    
    /* Rebuild settings */
    bool enable_pattern_match;  /**< Use pattern matching */
    bool enable_boot_rebuild;   /**< Attempt boot block reconstruction */
    
    /* Quality thresholds */
    double min_confidence;      /**< Minimum acceptable confidence */
    double min_consensus;       /**< Minimum multi-rev consensus */
    
    /* Output options */
    bool keep_original;         /**< Keep original data for comparison */
    bool generate_report;       /**< Generate detailed report */
    bool strict_mode;           /**< Fail on any unrecovered sector */
} uft_recovery_config_t;

/*============================================================================
 * PIPELINE STATISTICS
 *============================================================================*/

typedef struct {
    /* Counters */
    int tracks_processed;
    int tracks_clean;           /**< No errors */
    int tracks_repaired;        /**< Had errors, all recovered */
    int tracks_partial;         /**< Some sectors unrecoverable */
    int tracks_failed;          /**< Completely failed */
    
    int sectors_total;
    int sectors_good;
    int sectors_repaired;
    int sectors_failed;
    
    /* Repair breakdown */
    int repairs_crc_flip;
    int repairs_confidence;
    int repairs_multi_rev;
    int repairs_interpolate;
    int repairs_pattern;
    
    /* Quality */
    double average_confidence;
    double worst_track_quality;
    int worst_track_number;
    
    /* Timing */
    double elapsed_seconds;
    double reads_per_second;
} uft_recovery_stats_t;

/*============================================================================
 * CALLBACK TYPES
 *============================================================================*/

/**
 * @brief Progress callback
 */
typedef void (*uft_recovery_progress_t)(
    uft_rec_stage_t stage,
    int current,
    int total,
    const char* message,
    void* user);

/**
 * @brief Sector callback (called for each sector)
 */
typedef void (*uft_recovery_sector_cb_t)(
    const uft_sector_status_t* sector,
    void* user);

/*============================================================================
 * PIPELINE CONTEXT
 *============================================================================*/

typedef struct uft_recovery_ctx_s uft_recovery_ctx_t;

/**
 * @brief Create recovery pipeline context
 */
uft_recovery_ctx_t* uft_recovery_create(void);

/**
 * @brief Destroy context
 */
void uft_recovery_destroy(uft_recovery_ctx_t* ctx);

/**
 * @brief Configure pipeline
 */
int uft_recovery_configure(uft_recovery_ctx_t* ctx, 
                            const uft_recovery_config_t* config);

/**
 * @brief Get default configuration
 */
void uft_recovery_config_default(uft_recovery_config_t* config);

/**
 * @brief Get paranoid (maximum recovery) configuration
 */
void uft_recovery_config_paranoid(uft_recovery_config_t* config);

/*============================================================================
 * PIPELINE EXECUTION
 *============================================================================*/

/**
 * @brief Set input source (flux file)
 */
int uft_recovery_set_flux_input(uft_recovery_ctx_t* ctx,
                                 const char* path);

/**
 * @brief Set input source (HAL handle for direct capture)
 */
int uft_recovery_set_hal_input(uft_recovery_ctx_t* ctx,
                                void* hal_handle);

/**
 * @brief Set input source (sector image)
 */
int uft_recovery_set_image_input(uft_recovery_ctx_t* ctx,
                                  const char* path);

/**
 * @brief Set output path
 */
int uft_recovery_set_output(uft_recovery_ctx_t* ctx,
                             const char* path);

/**
 * @brief Set progress callback
 */
int uft_recovery_set_progress(uft_recovery_ctx_t* ctx,
                               uft_recovery_progress_t callback,
                               void* user);

/**
 * @brief Set sector callback
 */
int uft_recovery_set_sector_callback(uft_recovery_ctx_t* ctx,
                                      uft_recovery_sector_cb_t callback,
                                      void* user);

/**
 * @brief Run the pipeline
 */
int uft_recovery_run(uft_recovery_ctx_t* ctx);

/**
 * @brief Get pipeline statistics
 */
int uft_recovery_get_stats(const uft_recovery_ctx_t* ctx,
                            uft_recovery_stats_t* stats);

/**
 * @brief Get track status
 */
int uft_recovery_get_track_status(const uft_recovery_ctx_t* ctx,
                                   int track, int side,
                                   uft_track_status_t* status);

/*============================================================================
 * INDIVIDUAL STAGE ACCESS
 *============================================================================*/

/**
 * @brief Run single stage
 */
int uft_recovery_run_stage(uft_recovery_ctx_t* ctx, uft_rec_stage_t stage);

/**
 * @brief Get current stage
 */
uft_rec_stage_t uft_recovery_current_stage(const uft_recovery_ctx_t* ctx);

/*============================================================================
 * REPORTING
 *============================================================================*/

/**
 * @brief Generate recovery report
 */
int uft_recovery_report(const uft_recovery_ctx_t* ctx,
                         const char* path, bool verbose);

/**
 * @brief Get last error message
 */
const char* uft_recovery_get_error(const uft_recovery_ctx_t* ctx);

#ifdef __cplusplus
}
#endif

#endif /* UFT_RECOVERY_PIPELINE_H */
