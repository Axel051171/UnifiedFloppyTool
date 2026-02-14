/**
 * @file uft_cfi.c
 * @brief CFI (Compressed Floppy Image) format implementation
 * @version 3.9.0
 * 
 * CFI format from FDCOPY.COM, used for Amstrad PC distribution.
 * Reference: libdsk drvcfi.c by John Elliott
 */

#include "uft/formats/uft_cfi.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

static uint16_t read_le16(const uint8_t *p) {
    return p[0] | (p[1] << 8);
}

static void write_le16(uint8_t *p, uint16_t v) {
    p[0] = v & 0xFF;
    p[1] = (v >> 8) & 0xFF;
}

static uint8_t code_from_size(uint16_t size) {
    switch (size) {
        case 128:  return 0;
        case 256:  return 1;
        case 512:  return 2;
        case 1024: return 3;
        default:   return 2;
    }
}

/* ============================================================================
 * CFI Compression/Decompression
 * 
 * CFI block format:
 * - 2-byte length (little-endian), high bit of byte[1] indicates RLE
 * - If RLE: 1 byte fill value follows
 * - If literal: length bytes of data follow
 * ============================================================================ */

int cfi_decompress_track(const uint8_t *input, size_t track_block_size,
                         uint8_t *output, size_t output_capacity) {
    size_t in_pos = 0;
    size_t out_pos = 0;
    
    while (in_pos < track_block_size && out_pos < output_capacity) {
        if (in_pos + 2 > track_block_size) break;
        
        uint8_t lo = input[in_pos++];
        uint8_t hi = input[in_pos++];
        
        uint16_t block_len = lo | ((hi & 0x7F) << 8);
        bool is_rle = (hi & 0x80) != 0;
        
        if (block_len == 0) break;
        
        if (is_rle) {
            /* RLE block */
            if (in_pos >= track_block_size) break;
            uint8_t fill = input[in_pos++];
            
            if (out_pos + block_len > output_capacity) {
                block_len = output_capacity - out_pos;
            }
            memset(output + out_pos, fill, block_len);
            out_pos += block_len;
        } else {
            /* Literal block */
            if (in_pos + block_len > track_block_size) break;
            if (out_pos + block_len > output_capacity) {
                block_len = output_capacity - out_pos;
            }
            memcpy(output + out_pos, input + in_pos, block_len);
            in_pos += block_len;
            out_pos += block_len;
        }
    }
    
    return (int)out_pos;
}

int cfi_compress_track(const uint8_t *input, size_t input_size,
                       uint8_t *output, size_t output_capacity) {
    size_t in_pos = 0;
    size_t out_pos = 0;
    
    while (in_pos < input_size) {
        /* Check for RLE opportunity */
        uint8_t run_byte = input[in_pos];
        size_t run_len = 1;
        
        while (in_pos + run_len < input_size &&
               input[in_pos + run_len] == run_byte &&
               run_len < 0x7FFF) {
            run_len++;
        }
        
        if (run_len >= 4) {
            /* Encode as RLE */
            if (out_pos + 3 > output_capacity) return -1;
            write_le16(output + out_pos, run_len | 0x8000);
            out_pos += 2;
            output[out_pos++] = run_byte;
            in_pos += run_len;
        } else {
            /* Find literal block */
            size_t lit_start = in_pos;
            size_t lit_len = 0;
            
            while (in_pos + lit_len < input_size && lit_len < 0x7FFF) {
                /* Check if should switch to RLE */
                if (in_pos + lit_len + 3 < input_size) {
                    uint8_t b = input[in_pos + lit_len];
                    if (input[in_pos + lit_len + 1] == b &&
                        input[in_pos + lit_len + 2] == b &&
                        input[in_pos + lit_len + 3] == b) {
                        break;
                    }
                }
                lit_len++;
            }
            
            if (lit_len == 0) lit_len = 1;
            
            /* Write literal block */
            if (out_pos + 2 + lit_len > output_capacity) return -1;
            write_le16(output + out_pos, lit_len);
            out_pos += 2;
            memcpy(output + out_pos, input + in_pos, lit_len);
            out_pos += lit_len;
            in_pos += lit_len;
        }
    }
    
    return (int)out_pos;
}

/* ============================================================================
 * BPB Parsing (for geometry detection)
 * ============================================================================ */

typedef struct {
    uint8_t  jmp[3];
    char     oem[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  num_fats;
    uint16_t root_entries;
    uint16_t total_sectors_16;
    uint8_t  media_descriptor;
    uint16_t sectors_per_fat;
    uint16_t sectors_per_track;
    uint16_t heads;
    /* ... more fields follow ... */
} dos_bpb_t;

static bool parse_bpb(const uint8_t *data, size_t size,
                      uint16_t *out_cyls, uint8_t *out_heads,
                      uint8_t *out_spt, uint16_t *out_secsize) {
    if (size < 32) return false;
    
    /* Check for valid boot sector */
    if (data[0] != 0xEB && data[0] != 0xE9 && data[0] != 0x00) {
        return false;
    }
    
    uint16_t bps = read_le16(data + 11);  /* Bytes per sector */
    uint16_t spt = read_le16(data + 24);  /* Sectors per track */
    uint16_t heads = read_le16(data + 26); /* Heads */
    uint16_t total = read_le16(data + 19); /* Total sectors (16-bit) */
    
    /* Validate values */
    if (bps != 128 && bps != 256 && bps != 512 && bps != 1024 && bps != 2048) {
        return false;
    }
    if (spt == 0 || spt > 63) return false;
    if (heads == 0 || heads > 8) return false;
    if (total == 0) return false;
    
    /* Calculate cylinders */
    uint16_t cyls = total / (spt * heads);
    if (cyls == 0) cyls = 80;
    
    *out_cyls = cyls;
    *out_heads = (uint8_t)heads;
    *out_spt = (uint8_t)spt;
    *out_secsize = bps;
    
    return true;
}

/* ============================================================================
 * Read Implementation
 * ============================================================================ */

uft_error_t uft_cfi_read_mem(const uint8_t *data, size_t size,
                             uft_disk_image_t **out_disk,
                             cfi_read_result_t *result) {
    if (!data || !out_disk || size < CFI_MIN_FILE_SIZE) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    /* Initialize result */
    if (result) {
        memset(result, 0, sizeof(*result));
    }
    
    /* Allocate decompression buffer (max 2.88MB) */
    size_t max_size = 80 * 2 * 36 * 512;  /* 2.88MB */
    uint8_t *decompressed = malloc(max_size);
    if (!decompressed) {
        return UFT_ERR_MEMORY;
    }
    
    /* Decompress all tracks */
    size_t pos = 0;
    size_t decomp_pos = 0;
    uint32_t track_count = 0;
    
    while (pos + 2 <= size) {
        uint16_t track_len = read_le16(data + pos);
        pos += 2;
        
        if (track_len == 0) break;
        if (pos + track_len > size) break;
        
        int decomp_len = cfi_decompress_track(data + pos, track_len,
                                               decompressed + decomp_pos,
                                               max_size - decomp_pos);
        if (decomp_len > 0) {
            decomp_pos += decomp_len;
        }
        
        pos += track_len;
        track_count++;
    }
    
    if (decomp_pos < 512) {
        free(decompressed);
        if (result) {
            result->error = UFT_ERR_FORMAT;
            result->error_detail = "CFI decompression failed";
        }
        return UFT_ERR_FORMAT;
    }
    
    /* Parse BPB to get geometry */
    uint16_t cyls, secsize;
    uint8_t heads, spt;
    
    if (!parse_bpb(decompressed, decomp_pos, &cyls, &heads, &spt, &secsize)) {
        /* Try default geometry based on size */
        secsize = 512;
        size_t sectors = decomp_pos / secsize;
        
        if (sectors == 720) { cyls = 40; heads = 2; spt = 9; }
        else if (sectors == 1440) { cyls = 80; heads = 2; spt = 9; }
        else if (sectors == 2880) { cyls = 80; heads = 2; spt = 18; }
        else if (sectors == 5760) { cyls = 80; heads = 2; spt = 36; }
        else {
            free(decompressed);
            if (result) {
                result->error = UFT_ERR_FORMAT;
                result->error_detail = "Cannot determine CFI geometry";
            }
            return UFT_ERR_FORMAT;
        }
    }
    
    if (result) {
        result->cylinders = cyls;
        result->heads = heads;
        result->sectors = spt;
        result->sector_size = secsize;
        result->compressed_size = size;
        result->uncompressed_size = decomp_pos;
        result->track_count = track_count;
    }
    
    /* Allocate disk image */
    uft_disk_image_t *disk = uft_disk_alloc(cyls, heads);
    if (!disk) {
        free(decompressed);
        return UFT_ERR_MEMORY;
    }
    
    disk->format = UFT_FMT_RAW;
    snprintf(disk->format_name, sizeof(disk->format_name), "CFI");
    disk->sectors_per_track = spt;
    disk->bytes_per_sector = secsize;
    
    /* Copy sector data */
    size_t data_pos = 0;
    uint8_t size_code = code_from_size(secsize);
    
    for (uint16_t c = 0; c < cyls; c++) {
        for (uint8_t h = 0; h < heads; h++) {
            size_t idx = c * heads + h;
            
            uft_track_t *track = uft_track_alloc(spt, 0);
            if (!track) {
                uft_disk_free(disk);
                free(decompressed);
                return UFT_ERR_MEMORY;
            }
            
            track->cylinder = c;
            track->head = h;
            track->encoding = UFT_ENC_MFM;
            
            for (uint8_t s = 0; s < spt; s++) {
                uft_sector_t *sect = &track->sectors[s];
                sect->id.cylinder = c;
                sect->id.head = h;
                sect->id.sector = s + 1;
                sect->id.size_code = size_code;
                sect->status = UFT_SECTOR_OK;
                
                sect->data = malloc(secsize);
                sect->data_size = secsize;
                
                if (sect->data) {
                    if (data_pos + secsize <= decomp_pos) {
                        memcpy(sect->data, decompressed + data_pos, secsize);
                    } else {
                        memset(sect->data, 0xE5, secsize);
                    }
                }
                data_pos += secsize;
                track->sector_count++;
            }
            
            disk->track_data[idx] = track;
        }
    }
    
    free(decompressed);
    
    if (result) {
        result->success = true;
    }
    
    *out_disk = disk;
    return UFT_OK;
}

uft_error_t uft_cfi_read(const char *path,
                         uft_disk_image_t **out_disk,
                         cfi_read_result_t *result) {
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        return UFT_ERR_IO;
    }
    
    fseek(fp, 0, SEEK_END);
    size_t size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    uint8_t *data = malloc(size);
    if (!data) {
        fclose(fp);
        return UFT_ERR_MEMORY;
    }
    
    if (fread(data, 1, size, fp) != size) {
        free(data);
        fclose(fp);
        return UFT_ERR_IO;
    }
    
    fclose(fp);
    
    uft_error_t err = uft_cfi_read_mem(data, size, out_disk, result);
    free(data);
    
    return err;
}

/* ============================================================================
 * Probe Implementation
 * ============================================================================ */

bool uft_cfi_probe(const uint8_t *data, size_t size, int *confidence) {
    if (!data || size < CFI_MIN_FILE_SIZE) return false;
    
    /* CFI has no signature - must try to decompress and validate BPB */
    /* For probing, just check if first track looks valid */
    
    if (size < 4) return false;
    
    uint16_t first_track_len = read_le16(data);
    if (first_track_len == 0 || first_track_len > CFI_MAX_TRACK_SIZE) {
        return false;
    }
    
    if (2 + first_track_len > size) return false;
    
    /* Try to decompress first track and check for valid BPB */
    uint8_t *test_buf = malloc(32768);
    if (!test_buf) return false;
    
    int decomp_len = cfi_decompress_track(data + 2, first_track_len,
                                           test_buf, 32768);
    
    bool valid = false;
    if (decomp_len >= 512) {
        uint16_t cyls, secsize;
        uint8_t heads, spt;
        if (parse_bpb(test_buf, decomp_len, &cyls, &heads, &spt, &secsize)) {
            valid = true;
            if (confidence) *confidence = 70;  /* Lower confidence - no signature */
        }
    }
    
    free(test_buf);
    return valid;
}

/* ============================================================================
 * Write Implementation
 * ============================================================================ */

void uft_cfi_write_options_init(cfi_write_options_t *opts) {
    if (!opts) return;
    memset(opts, 0, sizeof(*opts));
    opts->use_compression = true;
}

uft_error_t uft_cfi_write(const uft_disk_image_t *disk,
                          const char *path,
                          const cfi_write_options_t *opts) {
    if (!disk || !path) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    cfi_write_options_t default_opts;
    if (!opts) {
        uft_cfi_write_options_init(&default_opts);
        opts = &default_opts;
    }
    
    /* Calculate sizes */
    size_t track_size = (size_t)disk->sectors_per_track * disk->bytes_per_sector;
    size_t max_output = (size_t)disk->tracks * disk->heads * (track_size + 256);
    
    uint8_t *output = malloc(max_output);
    uint8_t *track_buffer = malloc(track_size);
    uint8_t *comp_buffer = malloc(track_size + 256);
    
    if (!output || !track_buffer || !comp_buffer) {
        free(output);
        free(track_buffer);
        free(comp_buffer);
        return UFT_ERR_MEMORY;
    }
    
    size_t out_pos = 0;
    
    /* Write each track */
    for (uint16_t c = 0; c < disk->tracks; c++) {
        for (uint8_t h = 0; h < disk->heads; h++) {
            size_t idx = c * disk->heads + h;
            uft_track_t *track = disk->track_data[idx];
            
            /* Build track data */
            memset(track_buffer, 0xE5, track_size);
            for (uint8_t s = 0; s < disk->sectors_per_track; s++) {
                if (track && s < track->sector_count && track->sectors[s].data) {
                    memcpy(track_buffer + s * disk->bytes_per_sector,
                           track->sectors[s].data, disk->bytes_per_sector);
                }
            }
            
            /* Compress track */
            const uint8_t *write_data = track_buffer;
            size_t write_size = track_size;
            
            if (opts->use_compression) {
                int comp_len = cfi_compress_track(track_buffer, track_size,
                                                   comp_buffer, track_size + 256);
                if (comp_len > 0 && (size_t)comp_len < track_size) {
                    write_data = comp_buffer;
                    write_size = comp_len;
                }
            }
            
            /* Write track length and data */
            if (out_pos + 2 + write_size > max_output) {
                free(output);
                free(track_buffer);
                free(comp_buffer);
                return UFT_ERR_IO;
            }
            
            write_le16(output + out_pos, write_size);
            out_pos += 2;
            memcpy(output + out_pos, write_data, write_size);
            out_pos += write_size;
        }
    }
    
    free(track_buffer);
    free(comp_buffer);
    
    /* Write file */
    FILE *fp = fopen(path, "wb");
    if (!fp) {
        free(output);
        return UFT_ERR_IO;
    }
    
    size_t written = fwrite(output, 1, out_pos, fp);
    fclose(fp);
    free(output);
    
    if (written != out_pos) {
        return UFT_ERR_IO;
    }
    
    return UFT_OK;
}

/* ============================================================================
 * Format Plugin Registration
 * ============================================================================ */

static bool cfi_probe_plugin(const uint8_t *data, size_t size,
                             size_t file_size, int *confidence) {
    (void)file_size;
    return uft_cfi_probe(data, size, confidence);
}

static uft_error_t cfi_open(uft_disk_t *disk, const char *path, bool read_only) {
    (void)read_only;
    uft_disk_image_t *image = NULL;
    uft_error_t err = uft_cfi_read(path, &image, NULL);
    if (err == UFT_OK && image) {
        disk->plugin_data = image;
        disk->geometry.cylinders = image->tracks;
        disk->geometry.heads = image->heads;
        disk->geometry.sectors = image->sectors_per_track;
        disk->geometry.sector_size = image->bytes_per_sector;
    }
    return err;
}

static void cfi_close(uft_disk_t *disk) {
    if (disk && disk->plugin_data) {
        uft_disk_free((uft_disk_image_t*)disk->plugin_data);
        disk->plugin_data = NULL;
    }
}

static uft_error_t cfi_read_track(uft_disk_t *disk, int cyl, int head,
                                   uft_track_t *track) {
    uft_disk_image_t *image = (uft_disk_image_t*)disk->plugin_data;
    if (!image || !track) return UFT_ERR_INVALID_PARAM;
    
    size_t idx = cyl * image->heads + head;
    if (idx >= (size_t)(image->tracks * image->heads)) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    uft_track_t *src = image->track_data[idx];
    if (!src) return UFT_ERR_INVALID_PARAM;
    
    track->cylinder = cyl;
    track->head = head;
    track->sector_count = src->sector_count;
    track->encoding = src->encoding;
    
    for (uint8_t s = 0; s < src->sector_count; s++) {
        track->sectors[s] = src->sectors[s];
        if (src->sectors[s].data) {
            track->sectors[s].data = malloc(src->sectors[s].data_size);
            if (track->sectors[s].data) {
                memcpy(track->sectors[s].data, src->sectors[s].data,
                       src->sectors[s].data_size);
            }
        }
    }
    
    return UFT_OK;
}

const uft_format_plugin_t uft_format_plugin_cfi = {
    .name = "CFI",
    .description = "Compressed Floppy Image (FDCOPY)",
    .extensions = "cfi",
    .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE,
    .probe = cfi_probe_plugin,
    .open = cfi_open,
    .close = cfi_close,
    .read_track = cfi_read_track,
};

UFT_REGISTER_FORMAT_PLUGIN(cfi)
