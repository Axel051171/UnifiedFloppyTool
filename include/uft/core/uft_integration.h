/**
 * @file uft_integration.h
 * @brief Integration layer for module interoperability
 * 
 * P0-004: Fix API breaks between modules
 * 
 * This layer provides:
 * - Conversion functions between old and new types
 * - Unified callbacks for module communication
 * - Data transfer APIs
 */

#ifndef UFT_INTEGRATION_H
#define UFT_INTEGRATION_H

#include "uft/core/uft_unified_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Module Identification
 * ============================================================================ */

typedef enum {
    UFT_MODULE_PARSER = 0,
    UFT_MODULE_DECODER,
    UFT_MODULE_ENCODER,
    UFT_MODULE_WRITER,
    UFT_MODULE_XCOPY,
    UFT_MODULE_RECOVERY,
    UFT_MODULE_FORENSIC,
    UFT_MODULE_PROTECTION,
    UFT_MODULE_HAL,
    UFT_MODULE_GUI,
    UFT_MODULE_MAX
} uft_module_t;

/**
 * @brief Module capabilities
 */
typedef struct {
    uft_module_t module;
    const char *name;
    const char *version;
    
    bool can_read;
    bool can_write;
    bool can_analyze;
    bool supports_multi_rev;
    bool supports_protection;
    bool supports_timing;
    bool supports_weak_bits;
    
} uft_module_caps_t;

/**
 * @brief Get module capabilities
 */
const uft_module_caps_t* uft_module_get_caps(uft_module_t module);

/* ============================================================================
 * Legacy Type Conversion (for backward compatibility)
 * ============================================================================ */

/* 
 * These functions convert between legacy types used in existing code
 * and the new unified types. They ensure gradual migration without
 * breaking existing functionality.
 */

/* Forward declarations for legacy types (actual defs in original headers) */
struct flux_sector_id;
struct xcopy_sector;
struct c64_sector_id;
struct mfm_idam;

/**
 * @brief Convert legacy flux_sector_id to unified
 */
void uft_convert_from_flux_sector_id(uft_sector_id_t *dest,
                                     const struct flux_sector_id *src);

/**
 * @brief Convert unified to legacy flux_sector_id
 */
void uft_convert_to_flux_sector_id(struct flux_sector_id *dest,
                                   const uft_sector_id_t *src);

/**
 * @brief Convert legacy xcopy_sector to unified
 */
void uft_convert_from_xcopy_sector(uft_sector_id_t *dest,
                                   const struct xcopy_sector *src);

/**
 * @brief Convert unified to legacy xcopy_sector
 */
void uft_convert_to_xcopy_sector(struct xcopy_sector *dest,
                                 const uft_sector_id_t *src);

/* ============================================================================
 * Data Transfer Between Modules
 * ============================================================================ */

/**
 * @brief Track data callback
 */
typedef int (*uft_track_callback_t)(const uft_track_t *track, void *user_data);

/**
 * @brief Sector data callback
 */
typedef int (*uft_sector_callback_t)(const uft_sector_t *sector, void *user_data);

/**
 * @brief Error callback
 */
typedef void (*uft_error_callback_t)(uft_error_t error, 
                                     const char *message,
                                     void *user_data);

/**
 * @brief Progress callback
 */
typedef bool (*uft_progress_callback_t)(int current, int total, 
                                        const char *status,
                                        void *user_data);

/**
 * @brief Integration context for cross-module operations
 */
typedef struct {
    /* Callbacks */
    uft_track_callback_t on_track;
    uft_sector_callback_t on_sector;
    uft_error_callback_t on_error;
    uft_progress_callback_t on_progress;
    void *user_data;
    
    /* Options */
    bool preserve_timing;
    bool preserve_weak_bits;
    bool preserve_errors;
    bool multi_revision;
    
    /* Statistics */
    size_t tracks_processed;
    size_t sectors_processed;
    size_t errors_encountered;
    
} uft_integration_ctx_t;

/**
 * @brief Initialize integration context
 */
void uft_integration_init(uft_integration_ctx_t *ctx);

/* ============================================================================
 * Parser → Other Modules
 * ============================================================================ */

/**
 * @brief Export track from parser to specified module
 * @param track Source track data
 * @param ctx Integration context
 * @param target_module Target module type
 * @return UFT_OK on success
 */
uft_error_t uft_integration_export_track(const uft_track_t *track,
                                         uft_integration_ctx_t *ctx,
                                         uft_module_t target_module);

/**
 * @brief Export entire disk image
 */
uft_error_t uft_integration_export_disk(const uft_disk_image_t *disk,
                                        uft_integration_ctx_t *ctx,
                                        uft_module_t target_module);

/* ============================================================================
 * XCopy Integration
 * ============================================================================ */

/**
 * @brief XCopy context (forward declaration)
 */
typedef struct uft_xcopy_context uft_xcopy_context_t;

/**
 * @brief Import track into XCopy
 */
uft_error_t uft_xcopy_import_track(uft_xcopy_context_t *ctx,
                                   const uft_track_t *track);

/**
 * @brief Export track from XCopy
 */
uft_error_t uft_xcopy_export_track(const uft_xcopy_context_t *ctx,
                                   uint16_t track_num,
                                   uint8_t head,
                                   uft_track_t *out_track);

/* ============================================================================
 * Recovery Integration
 * ============================================================================ */

/**
 * @brief Recovery context (forward declaration)
 */
typedef struct uft_recovery_context uft_recovery_context_t;

/**
 * @brief Import track into Recovery
 */
uft_error_t uft_recovery_import_track(uft_recovery_context_t *ctx,
                                      const uft_track_t *track);

/**
 * @brief Import multiple revisions for fusion
 */
uft_error_t uft_recovery_import_revisions(uft_recovery_context_t *ctx,
                                          const uft_track_t *const *tracks,
                                          size_t track_count);

/**
 * @brief Get recovered track
 */
uft_error_t uft_recovery_get_result(const uft_recovery_context_t *ctx,
                                    uft_track_t *out_track);

/* ============================================================================
 * Forensic Integration
 * ============================================================================ */

/**
 * @brief Forensic report (forward declaration)
 */
typedef struct uft_forensic_report uft_forensic_report_t;

/**
 * @brief Add track to forensic report
 */
uft_error_t uft_forensic_add_track(uft_forensic_report_t *report,
                                   const uft_track_t *track,
                                   const char *source_module);

/**
 * @brief Add sector to forensic report
 */
uft_error_t uft_forensic_add_sector(uft_forensic_report_t *report,
                                    const uft_sector_t *sector,
                                    const char *analysis);

/* ============================================================================
 * Writer Integration
 * ============================================================================ */

/**
 * @brief Writer context (forward declaration)
 */
typedef struct uft_writer uft_writer_t;

/**
 * @brief Queue track for writing
 */
uft_error_t uft_writer_queue_track(uft_writer_t *writer,
                                   const uft_track_t *track);

/**
 * @brief Write queued tracks
 */
uft_error_t uft_writer_flush(uft_writer_t *writer);

/* ============================================================================
 * Protection Analysis Integration
 * ============================================================================ */

/**
 * @brief Protection analyzer (forward declaration)
 */
typedef struct uft_protection_analyzer uft_protection_analyzer_t;

/**
 * @brief Analyze track for protection
 */
uft_error_t uft_protection_analyze_track(uft_protection_analyzer_t *analyzer,
                                         const uft_track_t *track,
                                         uft_protection_info_t *out_info);

/**
 * @brief Analyze full disk for protection
 */
uft_error_t uft_protection_analyze_disk(uft_protection_analyzer_t *analyzer,
                                        const uft_disk_image_t *disk,
                                        uft_protection_info_t *out_info);

/* ============================================================================
 * Pipeline Support
 * ============================================================================ */

/**
 * @brief Pipeline stage
 */
typedef enum {
    UFT_STAGE_READ = 0,
    UFT_STAGE_ANALYZE,
    UFT_STAGE_DECIDE,
    UFT_STAGE_PRESERVE,
    UFT_STAGE_WRITE
} uft_pipeline_stage_t;

/**
 * @brief Pipeline context
 */
typedef struct {
    uft_pipeline_stage_t current_stage;
    uft_disk_image_t *disk;
    
    /* Per-stage results */
    struct {
        bool completed;
        uft_error_t error;
        void *data;
    } stages[5];
    
    /* Callbacks */
    uft_progress_callback_t on_progress;
    uft_error_callback_t on_error;
    void *user_data;
    
} uft_pipeline_ctx_t;

/**
 * @brief Initialize pipeline
 */
uft_error_t uft_pipeline_init(uft_pipeline_ctx_t *ctx);

/**
 * @brief Run pipeline stage
 */
uft_error_t uft_pipeline_run_stage(uft_pipeline_ctx_t *ctx,
                                   uft_pipeline_stage_t stage);

/**
 * @brief Run full pipeline
 */
uft_error_t uft_pipeline_run_full(uft_pipeline_ctx_t *ctx);

/**
 * @brief Get pipeline status string
 */
const char* uft_pipeline_stage_name(uft_pipeline_stage_t stage);

#ifdef __cplusplus
}
#endif

#endif /* UFT_INTEGRATION_H */
