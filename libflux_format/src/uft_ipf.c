/*
 * uft_ipf.c
 * 
 * UnifiedFloppyTool - IPF (Interchangeable Preservation Format) Parser
 * 
 * SPS/CAPS format implementation for copy-protected disk preservation
 * 
 * Copyright (c) 2025 UFT Project
 * SPDX-License-Identifier: MIT
 */

#include "uft/uft_ipf.h"
#include <stdlib.h>
#include <string.h>

/*============================================================================
 * Internal Helpers
 *============================================================================*/

/* Read big-endian uint32 */
static inline uint32_t read_be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8)  | (uint32_t)p[3];
}

/* Read little-endian uint32 (IPF uses big-endian, but some fields may vary) */
static inline uint32_t read_le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* Byte swap */
static inline uint32_t bswap32(uint32_t x)
{
    return ((x >> 24) & 0xFF) | ((x >> 8) & 0xFF00) |
           ((x << 8) & 0xFF0000) | ((x << 24) & 0xFF000000);
}

/*============================================================================
 * CRC32 Implementation (IPF-compatible)
 *============================================================================*/

static uint32_t g_crc32_table[256];
static bool g_crc32_initialized = false;

static void init_crc32_table(void)
{
    if (g_crc32_initialized) return;
    
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t crc = i;
        for (int j = 0; j < 8; j++) {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xEDB88320;
            else
                crc >>= 1;
        }
        g_crc32_table[i] = crc;
    }
    g_crc32_initialized = true;
}

uint32_t ipf_crc32(const uint8_t *data, size_t len)
{
    init_crc32_table();
    
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
        crc = g_crc32_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    return ~crc;
}

bool ipf_verify_crc(const uint8_t *data, size_t len, uint32_t expected)
{
    return ipf_crc32(data, len) == expected;
}

/*============================================================================
 * Format Detection
 *============================================================================*/

bool ipf_is_ipf_buffer(const uint8_t *data, size_t len)
{
    if (!data || len < 12) return false;
    
    /* Check CAPS magic in first chunk */
    uint32_t chunk_type = read_be32(data);
    return chunk_type == IPF_CHUNK_CAPS;
}

bool ipf_is_ipf_file(const char *path)
{
    if (!path) return false;
    
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    
    uint8_t header[12];
    size_t read = fread(header, 1, sizeof(header), f);
    fclose(f);
    
    if (read < 12) return false;
    return ipf_is_ipf_buffer(header, read);
}

/*============================================================================
 * Memory Management
 *============================================================================*/

ipf_image_t* ipf_image_create(void)
{
    ipf_image_t *img = calloc(1, sizeof(ipf_image_t));
    if (!img) return NULL;
    
    img->media_type = IPF_MEDIA_FLOPPY;
    img->encoder_type = IPF_ENCODER_UNKNOWN;
    
    return img;
}

static void free_track(ipf_track_t *track)
{
    if (!track) return;
    
    free(track->bitstream);
    free(track->weak_mask);
    free(track->timing);
    
    for (size_t i = 0; i < track->sector_count; i++) {
        free(track->sectors[i].data);
    }
    
    free(track);
}

void ipf_image_free(ipf_image_t *img)
{
    if (!img) return;
    
    for (size_t i = 0; i < IPF_MAX_TRACKS; i++) {
        if (img->tracks[i]) {
            free_track(img->tracks[i]);
        }
    }
    
    free(img);
}

/*============================================================================
 * Chunk Parsing
 *============================================================================*/

typedef struct parse_context {
    const uint8_t *data;
    size_t len;
    size_t pos;
    ipf_image_t *img;
    ipf_error_t error;
    
    /* Collected records for later processing */
    ipf_info_record_t info;
    bool has_info;
    
    /* Image records indexed by key */
    ipf_image_record_t *image_records;
    size_t image_record_count;
    size_t image_record_cap;
} parse_context_t;

static bool parse_info_chunk(parse_context_t *ctx, size_t chunk_len)
{
    if (chunk_len < sizeof(ipf_info_record_t)) {
        ctx->error = IPF_ERR_FORMAT;
        return false;
    }
    
    const uint8_t *p = ctx->data + ctx->pos;
    
    ctx->info.media_type = read_be32(p + 0);
    ctx->info.encoder_type = read_be32(p + 4);
    ctx->info.encoder_rev = read_be32(p + 8);
    ctx->info.file_key = read_be32(p + 12);
    ctx->info.file_rev = read_be32(p + 16);
    ctx->info.origin = read_be32(p + 20);
    ctx->info.min_track = read_be32(p + 24);
    ctx->info.max_track = read_be32(p + 28);
    ctx->info.min_side = read_be32(p + 32);
    ctx->info.max_side = read_be32(p + 36);
    ctx->info.creation_date = read_be32(p + 40);
    ctx->info.creation_time = read_be32(p + 44);
    
    for (int i = 0; i < 4; i++) {
        ctx->info.platforms[i] = read_be32(p + 48 + i * 4);
    }
    
    ctx->info.disk_number = read_be32(p + 64);
    ctx->info.creator_id = read_be32(p + 68);
    
    /* Copy to image */
    ctx->img->media_type = (ipf_media_type_t)ctx->info.media_type;
    ctx->img->encoder_type = (ipf_encoder_type_t)ctx->info.encoder_type;
    ctx->img->encoder_rev = ctx->info.encoder_rev;
    ctx->img->min_track = ctx->info.min_track;
    ctx->img->max_track = ctx->info.max_track;
    ctx->img->min_side = ctx->info.min_side;
    ctx->img->max_side = ctx->info.max_side;
    ctx->img->creation_date = ctx->info.creation_date;
    ctx->img->creation_time = ctx->info.creation_time;
    ctx->img->disk_number = ctx->info.disk_number;
    ctx->img->creator_id = ctx->info.creator_id;
    
    /* Convert platforms */
    ctx->img->platform_count = 0;
    for (int i = 0; i < 4; i++) {
        if (ctx->info.platforms[i] != 0) {
            ctx->img->platforms[ctx->img->platform_count++] = 
                (ipf_platform_t)ctx->info.platforms[i];
        }
    }
    
    ctx->has_info = true;
    return true;
}

static bool parse_image_chunk(parse_context_t *ctx, size_t chunk_len)
{
    if (chunk_len < 80) {
        ctx->error = IPF_ERR_FORMAT;
        return false;
    }
    
    /* Expand array if needed */
    if (ctx->image_record_count >= ctx->image_record_cap) {
        size_t new_cap = ctx->image_record_cap ? ctx->image_record_cap * 2 : 32;
        ipf_image_record_t *new_records = realloc(ctx->image_records, 
                                                   new_cap * sizeof(ipf_image_record_t));
        if (!new_records) {
            ctx->error = IPF_ERR_MEMORY;
            return false;
        }
        ctx->image_records = new_records;
        ctx->image_record_cap = new_cap;
    }
    
    const uint8_t *p = ctx->data + ctx->pos;
    ipf_image_record_t *rec = &ctx->image_records[ctx->image_record_count];
    
    rec->track = read_be32(p + 0);
    rec->side = read_be32(p + 4);
    rec->density = read_be32(p + 8);
    rec->signal_type = read_be32(p + 12);
    rec->track_bytes = read_be32(p + 16);
    rec->start_byte = read_be32(p + 20);
    rec->start_bit = read_be32(p + 24);
    rec->data_bits = read_be32(p + 28);
    rec->gap_bits = read_be32(p + 32);
    rec->track_bits = read_be32(p + 36);
    rec->block_count = read_be32(p + 40);
    rec->encoder_process = read_be32(p + 44);
    rec->track_flags = read_be32(p + 48);
    rec->data_key = read_be32(p + 52);
    
    ctx->image_record_count++;
    return true;
}

/* Decode variable-length size from data stream */
static size_t decode_stream_size(const uint8_t *p, size_t width)
{
    size_t size = 0;
    for (size_t i = 0; i < width; i++) {
        size = (size << 8) | p[i];
    }
    return size;
}

/* Parse data stream elements and build bitstream */
static bool parse_data_stream(parse_context_t *ctx, ipf_track_t *track,
                              const uint8_t *stream, size_t stream_len,
                              bool data_is_bits, size_t data_bytes)
{
    size_t pos = 0;
    size_t bit_pos = 0;
    
    /* Allocate bitstream buffer */
    size_t max_bits = track->bit_length ? track->bit_length : 200000;
    size_t buffer_size = (max_bits + 7) / 8;
    
    track->bitstream = calloc(buffer_size, 1);
    track->weak_mask = calloc(buffer_size, 1);
    if (!track->bitstream || !track->weak_mask) {
        ctx->error = IPF_ERR_MEMORY;
        return false;
    }
    
    while (pos < stream_len) {
        uint8_t head = stream[pos++];
        
        /* Extract data type (bits 0-2) and size width (bits 5-7) */
        ipf_data_type_t dtype = (ipf_data_type_t)(head & 0x07);
        uint8_t size_width = (head >> 5) & 0x07;
        
        if (dtype == IPF_DATA_END && size_width == 0) {
            break;  /* End of stream */
        }
        
        if (pos + size_width > stream_len) {
            ctx->error = IPF_ERR_CORRUPT;
            return false;
        }
        
        size_t sample_size = decode_stream_size(stream + pos, size_width);
        pos += size_width;
        
        /* Calculate sample bytes */
        size_t sample_bytes;
        if (data_is_bits) {
            sample_bytes = (sample_size + 7) / 8;
        } else {
            sample_bytes = sample_size;
        }
        
        switch (dtype) {
            case IPF_DATA_SYNC:
            case IPF_DATA_DATA:
            case IPF_DATA_RAW:
                /* Copy sample data to bitstream */
                if (pos + sample_bytes <= stream_len) {
                    size_t byte_pos = bit_pos / 8;
                    if (byte_pos + sample_bytes <= buffer_size) {
                        memcpy(track->bitstream + byte_pos, stream + pos, sample_bytes);
                    }
                    pos += sample_bytes;
                    bit_pos += data_is_bits ? sample_size : sample_size * 8;
                }
                break;
                
            case IPF_DATA_GAP:
                /* Fill gap with default pattern */
                {
                    size_t gap_bits = data_is_bits ? sample_size : sample_size * 8;
                    /* Skip gap bits (leave as zero) */
                    bit_pos += gap_bits;
                }
                break;
                
            case IPF_DATA_FUZZY:
                /* Mark weak bits in mask */
                {
                    size_t weak_bits = data_is_bits ? sample_size : sample_size * 8;
                    size_t start_byte = bit_pos / 8;
                    size_t end_byte = (bit_pos + weak_bits + 7) / 8;
                    
                    for (size_t i = start_byte; i < end_byte && i < buffer_size; i++) {
                        track->weak_mask[i] = 0xFF;
                    }
                    bit_pos += weak_bits;
                }
                break;
                
            case IPF_DATA_END:
            default:
                break;
        }
    }
    
    track->bitstream_len = (bit_pos + 7) / 8;
    return true;
}

static bool parse_data_chunk(parse_context_t *ctx, size_t chunk_len)
{
    if (chunk_len < 28) {
        ctx->error = IPF_ERR_FORMAT;
        return false;
    }
    
    const uint8_t *p = ctx->data + ctx->pos;
    
    ipf_data_record_t data_rec;
    data_rec.length = read_be32(p + 0);
    data_rec.bit_size = read_be32(p + 4);
    data_rec.crc = read_be32(p + 8);
    data_rec.data_key = read_be32(p + 12);
    
    /* Find matching image record */
    ipf_image_record_t *img_rec = NULL;
    for (size_t i = 0; i < ctx->image_record_count; i++) {
        if (ctx->image_records[i].data_key == data_rec.data_key) {
            img_rec = &ctx->image_records[i];
            break;
        }
    }
    
    if (!img_rec) {
        /* No matching image record - skip */
        return true;
    }
    
    /* Calculate track index */
    uint32_t track_idx = img_rec->track * 2 + img_rec->side;
    if (track_idx >= IPF_MAX_TRACKS) {
        ctx->error = IPF_ERR_INVALID_TRACK;
        return false;
    }
    
    /* Create track */
    ipf_track_t *track = calloc(1, sizeof(ipf_track_t));
    if (!track) {
        ctx->error = IPF_ERR_MEMORY;
        return false;
    }
    
    track->track = img_rec->track;
    track->side = img_rec->side;
    track->bit_length = img_rec->track_bits;
    track->byte_length = img_rec->track_bytes;
    track->density = (ipf_density_t)img_rec->density;
    track->signal_type = (ipf_signal_type_t)img_rec->signal_type;
    track->flags = img_rec->track_flags;
    
    /* Parse block descriptors */
    size_t block_desc_pos = ctx->pos + 28;
    size_t block_desc_size = (ctx->img->encoder_type == IPF_ENCODER_V2) ? 32 : 28;
    
    if (block_desc_pos + img_rec->block_count * block_desc_size > ctx->pos + chunk_len) {
        free(track);
        ctx->error = IPF_ERR_CORRUPT;
        return false;
    }
    
    /* Parse data stream after block descriptors */
    size_t stream_start = block_desc_pos + img_rec->block_count * block_desc_size;
    size_t stream_len = data_rec.length - (stream_start - ctx->pos - 28);
    
    if (stream_start + stream_len > ctx->len) {
        free(track);
        ctx->error = IPF_ERR_CORRUPT;
        return false;
    }
    
    /* Determine if data is in bits */
    bool data_is_bits = false;
    size_t data_bytes = 0;
    
    /* Check first block for flags */
    if (img_rec->block_count > 0) {
        const uint8_t *block_p = ctx->data + block_desc_pos;
        uint32_t block_flags = read_be32(block_p + 20);
        data_is_bits = (block_flags & IPF_BLOCK_DATA_IN_BIT) != 0;
        data_bytes = read_be32(block_p + 8);  /* V1 data_bytes field */
    }
    
    /* Parse data stream */
    if (!parse_data_stream(ctx, track, ctx->data + stream_start, stream_len,
                           data_is_bits, data_bytes)) {
        free_track(track);
        return false;
    }
    
    /* Store track */
    if (ctx->img->tracks[track_idx]) {
        free_track(ctx->img->tracks[track_idx]);
    }
    ctx->img->tracks[track_idx] = track;
    ctx->img->track_count++;
    
    return true;
}

/*============================================================================
 * Main Parser
 *============================================================================*/

ipf_error_t ipf_load_buffer(const uint8_t *data, size_t len, ipf_image_t **img)
{
    if (!data || !img) return IPF_ERR_FORMAT;
    
    if (!ipf_is_ipf_buffer(data, len)) {
        return IPF_ERR_FORMAT;
    }
    
    /* Create image */
    ipf_image_t *new_img = ipf_image_create();
    if (!new_img) return IPF_ERR_MEMORY;
    
    /* Initialize parse context */
    parse_context_t ctx = {
        .data = data,
        .len = len,
        .pos = 0,
        .img = new_img,
        .error = IPF_OK
    };
    
    /* Parse chunks */
    while (ctx.pos + 8 <= len) {
        uint32_t chunk_type = read_be32(data + ctx.pos);
        uint32_t chunk_len = read_be32(data + ctx.pos + 4);
        
        ctx.pos += 8;  /* Skip header */
        
        if (ctx.pos + chunk_len > len) {
            ctx.error = IPF_ERR_CORRUPT;
            break;
        }
        
        switch (chunk_type) {
            case IPF_CHUNK_CAPS:
                /* Skip CAPS header chunk */
                break;
                
            case IPF_CHUNK_INFO:
                if (!parse_info_chunk(&ctx, chunk_len)) {
                    goto error;
                }
                break;
                
            case IPF_CHUNK_IMGE:
                if (!parse_image_chunk(&ctx, chunk_len)) {
                    goto error;
                }
                break;
                
            case IPF_CHUNK_DATA:
                if (!parse_data_chunk(&ctx, chunk_len)) {
                    goto error;
                }
                break;
                
            default:
                /* Skip unknown chunks */
                break;
        }
        
        ctx.pos += chunk_len;
    }
    
    /* Cleanup temp data */
    free(ctx.image_records);
    
    *img = new_img;
    return IPF_OK;
    
error:
    free(ctx.image_records);
    ipf_image_free(new_img);
    return ctx.error;
}

ipf_error_t ipf_load_file(const char *path, ipf_image_t **img)
{
    if (!path || !img) return IPF_ERR_FORMAT;
    
    FILE *f = fopen(path, "rb");
    if (!f) return IPF_ERR_IO;
    
    /* Get file size */
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    if (size <= 0 || size > 100 * 1024 * 1024) {  /* 100MB limit */
        fclose(f);
        return IPF_ERR_FORMAT;
    }
    
    /* Read file */
    uint8_t *data = malloc((size_t)size);
    if (!data) {
        fclose(f);
        return IPF_ERR_MEMORY;
    }
    
    if (fread(data, 1, (size_t)size, f) != (size_t)size) {
        free(data);
        fclose(f);
        return IPF_ERR_IO;
    }
    fclose(f);
    
    /* Parse */
    ipf_error_t err = ipf_load_buffer(data, (size_t)size, img);
    
    /* Store filename */
    if (err == IPF_OK && *img) {
        strncpy((*img)->filename, path, sizeof((*img)->filename) - 1);
    }
    
    free(data);
    return err;
}

/*============================================================================
 * Accessor Functions
 *============================================================================*/

const ipf_track_t* ipf_get_track(const ipf_image_t *img, uint32_t track, uint32_t side)
{
    if (!img) return NULL;
    
    uint32_t idx = track * 2 + side;
    if (idx >= IPF_MAX_TRACKS) return NULL;
    
    return img->tracks[idx];
}

int ipf_read_sector(const ipf_image_t *img, uint32_t track, uint32_t side,
                    uint32_t sector, uint8_t *buffer, size_t buffer_size)
{
    const ipf_track_t *trk = ipf_get_track(img, track, side);
    if (!trk) return IPF_ERR_INVALID_TRACK;
    
    for (size_t i = 0; i < trk->sector_count; i++) {
        if (trk->sectors[i].sector == sector) {
            const ipf_sector_t *sec = &trk->sectors[i];
            size_t copy_size = sec->actual_size < buffer_size ? sec->actual_size : buffer_size;
            if (sec->data) {
                memcpy(buffer, sec->data, copy_size);
            }
            return (int)copy_size;
        }
    }
    
    return IPF_ERR_INVALID_SECTOR;
}

int ipf_extract_bitstream(const ipf_image_t *img, uint32_t track, uint32_t side,
                          uint8_t *buffer, size_t buffer_size, bool include_weak)
{
    const ipf_track_t *trk = ipf_get_track(img, track, side);
    if (!trk || !trk->bitstream) return IPF_ERR_INVALID_TRACK;
    
    size_t copy_size = trk->bitstream_len < buffer_size ? trk->bitstream_len : buffer_size;
    memcpy(buffer, trk->bitstream, copy_size);
    
    /* Apply weak bit randomization if requested */
    if (include_weak && trk->weak_mask) {
        for (size_t i = 0; i < copy_size; i++) {
            if (trk->weak_mask[i]) {
                /* XOR with random data for weak bits */
                buffer[i] ^= (uint8_t)(rand() & trk->weak_mask[i]);
            }
        }
    }
    
    return (int)(trk->bit_length ? trk->bit_length : copy_size * 8);
}

int ipf_get_weak_mask(const ipf_image_t *img, uint32_t track, uint32_t side,
                      uint8_t *buffer, size_t buffer_size)
{
    const ipf_track_t *trk = ipf_get_track(img, track, side);
    if (!trk || !trk->weak_mask) return IPF_ERR_INVALID_TRACK;
    
    size_t copy_size = trk->bitstream_len < buffer_size ? trk->bitstream_len : buffer_size;
    memcpy(buffer, trk->weak_mask, copy_size);
    
    return (int)copy_size;
}

int ipf_to_sector_image(const ipf_image_t *img, uint8_t *buffer, size_t buffer_size,
                        uint16_t bytes_per_sector)
{
    if (!img || !buffer) return IPF_ERR_FORMAT;
    
    size_t pos = 0;
    
    for (uint32_t track = img->min_track; track <= img->max_track; track++) {
        for (uint32_t side = img->min_side; side <= img->max_side; side++) {
            const ipf_track_t *trk = ipf_get_track(img, track, side);
            if (!trk) continue;
            
            for (size_t s = 0; s < trk->sector_count; s++) {
                const ipf_sector_t *sec = &trk->sectors[s];
                
                if (pos + bytes_per_sector > buffer_size) {
                    return (int)pos;
                }
                
                if (sec->data && sec->actual_size > 0) {
                    size_t copy = sec->actual_size < bytes_per_sector ? 
                                  sec->actual_size : bytes_per_sector;
                    memcpy(buffer + pos, sec->data, copy);
                    if (copy < bytes_per_sector) {
                        memset(buffer + pos + copy, 0, bytes_per_sector - copy);
                    }
                } else {
                    memset(buffer + pos, 0, bytes_per_sector);
                }
                
                pos += bytes_per_sector;
            }
        }
    }
    
    return (int)pos;
}

/*============================================================================
 * Helper Functions
 *============================================================================*/

const char* ipf_error_string(ipf_error_t err)
{
    switch (err) {
        case IPF_OK: return "OK";
        case IPF_ERR_IO: return "I/O error";
        case IPF_ERR_FORMAT: return "Invalid format";
        case IPF_ERR_CRC: return "CRC error";
        case IPF_ERR_MEMORY: return "Out of memory";
        case IPF_ERR_UNSUPPORTED: return "Unsupported feature";
        case IPF_ERR_CORRUPT: return "Corrupt data";
        case IPF_ERR_INVALID_TRACK: return "Invalid track";
        case IPF_ERR_INVALID_SECTOR: return "Invalid sector";
        default: return "Unknown error";
    }
}

const char* ipf_platform_name(ipf_platform_t platform)
{
    switch (platform) {
        case IPF_PLAT_NONE: return "None";
        case IPF_PLAT_AMIGA: return "Amiga";
        case IPF_PLAT_ATARI_ST: return "Atari ST";
        case IPF_PLAT_PC: return "PC";
        case IPF_PLAT_AMSTRAD_CPC: return "Amstrad CPC";
        case IPF_PLAT_SPECTRUM: return "ZX Spectrum";
        case IPF_PLAT_SAM_COUPE: return "SAM Coupé";
        case IPF_PLAT_ARCHIMEDES: return "Archimedes";
        case IPF_PLAT_C64: return "Commodore 64";
        case IPF_PLAT_ATARI_8BIT: return "Atari 8-bit";
        default: return "Unknown";
    }
}

void ipf_print_info(const ipf_image_t *img, FILE *stream)
{
    if (!img) return;
    if (!stream) stream = stdout;
    
    fprintf(stream, "IPF Image: %s\n", img->filename[0] ? img->filename : "(buffer)");
    fprintf(stream, "Encoder: V%d (rev %u)\n", img->encoder_type, img->encoder_rev);
    fprintf(stream, "Tracks: %u-%u\n", img->min_track, img->max_track);
    fprintf(stream, "Sides: %u-%u\n", img->min_side, img->max_side);
    fprintf(stream, "Track count: %zu\n", img->track_count);
    
    fprintf(stream, "Platforms:");
    for (size_t i = 0; i < img->platform_count; i++) {
        fprintf(stream, " %s", ipf_platform_name(img->platforms[i]));
    }
    fprintf(stream, "\n");
    
    if (img->creation_date) {
        fprintf(stream, "Created: %04u-%02u-%02u %02u:%02u:%02u\n",
                img->creation_date / 10000,
                (img->creation_date / 100) % 100,
                img->creation_date % 100,
                img->creation_time / 10000,
                (img->creation_time / 100) % 100,
                img->creation_time % 100);
    }
    
    /* Track summary */
    size_t total_weak = 0;
    for (size_t i = 0; i < IPF_MAX_TRACKS; i++) {
        const ipf_track_t *trk = img->tracks[i];
        if (trk && trk->weak_mask) {
            for (size_t j = 0; j < trk->bitstream_len; j++) {
                if (trk->weak_mask[j]) total_weak++;
            }
        }
    }
    
    if (total_weak > 0) {
        fprintf(stream, "Weak bit regions: %zu bytes\n", total_weak);
    }
}
