/**
 * @file uft_mfm.c
 * @brief HxC MFM Format Implementation
 * 
 * @version 1.0.0
 * @date 2026-01-15
 */

#include "uft/formats/uft_mfm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*===========================================================================
 * CONTEXT
 *===========================================================================*/

struct uft_mfm_context {
    FILE *file;
    char *path;
    uft_mfm_header_t header;
    uft_mfm_track_t *tracks;
    int num_track_entries;
    bool writable;
};

/*===========================================================================
 * LIFECYCLE
 *===========================================================================*/

bool uft_mfm_probe(const char *path) {
    if (!path) return false;
    
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    
    char sig[8];
    bool valid = (fread(sig, 1, 8, f) == 8 && 
                  memcmp(sig, "HXCMFM", 6) == 0);
    fclose(f);
    
    return valid;
}

uft_mfm_context_t* uft_mfm_open(const char *path) {
    if (!path) return NULL;
    
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    
    /* Read header */
    uft_mfm_header_t header;
    if (fread(&header, sizeof(header), 1, f) != 1) {
        fclose(f);
        return NULL;
    }
    
    /* Verify signature */
    if (memcmp(header.signature, "HXCMFM", 6) != 0) {
        fclose(f);
        return NULL;
    }
    
    /* Allocate context */
    uft_mfm_context_t *ctx = (uft_mfm_context_t *)calloc(1, sizeof(*ctx));
    if (!ctx) {
        fclose(f);
        return NULL;
    }
    
    ctx->file = f;
    ctx->path = strdup(path);
    ctx->header = header;
    ctx->num_track_entries = header.num_tracks * header.num_sides;
    
    /* Read track list */
    if (header.track_list_offset > 0 && ctx->num_track_entries > 0) {
        ctx->tracks = (uft_mfm_track_t *)calloc(ctx->num_track_entries, 
                                                 sizeof(uft_mfm_track_t));
        if (ctx->tracks) {
            fseek(f, header.track_list_offset, SEEK_SET);
            fread(ctx->tracks, sizeof(uft_mfm_track_t), 
                  ctx->num_track_entries, f);
        }
    }
    
    return ctx;
}

uft_mfm_context_t* uft_mfm_create(const char *path,
                                   int num_tracks, int num_sides,
                                   int rpm, uint32_t bitrate) {
    if (!path || num_tracks <= 0 || num_sides <= 0) return NULL;
    
    FILE *f = fopen(path, "wb");
    if (!f) return NULL;
    
    uft_mfm_context_t *ctx = (uft_mfm_context_t *)calloc(1, sizeof(*ctx));
    if (!ctx) {
        fclose(f);
        return NULL;
    }
    
    ctx->file = f;
    ctx->path = strdup(path);
    ctx->writable = true;
    ctx->num_track_entries = num_tracks * num_sides;
    
    /* Initialize header */
    memcpy(ctx->header.signature, "HXCMFM\0\0", 8);
    ctx->header.format_revision = 1;
    ctx->header.num_tracks = num_tracks;
    ctx->header.num_sides = num_sides;
    ctx->header.rpm = rpm;
    ctx->header.bitrate = bitrate;
    ctx->header.track_encoding = UFT_MFM_ENC_MFM;
    ctx->header.interface_mode = UFT_MFM_IF_IBM_PC;
    ctx->header.track_list_offset = sizeof(uft_mfm_header_t);
    
    /* Allocate track list */
    ctx->tracks = (uft_mfm_track_t *)calloc(ctx->num_track_entries,
                                             sizeof(uft_mfm_track_t));
    
    /* Write header (will be updated on close) */
    fwrite(&ctx->header, sizeof(ctx->header), 1, f);
    
    /* Write placeholder track list */
    if (ctx->tracks) {
        for (int t = 0; t < num_tracks; t++) {
            for (int s = 0; s < num_sides; s++) {
                int idx = t * num_sides + s;
                ctx->tracks[idx].track_number = t;
                ctx->tracks[idx].side_number = s;
            }
        }
        fwrite(ctx->tracks, sizeof(uft_mfm_track_t), 
               ctx->num_track_entries, f);
    }
    
    return ctx;
}

void uft_mfm_close(uft_mfm_context_t *ctx) {
    if (!ctx) return;
    
    if (ctx->writable && ctx->file) {
        /* Update header and track list */
        fseek(ctx->file, 0, SEEK_SET);
        fwrite(&ctx->header, sizeof(ctx->header), 1, ctx->file);
        
        if (ctx->tracks) {
            fseek(ctx->file, ctx->header.track_list_offset, SEEK_SET);
            fwrite(ctx->tracks, sizeof(uft_mfm_track_t),
                   ctx->num_track_entries, ctx->file);
        }
    }
    
    if (ctx->file) fclose(ctx->file);
    free(ctx->tracks);
    free(ctx->path);
    free(ctx);
}

/*===========================================================================
 * INFORMATION
 *===========================================================================*/

const uft_mfm_header_t* uft_mfm_get_header(uft_mfm_context_t *ctx) {
    return ctx ? &ctx->header : NULL;
}

int uft_mfm_get_num_tracks(uft_mfm_context_t *ctx) {
    return ctx ? ctx->header.num_tracks : 0;
}

int uft_mfm_get_num_sides(uft_mfm_context_t *ctx) {
    return ctx ? ctx->header.num_sides : 0;
}

const uft_mfm_track_t* uft_mfm_get_track_info(uft_mfm_context_t *ctx,
                                               int track, int side) {
    if (!ctx || !ctx->tracks) return NULL;
    if (track < 0 || track >= ctx->header.num_tracks) return NULL;
    if (side < 0 || side >= ctx->header.num_sides) return NULL;
    
    int idx = track * ctx->header.num_sides + side;
    return &ctx->tracks[idx];
}

/*===========================================================================
 * TRACK I/O
 *===========================================================================*/

int uft_mfm_read_track(uft_mfm_context_t *ctx, int track, int side,
                        uint8_t *data, size_t max_size) {
    if (!ctx || !ctx->file || !data) return -1;
    
    const uft_mfm_track_t *ti = uft_mfm_get_track_info(ctx, track, side);
    if (!ti || ti->data_offset == 0 || ti->data_length == 0) return -1;
    
    size_t to_read = ti->data_length;
    if (to_read > max_size) to_read = max_size;
    
    fseek(ctx->file, ti->data_offset, SEEK_SET);
    return (int)fread(data, 1, to_read, ctx->file);
}

int uft_mfm_write_track(uft_mfm_context_t *ctx, int track, int side,
                         const uint8_t *data, size_t size) {
    if (!ctx || !ctx->file || !ctx->writable || !data) return -1;
    if (!ctx->tracks) return -1;
    
    int idx = track * ctx->header.num_sides + side;
    if (idx < 0 || idx >= ctx->num_track_entries) return -1;
    
    /* Seek to end for new data */
    fseek(ctx->file, 0, SEEK_END);
    uint32_t offset = (uint32_t)ftell(ctx->file);
    
    /* Write track data */
    size_t written = fwrite(data, 1, size, ctx->file);
    if (written != size) return -1;
    
    /* Update track entry */
    ctx->tracks[idx].track_number = track;
    ctx->tracks[idx].side_number = side;
    ctx->tracks[idx].data_offset = offset;
    ctx->tracks[idx].data_length = (uint32_t)size;
    
    return 0;
}

size_t uft_mfm_get_track_length(uft_mfm_context_t *ctx, int track, int side) {
    const uft_mfm_track_t *ti = uft_mfm_get_track_info(ctx, track, side);
    return ti ? ti->data_length : 0;
}

/*===========================================================================
 * BITSTREAM OPERATIONS
 *===========================================================================*/

int uft_mfm_to_flux(const uint8_t *mfm_data, size_t mfm_bits,
                    uint32_t bitrate,
                    uint32_t *flux_ns, size_t max_flux) {
    if (!mfm_data || !flux_ns || mfm_bits == 0 || bitrate == 0) return 0;
    
    /* Calculate bit time in nanoseconds */
    uint32_t bit_time_ns = (uint32_t)(1000000000ULL / bitrate);
    
    size_t flux_count = 0;
    uint32_t accumulated_time = 0;
    
    for (size_t bit = 0; bit < mfm_bits && flux_count < max_flux; bit++) {
        size_t byte_idx = bit / 8;
        int bit_idx = 7 - (bit % 8);
        
        int bit_val = (mfm_data[byte_idx] >> bit_idx) & 1;
        
        accumulated_time += bit_time_ns;
        
        if (bit_val) {
            /* Flux transition */
            flux_ns[flux_count++] = accumulated_time;
            accumulated_time = 0;
        }
    }
    
    return (int)flux_count;
}

int uft_flux_to_mfm(const uint32_t *flux_ns, size_t flux_count,
                    uint32_t bitrate,
                    uint8_t *mfm_data, size_t max_bytes) {
    if (!flux_ns || !mfm_data || flux_count == 0 || bitrate == 0) return 0;
    
    uint32_t bit_time_ns = (uint32_t)(1000000000ULL / bitrate);
    uint32_t half_bit = bit_time_ns / 2;
    
    memset(mfm_data, 0, max_bytes);
    
    size_t bit_pos = 0;
    size_t max_bits = max_bytes * 8;
    
    for (size_t i = 0; i < flux_count && bit_pos < max_bits; i++) {
        /* Calculate number of bit cells for this flux interval */
        uint32_t interval = flux_ns[i];
        int bits = (interval + half_bit) / bit_time_ns;
        
        /* Advance by (bits-1) zeros, then write a 1 */
        bit_pos += (bits > 0) ? (bits - 1) : 0;
        
        if (bit_pos < max_bits) {
            size_t byte_idx = bit_pos / 8;
            int bit_idx = 7 - (bit_pos % 8);
            mfm_data[byte_idx] |= (1 << bit_idx);
            bit_pos++;
        }
    }
    
    return (int)((bit_pos + 7) / 8);
}

/*===========================================================================
 * CONVERSION STUBS
 *===========================================================================*/

int uft_mfm_to_hfe(const char *mfm_path, const char *hfe_path) {
    (void)mfm_path;
    (void)hfe_path;
    /* TODO: Implement MFM to HFE conversion */
    return -1;
}

int uft_hfe_to_mfm(const char *hfe_path, const char *mfm_path) {
    (void)hfe_path;
    (void)mfm_path;
    /* TODO: Implement HFE to MFM conversion */
    return -1;
}

int uft_mfm_to_scp(const char *mfm_path, const char *scp_path) {
    (void)mfm_path;
    (void)scp_path;
    /* TODO: Implement MFM to SCP conversion */
    return -1;
}
