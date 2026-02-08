/**
 * @file uft_kryoflux.c
 * @brief KryoFlux flux stream processing
 * @version 3.8.0
 */
/**
 * @file uft_kryoflux.c
 * 
 * 
 * Features:
 * - Stream file parsing (.raw)
 * - OOB (Out-of-Band) block handling
 * - Index pulse detection
 * - Multi-revolution support
 * - Flux timing extraction
 */

#include "uft/flux/uft_kryoflux.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/*===========================================================================
 * Constants
 *===========================================================================*/

/* Sample clock: 24.027428 MHz (official spec) */
#define UFT_KF_SAMPLE_CLOCK     24027428.0
#define UFT_KF_SAMPLE_PERIOD    (1.0 / UFT_KF_SAMPLE_CLOCK)

/* Stream opcodes */
#define UFT_KF_FLUX1            0x00    /* Flux1: value 0x00-0x07 */
#define UFT_KF_FLUX2            0x08    /* Flux2: + next byte */
#define UFT_KF_FLUX3            0x09    /* Flux3: + 2 bytes (big endian) */
#define UFT_KF_OVERFLOW         0x0A    /* Overflow: add 0x10000 */
#define UFT_KF_VALUE3           0x0B    /* Value3: + 2 bytes (little endian) */
#define UFT_KF_OOB              0x0D    /* OOB: out-of-band data follows */

/* OOB types */
#define OOB_INVALID         0x00
#define OOB_STREAMINFO      0x01
#define OOB_INDEX           0x02
#define OOB_STREAMEND       0x03
#define OOB_KFINFO          0x04
#define OOB_EOF             0x0D

/*===========================================================================
 * OOB Parsing
 *===========================================================================*/

static int parse_oob(const uint8_t *data, size_t size, size_t *pos,
                     uft_kf_oob_t *oob)
{
    if (*pos + 4 > size) return -1;
    
    memset(oob, 0, sizeof(*oob));
    
    oob->type = data[*pos];
    (*pos)++;
    
    /* Size is little-endian 16-bit */
    uint16_t oob_size = data[*pos] | (data[*pos + 1] << 8);
    *pos += 2;
    
    oob->size = oob_size;
    
    if (*pos + oob_size > size) return -1;
    
    switch (oob->type) {
        case OOB_STREAMINFO:
            if (oob_size >= 8) {
                oob->stream.stream_pos = data[*pos] | 
                                         (data[*pos + 1] << 8) |
                                         ((uint32_t)data[*pos + 2] << 16) |
                                         ((uint32_t)data[*pos + 3] << 24);
                oob->stream.transfer_time = data[*pos + 4] |
                                            (data[*pos + 5] << 8) |
                                            ((uint32_t)data[*pos + 6] << 16) |
                                            ((uint32_t)data[*pos + 7] << 24);
            }
            break;
            
        case OOB_INDEX:
            if (oob_size >= 12) {
                oob->index.stream_pos = data[*pos] |
                                        (data[*pos + 1] << 8) |
                                        ((uint32_t)data[*pos + 2] << 16) |
                                        ((uint32_t)data[*pos + 3] << 24);
                oob->index.sample_counter = data[*pos + 4] |
                                            (data[*pos + 5] << 8) |
                                            ((uint32_t)data[*pos + 6] << 16) |
                                            ((uint32_t)data[*pos + 7] << 24);
                oob->index.index_counter = data[*pos + 8] |
                                           (data[*pos + 9] << 8) |
                                           ((uint32_t)data[*pos + 10] << 16) |
                                           ((uint32_t)data[*pos + 11] << 24);
            }
            break;
            
        case OOB_STREAMEND:
            if (oob_size >= 8) {
                oob->end.stream_pos = data[*pos] |
                                      (data[*pos + 1] << 8) |
                                      ((uint32_t)data[*pos + 2] << 16) |
                                      ((uint32_t)data[*pos + 3] << 24);
                oob->end.result_code = data[*pos + 4] |
                                       (data[*pos + 5] << 8) |
                                       ((uint32_t)data[*pos + 6] << 16) |
                                       ((uint32_t)data[*pos + 7] << 24);
            }
            break;
            
        case OOB_KFINFO:
            /* ASCII text info */
            if (oob_size < sizeof(oob->info.text)) {
                memcpy(oob->info.text, data + *pos, oob_size);
                oob->info.text[oob_size] = '\0';
            }
            break;
    }
    
    *pos += oob_size;
    return 0;
}

/*===========================================================================
 * Stream Parsing
 *===========================================================================*/

int uft_kf_open(uft_kf_ctx_t *ctx, const uint8_t *data, size_t size)
{
    if (!ctx || !data || size < 16) return -1;
    
    memset(ctx, 0, sizeof(*ctx));
    ctx->data = data;
    ctx->size = size;
    
    /* Scan for OOB blocks to get metadata */
    size_t pos = 0;
    
    while (pos < size) {
        uint8_t opcode = data[pos];
        
        if (opcode == UFT_KF_OOB) {
            pos++;
            uft_kf_oob_t oob;
            if (parse_oob(data, size, &pos, &oob) != 0) break;
            
            if (oob.type == OOB_INDEX) {
                ctx->index_count++;
            } else if (oob.type == OOB_KFINFO) {
                /* Parse info string */
                /* Format: "name=value, name=value, ..." */
                char *p = oob.info.text;
                while (*p) {
                    if (strncmp(p, "sck=", 4) == 0) {
                        ctx->sample_clock = atof(p + 4);
                    } else if (strncmp(p, "ick=", 4) == 0) {
                        ctx->index_clock = atof(p + 4);
                    }
                    /* Skip to next comma */
                    while (*p && *p != ',') p++;
                    if (*p == ',') p++;
                    while (*p == ' ') p++;
                }
            } else if (oob.type == OOB_STREAMEND) {
                ctx->result_code = oob.end.result_code;
            }
        } else if (opcode <= 0x07) {
            pos++;  /* Flux1 */
        } else if (opcode == UFT_KF_FLUX2) {
            pos += 2;
        } else if (opcode == UFT_KF_FLUX3) {
            pos += 3;
        } else if (opcode == UFT_KF_OVERFLOW) {
            pos++;
        } else if (opcode == UFT_KF_VALUE3) {
            pos += 3;
        } else {
            pos++;
        }
    }
    
    /* Default clock if not specified */
    if (ctx->sample_clock == 0) {
        ctx->sample_clock = UFT_KF_SAMPLE_CLOCK;
    }
    
    ctx->is_valid = true;
    return 0;
}

void uft_kf_close(uft_kf_ctx_t *ctx)
{
    if (ctx) {
        memset(ctx, 0, sizeof(*ctx));
    }
}

/*===========================================================================
 * Flux Extraction
 *===========================================================================*/

int uft_kf_extract_flux(const uft_kf_ctx_t *ctx,
                        double *flux_times, size_t max_flux,
                        size_t *flux_count,
                        uft_kf_index_t *indices, size_t max_indices,
                        size_t *index_count)
{
    if (!ctx || !flux_times || !flux_count || !ctx->is_valid) {
        return -1;
    }
    
    *flux_count = 0;
    if (index_count) *index_count = 0;
    
    size_t pos = 0;
    uint32_t overflow = 0;
    double current_time = 0;
    uint32_t stream_pos = 0;
    
    while (pos < ctx->size && *flux_count < max_flux) {
        uint8_t opcode = ctx->data[pos];
        
        if (opcode == UFT_KF_OOB) {
            pos++;
            uft_kf_oob_t oob;
            if (parse_oob(ctx->data, ctx->size, &pos, &oob) != 0) break;
            
            if (oob.type == OOB_INDEX && indices && index_count && 
                *index_count < max_indices) {
                indices[*index_count].stream_pos = oob.index.stream_pos;
                indices[*index_count].sample_counter = oob.index.sample_counter;
                indices[*index_count].time = (double)oob.index.sample_counter / 
                                             ctx->sample_clock;
                (*index_count)++;
            }
            continue;
        }
        
        uint32_t flux_value = 0;
        
        if (opcode <= 0x07) {
            /* Flux1: value encoded in opcode */
            flux_value = overflow + opcode + 1;
            overflow = 0;
            pos++;
        } else if (opcode == UFT_KF_FLUX2) {
            /* Flux2: next byte */
            if (pos + 1 >= ctx->size) break;
            flux_value = overflow + 0x08 + ctx->data[pos + 1];
            overflow = 0;
            pos += 2;
        } else if (opcode == UFT_KF_FLUX3) {
            /* Flux3: 2 bytes big-endian */
            if (pos + 2 >= ctx->size) break;
            flux_value = overflow + (ctx->data[pos + 1] << 8) + ctx->data[pos + 2];
            overflow = 0;
            pos += 3;
        } else if (opcode == UFT_KF_OVERFLOW) {
            /* Add 0x10000 to next flux value */
            overflow += 0x10000;
            pos++;
            continue;
        } else if (opcode == UFT_KF_VALUE3) {
            /* Value3: 2 bytes little-endian (used for special purposes) */
            pos += 3;
            continue;
        } else {
            /* Unknown opcode - skip */
            pos++;
            continue;
        }
        
        /* Convert to time */
        double delta = (double)flux_value / ctx->sample_clock;
        current_time += delta;
        
        flux_times[*flux_count] = current_time;
        (*flux_count)++;
        stream_pos++;
    }
    
    return 0;
}

/*===========================================================================
 * Revolution Extraction
 *===========================================================================*/

int uft_kf_extract_revolution(const uft_kf_ctx_t *ctx, int revolution,
                              double *flux_times, size_t max_flux,
                              size_t *flux_count)
{
    if (!ctx || !flux_times || !flux_count || !ctx->is_valid) {
        return -1;
    }
    
    /* First extract all flux and indices */
    double *all_flux = malloc(1000000 * sizeof(double));
    uft_kf_index_t *indices = malloc(100 * sizeof(uft_kf_index_t));
    
    if (!all_flux || !indices) {
        free(all_flux);
        free(indices);
        return -1;
    }
    
    size_t total_flux, total_indices;
    
    int ret = uft_kf_extract_flux(ctx, all_flux, 1000000, &total_flux,
                                  indices, 100, &total_indices);
    
    if (ret != 0 || total_indices < 2) {
        free(all_flux);
        free(indices);
        return -1;
    }
    
    /* Find revolution boundaries */
    if (revolution < 0 || (size_t)revolution >= total_indices - 1) {
        free(all_flux);
        free(indices);
        return -1;
    }
    
    double start_time = indices[revolution].time;
    double end_time = indices[revolution + 1].time;
    
    /* Copy flux within revolution */
    *flux_count = 0;
    
    for (size_t i = 0; i < total_flux && *flux_count < max_flux; i++) {
        if (all_flux[i] >= start_time && all_flux[i] < end_time) {
            flux_times[*flux_count] = all_flux[i] - start_time;
            (*flux_count)++;
        }
    }
    
    free(all_flux);
    free(indices);
    return 0;
}

/*===========================================================================
 * Track Statistics
 *===========================================================================*/

int uft_kf_get_stats(const uft_kf_ctx_t *ctx, uft_kf_stats_t *stats)
{
    if (!ctx || !stats || !ctx->is_valid) return -1;
    
    memset(stats, 0, sizeof(*stats));
    
    stats->sample_clock = ctx->sample_clock;
    stats->index_clock = ctx->index_clock;
    stats->index_count = ctx->index_count;
    stats->result_code = ctx->result_code;
    
    /* Extract flux to count */
    double *flux = malloc(1000000 * sizeof(double));
    uft_kf_index_t *indices = malloc(100 * sizeof(uft_kf_index_t));
    
    if (!flux || !indices) {
        free(flux);
        free(indices);
        return -1;
    }
    
    size_t flux_count, index_count;
    
    if (uft_kf_extract_flux(ctx, flux, 1000000, &flux_count,
                            indices, 100, &index_count) == 0) {
        stats->flux_count = flux_count;
        
        if (flux_count > 1) {
            stats->total_time = flux[flux_count - 1];
            
            /* Calculate intervals */
            double sum = 0, min_int = flux[1], max_int = flux[1];
            
            for (size_t i = 1; i < flux_count; i++) {
                double interval = flux[i] - flux[i - 1];
                sum += interval;
                if (interval < min_int) min_int = interval;
                if (interval > max_int) max_int = interval;
            }
            
            stats->mean_interval = sum / (flux_count - 1);
            stats->min_interval = min_int;
            stats->max_interval = max_int;
        }
        
        /* Calculate RPM from index intervals */
        if (index_count >= 2) {
            double rev_time = indices[1].time - indices[0].time;
            stats->estimated_rpm = 60.0 / rev_time;
        }
    }
    
    free(flux);
    free(indices);
    return 0;
}

/*===========================================================================
 * Report
 *===========================================================================*/

int uft_kf_report_json(const uft_kf_ctx_t *ctx, char *buffer, size_t size)
{
    if (!ctx || !buffer || size == 0) return -1;
    
    uft_kf_stats_t stats;
    if (uft_kf_get_stats(ctx, &stats) != 0) {
        return -1;
    }
    
    int written = snprintf(buffer, size,
        "{\n"
        "  \"format\": \"KryoFlux RAW\",\n"
        "  \"valid\": %s,\n"
        "  \"sample_clock\": %.0f,\n"
        "  \"flux_count\": %zu,\n"
        "  \"index_count\": %zu,\n"
        "  \"estimated_rpm\": %.1f,\n"
        "  \"total_time_ms\": %.3f,\n"
        "  \"mean_interval_us\": %.3f,\n"
        "  \"result_code\": %d,\n"
        "  \"file_size\": %zu\n"
        "}",
        ctx->is_valid ? "true" : "false",
        stats.sample_clock,
        stats.flux_count,
        stats.index_count,
        stats.estimated_rpm,
        stats.total_time * 1000.0,
        stats.mean_interval * 1000000.0,
        stats.result_code,
        ctx->size
    );
    
    return (written > 0 && (size_t)written < size) ? 0 : -1;
}
