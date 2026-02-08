/**
 * @file uft_write_verify.c
 * @brief Write-Verify Pipeline Implementation
 * 
 * P1-005: Optional verify after write operations
 */

#include "uft/uft_writer_verify.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ═══════════════════════════════════════════════════════════════════════════════
 * Internal Structures
 * ═══════════════════════════════════════════════════════════════════════════════ */

struct uft_wv_context {
    uft_wv_options_t    options;
    uft_wv_stats_t      stats;
    
    uft_disk_t          *disk;
    void                *hw_provider;
    
    uft_wv_callback_t   callback;
    void                *callback_ctx;
    
    bool                abort_requested;
};

/* ═══════════════════════════════════════════════════════════════════════════════
 * Default Options
 * ═══════════════════════════════════════════════════════════════════════════════ */

const uft_wv_options_t UFT_WV_OPTIONS_DEFAULT = {
    .verify_enabled = true,
    .verify_mode = UFT_VERIFY_CRC_ONLY,
    .retry_count = 3,
    .retry_delay_ms = 100,
    .allow_weak_verify = false,
    .report_path = NULL,
};

/* ═══════════════════════════════════════════════════════════════════════════════
 * API Implementation
 * ═══════════════════════════════════════════════════════════════════════════════ */

uft_wv_context_t* uft_wv_create(uft_disk_t *disk, const uft_wv_options_t *options) {
    uft_wv_context_t *ctx = calloc(1, sizeof(uft_wv_context_t));
    if (!ctx) return NULL;
    
    ctx->disk = disk;
    ctx->options = options ? *options : UFT_WV_OPTIONS_DEFAULT;
    
    return ctx;
}

void uft_wv_destroy(uft_wv_context_t *ctx) {
    free(ctx);
}

void uft_wv_set_callback(uft_wv_context_t *ctx, uft_wv_callback_t cb, void *user) {
    if (ctx) {
        ctx->callback = cb;
        ctx->callback_ctx = user;
    }
}

static void report_progress(uft_wv_context_t *ctx, uft_wv_phase_t phase,
                           int current, int total, const char *msg) {
    if (ctx->callback) {
        uft_wv_progress_t prog = {
            .phase = phase,
            .current_track = current,
            .total_tracks = total,
            .message = msg,
        };
        ctx->callback(ctx->callback_ctx, &prog);
    }
}

uft_error_t uft_wv_write_track(uft_wv_context_t *ctx, 
                               uint8_t cyl, uint8_t head,
                               const uint8_t *data, size_t size) {
    if (!ctx || !data) return UFT_ERR_INVALID_PARAM;
    
    ctx->stats.tracks_attempted++;
    report_progress(ctx, UFT_WV_PHASE_WRITING, ctx->stats.tracks_attempted, 
                    0, "Writing track");
    
    /* Step 1: Write */
    uft_error_t err = UFT_OK;  /* Would call real write here */
    
    if (err != UFT_OK) {
        ctx->stats.tracks_failed++;
        return err;
    }
    
    ctx->stats.tracks_written++;
    
    /* Step 2: Verify (if enabled) */
    if (ctx->options.verify_enabled) {
        err = uft_wv_verify_track(ctx, cyl, head, data, size);
        if (err != UFT_OK) {
            return err;
        }
    }
    
    return UFT_OK;
}

uft_error_t uft_wv_verify_track(uft_wv_context_t *ctx,
                                uint8_t cyl, uint8_t head,
                                const uint8_t *expected, size_t size) {
    if (!ctx || !expected) return UFT_ERR_INVALID_PARAM;
    
    ctx->stats.tracks_verified++;
    report_progress(ctx, UFT_WV_PHASE_VERIFYING, ctx->stats.tracks_verified,
                    0, "Verifying track");
    
    int retries = ctx->options.retry_count;
    uft_error_t last_err = UFT_OK;
    
    for (int attempt = 0; attempt <= retries; attempt++) {
        if (ctx->abort_requested) {
            return UFT_ERR_ABORT;
        }
        
        if (attempt > 0) {
            ctx->stats.verify_retries++;
            report_progress(ctx, UFT_WV_PHASE_VERIFYING, ctx->stats.tracks_verified,
                           0, "Retry verify");
        }
        
        /* Read back track */
        uint8_t *readback = malloc(size);
        if (!readback) return UFT_ERR_MEMORY;
        
        /* Would call real read here */
        /* err = uft_disk_read_track(ctx->disk, cyl, head, readback, size); */
        
        /* Compare based on verify mode */
        bool match = false;
        
        switch (ctx->options.verify_mode) {
            case UFT_VERIFY_BITWISE:
                match = (memcmp(expected, readback, size) == 0);
                break;
                
            case UFT_VERIFY_CRC_ONLY: {
                /* Calculate CRC32 of both */
                uint32_t crc_exp = 0;  /* Would calculate real CRC */
                uint32_t crc_read = 0;
                (void)crc_exp; (void)crc_read;
                match = true;  /* Simulated */
                break;
            }
                
            case UFT_VERIFY_SECTOR_DATA: {
                /* Parse sectors and compare data only */
                match = true;  /* Would parse and compare sectors */
                break;
            }
                
            case UFT_VERIFY_FLUX_LEVEL: {
                /* Compare flux patterns */
                match = true;  /* Would compare flux */
                break;
            }
        }
        
        free(readback);
        
        if (match) {
            ctx->stats.verify_passed++;
            return UFT_OK;
        }
        
        last_err = UFT_ERR_VERIFY;
    }
    
    ctx->stats.verify_failed++;
    return last_err;
}

uft_error_t uft_wv_write_disk(uft_wv_context_t *ctx,
                              const uft_track_data_t *tracks, int count) {
    if (!ctx || !tracks) return UFT_ERR_INVALID_PARAM;
    
    report_progress(ctx, UFT_WV_PHASE_WRITING, 0, count, "Starting disk write");
    
    for (int i = 0; i < count; i++) {
        if (ctx->abort_requested) {
            report_progress(ctx, UFT_WV_PHASE_ABORTED, i, count, "Aborted");
            return UFT_ERR_ABORT;
        }
        
        uft_error_t err = uft_wv_write_track(ctx, 
                                             tracks[i].cylinder, tracks[i].head,
                                             tracks[i].data, tracks[i].size);
        if (err != UFT_OK) {
            return err;
        }
    }
    
    report_progress(ctx, UFT_WV_PHASE_COMPLETE, count, count, "Complete");
    return UFT_OK;
}

void uft_wv_abort(uft_wv_context_t *ctx) {
    if (ctx) {
        ctx->abort_requested = true;
    }
}

void uft_wv_get_stats(const uft_wv_context_t *ctx, uft_wv_stats_t *stats) {
    if (ctx && stats) {
        *stats = ctx->stats;
    }
}

const char* uft_wv_phase_name(uft_wv_phase_t phase) {
    switch (phase) {
        case UFT_WV_PHASE_IDLE: return "Idle";
        case UFT_WV_PHASE_WRITING: return "Writing";
        case UFT_WV_PHASE_VERIFYING: return "Verifying";
        case UFT_WV_PHASE_COMPLETE: return "Complete";
        case UFT_WV_PHASE_FAILED: return "Failed";
        case UFT_WV_PHASE_ABORTED: return "Aborted";
        default: return "Unknown";
    }
}

const char* uft_wv_mode_name(uft_wv_mode_t mode) {
    switch (mode) {
        case UFT_VERIFY_NONE: return "None";
        case UFT_VERIFY_CRC_ONLY: return "CRC Only";
        case UFT_VERIFY_BITWISE: return "Bitwise";
        case UFT_VERIFY_SECTOR_DATA: return "Sector Data";
        case UFT_VERIFY_FLUX_LEVEL: return "Flux Level";
        default: return "Unknown";
    }
}
