/**
 * @file uft_uca_api.c
 * @brief Unified Copy & Analysis API Implementation
 */

#include "uft_uca_api.h"
#include "uft_scp_reader.h"
#include "uft_gwraw_reader.h"
#include <stdlib.h>
#include <string.h>

/* ========================================================================
 * LAYER 1: TRANSPORT IMPLEMENTATIONS
 * ======================================================================== */

typedef struct {
    uft_gwraw_ctx_t* gwraw_ctx;
} uft_gw_transport_ctx_t;

static uft_rc_t uft_gw_open(void* ctx, const char* path) {
    uft_gw_transport_ctx_t* gw = ctx;
    return uft_gwraw_open(path, &gw->gwraw_ctx);
}

static uft_rc_t uft_gw_close(void* ctx) {
    uft_gw_transport_ctx_t* gw = ctx;
    uft_gwraw_close(&gw->gwraw_ctx);
    return UFT_SUCCESS;
}

static uft_rc_t uft_gw_calibrate(void* ctx) {
    uft_gw_transport_ctx_t* gw = ctx;
    return uft_gwraw_rewind(gw->gwraw_ctx);
}

static uft_rc_t uft_gw_seek(void* ctx, uint8_t track, uint8_t head) {
    /* GWRAW is sequential - cannot seek */
    (void)ctx; (void)track; (void)head;
    return UFT_SUCCESS;
}

static uft_rc_t uft_gw_read_flux(void* ctx, uint32_t** flux_ns, uint32_t* count) {
    uft_gw_transport_ctx_t* gw = ctx;
    bool has_index;
    return uft_gwraw_read_track(gw->gwraw_ctx, flux_ns, count, &has_index);
}

static uft_rc_t uft_gw_write_flux(void* ctx, const uint32_t* flux_ns, uint32_t count) {
    /* Not implemented */
    (void)ctx; (void)flux_ns; (void)count;
    return UFT_ERR_UNSUPPORTED;
}

static const uft_transport_ops_t uft_gw_ops = {
    .open = uft_gw_open,
    .close = uft_gw_close,
    .calibrate = uft_gw_calibrate,
    .seek = uft_gw_seek,
    .read_flux = uft_gw_read_flux,
    .write_flux = uft_gw_write_flux
};

/* SuperCard Pro Transport */
typedef struct {
    uft_scp_ctx_t* scp_ctx;
    uint8_t current_track;
    uint8_t current_head;
} scp_transport_ctx_t;

static uft_rc_t scp_open(void* ctx, const char* path) {
    scp_transport_ctx_t* scp = ctx;
    return uft_scp_open(path, &scp->scp_ctx);
}

static uft_rc_t scp_close(void* ctx) {
    scp_transport_ctx_t* scp = ctx;
    uft_scp_close(&scp->scp_ctx);
    return UFT_SUCCESS;
}

static uft_rc_t scp_calibrate(void* ctx) {
    scp_transport_ctx_t* scp = ctx;
    scp->current_track = 0;
    scp->current_head = 0;
    return UFT_SUCCESS;
}

static uft_rc_t scp_seek(void* ctx, uint8_t track, uint8_t head) {
    scp_transport_ctx_t* scp = ctx;
    scp->current_track = track;
    scp->current_head = head;
    return UFT_SUCCESS;
}

static uft_rc_t scp_read_flux(void* ctx, uint32_t** flux_ns, uint32_t* count) {
    scp_transport_ctx_t* scp = ctx;
    return uft_scp_read_track(scp->scp_ctx, 
                              scp->current_track,
                              scp->current_head,
                              0, /* revolution 0 */
                              flux_ns, count);
}

static uft_rc_t scp_write_flux(void* ctx, const uint32_t* flux_ns, uint32_t count) {
    (void)ctx; (void)flux_ns; (void)count;
    return UFT_ERR_UNSUPPORTED;
}

static const uft_transport_ops_t uft_sc_pro_ops = {
    .open = scp_open,
    .close = scp_close,
    .calibrate = scp_calibrate,
    .seek = scp_seek,
    .read_flux = scp_read_flux,
    .write_flux = scp_write_flux
};

/* ========================================================================
 * TRANSPORT FACTORY
 * ======================================================================== */

uft_rc_t uft_transport_create(
    uft_transport_type_t type,
    const char* device_path,
    uft_transport_t** transport
) {
    UFT_CHECK_NULLS(device_path, transport);
    
    *transport = NULL;
    
    uft_transport_t* t = calloc(1, sizeof(uft_transport_t));
    if (!t) return UFT_ERR_MEMORY;
    
    t->type = type;
    
    /* Select operations & allocate device context */
    switch (type) {
    case UFT_TRANSPORT_GREASEWEAZLE: {
        t->ops = &uft_gw_ops;
        uft_gw_transport_ctx_t* gw = calloc(1, sizeof(uft_gw_transport_ctx_t));
        if (!gw) { free(t); return UFT_ERR_MEMORY; }
        t->device_ctx = gw;
        t->caps.supports_flux_read = true;
        t->caps.max_bitrate = 1000000;
        t->caps.max_tracks = 84;
        t->caps.max_heads = 2;
        break;
    }
    
    case UFT_TRANSPORT_SUPERCARD_PRO: {
        t->ops = &uft_sc_pro_ops;
        scp_transport_ctx_t* scp = calloc(1, sizeof(scp_transport_ctx_t));
        if (!scp) { free(t); return UFT_ERR_MEMORY; }
        t->device_ctx = scp;
        t->caps.supports_flux_read = true;
        t->caps.supports_index_sync = true;
        t->caps.max_bitrate = 1000000;
        t->caps.max_tracks = 84;
        t->caps.max_heads = 2;
        break;
    }
    
    default:
        free(t);
        return UFT_ERR_UNSUPPORTED;
    }
    
    /* Open device */
    uft_rc_t rc = t->ops->open(t->device_ctx, device_path);
    if (uft_failed(rc)) {
        free(t->device_ctx);
        free(t);
        return rc;
    }
    
    *transport = t;
    return UFT_SUCCESS;
}

void uft_transport_destroy(uft_transport_t** transport) {
    if (transport && *transport) {
        if ((*transport)->ops && (*transport)->ops->close) {
            (*transport)->ops->close((*transport)->device_ctx);
        }
        if ((*transport)->device_ctx) {
            free((*transport)->device_ctx);
        }
        free(*transport);
        *transport = NULL;
    }
}

/* ========================================================================
 * LAYER 2: CAPTURE
 * ======================================================================== */

uft_rc_t uft_capture_create(
    uft_transport_t* transport,
    uft_capture_ctx_t** capture
) {
    UFT_CHECK_NULLS(transport, capture);
    
    *capture = calloc(1, sizeof(uft_capture_ctx_t));
    if (!*capture) return UFT_ERR_MEMORY;
    
    (*capture)->transport = transport;
    (*capture)->mode = UFT_CAPTURE_MODE_FLUX;
    (*capture)->buffer_size = 1024 * 1024; /* 1MB */
    (*capture)->buffer = malloc((*capture)->buffer_size);
    
    if (!(*capture)->buffer) {
        free(*capture);
        *capture = NULL;
        return UFT_ERR_MEMORY;
    }
    
    return UFT_SUCCESS;
}

uft_rc_t uft_capture_track(
    uft_capture_ctx_t* capture,
    uint8_t track,
    uint8_t head,
    uft_capture_data_t** data
) {
    UFT_CHECK_NULLS(capture, data);
    
    *data = calloc(1, sizeof(uft_capture_data_t));
    if (!*data) return UFT_ERR_MEMORY;
    
    (*data)->track = track;
    (*data)->head = head;
    
    /* Seek */
    uft_rc_t rc = capture->transport->ops->seek(
        capture->transport->device_ctx, track, head);
    if (uft_failed(rc)) {
        free(*data);
        *data = NULL;
        return rc;
    }
    
    /* Read flux */
    rc = capture->transport->ops->read_flux(
        capture->transport->device_ctx,
        &(*data)->flux_ns,
        &(*data)->flux_count);
    
    if (uft_failed(rc)) {
        free(*data);
        *data = NULL;
        return rc;
    }
    
    capture->tracks_captured++;
    capture->total_flux_transitions += (*data)->flux_count;
    
    return UFT_SUCCESS;
}

void uft_capture_destroy(uft_capture_ctx_t** capture) {
    if (capture && *capture) {
        if ((*capture)->buffer) free((*capture)->buffer);
        free(*capture);
        *capture = NULL;
    }
}

/* ========================================================================
 * LAYER 3: ANALYSIS
 * ======================================================================== */

uft_rc_t uft_analysis_create(
    uft_analysis_mode_t mode,
    uft_analysis_ctx_t** analysis
) {
    UFT_CHECK_NULL(analysis);
    
    *analysis = calloc(1, sizeof(uft_analysis_ctx_t));
    if (!*analysis) return UFT_ERR_MEMORY;
    
    (*analysis)->mode = mode;
    
    if (mode == UFT_ANALYSIS_MODE_DEEP_SCAN) {
        uft_dpm_create(&(*analysis)->protection_ctx);
    }
    
    return UFT_SUCCESS;
}

uft_rc_t uft_analysis_process_track(
    uft_analysis_ctx_t* analysis,
    const uft_capture_data_t* capture
) {
    UFT_CHECK_NULLS(analysis, capture);
    
    if (analysis->mode == UFT_ANALYSIS_MODE_DEEP_SCAN) {
        /* Run protection analysis */
        /* This would call DPM, weak bit detection, etc. */
    }
    
    return UFT_SUCCESS;
}

void uft_analysis_destroy(uft_analysis_ctx_t** analysis) {
    if (analysis && *analysis) {
        if ((*analysis)->protection_ctx) {
            uft_protection_destroy(&(*analysis)->protection_ctx);
        }
        free(*analysis);
        *analysis = NULL;
    }
}

/* ========================================================================
 * LAYER 4: VERIFICATION
 * ======================================================================== */

uft_rc_t uft_verification_create(
    uft_verification_mode_t mode,
    uft_verification_ctx_t** verification
) {
    UFT_CHECK_NULL(verification);
    
    *verification = calloc(1, sizeof(uft_verification_ctx_t));
    if (!*verification) return UFT_ERR_MEMORY;
    
    (*verification)->mode = mode;
    
    return UFT_SUCCESS;
}

uft_rc_t uft_verification_verify_track(
    uft_verification_ctx_t* verification,
    const uft_capture_data_t* source,
    const uft_capture_data_t* dest
) {
    UFT_CHECK_NULLS(verification, source, dest);
    
    /* Compare flux data */
    if (source->flux_count != dest->flux_count) {
        verification->result.sectors_mismatched++;
        return UFT_SUCCESS;
    }
    
    /* Detailed comparison would go here */
    verification->result.sectors_matched++;
    
    return UFT_SUCCESS;
}

void uft_verification_destroy(uft_verification_ctx_t** verification) {
    if (verification && *verification) {
        free(*verification);
        *verification = NULL;
    }
}

/* ========================================================================
 * UNIFIED COPY CONTEXT
 * ======================================================================== */

uft_rc_t uft_uca_create(uft_uca_ctx_t** ctx) {
    UFT_CHECK_NULL(ctx);
    
    *ctx = calloc(1, sizeof(uft_uca_ctx_t));
    if (!*ctx) return UFT_ERR_MEMORY;
    
    (*ctx)->mode = UFT_UCA_MODE_NORMAL;
    
    return UFT_SUCCESS;
}

uft_rc_t uft_uca_set_mode(uft_uca_ctx_t* ctx, uft_uca_mode_t mode) {
    UFT_CHECK_NULL(ctx);
    ctx->mode = mode;
    return UFT_SUCCESS;
}

uft_rc_t uft_uca_set_source(uft_uca_ctx_t* ctx, uft_transport_t* transport) {
    UFT_CHECK_NULLS(ctx, transport);
    ctx->source_transport = transport;
    return UFT_SUCCESS;
}

uft_rc_t uft_uca_set_dest(uft_uca_ctx_t* ctx, uft_transport_t* transport) {
    UFT_CHECK_NULLS(ctx, transport);
    ctx->dest_transport = transport;
    return UFT_SUCCESS;
}

uft_rc_t uft_uca_set_progress(
    uft_uca_ctx_t* ctx,
    uft_uca_progress_cb_t callback,
    void* user_data
) {
    UFT_CHECK_NULL(ctx);
    ctx->progress_cb = callback;
    ctx->progress_user_data = user_data;
    return UFT_SUCCESS;
}

uft_rc_t uft_uca_copy_disk(uft_uca_ctx_t* ctx) {
    UFT_CHECK_NULL(ctx);
    UFT_CHECK_NULL(ctx->source_transport);
    
    /* Create layers */
    uft_capture_create(ctx->source_transport, &ctx->capture);
    
    if (ctx->mode == UFT_UCA_MODE_DEEP_SCAN) {
        uft_analysis_create(UFT_ANALYSIS_MODE_DEEP_SCAN, &ctx->analysis);
        uft_verification_create(UFT_VERIFY_FULL, &ctx->verification);
    }
    
    /* Copy all tracks */
    for (uint8_t t = 0; t < 80; t++) {
        for (uint8_t h = 0; h < 2; h++) {
            uft_capture_data_t* capture_data;
            
            uft_rc_t rc = uft_capture_track(ctx->capture, t, h, &capture_data);
            if (uft_failed(rc)) continue;
            
            /* Analysis */
            if (ctx->analysis) {
                uft_analysis_process_track(ctx->analysis, capture_data);
            }
            
            /* Write to destination */
            /* ... */
            
            /* Progress */
            if (ctx->progress_cb) {
                uint8_t pct = ((t * 2 + h) * 100) / (80 * 2);
                ctx->progress_cb(pct, "Copying...", ctx->progress_user_data);
            }
            
            ctx->tracks_copied++;
            free(capture_data->flux_ns);
            free(capture_data);
        }
    }
    
    return UFT_SUCCESS;
}

const uft_protection_analysis_t* uft_uca_get_analysis(const uft_uca_ctx_t* ctx) {
    return ctx && ctx->analysis ? ctx->analysis->analysis : NULL;
}

const uft_verification_result_t* uft_uca_get_verification(const uft_uca_ctx_t* ctx) {
    return ctx && ctx->verification ? &ctx->verification->result : NULL;
}

uft_rc_t uft_uca_export_flux_profile(const uft_uca_ctx_t* ctx, const char* path) {
    UFT_CHECK_NULLS(ctx, path);
    
    if (!ctx->analysis || !ctx->analysis->analysis) {
        return UFT_ERR_INVALID_STATE;
    }
    
    return uft_protection_export_flux_profile(ctx->analysis->analysis, path);
}

void uft_uca_destroy(uft_uca_ctx_t** ctx) {
    if (ctx && *ctx) {
        if ((*ctx)->capture) uft_capture_destroy(&(*ctx)->capture);
        if ((*ctx)->analysis) uft_analysis_destroy(&(*ctx)->analysis);
        if ((*ctx)->verification) uft_verification_destroy(&(*ctx)->verification);
        free(*ctx);
        *ctx = NULL;
    }
}
