/**
 * @file uft_a2r_parser.c
 * @brief A2R Apple II Flux Format Parser Implementation
 * 
 * Implements parsing of A2R v2 and v3 flux format files.
 * 
 * A2R Format Structure:
 * - Header: "A2R2" or "A2R3" + 0xFF 0x0A 0x0D 0x0A (8 bytes)
 * - Chunks: 4-byte ID + 4-byte size + data
 * 
 * Chunk Types:
 * - INFO: Disk information
 * - STRM: Flux stream data (v2)
 * - RWCP: Raw capture data (v3)
 * - SLVD: Solved/decoded data (v3)
 * - META: Optional metadata
 * 
 * @author UFT Team
 * @version 3.4.0
 */

#include "uft/parsers/uft_a2r_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/*============================================================================
 * Internal Structures
 *============================================================================*/

/** Chunk header */
typedef struct {
    char        id[4];
    uint32_t    size;
} chunk_header_t;

/** STRM location entry (v2) */
typedef struct {
    uint8_t     location;       /**< Quarter track */
    uint8_t     capture_type;   /**< 1=timing, 2=bits, 3=xtiming */
    uint32_t    data_length;    /**< Flux data length */
    uint32_t    tick_count;     /**< Loop point tick */
} strm_entry_t;

/** RWCP location entry (v3) */
typedef struct {
    uint8_t     track;          /**< Track * 4 + quarter */
    uint8_t     side;           /**< Side (0 or 1) */
    uint32_t    index_count;    /**< Number of index marks */
    uint32_t    *indices;       /**< Index positions */
    uint32_t    data_length;    /**< Capture data length */
    uint8_t     *data;          /**< Capture data */
} rwcp_entry_t;

/*============================================================================
 * Error Strings
 *============================================================================*/

static const char *error_strings[A2R_ERR_COUNT] = {
    "OK",
    "Null parameter",
    "Cannot open file",
    "File read error",
    "Invalid A2R signature",
    "Unsupported A2R version",
    "Invalid chunk",
    "Missing INFO chunk",
    "No flux data",
    "Track out of range",
    "Capture out of range",
    "Memory allocation failed",
    "Corrupt data"
};

static const char *disk_type_strings[] = {
    "Unknown",
    "5.25\" Single-Sided (Disk II)",
    "5.25\" Double-Sided",
    "3.5\" Single-Sided (400K)",
    "3.5\" Double-Sided (800K)"
};

/*============================================================================
 * Utility Functions
 *============================================================================*/

/** Read little-endian uint16_t */
static inline uint16_t read_le16(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

/** Read little-endian uint32_t */
static inline uint32_t read_le32(const uint8_t *p) {
    return (uint32_t)p[0] | 
           ((uint32_t)p[1] << 8) | 
           ((uint32_t)p[2] << 16) | 
           ((uint32_t)p[3] << 24);
}

/** Safe string copy with null termination */
static void safe_strcpy(char *dst, const char *src, size_t dst_size) {
    if (!dst || !src || dst_size == 0) return;
    size_t len = strlen(src);
    if (len >= dst_size) len = dst_size - 1;
    memcpy(dst, src, len);
    dst[len] = '\0';
}

/** Copy fixed-length string with null termination */
static void copy_fixed_string(char *dst, const uint8_t *src, 
                              size_t src_len, size_t dst_size) {
    if (!dst || !src || dst_size == 0) return;
    size_t len = src_len;
    if (len >= dst_size) len = dst_size - 1;
    memcpy(dst, src, len);
    dst[len] = '\0';
    
    /* Trim trailing spaces */
    while (len > 0 && dst[len - 1] == ' ') {
        dst[--len] = '\0';
    }
}

/*============================================================================
 * Chunk Parsing
 *============================================================================*/

/** Parse INFO chunk (v2: 60 bytes, v3: 60 bytes with different layout) */
static a2r_error_t parse_info_chunk(const uint8_t *data, size_t size,
                                    uint8_t version, a2r_info_t *info) {
    if (!data || !info) return A2R_ERR_NULL_PARAM;
    if (size < 60) return A2R_ERR_BAD_CHUNK;
    
    memset(info, 0, sizeof(*info));
    info->version = version;
    
    if (version == 2) {
        /* A2R v2 INFO layout */
        copy_fixed_string(info->creator, data, 32, sizeof(info->creator));
        info->disk_type = data[32];
        info->write_protected = data[33] != 0;
        info->synchronized = data[34] != 0;
        /* Bytes 35-59: reserved */
    } else if (version == 3) {
        /* A2R v3 INFO layout */
        copy_fixed_string(info->creator, data, 32, sizeof(info->creator));
        info->disk_type = data[32];
        info->write_protected = data[33] != 0;
        info->synchronized = data[34] != 0;
        info->cleaned = data[35] != 0;
        info->optimal_timing = data[36] != 0;
        
        /* v3 extended fields */
        if (size >= 56) {
            info->disk_sides = data[37];
            info->boot_sector_format = data[38];
            info->data_format = data[39];
            info->optimal_bit_timing = read_le32(&data[40]);
            info->compatible_hw = read_le16(&data[44]);
            info->required_ram = read_le16(&data[46]);
            info->largest_track = read_le16(&data[48]);
        }
    }
    
    return A2R_OK;
}

/** Parse STRM chunk (v2) */
static a2r_error_t parse_strm_chunk(a2r_context_t *ctx, 
                                    const uint8_t *data, size_t size) {
    if (!ctx || !data) return A2R_ERR_NULL_PARAM;
    
    /* STRM format: location entries followed by flux data */
    const uint8_t *ptr = data;
    const uint8_t *end = data + size;
    
    /* Count tracks first */
    uint8_t track_counts[A2R_MAX_TRACKS] = {0};
    const uint8_t *scan = data;
    
    while (scan + 10 <= end) {
        uint8_t location = scan[0];
        uint8_t capture_type = scan[1];
        uint32_t data_len = read_le32(&scan[2]);
        uint32_t tick_count = read_le32(&scan[6]);
        
        if (location == 0xFF) break;  /* End marker */
        if (location >= A2R_MAX_TRACKS) {
            scan += 10 + data_len;
            continue;
        }
        
        track_counts[location]++;
        scan += 10 + data_len;
    }
    
    /* Count unique tracks */
    ctx->track_count = 0;
    for (int i = 0; i < A2R_MAX_TRACKS; i++) {
        if (track_counts[i] > 0) ctx->track_count++;
    }
    
    if (ctx->track_count == 0) return A2R_ERR_NO_FLUX;
    
    /* Allocate tracks */
    ctx->tracks = calloc(ctx->track_count, sizeof(a2r_track_t));
    if (!ctx->tracks) return A2R_ERR_ALLOC;
    
    /* Parse entries */
    uint8_t track_idx = 0;
    uint8_t current_location = 0xFF;
    a2r_track_t *current_track = NULL;
    
    ptr = data;
    while (ptr + 10 <= end) {
        uint8_t location = ptr[0];
        uint8_t capture_type = ptr[1];
        uint32_t data_len = read_le32(&ptr[2]);
        uint32_t tick_count = read_le32(&ptr[6]);
        
        if (location == 0xFF) break;
        if (location >= A2R_MAX_TRACKS) {
            ptr += 10 + data_len;
            continue;
        }
        
        /* New track? */
        if (location != current_location) {
            current_location = location;
            current_track = &ctx->tracks[track_idx++];
            current_track->track_number = location;
            current_track->side = 0;  /* v2 is always side 0 */
            current_track->capture_count = 0;
        }
        
        /* Add capture */
        if (current_track && 
            current_track->capture_count < A2R_MAX_CAPTURES) {
            
            a2r_capture_t *cap = &current_track->captures[current_track->capture_count];
            cap->capture_type = capture_type;
            cap->data_length = data_len;
            cap->tick_count = tick_count;
            
            /* Copy flux data */
            if (ptr + 10 + data_len <= end && data_len > 0) {
                cap->data = malloc(data_len);
                if (cap->data) {
                    memcpy(cap->data, ptr + 10, data_len);
                    
                    /* Calculate duration */
                    uint64_t total_ticks = 0;
                    const uint8_t *flux = cap->data;
                    for (uint32_t i = 0; i < data_len; i++) {
                        if (flux[i] == 0xFF && i + 1 < data_len) {
                            total_ticks += flux[++i];
                        } else {
                            total_ticks += flux[i];
                        }
                    }
                    cap->duration_us = (total_ticks * A2R_TICK_NS) / 1000.0;
                    cap->rpm = a2r_duration_to_rpm(cap->duration_us);
                    
                    ctx->total_flux_bytes += data_len;
                    ctx->total_captures++;
                    
                    if (ctx->min_rpm == 0.0 || cap->rpm < ctx->min_rpm)
                        ctx->min_rpm = cap->rpm;
                    if (cap->rpm > ctx->max_rpm)
                        ctx->max_rpm = cap->rpm;
                }
            }
            
            current_track->capture_count++;
        }
        
        ptr += 10 + data_len;
    }
    
    return A2R_OK;
}

/** Parse RWCP chunk (v3) */
static a2r_error_t parse_rwcp_chunk(a2r_context_t *ctx,
                                    const uint8_t *data, size_t size) {
    if (!ctx || !data) return A2R_ERR_NULL_PARAM;
    
    /* RWCP format: entries with track/side/indices/data */
    const uint8_t *ptr = data;
    const uint8_t *end = data + size;
    
    /* First pass: count tracks */
    uint8_t track_map[A2R_MAX_TRACKS][2] = {{0}};  /* [track][side] */
    const uint8_t *scan = data;
    
    while (scan + 10 <= end) {
        uint8_t location = scan[0];
        uint8_t capture_type = scan[1];
        
        if (location == 0xFF) break;
        
        uint8_t track = location;
        uint8_t side = scan[2];
        
        /* Skip based on capture type */
        if (capture_type == 1) {
            /* Timing capture */
            uint32_t data_len = read_le32(&scan[6]);
            track_map[track][side & 1] = 1;
            scan += 10 + data_len;
        } else if (capture_type == 2) {
            /* Bits capture */
            uint32_t data_len = read_le32(&scan[6]);
            track_map[track][side & 1] = 1;
            scan += 10 + data_len;
        } else {
            break;  /* Unknown type */
        }
    }
    
    /* Count unique track/side combinations */
    ctx->track_count = 0;
    for (int t = 0; t < A2R_MAX_TRACKS; t++) {
        for (int s = 0; s < 2; s++) {
            if (track_map[t][s]) ctx->track_count++;
        }
    }
    
    if (ctx->track_count == 0) return A2R_ERR_NO_FLUX;
    
    /* Allocate tracks */
    ctx->tracks = calloc(ctx->track_count, sizeof(a2r_track_t));
    if (!ctx->tracks) return A2R_ERR_ALLOC;
    
    /* Second pass: parse data */
    ptr = data;
    uint8_t track_idx = 0;
    uint8_t current_track = 0xFF;
    uint8_t current_side = 0xFF;
    a2r_track_t *current = NULL;
    
    while (ptr + 10 <= end && track_idx < ctx->track_count) {
        uint8_t location = ptr[0];
        uint8_t capture_type = ptr[1];
        
        if (location == 0xFF) break;
        
        uint8_t track = location;
        uint8_t side = ptr[2];
        uint32_t data_len = read_le32(&ptr[6]);
        
        /* New track/side? */
        if (track != current_track || side != current_side) {
            current = &ctx->tracks[track_idx++];
            current->track_number = track;
            current->side = side;
            current->capture_count = 0;
            current_track = track;
            current_side = side;
        }
        
        /* Add capture */
        if (current && current->capture_count < A2R_MAX_CAPTURES) {
            a2r_capture_t *cap = &current->captures[current->capture_count];
            cap->capture_type = capture_type;
            cap->data_length = data_len;
            cap->tick_count = 0;
            
            if (ptr + 10 + data_len <= end && data_len > 0) {
                cap->data = malloc(data_len);
                if (cap->data) {
                    memcpy(cap->data, ptr + 10, data_len);
                    
                    /* Calculate duration from flux data */
                    uint64_t total_ticks = 0;
                    const uint8_t *flux = cap->data;
                    for (uint32_t i = 0; i < data_len; i++) {
                        if (flux[i] == 0xFF && i + 1 < data_len) {
                            total_ticks += flux[++i];
                        } else {
                            total_ticks += flux[i];
                        }
                    }
                    cap->duration_us = (total_ticks * A2R_TICK_NS) / 1000.0;
                    cap->rpm = a2r_duration_to_rpm(cap->duration_us);
                    
                    ctx->total_flux_bytes += data_len;
                    ctx->total_captures++;
                    
                    if (ctx->min_rpm == 0.0 || cap->rpm < ctx->min_rpm)
                        ctx->min_rpm = cap->rpm;
                    if (cap->rpm > ctx->max_rpm)
                        ctx->max_rpm = cap->rpm;
                }
            }
            
            current->capture_count++;
        }
        
        ptr += 10 + data_len;
    }
    
    return A2R_OK;
}

/** Parse META chunk */
static a2r_error_t parse_meta_chunk(a2r_context_t *ctx,
                                    const uint8_t *data, size_t size) {
    if (!ctx || !data) return A2R_ERR_NULL_PARAM;
    if (size == 0 || size > A2R_MAX_META_SIZE) return A2R_OK;
    
    /* Count entries */
    uint16_t count = 0;
    const uint8_t *ptr = data;
    const uint8_t *end = data + size;
    
    while (ptr < end) {
        const uint8_t *tab = memchr(ptr, '\t', end - ptr);
        if (!tab) break;
        const uint8_t *nl = memchr(tab, '\n', end - tab);
        if (!nl) nl = end;
        count++;
        ptr = nl + 1;
    }
    
    if (count == 0) return A2R_OK;
    
    /* Allocate entries */
    ctx->metadata = calloc(count, sizeof(a2r_meta_entry_t));
    if (!ctx->metadata) return A2R_ERR_ALLOC;
    
    /* Parse entries */
    ptr = data;
    ctx->meta_count = 0;
    
    while (ptr < end && ctx->meta_count < count) {
        const uint8_t *tab = memchr(ptr, '\t', end - ptr);
        if (!tab) break;
        
        const uint8_t *nl = memchr(tab, '\n', end - tab);
        if (!nl) nl = end;
        
        a2r_meta_entry_t *entry = &ctx->metadata[ctx->meta_count];
        
        /* Copy key */
        size_t key_len = tab - ptr;
        if (key_len >= sizeof(entry->key)) key_len = sizeof(entry->key) - 1;
        memcpy(entry->key, ptr, key_len);
        entry->key[key_len] = '\0';
        
        /* Copy value */
        size_t val_len = nl - (tab + 1);
        if (val_len >= sizeof(entry->value)) val_len = sizeof(entry->value) - 1;
        memcpy(entry->value, tab + 1, val_len);
        entry->value[val_len] = '\0';
        
        ctx->meta_count++;
        ptr = nl + 1;
    }
    
    return A2R_OK;
}

/** Parse SLVD chunk (v3 solved/decoded data) */
static a2r_error_t parse_slvd_chunk(a2r_context_t *ctx,
                                    const uint8_t *data, size_t size) {
    if (!ctx || !data || !ctx->tracks) return A2R_ERR_NULL_PARAM;
    
    const uint8_t *ptr = data;
    const uint8_t *end = data + size;
    
    while (ptr + 6 <= end) {
        uint8_t track = ptr[0];
        uint8_t side = ptr[1];
        uint32_t nibble_len = read_le32(&ptr[2]);
        
        if (track == 0xFF) break;
        if (ptr + 6 + nibble_len > end) break;
        
        /* Find matching track */
        for (uint8_t i = 0; i < ctx->track_count; i++) {
            if (ctx->tracks[i].track_number == track &&
                ctx->tracks[i].side == side) {
                
                ctx->tracks[i].has_solved = true;
                ctx->tracks[i].nibble_count = nibble_len;
                ctx->tracks[i].nibbles = malloc(nibble_len);
                
                if (ctx->tracks[i].nibbles) {
                    memcpy(ctx->tracks[i].nibbles, ptr + 6, nibble_len);
                }
                break;
            }
        }
        
        ptr += 6 + nibble_len;
    }
    
    return A2R_OK;
}

/*============================================================================
 * Public API Implementation
 *============================================================================*/

a2r_context_t *a2r_open(const char *path) {
    if (!path) return NULL;
    
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    
    /* Get file size */
    if (fseek(f, 0, SEEK_END) != 0) { /* seek error */ }
    long file_size = ftell(f);
    if (fseek(f, 0, SEEK_SET) != 0) { /* seek error */ }
    if (file_size < A2R_HEADER_SIZE + 8) {
        fclose(f);
        return NULL;
    }
    
    /* Read entire file */
    uint8_t *file_data = malloc(file_size);
    if (!file_data) {
        fclose(f);
        return NULL;
    }
    
    if (fread(file_data, 1, file_size, f) != (size_t)file_size) {
        free(file_data);
        fclose(f);
        return NULL;
    }
    fclose(f);
    
    /* Verify header */
    if (memcmp(file_data + 4, A2R_HEADER_SUFFIX, 4) != 0) {
        free(file_data);
        return NULL;
    }
    
    uint8_t version = 0;
    if (memcmp(file_data, A2R_MAGIC_V2, 4) == 0) {
        version = 2;
    } else if (memcmp(file_data, A2R_MAGIC_V3, 4) == 0) {
        version = 3;
    } else {
        free(file_data);
        return NULL;
    }
    
    /* Allocate context */
    a2r_context_t *ctx = calloc(1, sizeof(a2r_context_t));
    if (!ctx) {
        free(file_data);
        return NULL;
    }
    
    safe_strncpy(ctx->path, path, sizeof(ctx->path, sizeof(ctx->path)-1); ctx->path[sizeof(ctx->path)-1] = '\0');
    ctx->version = version;
    ctx->file_data = file_data;
    ctx->file_size = file_size;
    
    /* Parse chunks */
    const uint8_t *ptr = file_data + A2R_HEADER_SIZE;
    const uint8_t *end = file_data + file_size;
    bool has_info = false;
    bool has_flux = false;
    
    while (ptr + 8 <= end) {
        chunk_header_t hdr;
        memcpy(hdr.id, ptr, 4);
        hdr.size = read_le32(ptr + 4);
        
        if (ptr + 8 + hdr.size > end) break;
        
        const uint8_t *chunk_data = ptr + 8;
        
        if (memcmp(hdr.id, A2R_CHUNK_INFO, 4) == 0) {
            parse_info_chunk(chunk_data, hdr.size, version, &ctx->info);
            has_info = true;
        } else if (memcmp(hdr.id, A2R_CHUNK_STRM, 4) == 0 && version == 2) {
            parse_strm_chunk(ctx, chunk_data, hdr.size);
            has_flux = true;
        } else if (memcmp(hdr.id, A2R_CHUNK_RWCP, 4) == 0 && version == 3) {
            parse_rwcp_chunk(ctx, chunk_data, hdr.size);
            has_flux = true;
        } else if (memcmp(hdr.id, A2R_CHUNK_SLVD, 4) == 0 && version == 3) {
            parse_slvd_chunk(ctx, chunk_data, hdr.size);
        } else if (memcmp(hdr.id, A2R_CHUNK_META, 4) == 0) {
            parse_meta_chunk(ctx, chunk_data, hdr.size);
        }
        
        ptr += 8 + hdr.size;
    }
    
    if (!has_info || !has_flux) {
        a2r_close(ctx);
        return NULL;
    }
    
    return ctx;
}

void a2r_close(a2r_context_t *ctx) {
    if (!ctx) return;
    
    /* Free tracks */
    if (ctx->tracks) {
        for (uint8_t i = 0; i < ctx->track_count; i++) {
            a2r_track_t *track = &ctx->tracks[i];
            
            /* Free captures */
            for (uint8_t j = 0; j < track->capture_count; j++) {
                free(track->captures[j].data);
            }
            
            /* Free nibbles */
            free(track->nibbles);
        }
        free(ctx->tracks);
    }
    
    /* Free metadata */
    free(ctx->metadata);
    
    /* Free file data */
    free(ctx->file_data);
    
    free(ctx);
}

a2r_error_t a2r_read_track(a2r_context_t *ctx, 
                           uint8_t quarter_track,
                           uint8_t side,
                           a2r_track_t *track) {
    if (!ctx || !track) return A2R_ERR_NULL_PARAM;
    if (!ctx->tracks) return A2R_ERR_NO_FLUX;
    
    memset(track, 0, sizeof(*track));
    
    /* Find track */
    for (uint8_t i = 0; i < ctx->track_count; i++) {
        if (ctx->tracks[i].track_number == quarter_track &&
            ctx->tracks[i].side == side) {
            
            /* Copy track info */
            track->track_number = ctx->tracks[i].track_number;
            track->side = ctx->tracks[i].side;
            track->capture_count = ctx->tracks[i].capture_count;
            track->has_solved = ctx->tracks[i].has_solved;
            
            /* Copy captures (deep copy data) */
            for (uint8_t j = 0; j < track->capture_count; j++) {
                a2r_capture_t *src = &ctx->tracks[i].captures[j];
                a2r_capture_t *dst = &track->captures[j];
                
                dst->capture_type = src->capture_type;
                dst->data_length = src->data_length;
                dst->tick_count = src->tick_count;
                dst->duration_us = src->duration_us;
                dst->rpm = src->rpm;
                
                if (src->data && src->data_length > 0) {
                    dst->data = malloc(src->data_length);
                    if (dst->data) {
                        memcpy(dst->data, src->data, src->data_length);
                    }
                }
            }
            
            /* Copy nibbles if present */
            if (ctx->tracks[i].has_solved && ctx->tracks[i].nibbles) {
                track->nibble_count = ctx->tracks[i].nibble_count;
                track->nibbles = malloc(track->nibble_count);
                if (track->nibbles) {
                    memcpy(track->nibbles, ctx->tracks[i].nibbles, 
                           track->nibble_count);
                }
            }
            
            return A2R_OK;
        }
    }
    
    return A2R_ERR_TRACK_RANGE;
}

void a2r_free_track(a2r_track_t *track) {
    if (!track) return;
    
    for (uint8_t i = 0; i < track->capture_count; i++) {
        free(track->captures[i].data);
        track->captures[i].data = NULL;
    }
    
    free(track->nibbles);
    track->nibbles = NULL;
    
    memset(track, 0, sizeof(*track));
}

a2r_error_t a2r_decode_flux(const a2r_capture_t *capture,
                            a2r_flux_sample_t *samples,
                            uint32_t max_samples,
                            uint32_t *out_count) {
    if (!capture || !samples || !out_count) return A2R_ERR_NULL_PARAM;
    if (!capture->data) return A2R_ERR_NO_FLUX;
    
    *out_count = 0;
    double time_ns = 0.0;
    const uint8_t *data = capture->data;
    uint32_t len = capture->data_length;
    
    for (uint32_t i = 0; i < len && *out_count < max_samples; i++) {
        uint32_t tick;
        bool is_extended = false;
        
        if (data[i] == 0xFF && i + 1 < len) {
            /* Extended timing: 0xFF followed by count */
            tick = data[++i];
            is_extended = true;
        } else if (data[i] == 0x00) {
            /* Sync byte, skip */
            continue;
        } else {
            tick = data[i];
        }
        
        time_ns += tick * A2R_TICK_NS;
        
        samples[*out_count].tick = tick;
        samples[*out_count].time_ns = time_ns;
        samples[*out_count].is_extended = is_extended;
        (*out_count)++;
    }
    
    return A2R_OK;
}

a2r_error_t a2r_flux_to_nibbles(const a2r_capture_t *capture,
                                double bit_time_ns,
                                uint8_t *nibbles,
                                uint32_t max_nibbles,
                                uint32_t *out_count) {
    if (!capture || !nibbles || !out_count) return A2R_ERR_NULL_PARAM;
    if (!capture->data) return A2R_ERR_NO_FLUX;
    
    /* Default Apple II bit timing: 4µs = 4000ns */
    if (bit_time_ns <= 0.0) bit_time_ns = 4000.0;
    
    *out_count = 0;
    
    const uint8_t *data = capture->data;
    uint32_t len = capture->data_length;
    
    /* Accumulate flux intervals and decode to bits */
    double accum_ns = 0.0;
    uint8_t current_byte = 0;
    int bit_count = 0;
    
    for (uint32_t i = 0; i < len; i++) {
        uint32_t tick;
        
        if (data[i] == 0xFF && i + 1 < len) {
            tick = data[++i];
        } else if (data[i] == 0x00) {
            continue;
        } else {
            tick = data[i];
        }
        
        double interval_ns = tick * A2R_TICK_NS;
        accum_ns += interval_ns;
        
        /* Determine number of bit cells */
        int bit_cells = (int)((accum_ns + bit_time_ns / 2) / bit_time_ns);
        accum_ns -= bit_cells * bit_time_ns;
        
        /* Generate bits */
        for (int b = 0; b < bit_cells && *out_count < max_nibbles; b++) {
            /* Last bit before flux transition is always 1 */
            int bit_value = (b == bit_cells - 1) ? 1 : 0;
            
            current_byte = (current_byte << 1) | bit_value;
            bit_count++;
            
            /* Apple II nibbles are 8 bits with high bit set */
            if (bit_count == 8) {
                nibbles[(*out_count)++] = current_byte;
                current_byte = 0;
                bit_count = 0;
            }
        }
    }
    
    return A2R_OK;
}

a2r_error_t a2r_get_info(const a2r_context_t *ctx, a2r_info_t *info) {
    if (!ctx || !info) return A2R_ERR_NULL_PARAM;
    *info = ctx->info;
    return A2R_OK;
}

a2r_error_t a2r_get_metadata(const a2r_context_t *ctx,
                             const char *key,
                             char *value,
                             size_t value_size) {
    if (!ctx || !key || !value || value_size == 0) return A2R_ERR_NULL_PARAM;
    
    for (uint16_t i = 0; i < ctx->meta_count; i++) {
        if (strcmp(ctx->metadata[i].key, key) == 0) {
            safe_strcpy(value, ctx->metadata[i].value, value_size);
            return A2R_OK;
        }
    }
    
    value[0] = '\0';
    return A2R_ERR_NULL_PARAM;  /* Key not found */
}

bool a2r_is_valid_file(const char *path) {
    if (!path) return false;
    
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    
    uint8_t header[8];
    bool valid = false;
    
    if (fread(header, 1, 8, f) == 8) {
        if ((memcmp(header, A2R_MAGIC_V2, 4) == 0 ||
             memcmp(header, A2R_MAGIC_V3, 4) == 0) &&
            memcmp(header + 4, A2R_HEADER_SUFFIX, 4) == 0) {
            valid = true;
        }
    }
    
    fclose(f);
    return valid;
}

uint8_t a2r_get_file_version(const char *path) {
    if (!path) return 0;
    
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    
    uint8_t header[8];
    uint8_t version = 0;
    
    if (fread(header, 1, 8, f) == 8) {
        if (memcmp(header + 4, A2R_HEADER_SUFFIX, 4) == 0) {
            if (memcmp(header, A2R_MAGIC_V2, 4) == 0) version = 2;
            else if (memcmp(header, A2R_MAGIC_V3, 4) == 0) version = 3;
        }
    }
    
    fclose(f);
    return version;
}

const char *a2r_error_string(a2r_error_t err) {
    if (err >= A2R_ERR_COUNT) return "Unknown error";
    return error_strings[err];
}

const char *a2r_disk_type_string(uint8_t disk_type) {
    if (disk_type > 4) return "Unknown";
    return disk_type_strings[disk_type];
}

a2r_error_t a2r_get_raw_timings(const a2r_capture_t *capture,
                                double *timings,
                                uint32_t max_timings,
                                uint32_t *out_count) {
    if (!capture || !timings || !out_count) return A2R_ERR_NULL_PARAM;
    if (!capture->data) return A2R_ERR_NO_FLUX;
    
    *out_count = 0;
    const uint8_t *data = capture->data;
    uint32_t len = capture->data_length;
    
    for (uint32_t i = 0; i < len && *out_count < max_timings; i++) {
        uint32_t tick;
        
        if (data[i] == 0xFF && i + 1 < len) {
            tick = data[++i];
        } else if (data[i] == 0x00) {
            continue;
        } else {
            tick = data[i];
        }
        
        timings[(*out_count)++] = tick * A2R_TICK_NS;
    }
    
    return A2R_OK;
}

a2r_error_t a2r_fuse_captures(const a2r_capture_t *captures,
                              uint8_t count,
                              a2r_capture_t *fused,
                              uint8_t *weak_mask,
                              size_t mask_size) {
    if (!captures || !fused || count == 0) return A2R_ERR_NULL_PARAM;
    if (count == 1) {
        /* Single capture, just copy */
        *fused = captures[0];
        fused->data = malloc(captures[0].data_length);
        if (fused->data) {
            memcpy(fused->data, captures[0].data, captures[0].data_length);
        }
        return A2R_OK;
    }
    
    /* Use first capture as reference */
    const a2r_capture_t *ref = &captures[0];
    fused->capture_type = ref->capture_type;
    fused->data_length = ref->data_length;
    fused->tick_count = ref->tick_count;
    fused->duration_us = ref->duration_us;
    fused->rpm = ref->rpm;
    
    fused->data = malloc(ref->data_length);
    if (!fused->data) return A2R_ERR_ALLOC;
    
    /* For each byte position, use majority vote */
    for (uint32_t i = 0; i < ref->data_length; i++) {
        uint8_t counts[256] = {0};
        uint8_t max_val = 0;
        uint8_t max_count = 0;
        
        for (uint8_t c = 0; c < count; c++) {
            if (i < captures[c].data_length) {
                uint8_t val = captures[c].data[i];
                counts[val]++;
                if (counts[val] > max_count) {
                    max_count = counts[val];
                    max_val = val;
                }
            }
        }
        
        fused->data[i] = max_val;
        
        /* Mark as weak if not unanimous */
        if (weak_mask && i / 8 < mask_size) {
            if (max_count < count) {
                weak_mask[i / 8] |= (1 << (i % 8));
            }
        }
    }
    
    return A2R_OK;
}

a2r_error_t a2r_analyze_protection(const a2r_track_t *track,
                                   bool *has_weak,
                                   bool *has_timing,
                                   bool *has_halftrack) {
    if (!track) return A2R_ERR_NULL_PARAM;
    
    /* Initialize outputs */
    if (has_weak) *has_weak = false;
    if (has_timing) *has_timing = false;
    if (has_halftrack) *has_halftrack = false;
    
    /* Check for half/quarter tracks */
    if (has_halftrack && (track->track_number % 4) != 0) {
        *has_halftrack = true;
    }
    
    /* Need multiple captures to detect weak bits */
    if (has_weak && track->capture_count > 1) {
        /* Compare captures for variations */
        const a2r_capture_t *ref = &track->captures[0];
        
        for (uint8_t c = 1; c < track->capture_count; c++) {
            const a2r_capture_t *cmp = &track->captures[c];
            uint32_t min_len = ref->data_length < cmp->data_length ? 
                               ref->data_length : cmp->data_length;
            
            uint32_t diff_count = 0;
            for (uint32_t i = 0; i < min_len; i++) {
                if (ref->data[i] != cmp->data[i]) diff_count++;
            }
            
            /* More than 1% difference indicates weak bits */
            if (diff_count * 100 > min_len) {
                *has_weak = true;
                break;
            }
        }
    }
    
    /* Check for timing protection (unusual RPM variance) */
    if (has_timing && track->capture_count > 0) {
        double min_rpm = track->captures[0].rpm;
        double max_rpm = track->captures[0].rpm;
        
        for (uint8_t c = 1; c < track->capture_count; c++) {
            if (track->captures[c].rpm < min_rpm)
                min_rpm = track->captures[c].rpm;
            if (track->captures[c].rpm > max_rpm)
                max_rpm = track->captures[c].rpm;
        }
        
        /* More than 2% RPM variance indicates timing protection */
        if (max_rpm > 0 && (max_rpm - min_rpm) / max_rpm > 0.02) {
            *has_timing = true;
        }
    }
    
    return A2R_OK;
}

/*============================================================================
 * Unit Tests
 *============================================================================*/

#ifdef UFT_UNIT_TESTS

#include <assert.h>

static void test_a2r_basics(void) {
    /* Test error strings */
    assert(strcmp(a2r_error_string(A2R_OK), "OK") == 0);
    assert(strcmp(a2r_error_string(A2R_ERR_BAD_MAGIC), 
                  "Invalid A2R signature") == 0);
    
    /* Test disk type strings */
    assert(strcmp(a2r_disk_type_string(1), 
                  "5.25\" Single-Sided (Disk II)") == 0);
    
    /* Test quarter track conversion */
    uint8_t track, quarter;
    a2r_quarter_to_track(35, &track, &quarter);
    assert(track == 8);
    assert(quarter == 3);
    assert(a2r_track_to_quarter(8, 3) == 35);
    
    /* Test RPM calculation */
    double rpm = a2r_duration_to_rpm(200000.0);  /* 200ms = 300 RPM */
    assert(fabs(rpm - 300.0) < 0.1);
    
    /* Test LE read */
    uint8_t buf[] = {0x34, 0x12, 0x78, 0x56};
    assert(read_le16(buf) == 0x1234);
    assert(read_le32(buf) == 0x56781234);
    
    printf("  ✓ a2r_basics passed\n");
}

static void test_a2r_file_detection(void) {
    /* Test with non-existent file */
    assert(a2r_is_valid_file("/nonexistent.a2r") == false);
    assert(a2r_get_file_version("/nonexistent.a2r") == 0);
    
    printf("  ✓ a2r_file_detection passed\n");
}

void uft_a2r_parser_tests(void) {
    printf("Running A2R parser tests...\n");
    test_a2r_basics();
    test_a2r_file_detection();
    printf("All A2R parser tests passed!\n");
}

#endif /* UFT_UNIT_TESTS */
