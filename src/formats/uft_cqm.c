/**
 * @file uft_cqm.c
 * @brief CopyQM (CQM) format implementation with LZSS
 * 
 * P1-003: Complete CQM support with decompression
 * 
 * LZSS Algorithm (CQM variant):
 * - 4096 byte ring buffer
 * - 12-bit offset, 4-bit length
 * - Minimum match length: 3
 * - Maximum match length: 18 (3 + 15)
 */

#include "uft/formats/uft_cqm.h"
#include "uft/core/uft_error_compat.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* LZSS Constants */
#define RING_SIZE       4096
#define RING_MASK       (RING_SIZE - 1)
#define MAX_MATCH_LEN   18
#define MIN_MATCH_LEN   3
#define THRESHOLD       2

/* ============================================================================
 * LZSS Decompression
 * ============================================================================ */

void cqm_decompress_init(cqm_decompress_ctx_t *ctx,
                         const uint8_t *input, size_t input_size,
                         uint8_t *output, size_t output_size) {
    if (!ctx) return;
    
    memset(ctx, 0, sizeof(*ctx));
    ctx->input = input;
    ctx->input_size = input_size;
    ctx->output = output;
    ctx->output_size = output_size;
    
    /* Initialize ring buffer with spaces (CQM convention) */
    memset(ctx->ring, ' ', RING_SIZE);
    ctx->ring_pos = RING_SIZE - MAX_MATCH_LEN;
}

static int decompress_read_byte(cqm_decompress_ctx_t *ctx) {
    if (ctx->input_pos >= ctx->input_size) {
        return -1;
    }
    ctx->bytes_read++;
    return ctx->input[ctx->input_pos++];
}

static int decompress_write_byte(cqm_decompress_ctx_t *ctx, uint8_t byte) {
    if (ctx->output_pos >= ctx->output_size) {
        return -1;
    }
    
    /* Write to output */
    ctx->output[ctx->output_pos++] = byte;
    
    /* Update ring buffer */
    ctx->ring[ctx->ring_pos] = byte;
    ctx->ring_pos = (ctx->ring_pos + 1) & RING_MASK;
    
    ctx->bytes_written++;
    return 0;
}

int cqm_decompress(cqm_decompress_ctx_t *ctx) {
    if (!ctx || !ctx->input || !ctx->output) {
        return -1;
    }
    
    while (ctx->input_pos < ctx->input_size) {
        /* Read flags byte */
        int flags = decompress_read_byte(ctx);
        if (flags < 0) break;
        
        /* Process 8 items (one for each bit in flags) */
        for (int i = 0; i < 8 && ctx->input_pos < ctx->input_size; i++) {
            if (flags & (1 << i)) {
                /* Literal byte */
                int byte = decompress_read_byte(ctx);
                if (byte < 0) goto done;
                
                if (decompress_write_byte(ctx, (uint8_t)byte) < 0) {
                    goto done;
                }
            } else {
                /* Match: read 2 bytes for offset/length */
                int lo = decompress_read_byte(ctx);
                int hi = decompress_read_byte(ctx);
                if (lo < 0 || hi < 0) goto done;
                
                /* Decode offset and length */
                /* CQM format: 12-bit offset, 4-bit length */
                int offset = lo | ((hi & 0xF0) << 4);
                int length = (hi & 0x0F) + MIN_MATCH_LEN;
                
                /* Copy from ring buffer */
                for (size_t j = 0; j < length; j++) {
                    uint8_t byte = ctx->ring[(offset + j) & RING_MASK];
                    if (decompress_write_byte(ctx, byte) < 0) {
                        goto done;
                    }
                }
            }
        }
    }
    
done:
    return (int)ctx->bytes_written;
}

int cqm_decompress_full(const uint8_t *compressed, size_t compressed_size,
                        uint8_t *output, size_t output_size,
                        size_t *out_decompressed_size) {
    cqm_decompress_ctx_t ctx;
    cqm_decompress_init(&ctx, compressed, compressed_size, output, output_size);
    
    int result = cqm_decompress(&ctx);
    
    if (out_decompressed_size) {
        *out_decompressed_size = ctx.bytes_written;
    }
    
    return result;
}

/* ============================================================================
 * LZSS Compression
 * ============================================================================ */

typedef struct {
    uint8_t ring[RING_SIZE];
    size_t ring_pos;
    
    const uint8_t *input;
    size_t input_size;
    size_t input_pos;
    
    uint8_t *output;
    size_t output_size;
    size_t output_pos;
    
    int level;  /* 1-9 */
} cqm_compress_ctx_t;

static void compress_init(cqm_compress_ctx_t *ctx,
                          const uint8_t *input, size_t input_size,
                          uint8_t *output, size_t output_size,
                          int level) {
    memset(ctx, 0, sizeof(*ctx));
    memset(ctx->ring, ' ', RING_SIZE);
    ctx->ring_pos = RING_SIZE - MAX_MATCH_LEN;
    
    ctx->input = input;
    ctx->input_size = input_size;
    ctx->output = output;
    ctx->output_size = output_size;
    ctx->level = (level < 1) ? 1 : ((level > 9) ? 9 : level);
}

/* Find longest match in ring buffer */
static int find_match(cqm_compress_ctx_t *ctx, size_t pos,
                      int *out_offset, int *out_length) {
    if (pos + MIN_MATCH_LEN > ctx->input_size) {
        return 0;
    }
    
    int best_offset = 0;
    int best_length = 0;
    
    /* Search window size based on compression level */
    int search_len = 32 << ctx->level;  /* 64 to 16384 */
    if (search_len > RING_SIZE) search_len = RING_SIZE;
    
    for (int off = 1; off < search_len; off++) {
        int ring_idx = ((int)ctx->ring_pos - off) & RING_MASK;
        int match_len = 0;
        
        while (match_len < MAX_MATCH_LEN &&
               pos + match_len < ctx->input_size &&
               ctx->input[pos + match_len] == ctx->ring[(ring_idx + match_len) & RING_MASK]) {
            match_len++;
        }
        
        if (match_len > best_length) {
            best_length = match_len;
            best_offset = ring_idx;
            
            if (best_length >= MAX_MATCH_LEN) break;
        }
    }
    
    if (best_length >= MIN_MATCH_LEN) {
        *out_offset = best_offset;
        *out_length = best_length;
        return 1;
    }
    
    return 0;
}

static void add_to_ring(cqm_compress_ctx_t *ctx, uint8_t byte) {
    ctx->ring[ctx->ring_pos] = byte;
    ctx->ring_pos = (ctx->ring_pos + 1) & RING_MASK;
}

int cqm_compress(const uint8_t *input, size_t input_size,
                 uint8_t *output, size_t output_capacity,
                 int level) {
    if (!input || !output || input_size == 0) {
        return -1;
    }
    
    cqm_compress_ctx_t ctx;
    compress_init(&ctx, input, input_size, output, output_capacity, level);
    
    size_t in_pos = 0;
    size_t out_pos = 0;
    
    while (in_pos < input_size) {
        if (out_pos + 17 > output_capacity) {
            return -1;  /* Output buffer full */
        }
        
        /* Reserve space for flags byte */
        size_t flags_pos = out_pos++;
        uint8_t flags = 0;
        
        /* Process up to 8 items */
        for (int i = 0; i < 8 && in_pos < input_size; i++) {
            int offset, length;
            
            if (find_match(&ctx, in_pos, &offset, &length)) {
                /* Write match */
                if (out_pos + 2 > output_capacity) {
                    return -1;
                }
                
                /* Encode: low byte of offset, high 4 bits of offset + length-3 */
                output[out_pos++] = offset & 0xFF;
                output[out_pos++] = ((offset >> 4) & 0xF0) | 
                                    ((length - MIN_MATCH_LEN) & 0x0F);
                
                /* Update ring buffer */
                for (size_t j = 0; j < length; j++) {
                    add_to_ring(&ctx, input[in_pos + j]);
                }
                in_pos += length;
                
            } else {
                /* Write literal */
                if (out_pos + 1 > output_capacity) {
                    return -1;
                }
                
                flags |= (1 << i);
                output[out_pos++] = input[in_pos];
                add_to_ring(&ctx, input[in_pos]);
                in_pos++;
            }
        }
        
        output[flags_pos] = flags;
    }
    
    return (int)out_pos;
}

/* ============================================================================
 * Header Validation
 * ============================================================================ */

bool uft_cqm_validate_header(const cqm_header_t *header) {
    if (!header) return false;
    
    /* Check signature */
    if (memcmp(header->signature, CQM_SIGNATURE, 3) != 0) {
        return false;
    }
    
    /* Check version */
    if (header->version < 1 || header->version > 2) {
        return false;
    }
    
    /* Check sector size */
    if (header->sector_size != 128 && header->sector_size != 256 &&
        header->sector_size != 512 && header->sector_size != 1024) {
        return false;
    }
    
    /* Check tracks */
    if (header->total_tracks > 86) {
        return false;
    }
    
    return true;
}

/* ============================================================================
 * File I/O
 * ============================================================================ */

void uft_cqm_write_options_init(cqm_write_options_t *opts) {
    if (!opts) return;
    
    memset(opts, 0, sizeof(*opts));
    opts->compress = true;
    opts->compression_level = 6;
    opts->include_bpb = true;
}

uft_error_t uft_cqm_read(const char *path,
                         uft_disk_image_t **out_disk,
                         cqm_read_result_t *result) {
    if (!path || !out_disk) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    /* Read file */
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        if (result) {
            result->error = UFT_ERR_IO;
            result->error_detail = "Failed to open file";
        }
        return UFT_ERR_IO;
    }
    
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return UFT_ERR_IO;
    }
    long file_size_l = ftell(fp);
    if (file_size_l < 0) {
        fclose(fp);
        return UFT_ERR_IO;
    }
    size_t file_size = (size_t)file_size_l;
    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return UFT_ERR_IO;
    }
    
    uint8_t *data = malloc(file_size);
    if (!data) {
        fclose(fp);
        return UFT_ERR_MEMORY;
    }
    
    if (fread(data, 1, file_size, fp) != file_size) {
        free(data);
        fclose(fp);
        return UFT_ERR_IO;
    }
    fclose(fp);
    
    uft_error_t err = uft_cqm_read_mem(data, file_size, out_disk, result);
    
    free(data);
    return err;
}

uft_error_t uft_cqm_read_mem(const uint8_t *data, size_t size,
                             uft_disk_image_t **out_disk,
                             cqm_read_result_t *result) {
    if (!data || !out_disk || size < CQM_HEADER_SIZE) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    if (result) memset(result, 0, sizeof(*result));
    
    /* Parse header */
    const cqm_header_t *hdr = (const cqm_header_t *)data;
    
    if (!uft_cqm_validate_header(hdr)) {
        if (result) {
            result->error = UFT_ERR_CORRUPT;
            result->error_detail = "Invalid CQM header";
        }
        return UFT_ERR_CORRUPT;
    }
    
    /* Extract geometry */
    uint16_t sectors_per_track = hdr->sectors_per_track;
    uint8_t heads = (hdr->heads > 0) ? hdr->heads : 1;
    uint8_t total_tracks = hdr->total_tracks;
    uint16_t sector_size = hdr->sector_size;
    
    if (total_tracks == 0) {
        /* Calculate from total sectors */
        uint32_t total_sectors = hdr->total_sectors_16 ? 
                                 hdr->total_sectors_16 : hdr->total_sectors_32;
        if (total_sectors > 0 && sectors_per_track > 0 && heads > 0) {
            total_tracks = (total_sectors / sectors_per_track / heads);
        }
    }
    
    if (result) {
        result->tracks = total_tracks;
        result->heads = heads;
        result->sectors_per_track = sectors_per_track;
        result->sector_size = sector_size;
    }
    
    /* Skip comment if present */
    size_t data_offset = CQM_HEADER_SIZE;
    uint16_t comment_len = hdr->comment_length_low | 
                          (hdr->comment_length_high << 8);
    
    if (comment_len > 0 && comment_len < CQM_MAX_COMMENT) {
        if (result) {
            result->comment = malloc(comment_len + 1);
            if (result->comment) {
                memcpy(result->comment, data + data_offset, comment_len);
                result->comment[comment_len] = '\0';
                result->comment_len = comment_len;
            }
        }
        data_offset += comment_len;
    }
    
    /* Calculate uncompressed size */
    size_t uncompressed_size = (size_t)total_tracks * heads * 
                               sectors_per_track * sector_size;
    
    /* Allocate decompression buffer */
    uint8_t *uncompressed = malloc(uncompressed_size);
    if (!uncompressed) {
        return UFT_ERR_MEMORY;
    }
    
    /* Decompress */
    size_t decompressed_size = 0;
    int decomp_result = cqm_decompress_full(
        data + data_offset, size - data_offset,
        uncompressed, uncompressed_size,
        &decompressed_size);
    
    if (decomp_result < 0) {
        free(uncompressed);
        if (result) {
            result->error = UFT_ERR_CORRUPT;
            result->error_detail = "Decompression failed";
        }
        return UFT_ERR_CORRUPT;
    }
    
    if (result) {
        result->compressed_size = size - data_offset;
        result->uncompressed_size = decompressed_size;
        if (result->compressed_size > 0) {
            result->compression_ratio = (double)result->uncompressed_size / 
                                        result->compressed_size;
        }
    }
    
    /* Create disk image */
    uft_disk_image_t *disk = uft_disk_alloc(total_tracks, heads);
    if (!disk) {
        free(uncompressed);
        return UFT_ERR_MEMORY;
    }
    
    disk->format = UFT_FMT_CQM;
    snprintf(disk->format_name, sizeof(disk->format_name), "CQM");
    disk->sectors_per_track = sectors_per_track;
    disk->bytes_per_sector = sector_size;
    
    /* Copy sector data */
    size_t data_pos = 0;
    uint8_t size_code = uft_code_from_size(sector_size);
    
    for (uint8_t t = 0; t < total_tracks; t++) {
        for (uint8_t h = 0; h < heads; h++) {
            size_t idx = t * heads + h;
            
            uft_track_t *track = uft_track_alloc(sectors_per_track, 0);
            if (!track) {
                uft_disk_free(disk);
                free(uncompressed);
                return UFT_ERR_MEMORY;
            }
            
            track->track_num = t;
            track->head = h;
            track->encoding = UFT_ENC_MFM;
            
            for (uint8_t s = 0; s < sectors_per_track; s++) {
                uft_sector_t *sect = &track->sectors[s];
                sect->id.track = t;
                sect->id.head = h;
                sect->id.sector = s + 1;
                sect->id.size_code = size_code;
                sect->id.status = UFT_SECTOR_OK;
                
                sect->data = malloc(sector_size);
                sect->data_len = sector_size;
                
                if (data_pos + sector_size <= decompressed_size) {
                    memcpy(sect->data, uncompressed + data_pos, sector_size);
                } else {
                    memset(sect->data, 0xE5, sector_size);
                }
                data_pos += sector_size;
                
                track->sector_count++;
            }
            
            disk->track_data[idx] = track;
        }
    }
    
    free(uncompressed);
    
    if (result) {
        result->success = true;
    }
    
    *out_disk = disk;
    return UFT_OK;
}

uft_error_t uft_cqm_write(const uft_disk_image_t *disk,
                          const char *path,
                          const cqm_write_options_t *opts) {
    if (!disk || !path) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    cqm_write_options_t default_opts;
    if (!opts) {
        uft_cqm_write_options_init(&default_opts);
        opts = &default_opts;
    }
    
    /* Calculate uncompressed size */
    size_t data_size = (size_t)disk->tracks * disk->heads * 
                       disk->sectors_per_track * disk->bytes_per_sector;
    
    /* Allocate uncompressed buffer */
    uint8_t *uncompressed = malloc(data_size);
    if (!uncompressed) {
        return UFT_ERR_MEMORY;
    }
    
    /* Copy sector data */
    size_t data_pos = 0;
    for (uint16_t t = 0; t < disk->tracks; t++) {
        for (uint8_t h = 0; h < disk->heads; h++) {
            size_t idx = t * disk->heads + h;
            uft_track_t *track = disk->track_data[idx];
            
            for (uint8_t s = 0; s < disk->sectors_per_track; s++) {
                if (track && s < track->sector_count && 
                    track->sectors[s].data) {
                    memcpy(uncompressed + data_pos, 
                           track->sectors[s].data,
                           disk->bytes_per_sector);
                } else {
                    memset(uncompressed + data_pos, 0xE5, 
                           disk->bytes_per_sector);
                }
                data_pos += disk->bytes_per_sector;
            }
        }
    }
    
    /* Prepare output buffer */
    size_t max_output = CQM_HEADER_SIZE + (opts->comment ? strlen(opts->comment) : 0) + 
                        data_size + 4096;  /* Extra for compression overhead */
    uint8_t *output = malloc(max_output);
    if (!output) {
        free(uncompressed);
        return UFT_ERR_MEMORY;
    }
    
    /* Build header */
    cqm_header_t *hdr = (cqm_header_t *)output;
    memset(hdr, 0, sizeof(*hdr));
    
    memcpy(hdr->signature, CQM_SIGNATURE, 3);
    hdr->version = 1;
    hdr->sector_size = disk->bytes_per_sector;
    hdr->sectors_per_track = disk->sectors_per_track;
    hdr->heads = disk->heads;
    hdr->total_tracks = disk->tracks;
    hdr->density = (disk->bytes_per_sector == 512 && 
                   disk->sectors_per_track >= 15) ? CQM_DENSITY_HD : CQM_DENSITY_DD;
    
    uint32_t total_sectors = disk->tracks * disk->heads * disk->sectors_per_track;
    hdr->total_sectors_16 = (total_sectors <= 0xFFFF) ? total_sectors : 0;
    hdr->total_sectors_32 = total_sectors;
    
    size_t output_pos = CQM_HEADER_SIZE;
    
    /* Add comment if present */
    if (opts->comment) {
        size_t comment_len = strlen(opts->comment);
        if (comment_len > CQM_MAX_COMMENT) comment_len = CQM_MAX_COMMENT;
        hdr->comment_length_low = comment_len & 0xFF;
        hdr->comment_length_high = (comment_len >> 8) & 0xFF;
        memcpy(output + output_pos, opts->comment, comment_len);
        output_pos += comment_len;
    }
    
    /* Compress or copy data */
    size_t data_output_size;
    if (opts->compress) {
        int compressed = cqm_compress(uncompressed, data_size,
                                      output + output_pos, 
                                      max_output - output_pos,
                                      opts->compression_level);
        if (compressed < 0) {
            /* Compression failed, write uncompressed */
            memcpy(output + output_pos, uncompressed, data_size);
            data_output_size = data_size;
        } else {
            data_output_size = compressed;
        }
    } else {
        memcpy(output + output_pos, uncompressed, data_size);
        data_output_size = data_size;
    }
    
    output_pos += data_output_size;
    
    /* Write file */
    FILE *fp = fopen(path, "wb");
    if (!fp) {
        free(output);
        free(uncompressed);
        return UFT_ERR_IO;
    }
    
    size_t written = fwrite(output, 1, output_pos, fp);
    fclose(fp);
    
    free(output);
    free(uncompressed);
    
    if (written != output_pos) {
        return UFT_ERR_IO;
    }
    
    return UFT_OK;
}
