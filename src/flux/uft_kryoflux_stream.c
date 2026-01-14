/**
 * @file uft_kryoflux_stream.c
 * @brief KryoFlux flux stream processing
 * @version 3.8.0
 */
/**
 * @file uft_uft_kf_stream.c
 * 
 * 
 * Features:
 * - C2 block parsing (OOB data)
 * - Multi-revolution support
 * - Flux timing extraction
 * - Index pulse detection
 * - Hardware info extraction
 */

#include "uft/flux/uft_uft_kf_stream.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/*===========================================================================
 * Constants
 *===========================================================================*/

/* Stream block types */
#define UFT_KF_FLUX_2       0x00  /* 2-byte flux value */
#define UFT_KF_FLUX_3       0x08  /* 3-byte flux value */
#define UFT_KF_NOP1         0x09  /* NOP (1 byte) */
#define UFT_KF_NOP2         0x0A  /* NOP (2 bytes) */
#define UFT_KF_NOP3         0x0B  /* NOP (3 bytes) */
#define UFT_KF_OVL16        0x0C  /* Overflow 16-bit */
#define UFT_KF_FLUX_1       0x0D  /* 1-byte flux value */
#define UFT_KF_OOB          0x0D  /* Out-of-band marker (context dependent) */

/* OOB block types */
#define OOB_INVALID     0x00
#define OOB_STREAMINFO  0x01
#define OOB_INDEX       0x02
#define OOB_STREAMEND   0x03
#define OOB_KFINFO      0x04
#define OOB_EOF         0x0D

/* Sample clock (nominally 24.027428 MHz for SCK) */
#define UFT_KF_SAMPLE_CLOCK 24027428.0
#define UFT_KF_INDEX_CLOCK  (UFT_KF_SAMPLE_CLOCK / 8.0)  /* ICK = SCK/8 */

/*===========================================================================
 * Stream Parsing Context
 *===========================================================================*/

typedef struct {
    const uint8_t *data;
    size_t size;
    size_t pos;
    
    uint32_t overflow;      /* Current overflow accumulator */
    uint32_t stream_pos;    /* Position in stream (for OOB) */
    
    /* Collected flux data */
    uint32_t *flux_times;
    size_t flux_count;
    size_t flux_capacity;
    
    /* Index positions */
    uft_kf_index_t *indices;
    size_t index_count;
    size_t index_capacity;
    
    /* Stream info */
    uft_kf_stream_info_t info;
} uft_kf_parse_ctx_t;

/*===========================================================================
 * Helpers
 *===========================================================================*/

static uint16_t read_le16(const uint8_t *p)
{
    return p[0] | (p[1] << 8);
}

static uint32_t read_le32(const uint8_t *p)
{
    return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
}

static int add_flux(uft_kf_parse_ctx_t *ctx, uint32_t value)
{
    if (ctx->flux_count >= ctx->flux_capacity) {
        size_t new_cap = ctx->flux_capacity * 2;
        if (new_cap == 0) new_cap = 65536;
        
        uint32_t *new_buf = realloc(ctx->flux_times, new_cap * sizeof(uint32_t));
        if (!new_buf) return -1;
        
        ctx->flux_times = new_buf;
        ctx->flux_capacity = new_cap;
    }
    
    /* Add overflow to value */
    value += ctx->overflow;
    ctx->overflow = 0;
    
    ctx->flux_times[ctx->flux_count++] = value;
    ctx->stream_pos++;
    
    return 0;
}

static int add_index(uft_kf_parse_ctx_t *ctx, const uft_kf_index_t *index)
{
    if (ctx->index_count >= ctx->index_capacity) {
        size_t new_cap = ctx->index_capacity * 2;
        if (new_cap == 0) new_cap = 16;
        
        uft_kf_index_t *new_buf = realloc(ctx->indices, 
                                          new_cap * sizeof(uft_kf_index_t));
        if (!new_buf) return -1;
        
        ctx->indices = new_buf;
        ctx->index_capacity = new_cap;
    }
    
    ctx->indices[ctx->index_count++] = *index;
    return 0;
}

/*===========================================================================
 * OOB Block Parsing
 *===========================================================================*/

static int parse_oob(uft_kf_parse_ctx_t *ctx)
{
    if (ctx->pos + 4 > ctx->size) return -1;
    
    uint8_t type = ctx->data[ctx->pos + 1];
    uint16_t len = read_le16(ctx->data + ctx->pos + 2);
    
    if (ctx->pos + 4 + len > ctx->size) return -1;
    
    const uint8_t *payload = ctx->data + ctx->pos + 4;
    
    switch (type) {
        case OOB_STREAMINFO:
            if (len >= 8) {
                ctx->info.stream_pos = read_le32(payload);
                ctx->info.transfer_time = read_le32(payload + 4);
            }
            break;
            
        case OOB_INDEX:
            if (len >= 12) {
                uft_kf_index_t idx;
                idx.stream_pos = read_le32(payload);
                idx.sample_counter = read_le32(payload + 4);
                idx.index_counter = read_le32(payload + 8);
                idx.flux_position = ctx->flux_count;
                
                add_index(ctx, &idx);
            }
            break;
            
        case OOB_STREAMEND:
            if (len >= 8) {
                ctx->info.stream_pos = read_le32(payload);
                ctx->info.result_code = read_le32(payload + 4);
            }
            ctx->info.stream_end = true;
            break;
            
        case OOB_KFINFO:
            /* Parse key=value pairs */
            {
                char kfinfo[256];
                size_t copy_len = (len < 255) ? len : 255;
                memcpy(kfinfo, payload, copy_len);
                kfinfo[copy_len] = '\0';
                
                /* Look for sck= and ick= */
                char *p = strstr(kfinfo, "sck=");
                if (p) {
                    ctx->info.sample_clock = atof(p + 4);
                }
                p = strstr(kfinfo, "ick=");
                if (p) {
                    ctx->info.index_clock = atof(p + 4);
                }
                
                /* Copy info string */
                strncpy(ctx->info.hw_info, kfinfo, sizeof(ctx->info.hw_info) - 1);
            }
            break;
            
        case OOB_EOF:
            ctx->info.eof_reached = true;
            break;
    }
    
    ctx->pos += 4 + len;
    return 0;
}

/*===========================================================================
 * Stream Block Parsing
 *===========================================================================*/

static int parse_block(uft_kf_parse_ctx_t *ctx)
{
    if (ctx->pos >= ctx->size) return -1;
    
    uint8_t byte = ctx->data[ctx->pos];
    
    /* Check for OOB marker */
    if (byte == 0x0D && ctx->pos + 1 < ctx->size && 
        ctx->data[ctx->pos + 1] != 0x00) {
        /* OOB block */
        return parse_oob(ctx);
    }
    
    /* Flux values */
    if (byte <= 0x07) {
        /* Flux2: 2-byte value */
        if (ctx->pos + 2 > ctx->size) return -1;
        
        uint16_t value = (byte << 8) | ctx->data[ctx->pos + 1];
        add_flux(ctx, value);
        ctx->pos += 2;
    }
    else if (byte == UFT_KF_FLUX_3) {
        /* Flux3: 3-byte value */
        if (ctx->pos + 3 > ctx->size) return -1;
        
        uint32_t value = (ctx->data[ctx->pos + 1] << 8) | ctx->data[ctx->pos + 2];
        add_flux(ctx, value);
        ctx->pos += 3;
    }
    else if (byte == UFT_KF_NOP1) {
        ctx->pos += 1;
    }
    else if (byte == UFT_KF_NOP2) {
        ctx->pos += 2;
    }
    else if (byte == UFT_KF_NOP3) {
        ctx->pos += 3;
    }
    else if (byte == UFT_KF_OVL16) {
        /* 16-bit overflow */
        ctx->overflow += 0x10000;
        ctx->pos += 1;
    }
    else if (byte >= 0x0E) {
        /* Flux1: 1-byte value (0x0E-0xFF = value + 0x0E) */
        uint32_t value = byte;
        add_flux(ctx, value);
        ctx->pos += 1;
    }
    else {
        /* Unknown/reserved */
        ctx->pos += 1;
    }
    
    return 0;
}

/*===========================================================================
 * Public API
 *===========================================================================*/

int uft_kf_stream_open(uft_kf_stream_t *stream, const uint8_t *data, size_t size)
{
    if (!stream || !data || size < 16) return -1;
    
    memset(stream, 0, sizeof(*stream));
    
    uft_kf_parse_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.data = data;
    ctx.size = size;
    ctx.pos = 0;
    
    /* Set default clocks */
    ctx.info.sample_clock = UFT_KF_SAMPLE_CLOCK;
    ctx.info.index_clock = UFT_KF_INDEX_CLOCK;
    
    /* Parse entire stream */
    while (ctx.pos < ctx.size && !ctx.info.eof_reached) {
        if (parse_block(&ctx) != 0) {
            break;
        }
    }
    
    /* Copy results to stream structure */
    stream->flux_times = ctx.flux_times;
    stream->flux_count = ctx.flux_count;
    stream->indices = ctx.indices;
    stream->index_count = ctx.index_count;
    stream->info = ctx.info;
    stream->owns_data = true;
    
    return 0;
}

void uft_kf_stream_close(uft_kf_stream_t *stream)
{
    if (!stream) return;
    
    if (stream->owns_data) {
        free(stream->flux_times);
        free(stream->indices);
    }
    
    memset(stream, 0, sizeof(*stream));
}

/*===========================================================================
 * Revolution Extraction
 *===========================================================================*/

int uft_kf_get_revolution(const uft_kf_stream_t *stream, int rev,
                          uint32_t **flux, size_t *count)
{
    if (!stream || !flux || !count) return -1;
    if (rev < 0 || (size_t)rev >= stream->index_count) return -1;
    
    size_t start_pos, end_pos;
    
    if (rev == 0) {
        start_pos = 0;
    } else {
        start_pos = stream->indices[rev - 1].flux_position;
    }
    
    if ((size_t)rev < stream->index_count) {
        end_pos = stream->indices[rev].flux_position;
    } else {
        end_pos = stream->flux_count;
    }
    
    *count = end_pos - start_pos;
    *flux = stream->flux_times + start_pos;
    
    return 0;
}

int uft_kf_get_revolution_count(const uft_kf_stream_t *stream)
{
    if (!stream) return 0;
    
    /* Number of revolutions = number of index pulses */
    return stream->index_count > 0 ? stream->index_count : 1;
}

/*===========================================================================
 * Timing Conversion
 *===========================================================================*/

double uft_kf_flux_to_us(const uft_kf_stream_t *stream, uint32_t flux_value)
{
    if (!stream || stream->info.sample_clock == 0) return 0.0;
    
    return (double)flux_value * 1000000.0 / stream->info.sample_clock;
}

double uft_kf_get_rpm(const uft_kf_stream_t *stream, int rev)
{
    if (!stream || stream->index_count < 2) return 0.0;
    
    if (rev < 0 || (size_t)rev >= stream->index_count - 1) {
        /* Use first complete revolution */
        rev = 0;
    }
    
    /* Revolution time from index counters */
    uint32_t start = stream->indices[rev].index_counter;
    uint32_t end = stream->indices[rev + 1].index_counter;
    uint32_t ticks = end - start;
    
    if (ticks == 0) return 0.0;
    
    double seconds = (double)ticks / stream->info.index_clock;
    return 60.0 / seconds;
}

/*===========================================================================
 * Statistics
 *===========================================================================*/

int uft_kf_get_stats(const uft_kf_stream_t *stream, uft_kf_stats_t *stats)
{
    if (!stream || !stats) return -1;
    
    memset(stats, 0, sizeof(*stats));
    
    stats->total_flux = stream->flux_count;
    stats->index_count = stream->index_count;
    stats->sample_clock = stream->info.sample_clock;
    
    if (stream->flux_count == 0) return 0;
    
    /* Calculate min/max/mean */
    uint64_t sum = 0;
    stats->min_flux = UINT32_MAX;
    stats->max_flux = 0;
    
    for (size_t i = 0; i < stream->flux_count; i++) {
        uint32_t v = stream->flux_times[i];
        sum += v;
        if (v < stats->min_flux) stats->min_flux = v;
        if (v > stats->max_flux) stats->max_flux = v;
    }
    
    stats->mean_flux = (double)sum / stream->flux_count;
    
    /* Calculate track time */
    if (stream->index_count >= 2) {
        uint32_t start = stream->indices[0].sample_counter;
        uint32_t end = stream->indices[stream->index_count - 1].sample_counter;
        stats->total_time_us = (double)(end - start) * 1000000.0 / 
                               stream->info.sample_clock;
        stats->rpm = uft_kf_get_rpm(stream, 0);
    }
    
    return 0;
}

/*===========================================================================
 * Multi-File Track Loading
 *===========================================================================*/

int uft_kf_load_track(const char *base_path, int track, int side,
                      uft_kf_stream_t *stream)
{
    if (!base_path || !stream) return -1;
    
    /* Build filename: track00.0.raw, track00.1.raw, etc. */
    char filename[512];
    snprintf(filename, sizeof(filename), "%s/track%02d.%d.raw", 
             base_path, track, side);
    
    /* Read file */
    FILE *f = fopen(filename, "rb");
    if (!f) return -1;
    
    if (fseek(f, 0, SEEK_END) != 0) { /* seek error */ }
    size_t size = ftell(f);
    if (fseek(f, 0, SEEK_SET) != 0) { /* seek error */ }
    uint8_t *data = malloc(size);
    if (!data) {
        fclose(f);
        return -1;
    }
    
    if (fread(data, 1, size, f) != size) {
        free(data);
        fclose(f);
        return -1;
    }
    
    fclose(f);
    
    /* Parse stream */
    int result = uft_kf_stream_open(stream, data, size);
    free(data);
    
    return result;
}

/*===========================================================================
 * Report
 *===========================================================================*/

int uft_kf_report_json(const uft_kf_stream_t *stream, char *buffer, size_t size)
{
    if (!stream || !buffer || size == 0) return -1;
    
    uft_kf_stats_t stats;
    uft_kf_get_stats(stream, &stats);
    
    int written = snprintf(buffer, size,
        "{\n"
        "  \"format\": \"KryoFlux RAW\",\n"
        "  \"flux_count\": %zu,\n"
        "  \"index_count\": %zu,\n"
        "  \"sample_clock\": %.0f,\n"
        "  \"rpm\": %.2f,\n"
        "  \"track_time_us\": %.2f,\n"
        "  \"min_flux\": %u,\n"
        "  \"max_flux\": %u,\n"
        "  \"mean_flux\": %.2f,\n"
        "  \"hw_info\": \"%s\"\n"
        "}",
        stream->flux_count,
        stream->index_count,
        stream->info.sample_clock,
        stats.rpm,
        stats.total_time_us,
        stats.min_flux,
        stats.max_flux,
        stats.mean_flux,
        stream->info.hw_info
    );
    
    return (written > 0 && (size_t)written < size) ? 0 : -1;
}
