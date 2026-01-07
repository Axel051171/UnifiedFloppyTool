/**
 * @file uft_uft_kf_streaming.c
 * 
 * P1-002: Memory optimization from 8MB to 2MB peak
 */

#include "uft/flux/uft_uft_kf_streaming.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <sys/stat.h>
#endif

#define UFT_KF_SAMPLE_CLOCK     (18432000 * 73 / 14 / 2)  /* ~48.054 MHz */
#define UFT_KF_INDEX_CLOCK      (18432000 * 73 / 14 / 16) /* ~6.007 MHz */

/* ============================================================================
 * Buffer Pool
 * ============================================================================ */

static void pool_init(uft_kf_stream_ctx_t *ctx) {
    for (int i = 0; i < UFT_KF_POOL_BUFFERS; i++) {
        ctx->pool[i].data = NULL;
        ctx->pool[i].size = 0;
        ctx->pool[i].used = 0;
        ctx->pool[i].in_use = false;
    }
}

static void pool_cleanup(uft_kf_stream_ctx_t *ctx) {
    for (int i = 0; i < UFT_KF_POOL_BUFFERS; i++) {
        free(ctx->pool[i].data);
        ctx->pool[i].data = NULL;
        ctx->pool[i].size = 0;
        ctx->pool[i].in_use = false;
    }
}

uft_kf_buffer_t* uft_kf_pool_acquire(uft_kf_stream_ctx_t *ctx, size_t min_size) {
    if (!ctx) return NULL;
    
    /* Find free buffer */
    for (int i = 0; i < UFT_KF_POOL_BUFFERS; i++) {
        if (!ctx->pool[i].in_use) {
            uft_kf_buffer_t *buf = &ctx->pool[i];
            
            /* Reallocate if needed */
            if (buf->size < min_size) {
                free(buf->data);
                buf->data = malloc(min_size);
                buf->size = buf->data ? min_size : 0;
            }
            
            if (!buf->data) return NULL;
            
            buf->used = 0;
            buf->in_use = true;
            return buf;
        }
    }
    
    return NULL;  /* All buffers in use */
}

void uft_kf_pool_release(uft_kf_stream_ctx_t *ctx, uft_kf_buffer_t *buf) {
    if (!ctx || !buf) return;
    
    buf->used = 0;
    buf->in_use = false;
}

size_t uft_kf_pool_memory_usage(const uft_kf_stream_ctx_t *ctx) {
    if (!ctx) return 0;
    
    size_t total = 0;
    for (int i = 0; i < UFT_KF_POOL_BUFFERS; i++) {
        total += ctx->pool[i].size;
    }
    return total;
}

/* ============================================================================
 * Context Management
 * ============================================================================ */

uft_error_t uft_kf_stream_init(uft_kf_stream_ctx_t *ctx) {
    if (!ctx) return UFT_ERR_INVALID_PARAM;
    
    memset(ctx, 0, sizeof(*ctx));
    ctx->fd = -1;
    ctx->sample_clock = UFT_KF_SAMPLE_CLOCK;
    ctx->index_clock = UFT_KF_INDEX_CLOCK;
    
    pool_init(ctx);
    
    /* Pre-allocate flux array */
    ctx->flux_capacity = 500000;  /* ~500K transitions typical */
    ctx->flux_times = malloc(ctx->flux_capacity * sizeof(uint32_t));
    if (!ctx->flux_times) {
        return UFT_ERR_MEMORY;
    }
    
    ctx->initialized = true;
    return UFT_OK;
}

void uft_kf_stream_free(uft_kf_stream_ctx_t *ctx) {
    if (!ctx) return;
    
    uft_kf_stream_close(ctx);
    pool_cleanup(ctx);
    
    free(ctx->flux_times);
    ctx->flux_times = NULL;
    ctx->flux_capacity = 0;
    
    ctx->initialized = false;
}

void uft_kf_stream_reset(uft_kf_stream_ctx_t *ctx) {
    if (!ctx) return;
    
    ctx->file_pos = 0;
    ctx->chunk_pos = 0;
    ctx->chunk_size = 0;
    ctx->flux_count = 0;
    ctx->index_count = 0;
    ctx->bytes_processed = 0;
    ctx->overflow_count = 0;
    ctx->eof_reached = false;
    ctx->error = UFT_OK;
}

/* ============================================================================
 * File I/O
 * ============================================================================ */

uft_error_t uft_kf_stream_open(uft_kf_stream_ctx_t *ctx, const char *path) {
    if (!ctx || !path) return UFT_ERR_INVALID_PARAM;
    
    uft_kf_stream_close(ctx);
    uft_kf_stream_reset(ctx);
    
#ifdef _WIN32
    /* Windows implementation */
    ctx->fd = open(path, O_RDONLY | O_BINARY);
#else
    /* POSIX implementation with mmap */
    ctx->fd = open(path, O_RDONLY);
    if (ctx->fd < 0) {
        ctx->error = UFT_ERR_IO;
        return UFT_ERR_IO;
    }
    
    struct stat st;
    if (fstat(ctx->fd, &st) < 0) {
        close(ctx->fd);
        ctx->fd = -1;
        ctx->error = UFT_ERR_IO;
        return UFT_ERR_IO;
    }
    
    ctx->mmap_size = st.st_size;
    ctx->mmap_base = mmap(NULL, ctx->mmap_size, PROT_READ, 
                          MAP_PRIVATE, ctx->fd, 0);
    
    if (ctx->mmap_base == MAP_FAILED) {
        ctx->mmap_base = NULL;
        /* Fall back to regular read */
    }
#endif
    
    return UFT_OK;
}

uft_error_t uft_kf_stream_open_mem(uft_kf_stream_ctx_t *ctx,
                               const uint8_t *data, size_t size) {
    if (!ctx || !data) return UFT_ERR_INVALID_PARAM;
    
    uft_kf_stream_close(ctx);
    uft_kf_stream_reset(ctx);
    
    ctx->mmap_base = data;
    ctx->mmap_size = size;
    ctx->fd = -1;  /* No file descriptor */
    
    return UFT_OK;
}

void uft_kf_stream_close(uft_kf_stream_ctx_t *ctx) {
    if (!ctx) return;
    
#ifndef _WIN32
    if (ctx->mmap_base && ctx->fd >= 0) {
        munmap((void*)ctx->mmap_base, ctx->mmap_size);
    }
#endif
    
    if (ctx->fd >= 0) {
        close(ctx->fd);
        ctx->fd = -1;
    }
    
    ctx->mmap_base = NULL;
    ctx->mmap_size = 0;
}

/* ============================================================================
 * Chunk Processing
 * ============================================================================ */

int uft_kf_stream_read_chunk(uft_kf_stream_ctx_t *ctx) {
    if (!ctx) return -1;
    
    if (ctx->eof_reached) return 0;
    
    size_t to_read = UFT_KF_CHUNK_SIZE;
    
    if (ctx->mmap_base) {
        /* Memory-mapped read */
        if (ctx->file_pos >= ctx->mmap_size) {
            ctx->eof_reached = true;
            return 0;
        }
        
        size_t remaining = ctx->mmap_size - ctx->file_pos;
        if (to_read > remaining) {
            to_read = remaining;
        }
        
        memcpy(ctx->chunk, ctx->mmap_base + ctx->file_pos, to_read);
        ctx->file_pos += to_read;
        ctx->chunk_size = to_read;
        ctx->chunk_pos = 0;
        
        return (int)to_read;
    }
    
#ifndef _WIN32
    if (ctx->fd >= 0) {
        /* Regular file read */
        ssize_t n = read(ctx->fd, ctx->chunk, to_read);
        if (n <= 0) {
            ctx->eof_reached = true;
            return (n < 0) ? -1 : 0;
        }
        
        ctx->chunk_size = n;
        ctx->chunk_pos = 0;
        return (int)n;
    }
#endif
    
    return -1;
}

/* Grow flux array if needed */
static bool ensure_flux_capacity(uft_kf_stream_ctx_t *ctx, size_t needed) {
    if (ctx->flux_count + needed <= ctx->flux_capacity) {
        return true;
    }
    
    size_t new_capacity = ctx->flux_capacity * 2;
    if (new_capacity > 10000000) {
        /* Hard limit: 10M transitions */
        return false;
    }
    
    uint32_t *new_flux = realloc(ctx->flux_times, 
                                  new_capacity * sizeof(uint32_t));
    if (!new_flux) {
        return false;
    }
    
    ctx->flux_times = new_flux;
    ctx->flux_capacity = new_capacity;
    return true;
}

uft_error_t uft_kf_stream_process_chunk(uft_kf_stream_ctx_t *ctx) {
    if (!ctx) return UFT_ERR_INVALID_PARAM;
    
    uint32_t flux_acc = 0;
    
    while (ctx->chunk_pos < ctx->chunk_size) {
        uint8_t byte = ctx->chunk[ctx->chunk_pos++];
        ctx->bytes_processed++;
        
        if (byte == 0x00) {
            /* Flux1: Single byte */
            /* Process any pending OOB */
        } else if (byte <= 0x07) {
            /* Flux2: Two-byte value */
            if (ctx->chunk_pos >= ctx->chunk_size) break;
            uint8_t byte2 = ctx->chunk[ctx->chunk_pos++];
            ctx->bytes_processed++;
            
            uint32_t flux = ((uint32_t)byte << 8) | byte2;
            flux_acc += flux;
            
            if (!ensure_flux_capacity(ctx, 1)) {
                return UFT_ERR_MEMORY;
            }
            ctx->flux_times[ctx->flux_count++] = flux_acc;
            flux_acc = 0;
            
        } else if (byte >= 0x08 && byte <= 0x0C) {
            /* OOB block */
            uint8_t oob_type = byte;
            
            if (ctx->chunk_pos + 2 > ctx->chunk_size) break;
            uint16_t oob_size = ctx->chunk[ctx->chunk_pos] |
                               (ctx->chunk[ctx->chunk_pos + 1] << 8);
            ctx->chunk_pos += 2;
            ctx->bytes_processed += 2;
            
            if (ctx->chunk_pos + oob_size > ctx->chunk_size) break;
            
            switch (oob_type) {
                case UFT_KF_OOB_INDEX:
                    if (oob_size >= 12 && ctx->index_count < 8) {
                        const uint8_t *p = &ctx->chunk[ctx->chunk_pos];
                        ctx->indices[ctx->index_count].stream_position =
                            p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
                        ctx->indices[ctx->index_count].sample_counter =
                            p[4] | (p[5] << 8) | (p[6] << 16) | (p[7] << 24);
                        ctx->indices[ctx->index_count].index_counter =
                            p[8] | (p[9] << 8) | (p[10] << 16) | (p[11] << 24);
                        ctx->index_count++;
                    }
                    break;
                    
                case UFT_KF_OOB_STREAM_END:
                case UFT_KF_OOB_EOF:
                    ctx->eof_reached = true;
                    break;
                    
                default:
                    break;
            }
            
            ctx->chunk_pos += oob_size;
            ctx->bytes_processed += oob_size;
            
        } else if (byte == 0x0D) {
            /* OOB EOF */
            ctx->eof_reached = true;
            break;
            
        } else if (byte >= 0x0E && byte <= 0xFF) {
            /* Flux3: Direct single byte */
            flux_acc += byte;
            
            if (!ensure_flux_capacity(ctx, 1)) {
                return UFT_ERR_MEMORY;
            }
            ctx->flux_times[ctx->flux_count++] = flux_acc;
            flux_acc = 0;
        }
    }
    
    return UFT_OK;
}

/* ============================================================================
 * Track Decoding
 * ============================================================================ */

uft_error_t uft_kf_stream_decode_track(uft_kf_stream_ctx_t *ctx,
                                   uft_kf_chunk_callback_t callback,
                                   void *user_data) {
    if (!ctx) return UFT_ERR_INVALID_PARAM;
    
    int chunk_index = 0;
    uft_error_t err;
    
    while (!ctx->eof_reached) {
        int bytes_read = uft_kf_stream_read_chunk(ctx);
        if (bytes_read < 0) {
            return UFT_ERR_IO;
        }
        if (bytes_read == 0) break;
        
        err = uft_kf_stream_process_chunk(ctx);
        if (err != UFT_OK) return err;
        
        /* Call callback with current flux data */
        if (callback) {
            int result = callback(ctx->flux_times, ctx->flux_count,
                                  chunk_index, user_data);
            if (result != 0) {
                return UFT_ERR_CANCELLED;
            }
        }
        
        chunk_index++;
    }
    
    return UFT_OK;
}

uft_error_t uft_kf_stream_to_track(uft_kf_stream_ctx_t *ctx, uft_track_t *out_track) {
    if (!ctx || !out_track) return UFT_ERR_INVALID_PARAM;
    
    /* Process entire file */
    uft_error_t err = uft_kf_stream_decode_track(ctx, NULL, NULL);
    if (err != UFT_OK) return err;
    
    /* Convert flux to track */
    out_track->flux_times = malloc(ctx->flux_count * sizeof(double));
    if (!out_track->flux_times) {
        return UFT_ERR_MEMORY;
    }
    
    /* Convert sample counts to nanoseconds */
    double ns_per_sample = 1e9 / ctx->sample_clock;
    
    for (size_t i = 0; i < ctx->flux_count; i++) {
        out_track->flux_times[i] = ctx->flux_times[i] * ns_per_sample;
    }
    out_track->flux_count = ctx->flux_count;
    
    return UFT_OK;
}

/* ============================================================================
 * Disk Decoding
 * ============================================================================ */

uft_error_t uft_kf_stream_to_disk(const char *directory,
                              uft_disk_image_t **out_disk) {
    if (!directory || !out_disk) return UFT_ERR_INVALID_PARAM;
    
    /* Create disk image */
    uft_disk_image_t *disk = uft_disk_alloc(84, 2);
    if (!disk) return UFT_ERR_MEMORY;
    
    disk->format = UFT_FMT_UFT_KF_RAW;
    snprintf(disk->format_name, sizeof(disk->format_name), "KF/RAW");
    
    uft_kf_stream_ctx_t ctx;
    if (uft_kf_stream_init(&ctx) != UFT_OK) {
        uft_disk_free(disk);
        return UFT_ERR_MEMORY;
    }
    
    /* Process each track file */
    char path[1024];
    
    for (int track = 0; track < 84; track++) {
        for (int side = 0; side < 2; side++) {
            /* Build filename: trackXX.Y.raw */
            snprintf(path, sizeof(path), "%s/track%02d.%d.raw",
                     directory, track, side);
            
            if (uft_kf_stream_open(&ctx, path) != UFT_OK) {
                continue;  /* Track file not found */
            }
            
            uft_track_t *trk = uft_track_alloc(0, 0);
            if (!trk) continue;
            
            trk->track_num = track;
            trk->head = side;
            
            if (uft_kf_stream_to_track(&ctx, trk) == UFT_OK) {
                size_t idx = track * 2 + side;
                disk->track_data[idx] = trk;
            } else {
                uft_track_free(trk);
            }
            
            uft_kf_stream_close(&ctx);
            uft_kf_stream_reset(&ctx);
        }
    }
    
    uft_kf_stream_free(&ctx);
    
    *out_disk = disk;
    return UFT_OK;
}

/* ============================================================================
 * Utilities
 * ============================================================================ */

double uft_kf_samples_to_ns(const uft_kf_stream_ctx_t *ctx, uint32_t samples) {
    if (!ctx || ctx->sample_clock == 0) return 0;
    return (double)samples * 1e9 / ctx->sample_clock;
}

void uft_kf_stream_get_memory_stats(const uft_kf_stream_ctx_t *ctx,
                                uft_kf_memory_stats_t *stats) {
    if (!ctx || !stats) return;
    
    memset(stats, 0, sizeof(*stats));
    
    stats->pool_memory = uft_kf_pool_memory_usage(ctx);
    stats->flux_memory = ctx->flux_capacity * sizeof(uint32_t);
    stats->current_memory = stats->pool_memory + stats->flux_memory + 
                            sizeof(uft_kf_stream_ctx_t);
    stats->peak_memory = stats->current_memory;  /* Simplified */
}
