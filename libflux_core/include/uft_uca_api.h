/**
 * @file uft_uca_api.h
 * @brief Unified Copy & Analysis API (UCA-API) - Complete Specification
 * 
 * 4-Layer Architecture:
 * 1. Transport Layer - Hardware abstraction
 * 2. Capture Layer - Flux streaming
 * 3. Analysis Layer - Protection detection
 * 4. Verification Layer - Quality check
 * 
 * @version 2.13.0
 * @date 2024-12-27
 */

#ifndef UFT_UCA_API_H
#define UFT_UCA_API_H

#include "uft/uft_error.h"
#include "uft_protection_analysis.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * LAYER 1: TRANSPORT API - Hardware Abstraction
 * ======================================================================== */

typedef enum {
    UFT_TRANSPORT_GREASEWEAZLE,
    UFT_TRANSPORT_SUPERCARD_PRO,
    UFT_TRANSPORT_KRYOFLUX,
    UFT_TRANSPORT_FLUXENGINE,
    UFT_TRANSPORT_FILE,
    UFT_TRANSPORT_MOCK
} uft_transport_type_t;

typedef struct {
    bool supports_flux_read;
    bool supports_flux_write;
    bool supports_index_sync;
    
    uint32_t max_bitrate;
    uint8_t max_tracks;
    uint8_t max_heads;
} uft_transport_caps_t;

/* Transport operations vtable */
typedef struct uft_transport_ops {
    uft_rc_t (*open)(void* ctx, const char* path);
    uft_rc_t (*close)(void* ctx);
    uft_rc_t (*calibrate)(void* ctx);
    uft_rc_t (*seek)(void* ctx, uint8_t track, uint8_t head);
    uft_rc_t (*read_flux)(void* ctx, uint32_t** flux_ns, uint32_t* count);
    uft_rc_t (*write_flux)(void* ctx, const uint32_t* flux_ns, uint32_t count);
} uft_transport_ops_t;

typedef struct {
    uft_transport_type_t type;
    const uft_transport_ops_t* ops;
    uft_transport_caps_t caps;
    void* device_ctx;
} uft_transport_t;

/* Transport API */
uft_rc_t uft_transport_create(
    uft_transport_type_t type,
    const char* device_path,
    uft_transport_t** transport
);

void uft_transport_destroy(uft_transport_t** transport);

/* ========================================================================
 * LAYER 2: CAPTURE API - Flux Streaming
 * ======================================================================== */

typedef enum {
    UFT_CAPTURE_MODE_FLUX,      /* Raw flux */
    UFT_CAPTURE_MODE_BITSTREAM, /* Decoded bits */
    UFT_CAPTURE_MODE_SECTOR     /* Sectors */
} uft_capture_mode_t;

typedef struct {
    uint8_t track;
    uint8_t head;
    
    /* Flux data */
    uint32_t* flux_ns;
    uint32_t flux_count;
    
    /* Bitstream */
    uint8_t* bitstream;
    uint32_t bit_count;
    
    /* Timing */
    uint32_t index_time_ns;
    uint32_t total_time_ns;
} uft_capture_data_t;

typedef struct {
    uft_transport_t* transport;
    uft_capture_mode_t mode;
    
    /* Buffer management */
    size_t buffer_size;
    uint8_t* buffer;
    size_t buffer_used;
    
    /* Statistics */
    uint32_t tracks_captured;
    uint64_t total_flux_transitions;
} uft_capture_ctx_t;

/* Capture API */
uft_rc_t uft_capture_create(
    uft_transport_t* transport,
    uft_capture_ctx_t** capture
);

uft_rc_t uft_capture_track(
    uft_capture_ctx_t* capture,
    uint8_t track,
    uint8_t head,
    uft_capture_data_t** data
);

void uft_capture_destroy(uft_capture_ctx_t** capture);

/* ========================================================================
 * LAYER 3: ANALYSIS API - Protection Detection
 * ======================================================================== */

typedef enum {
    UFT_ANALYSIS_MODE_NONE,
    UFT_ANALYSIS_MODE_FAST,      /* Basic CRC check */
    UFT_ANALYSIS_MODE_DEEP_SCAN  /* Full DPM/weak bits */
} uft_analysis_mode_t;

typedef struct {
    uft_analysis_mode_t mode;
    
    /* Protection context */
    uft_protection_ctx_t* protection_ctx;
    
    /* Results */
    uft_protection_analysis_t* analysis;
    
    /* Statistics */
    uint32_t sectors_analyzed;
    uint32_t weak_bits_found;
    uint32_t dpm_anomalies;
} uft_analysis_ctx_t;

/* Analysis API */
uft_rc_t uft_analysis_create(
    uft_analysis_mode_t mode,
    uft_analysis_ctx_t** analysis
);

uft_rc_t uft_analysis_process_track(
    uft_analysis_ctx_t* analysis,
    const uft_capture_data_t* capture
);

void uft_analysis_destroy(uft_analysis_ctx_t** analysis);

/* ========================================================================
 * LAYER 4: VERIFICATION API - Quality Check
 * ======================================================================== */

typedef enum {
    UFT_VERIFY_NONE,
    UFT_VERIFY_BASIC,       /* CRC only */
    UFT_VERIFY_FULL         /* Physical signature */
} uft_verification_mode_t;

typedef struct {
    /* Sector comparison */
    uint32_t sectors_total;
    uint32_t sectors_matched;
    uint32_t sectors_mismatched;
    
    /* Physical signature */
    bool signature_match;
    uint32_t dpm_deviations;
    
    /* Weak bits */
    bool weak_bits_valid;
    uint32_t weak_bit_mismatches;
    
    /* Quality score (0-100) */
    uint8_t quality_score;
    
    /* Detailed report */
    char report[2048];
} uft_verification_result_t;

typedef struct {
    uft_verification_mode_t mode;
    
    /* Source for comparison */
    uft_transport_t* source_transport;
    
    /* Results */
    uft_verification_result_t result;
} uft_verification_ctx_t;

/* Verification API */
uft_rc_t uft_verification_create(
    uft_verification_mode_t mode,
    uft_verification_ctx_t** verification
);

uft_rc_t uft_verification_verify_track(
    uft_verification_ctx_t* verification,
    const uft_capture_data_t* source,
    const uft_capture_data_t* dest
);

void uft_verification_destroy(uft_verification_ctx_t** verification);

/* ========================================================================
 * UNIFIED COPY CONTEXT - Complete API
 * ======================================================================== */

typedef enum {
    UFT_UCA_MODE_FAST,          /* Fast sector copy */
    UFT_UCA_MODE_NORMAL,        /* Normal with retry */
    UFT_UCA_MODE_DEEP_SCAN      /* Full analysis */
} uft_uca_mode_t;

typedef void (*uft_uca_progress_cb_t)(
    uint8_t percent,
    const char* status,
    void* user_data
);

typedef struct {
    /* Configuration */
    uft_uca_mode_t mode;
    
    /* Layers */
    uft_transport_t* source_transport;
    uft_transport_t* dest_transport;
    uft_capture_ctx_t* capture;
    uft_analysis_ctx_t* analysis;
    uft_verification_ctx_t* verification;
    
    /* Progress */
    uft_uca_progress_cb_t progress_cb;
    void* progress_user_data;
    
    /* Results */
    uft_protection_analysis_t* protection_analysis;
    uft_verification_result_t* verification_result;
    
    /* Statistics */
    uint32_t tracks_copied;
    uint32_t sectors_copied;
    uint32_t errors_encountered;
    
} uft_uca_ctx_t;

/* ========================================================================
 * MAIN API FUNCTIONS
 * ======================================================================== */

/**
 * Create UCA context
 */
uft_rc_t uft_uca_create(uft_uca_ctx_t** ctx);

/**
 * Configure mode
 */
uft_rc_t uft_uca_set_mode(
    uft_uca_ctx_t* ctx,
    uft_uca_mode_t mode
);

/**
 * Set transports
 */
uft_rc_t uft_uca_set_source(
    uft_uca_ctx_t* ctx,
    uft_transport_t* transport
);

uft_rc_t uft_uca_set_dest(
    uft_uca_ctx_t* ctx,
    uft_transport_t* transport
);

/**
 * Set progress callback
 */
uft_rc_t uft_uca_set_progress(
    uft_uca_ctx_t* ctx,
    uft_uca_progress_cb_t callback,
    void* user_data
);

/**
 * Main copy operation
 */
uft_rc_t uft_uca_copy_disk(uft_uca_ctx_t* ctx);

/**
 * Get results
 */
const uft_protection_analysis_t* uft_uca_get_analysis(
    const uft_uca_ctx_t* ctx
);

const uft_verification_result_t* uft_uca_get_verification(
    const uft_uca_ctx_t* ctx
);

/**
 * Export flux profile
 */
uft_rc_t uft_uca_export_flux_profile(
    const uft_uca_ctx_t* ctx,
    const char* path
);

/**
 * Destroy context
 */
void uft_uca_destroy(uft_uca_ctx_t** ctx);

#ifdef __cplusplus
}
#endif

#endif /* UFT_UCA_API_H */
