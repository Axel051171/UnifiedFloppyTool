/**
 * @file uft_mfm_image.c
 * @brief MFM native bitstream image format implementation
 * 
 */

#include "uft_mfm_image.h"
#include <stdlib.h>
#include <string.h>

/* Default values */
#define DEFAULT_SPINDLE_NS   166666667ULL  /* 300 RPM = 200ms = 200,000,000 ns */
#define DEFAULT_BIT_RATE     500000ULL     /* 500 Kbit/s (MFM DD) */
#define DEFAULT_SAMPLE_RATE  4000000ULL    /* 4 MHz */

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

static uint64_t read_le64(FILE *fp) {
    uint8_t buf[8];
    if (fread(buf, 1, 8, fp) != 8) return 0;
    
    return (uint64_t)buf[0]
         | ((uint64_t)buf[1] << 8)
         | ((uint64_t)buf[2] << 16)
         | ((uint64_t)buf[3] << 24)
         | ((uint64_t)buf[4] << 32)
         | ((uint64_t)buf[5] << 40)
         | ((uint64_t)buf[6] << 48)
         | ((uint64_t)buf[7] << 56);
}

static int write_le64(FILE *fp, uint64_t val) {
    uint8_t buf[8];
    buf[0] = (uint8_t)(val);
    buf[1] = (uint8_t)(val >> 8);
    buf[2] = (uint8_t)(val >> 16);
    buf[3] = (uint8_t)(val >> 24);
    buf[4] = (uint8_t)(val >> 32);
    buf[5] = (uint8_t)(val >> 40);
    buf[6] = (uint8_t)(val >> 48);
    buf[7] = (uint8_t)(val >> 56);
    
    return (fwrite(buf, 1, 8, fp) == 8) ? 0 : -1;
}

/* ============================================================================
 * Open/Close
 * ============================================================================ */

int uft_mfm_open_read(const char *path, uft_mfm_ctx_t *ctx) {
    if (!path || !ctx) return -1;
    
    memset(ctx, 0, sizeof(*ctx));
    
    ctx->fp = fopen(path, "rb");
    if (!ctx->fp) return -2;
    
    ctx->is_write = false;
    
    /* Read header */
    if (fread(ctx->header.id_str, 1, 8, ctx->fp) != 8) {
        fclose(ctx->fp);
        return -3;
    }
    
    /* Verify magic */
    if (memcmp(ctx->header.id_str, UFT_MFM_MAGIC, UFT_MFM_MAGIC_LEN) != 0) {
        fclose(ctx->fp);
        return -4;  /* Not an MFM image */
    }
    
    ctx->header.track_table_offset = read_le64(ctx->fp);
    ctx->header.number_of_tracks = read_le64(ctx->fp);
    ctx->header.spindle_time_ns = read_le64(ctx->fp);
    ctx->header.data_bit_rate = read_le64(ctx->fp);
    ctx->header.sampling_rate = read_le64(ctx->fp);
    
    ctx->track_count = (size_t)ctx->header.number_of_tracks;
    
    /* Allocate and read track table */
    if (ctx->track_count > 0) {
        ctx->tracks = calloc(ctx->track_count, sizeof(uft_mfm_track_entry_t));
        if (!ctx->tracks) {
            fclose(ctx->fp);
            return -5;
        }
        
        if (fseek(ctx->fp, (long)ctx->header.track_table_offset, SEEK_SET) != 0) {
            free(ctx->tracks);
            fclose(ctx->fp);
            return -6;
        }
        
        for (size_t i = 0; i < ctx->track_count; i++) {
            ctx->tracks[i].offset = read_le64(ctx->fp);
            ctx->tracks[i].length_bit = read_le64(ctx->fp);
        }
    }
    
    return 0;
}

int uft_mfm_open_write(const char *path, size_t track_count, uft_mfm_ctx_t *ctx) {
    if (!path || !ctx || track_count == 0) return -1;
    
    memset(ctx, 0, sizeof(*ctx));
    
    ctx->fp = fopen(path, "wb");
    if (!ctx->fp) return -2;
    
    ctx->is_write = true;
    ctx->track_count = track_count;
    
    /* Set defaults */
    ctx->default_spindle_ns = DEFAULT_SPINDLE_NS;
    ctx->default_bit_rate = DEFAULT_BIT_RATE;
    ctx->default_sample_rate = DEFAULT_SAMPLE_RATE;
    
    /* Allocate track table */
    ctx->tracks = calloc(track_count, sizeof(uft_mfm_track_entry_t));
    if (!ctx->tracks) {
        fclose(ctx->fp);
        return -3;
    }
    
    /* Initialize header */
    memcpy(ctx->header.id_str, UFT_MFM_MAGIC, UFT_MFM_MAGIC_LEN);
    ctx->header.track_table_offset = 48;  /* After header */
    ctx->header.number_of_tracks = track_count;
    ctx->header.spindle_time_ns = ctx->default_spindle_ns;
    ctx->header.data_bit_rate = ctx->default_bit_rate;
    ctx->header.sampling_rate = ctx->default_sample_rate;
    
    /* Write header (placeholder) */
    fwrite(ctx->header.id_str, 1, 8, ctx->fp);
    write_le64(ctx->fp, ctx->header.track_table_offset);
    write_le64(ctx->fp, ctx->header.number_of_tracks);
    write_le64(ctx->fp, ctx->header.spindle_time_ns);
    write_le64(ctx->fp, ctx->header.data_bit_rate);
    write_le64(ctx->fp, ctx->header.sampling_rate);
    
    /* Write track table (placeholder, will be updated on close) */
    for (size_t i = 0; i < track_count; i++) {
        write_le64(ctx->fp, 0);  /* offset */
        write_le64(ctx->fp, 0);  /* length_bit */
    }
    
    return 0;
}

void uft_mfm_close(uft_mfm_ctx_t *ctx) {
    if (!ctx) return;
    
    if (ctx->fp) {
        if (ctx->is_write && ctx->tracks) {
            /* Update track table */
            fseek(ctx->fp, (long)ctx->header.track_table_offset, SEEK_SET);
            for (size_t i = 0; i < ctx->track_count; i++) {
                write_le64(ctx->fp, ctx->tracks[i].offset);
                write_le64(ctx->fp, ctx->tracks[i].length_bit);
            }
        }
        fclose(ctx->fp);
        ctx->fp = NULL;
    }
    
    if (ctx->tracks) {
        free(ctx->tracks);
        ctx->tracks = NULL;
    }
}

/* ============================================================================
 * Track I/O
 * ============================================================================ */

int uft_mfm_read_track(uft_mfm_ctx_t *ctx,
                       size_t track_num,
                       uint8_t *bits,
                       size_t bits_capacity,
                       size_t *out_length) {
    if (!ctx || !ctx->fp || !bits || ctx->is_write) return -1;
    if (track_num >= ctx->track_count) return -2;
    
    uft_mfm_track_entry_t *entry = &ctx->tracks[track_num];
    
    if (out_length) *out_length = (size_t)entry->length_bit;
    
    /* Calculate byte length */
    size_t byte_len = (size_t)((entry->length_bit + 7) / 8);
    
    if (bits_capacity * 8 < entry->length_bit) {
        return -3;  /* Buffer too small */
    }
    
    if (fseek(ctx->fp, (long)entry->offset, SEEK_SET) != 0) {
        return -4;
    }
    
    if (fread(bits, 1, byte_len, ctx->fp) != byte_len) {
        return -5;
    }
    
    return 0;
}

int uft_mfm_write_track(uft_mfm_ctx_t *ctx,
                        size_t track_num,
                        const uint8_t *bits,
                        size_t length_bits) {
    if (!ctx || !ctx->fp || !bits || !ctx->is_write) return -1;
    if (track_num >= ctx->track_count) return -2;
    
    /* Get current position for track offset */
    long pos = ftell(ctx->fp);
    if (pos < 0) return -3;
    
    ctx->tracks[track_num].offset = (uint64_t)pos;
    ctx->tracks[track_num].length_bit = length_bits;
    
    /* Calculate byte length */
    size_t byte_len = (length_bits + 7) / 8;
    
    if (fwrite(bits, 1, byte_len, ctx->fp) != byte_len) {
        return -4;
    }
    
    return 0;
}

/* ============================================================================
 * Query Functions
 * ============================================================================ */

size_t uft_mfm_get_track_length(const uft_mfm_ctx_t *ctx, size_t track_num) {
    if (!ctx || track_num >= ctx->track_count || !ctx->tracks) return 0;
    return (size_t)ctx->tracks[track_num].length_bit;
}

void uft_mfm_set_params(uft_mfm_ctx_t *ctx,
                        uint64_t sample_rate,
                        uint64_t bit_rate,
                        uint64_t spindle_ns) {
    if (!ctx) return;
    
    if (sample_rate > 0) {
        ctx->default_sample_rate = sample_rate;
        ctx->header.sampling_rate = sample_rate;
    }
    
    if (bit_rate > 0) {
        ctx->default_bit_rate = bit_rate;
        ctx->header.data_bit_rate = bit_rate;
    }
    
    if (spindle_ns > 0) {
        ctx->default_spindle_ns = spindle_ns;
        ctx->header.spindle_time_ns = spindle_ns;
    }
}
