/**
 * @file uft_kryoflux_parser.c
 * @brief KryoFlux Stream Format Parser Implementation
 * @version 1.0.0
 * @date 2026-01-06
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h>

#include "uft/flux/uft_kryoflux_parser.h"

/*============================================================================
 * Constants
 *============================================================================*/

/* Sample clock: ~48.054 MHz */
static const double SAMPLE_CLOCK = UFT_KF_SAMPLE_CLOCK;

/* Index clock: 1.152 MHz */
static const double INDEX_CLOCK = UFT_KF_INDEX_CLOCK;

/*============================================================================
 * Lifecycle
 *============================================================================*/

uft_kf_ctx_t* uft_kf_create(void)
{
    uft_kf_ctx_t* ctx = calloc(1, sizeof(uft_kf_ctx_t));
    return ctx;
}

void uft_kf_destroy(uft_kf_ctx_t* ctx)
{
    if (!ctx) return;
    
    if (ctx->stream_data) {
        free(ctx->stream_data);
    }
    
    free(ctx);
}

/*============================================================================
 * File Loading
 *============================================================================*/

int uft_kf_load_file(uft_kf_ctx_t* ctx, const char* filename)
{
    if (!ctx || !filename) return UFT_KF_ERR_NULLPTR;
    
    /* Free existing data */
    if (ctx->stream_data) {
        free(ctx->stream_data);
        ctx->stream_data = NULL;
    }
    ctx->stream_size = 0;
    ctx->stream_pos = 0;
    ctx->index_count = 0;
    
    /* Open file */
    FILE* f = fopen(filename, "rb");
    if (!f) {
        ctx->last_error = UFT_KF_ERR_OPEN;
        return UFT_KF_ERR_OPEN;
    }
    
    /* Get size */
    fseek(f, 0, SEEK_END);
    ctx->stream_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    /* Allocate and read */
    ctx->stream_data = malloc(ctx->stream_size);
    if (!ctx->stream_data) {
        fclose(f);
        ctx->last_error = UFT_KF_ERR_MEMORY;
        return UFT_KF_ERR_MEMORY;
    }
    
    if (fread(ctx->stream_data, 1, ctx->stream_size, f) != ctx->stream_size) {
        free(ctx->stream_data);
        ctx->stream_data = NULL;
        fclose(f);
        ctx->last_error = UFT_KF_ERR_READ;
        return UFT_KF_ERR_READ;
    }
    
    fclose(f);
    return UFT_KF_OK;
}

int uft_kf_load_memory(uft_kf_ctx_t* ctx, const uint8_t* data, size_t size)
{
    if (!ctx || !data || size == 0) return UFT_KF_ERR_NULLPTR;
    
    /* Free existing data */
    if (ctx->stream_data) {
        free(ctx->stream_data);
    }
    
    ctx->stream_data = malloc(size);
    if (!ctx->stream_data) {
        ctx->last_error = UFT_KF_ERR_MEMORY;
        return UFT_KF_ERR_MEMORY;
    }
    
    memcpy(ctx->stream_data, data, size);
    ctx->stream_size = size;
    ctx->stream_pos = 0;
    ctx->index_count = 0;
    
    return UFT_KF_OK;
}

/*============================================================================
 * Stream Parsing
 *============================================================================*/

/**
 * @brief First pass: find all indices
 */
static int find_indices(uft_kf_ctx_t* ctx)
{
    ctx->index_count = 0;
    ctx->stream_pos = 0;
    
    while (ctx->stream_pos < ctx->stream_size) {
        uint8_t op = ctx->stream_data[ctx->stream_pos];
        
        if (op <= 0x07) {
            /* Flux2: 2 bytes total */
            ctx->stream_pos += 2;
        } else if (op == UFT_KF_OP_NOP1) {
            ctx->stream_pos += 1;
        } else if (op == UFT_KF_OP_NOP2) {
            ctx->stream_pos += 2;
        } else if (op == UFT_KF_OP_NOP3) {
            ctx->stream_pos += 3;
        } else if (op == UFT_KF_OP_OVL16) {
            ctx->stream_pos += 1;
        } else if (op == UFT_KF_OP_FLUX3) {
            ctx->stream_pos += 3;
        } else if (op == UFT_KF_OP_OOB) {
            /* Out-of-band block */
            if (ctx->stream_pos + 3 > ctx->stream_size) break;
            
            uint8_t oob_type = ctx->stream_data[ctx->stream_pos + 1];
            uint16_t oob_size = ctx->stream_data[ctx->stream_pos + 2] |
                               (ctx->stream_data[ctx->stream_pos + 3] << 8);
            
            if (oob_type == UFT_KF_OOB_INDEX && oob_size >= 12) {
                /* Index block */
                if (ctx->index_count < UFT_KF_MAX_REVOLUTIONS + 1) {
                    size_t data_offset = ctx->stream_pos + 4;
                    if (data_offset + 12 <= ctx->stream_size) {
                        uint8_t* p = &ctx->stream_data[data_offset];
                        ctx->indices[ctx->index_count].stream_pos = 
                            p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
                        ctx->indices[ctx->index_count].sample_counter = 
                            p[4] | (p[5] << 8) | (p[6] << 16) | (p[7] << 24);
                        ctx->indices[ctx->index_count].index_counter = 
                            p[8] | (p[9] << 8) | (p[10] << 16) | (p[11] << 24);
                        ctx->index_count++;
                    }
                }
            } else if (oob_type == UFT_KF_OOB_STREAMINFO && oob_size >= 8) {
                ctx->has_stream_info = true;
                size_t data_offset = ctx->stream_pos + 4;
                if (data_offset + 8 <= ctx->stream_size) {
                    uint8_t* p = &ctx->stream_data[data_offset];
                    ctx->stream_info_pos = p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
                    ctx->xfer_time = p[4] | (p[5] << 8) | (p[6] << 16) | (p[7] << 24);
                }
            } else if (oob_type == UFT_KF_OOB_EOF) {
                break;
            }
            
            ctx->stream_pos += 4 + oob_size;
        } else {
            /* Single byte flux (0x0E-0xFF) */
            ctx->stream_pos += 1;
        }
    }
    
    return (ctx->index_count > 0) ? UFT_KF_OK : UFT_KF_ERR_NO_INDEX;
}

/**
 * @brief Extract flux data between two stream positions
 */
static int extract_flux(uft_kf_ctx_t* ctx, 
                        uint32_t start_pos, uint32_t end_pos,
                        uint32_t** flux_out, uint32_t* count_out)
{
    /* Estimate max flux count */
    size_t max_flux = end_pos - start_pos;
    uint32_t* flux = malloc(max_flux * sizeof(uint32_t));
    if (!flux) return UFT_KF_ERR_MEMORY;
    
    uint32_t flux_count = 0;
    uint32_t overflow = 0;
    ctx->stream_pos = start_pos;
    
    while (ctx->stream_pos < end_pos && ctx->stream_pos < ctx->stream_size) {
        uint8_t op = ctx->stream_data[ctx->stream_pos];
        
        if (op <= 0x07) {
            /* Flux2: high bits in opcode, low byte follows */
            uint8_t low = ctx->stream_data[ctx->stream_pos + 1];
            uint32_t val = ((uint32_t)op << 8) | low;
            flux[flux_count++] = val + overflow;
            overflow = 0;
            ctx->stream_pos += 2;
        } else if (op == UFT_KF_OP_NOP1) {
            ctx->stream_pos += 1;
        } else if (op == UFT_KF_OP_NOP2) {
            ctx->stream_pos += 2;
        } else if (op == UFT_KF_OP_NOP3) {
            ctx->stream_pos += 3;
        } else if (op == UFT_KF_OP_OVL16) {
            overflow += 0x10000;
            ctx->stream_pos += 1;
        } else if (op == UFT_KF_OP_FLUX3) {
            /* 3-byte flux value */
            if (ctx->stream_pos + 3 <= ctx->stream_size) {
                uint32_t val = ctx->stream_data[ctx->stream_pos + 1] |
                              (ctx->stream_data[ctx->stream_pos + 2] << 8);
                flux[flux_count++] = val + overflow;
                overflow = 0;
            }
            ctx->stream_pos += 3;
        } else if (op == UFT_KF_OP_OOB) {
            /* Skip OOB block */
            if (ctx->stream_pos + 4 > ctx->stream_size) break;
            uint16_t oob_size = ctx->stream_data[ctx->stream_pos + 2] |
                               (ctx->stream_data[ctx->stream_pos + 3] << 8);
            ctx->stream_pos += 4 + oob_size;
        } else {
            /* Single byte flux (0x0E-0xFF) */
            flux[flux_count++] = op + overflow;
            overflow = 0;
            ctx->stream_pos += 1;
        }
    }
    
    *flux_out = flux;
    *count_out = flux_count;
    return UFT_KF_OK;
}

int uft_kf_parse_stream(uft_kf_ctx_t* ctx, uft_kf_track_data_t* track)
{
    if (!ctx || !track) return UFT_KF_ERR_NULLPTR;
    if (!ctx->stream_data) return UFT_KF_ERR_FORMAT;
    
    memset(track, 0, sizeof(*track));
    
    /* Find indices first */
    int err = find_indices(ctx);
    if (err != UFT_KF_OK) return err;
    
    /* Extract revolutions between indices */
    track->revolution_count = 0;
    
    for (int i = 0; i < ctx->index_count - 1 && i < UFT_KF_MAX_REVOLUTIONS; i++) {
        uft_kf_revolution_t* rev = &track->revolutions[track->revolution_count];
        
        rev->start_pos = ctx->indices[i].stream_pos;
        rev->end_pos = ctx->indices[i + 1].stream_pos;
        rev->sample_counter = ctx->indices[i].sample_counter;
        rev->index_counter = ctx->indices[i].index_counter;
        
        /* Calculate index time */
        uint32_t next_idx = ctx->indices[i + 1].index_counter;
        uint32_t this_idx = ctx->indices[i].index_counter;
        uint32_t delta = (next_idx >= this_idx) ? 
                         (next_idx - this_idx) : 
                         (0xFFFFFFFF - this_idx + next_idx + 1);
        rev->index_time_us = uft_kf_index_to_us(delta);
        
        /* Extract flux data */
        err = extract_flux(ctx, rev->start_pos, rev->end_pos,
                          &rev->flux_data, &rev->flux_count);
        if (err != UFT_KF_OK) {
            uft_kf_free_track(track);
            return err;
        }
        
        track->revolution_count++;
    }
    
    track->valid = (track->revolution_count > 0);
    return UFT_KF_OK;
}

void uft_kf_free_track(uft_kf_track_data_t* track)
{
    if (!track) return;
    
    for (int i = 0; i < UFT_KF_MAX_REVOLUTIONS; i++) {
        if (track->revolutions[i].flux_data) {
            free(track->revolutions[i].flux_data);
            track->revolutions[i].flux_data = NULL;
        }
    }
    
    memset(track, 0, sizeof(*track));
}

/*============================================================================
 * Utility Functions
 *============================================================================*/

int uft_kf_get_index_count(uft_kf_ctx_t* ctx)
{
    return ctx ? ctx->index_count : 0;
}

bool uft_kf_parse_filename(const char* filename, int* track, int* side)
{
    if (!filename || !track || !side) return false;
    
    /* Find "track" prefix */
    const char* p = filename;
    while (*p && *p != 't' && *p != 'T') p++;
    
    /* Try to match "trackXX.Y" pattern */
    if (strncasecmp(p, "track", 5) == 0) {
        p += 5;
        
        /* Parse track number */
        if (isdigit((unsigned char)p[0]) && isdigit((unsigned char)p[1])) {
            *track = (p[0] - '0') * 10 + (p[1] - '0');
            p += 2;
            
            /* Parse side */
            if (*p == '.') {
                p++;
                if (*p == '0' || *p == '1') {
                    *side = *p - '0';
                    return true;
                }
            }
        }
    }
    
    return false;
}

uint32_t uft_kf_ticks_to_ns(uint32_t ticks)
{
    /* ns = ticks * 1e9 / SAMPLE_CLOCK */
    return (uint32_t)((double)ticks * 1e9 / SAMPLE_CLOCK);
}

double uft_kf_index_to_us(uint32_t ticks)
{
    /* us = ticks * 1e6 / INDEX_CLOCK */
    return (double)ticks * 1e6 / INDEX_CLOCK;
}

uint32_t uft_kf_calculate_rpm(double index_time_us)
{
    if (index_time_us <= 0) return 0;
    /* RPM = 60,000,000 us / index_time_us */
    return (uint32_t)(60000000.0 / index_time_us);
}
