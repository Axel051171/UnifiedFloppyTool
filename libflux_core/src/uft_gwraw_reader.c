/**
 * @file uft_gwraw_reader.c
 * 
 * UPGRADES IN v3.0:
 * ✅ Thread-safe (mutex protection)
 * ✅ Comprehensive error handling
 * ✅ Input validation
 * ✅ Logging & telemetry
 * ✅ Resource cleanup
 * 
 * @version 3.0.0
 * @date 2024-12-27
 */

#include "uft_gwraw_reader.h"
#include "uft_error_handling.h"
#include "uft_logging.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>

#define DEFAULT_SAMPLE_FREQ 72000000

/* Initial flux buffer capacity */
#define INITIAL_FLUX_CAPACITY 100000

/* GWRAW context - THREAD-SAFE */
struct uft_gwraw_ctx {
    FILE* fp;
    
    /* Configuration */
    uint32_t sample_freq;       /* Sampling frequency (Hz) */
    
    /* Current position */
    long track_start_pos;
    uint8_t current_track;
    uint8_t current_head;
    
    /* Thread safety */
    pthread_mutex_t mutex;
    bool mutex_initialized;
    
    /* Telemetry */
    uft_telemetry_t* telemetry;
    uint64_t total_flux_read;
    uint32_t read_errors;
};

/* ========================================================================
 * HELPER FUNCTIONS
 * ======================================================================== */

/**
 * Default: 72MHz = 13.888ns per tick
 */
static inline uint32_t ticks_to_ns(uint32_t ticks, uint32_t freq_hz) {
    return (uint32_t)(((uint64_t)ticks * 1000000000ULL) / freq_hz);
}

/**
 * Read variable-length encoded value
 * Format: 0-249: value, 250-254: extend, 255: 32-bit follows
 */
static uft_rc_t read_varlen(FILE* fp, uint32_t* value) {
    uint32_t val = 0;
    uint8_t byte;
    
    for (;;) {
        if (fread(&byte, 1, 1, fp) != 1) {
            UFT_RETURN_ERROR(UFT_ERR_IO, "Failed to read varlen byte");
        }
        
        if (byte < 250) {
            /* 0-249: final value */
            val += byte;
            break;
        } else if (byte == 255) {
            /* 255: 32-bit value follows */
            uint8_t buf[4];
            if (fread(buf, 1, 4, fp) != 4) {
                UFT_RETURN_ERROR(UFT_ERR_IO, "Failed to read 32-bit value");
            }
            val += (uint32_t)buf[0] | ((uint32_t)buf[1] << 8) |
                   ((uint32_t)buf[2] << 16) | ((uint32_t)buf[3] << 24);
            break;
        } else {
            /* 250-254: extend and continue */
            val += byte;
        }
    }
    
    *value = val;
    return UFT_SUCCESS;
}

/* ========================================================================
 * OPEN/CLOSE - PROFESSIONAL EDITION
 * ======================================================================== */

uft_rc_t uft_gwraw_open(const char* path, uft_gwraw_ctx_t** ctx) {
    /* INPUT VALIDATION */
    if (!path) {
        UFT_RETURN_ERROR(UFT_ERR_INVALID_ARG, "path is NULL");
    }
    if (!ctx) {
        UFT_RETURN_ERROR(UFT_ERR_INVALID_ARG, "ctx output pointer is NULL");
    }
    
    *ctx = NULL;
    
    UFT_LOG_INFO("Opening GWRAW file: %s", path);
    UFT_TIME_START(t_open);
    
    /* Allocate context with auto-cleanup */
    uft_auto_cleanup(cleanup_free) uft_gwraw_ctx_t* gw = 
        calloc(1, sizeof(uft_gwraw_ctx_t));
    
    if (!gw) {
        UFT_RETURN_ERROR(UFT_ERR_MEMORY,
                        "Failed to allocate GWRAW context (%zu bytes)",
                        sizeof(uft_gwraw_ctx_t));
    }
    
    /* Initialize mutex */
    if (pthread_mutex_init(&gw->mutex, NULL) != 0) {
        UFT_RETURN_ERROR(UFT_ERR_INTERNAL, "Failed to initialize mutex");
    }
    gw->mutex_initialized = true;
    
    /* Open file */
    gw->fp = fopen(path, "rb");
    if (!gw->fp) {
        UFT_RETURN_ERROR(UFT_ERR_NOT_FOUND, "Cannot open file: %s", path);
    }
    
    /* Set default configuration */
    gw->sample_freq = DEFAULT_SAMPLE_FREQ;
    gw->track_start_pos = 0;
    gw->current_track = 0;
    gw->current_head = 0;
    
    /* Create telemetry */
    gw->telemetry = uft_telemetry_create();
    if (!gw->telemetry) {
        UFT_LOG_WARN("Failed to create telemetry (non-fatal)");
    }
    
    /* Success! Transfer ownership */
    *ctx = gw;
    gw = NULL;  /* Prevent cleanup */
    
    UFT_TIME_LOG(t_open, "GWRAW file opened in %.2f ms");
    UFT_LOG_DEBUG("GWRAW: Sample frequency: %u Hz (%.2f MHz)",
                  (*ctx)->sample_freq, (*ctx)->sample_freq / 1000000.0);
    
    return UFT_SUCCESS;
}

void uft_gwraw_close(uft_gwraw_ctx_t** ctx) {
    if (!ctx || !*ctx) {
        return;
    }
    
    uft_gwraw_ctx_t* gw = *ctx;
    
    UFT_LOG_DEBUG("Closing GWRAW context");
    
    /* Log statistics */
    if (gw->telemetry) {
        UFT_LOG_INFO("GWRAW Statistics: %llu flux transitions read, %u errors",
                     (unsigned long long)gw->total_flux_read,
                     gw->read_errors);
        uft_telemetry_log(gw->telemetry);
        uft_telemetry_destroy(&gw->telemetry);
    }
    
    /* Close file */
    if (gw->fp) {
        fclose(gw->fp);
        gw->fp = NULL;
    }
    
    /* Destroy mutex */
    if (gw->mutex_initialized) {
        pthread_mutex_destroy(&gw->mutex);
        gw->mutex_initialized = false;
    }
    
    /* Free context */
    free(gw);
    *ctx = NULL;
    
    UFT_LOG_DEBUG("GWRAW context closed");
}

/* ========================================================================
 * READ TRACK - THREAD-SAFE & VALIDATED
 * ======================================================================== */

uft_rc_t uft_gwraw_read_track(
    uft_gwraw_ctx_t* ctx,
    uint32_t** flux_ns,
    uint32_t* count
) {
    /* INPUT VALIDATION */
    if (!ctx) {
        UFT_RETURN_ERROR(UFT_ERR_INVALID_ARG, "context is NULL");
    }
    
    if (!flux_ns || !count) {
        UFT_RETURN_ERROR(UFT_ERR_INVALID_ARG,
                        "Output pointers are NULL: flux=%p, count=%p",
                        flux_ns, count);
    }
    
    /* THREAD-SAFE LOCK */
    pthread_mutex_lock(&ctx->mutex);
    
    UFT_LOG_DEBUG("Reading GWRAW track");
    UFT_TIME_START(t_read);
    
    *flux_ns = NULL;
    *count = 0;
    
    uft_rc_t result = UFT_SUCCESS;
    
    /* Allocate flux buffer */
    uint32_t capacity = INITIAL_FLUX_CAPACITY;
    uint32_t* flux_buf = malloc(capacity * sizeof(uint32_t));
    
    if (!flux_buf) {
        result = UFT_ERR_MEMORY;
        UFT_SET_ERROR(result, 
                     "Failed to allocate flux buffer (%u entries)",
                     capacity);
        goto cleanup;
    }
    
    uint32_t flux_count = 0;
    
    /* Read flux transitions until end of track marker or EOF */
    while (!feof(ctx->fp)) {
        uint32_t ticks;
        uft_rc_t rc = read_varlen(ctx->fp, &ticks);
        
        if (uft_failed(rc)) {
            if (feof(ctx->fp)) {
                /* EOF is normal end of track */
                break;
            }
            
            result = rc;
            free(flux_buf);
            ctx->read_errors++;
            goto cleanup;
        }
        
        /* Check for end-of-track marker (ticks == 0) */
        if (ticks == 0) {
            break;
        }
        
        /* Expand buffer if needed */
        if (flux_count >= capacity) {
            capacity *= 2;
            uint32_t* new_buf = realloc(flux_buf, capacity * sizeof(uint32_t));
            
            if (!new_buf) {
                result = UFT_ERR_MEMORY;
                UFT_SET_ERROR(result,
                             "Failed to expand flux buffer to %u entries",
                             capacity);
                free(flux_buf);
                goto cleanup;
            }
            
            flux_buf = new_buf;
        }
        
        /* Convert ticks to nanoseconds */
        flux_buf[flux_count++] = ticks_to_ns(ticks, ctx->sample_freq);
    }
    
    /* Shrink buffer to actual size */
    if (flux_count > 0 && flux_count < capacity) {
        uint32_t* final_buf = realloc(flux_buf, flux_count * sizeof(uint32_t));
        if (final_buf) {
            flux_buf = final_buf;
        }
    }
    
    /* Success! */
    *flux_ns = flux_buf;
    *count = flux_count;
    
    /* Update telemetry */
    ctx->total_flux_read += flux_count;
    if (ctx->telemetry) {
        uft_telemetry_update(ctx->telemetry, "flux_transitions", flux_count);
        uft_telemetry_update(ctx->telemetry, "tracks_processed", 1);
    }
    
    UFT_TIME_LOG(t_read, "GWRAW track read in %.2f ms (%u flux)", flux_count);
    
cleanup:
    pthread_mutex_unlock(&ctx->mutex);
    return result;
}

/* ========================================================================
 * CONFIGURATION
 * ======================================================================== */

uft_rc_t uft_gwraw_set_freq(uft_gwraw_ctx_t* ctx, uint32_t freq_hz) {
    if (!ctx) {
        UFT_RETURN_ERROR(UFT_ERR_INVALID_ARG, "context is NULL");
    }
    
    if (freq_hz == 0) {
        UFT_RETURN_ERROR(UFT_ERR_INVALID_ARG, "frequency cannot be 0");
    }
    
    pthread_mutex_lock(&ctx->mutex);
    
    uint32_t old_freq = ctx->sample_freq;
    ctx->sample_freq = freq_hz;
    
    pthread_mutex_unlock(&ctx->mutex);
    
    UFT_LOG_INFO("GWRAW sample frequency changed: %u Hz → %u Hz",
                 old_freq, freq_hz);
    
    return UFT_SUCCESS;
}

uft_rc_t uft_gwraw_rewind(uft_gwraw_ctx_t* ctx) {
    if (!ctx) {
        UFT_RETURN_ERROR(UFT_ERR_INVALID_ARG, "context is NULL");
    }
    
    pthread_mutex_lock(&ctx->mutex);
    
    if (fseek(ctx->fp, 0, SEEK_SET) != 0) {
        pthread_mutex_unlock(&ctx->mutex);
        UFT_RETURN_ERROR(UFT_ERR_IO, "Failed to rewind file");
    }
    
    ctx->track_start_pos = 0;
    ctx->current_track = 0;
    ctx->current_head = 0;
    
    pthread_mutex_unlock(&ctx->mutex);
    
    UFT_LOG_DEBUG("GWRAW file rewound");
    
    return UFT_SUCCESS;
}

/* ========================================================================
 * METADATA
 * ======================================================================== */

uft_rc_t uft_gwraw_get_info(const uft_gwraw_ctx_t* ctx, uft_gwraw_info_t* info) {
    if (!ctx || !info) {
        UFT_RETURN_ERROR(UFT_ERR_INVALID_ARG, "NULL argument");
    }
    
    memset(info, 0, sizeof(*info));
    
    info->sample_freq = ctx->sample_freq;
    info->current_track = ctx->current_track;
    info->current_head = ctx->current_head;
    
    return UFT_SUCCESS;
}
