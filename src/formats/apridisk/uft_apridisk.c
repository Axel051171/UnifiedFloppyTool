/**
 * @file uft_apridisk.c
 * @brief ApriDisk format implementation
 * @version 3.9.0
 * 
 * ApriDisk format support with RLE compression.
 * Reference: libdsk drvapdsk.c by John Elliott
 */

#include "uft/formats/uft_apridisk.h"
#include "uft/uft_track.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

static uint32_t read_le32(const uint8_t *p) {
    return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
}

static void write_le32(uint8_t *p, uint32_t v) {
    p[0] = v & 0xFF;
    p[1] = (v >> 8) & 0xFF;
    p[2] = (v >> 16) & 0xFF;
    p[3] = (v >> 24) & 0xFF;
}

static uint16_t sector_size_from_code(uint8_t code) {
    switch (code) {
        case 0: return 128;
        case 1: return 256;
        case 2: return 512;
        case 3: return 1024;
        case 4: return 2048;
        case 5: return 4096;
        default: return 512;
    }
}

static uint8_t code_from_sector_size(uint16_t size) {
    switch (size) {
        case 128:  return 0;
        case 256:  return 1;
        case 512:  return 2;
        case 1024: return 3;
        case 2048: return 4;
        case 4096: return 5;
        default:   return 2;
    }
}

/* ============================================================================
 * RLE Compression/Decompression
 * 
 * ApriDisk RLE format:
 * - Byte pair: count, value
 * - If count is 0, followed by literal count + that many bytes
 * ============================================================================ */

int apridisk_rle_decompress(const uint8_t *input, size_t input_size,
                            uint8_t *output, size_t output_size) {
    size_t in_pos = 0;
    size_t out_pos = 0;
    
    while (in_pos < input_size && out_pos < output_size) {
        uint8_t count = input[in_pos++];
        
        if (count == 0) {
            /* Literal block */
            if (in_pos >= input_size) break;
            uint8_t lit_count = input[in_pos++];
            
            for (int i = 0; i < lit_count && in_pos < input_size && out_pos < output_size; i++) {
                output[out_pos++] = input[in_pos++];
            }
        } else {
            /* RLE run */
            if (in_pos >= input_size) break;
            uint8_t value = input[in_pos++];
            
            for (int i = 0; i < count && out_pos < output_size; i++) {
                output[out_pos++] = value;
            }
        }
    }
    
    return (int)out_pos;
}

int apridisk_rle_compress(const uint8_t *input, size_t input_size,
                          uint8_t *output, size_t output_capacity) {
    size_t in_pos = 0;
    size_t out_pos = 0;
    
    while (in_pos < input_size) {
        /* Check for a run */
        size_t run_start = in_pos;
        uint8_t run_byte = input[in_pos];
        size_t run_len = 1;
        
        while (in_pos + run_len < input_size &&
               input[in_pos + run_len] == run_byte &&
               run_len < 255) {
            run_len++;
        }
        
        if (run_len >= 3) {
            /* Encode as RLE run */
            if (out_pos + 2 > output_capacity) return -1;
            output[out_pos++] = (uint8_t)run_len;
            output[out_pos++] = run_byte;
            in_pos += run_len;
        } else {
            /* Find literal block */
            size_t lit_start = in_pos;
            size_t lit_len = 0;
            
            while (in_pos + lit_len < input_size && lit_len < 255) {
                /* Check if we should switch to RLE */
                if (in_pos + lit_len + 2 < input_size &&
                    input[in_pos + lit_len] == input[in_pos + lit_len + 1] &&
                    input[in_pos + lit_len] == input[in_pos + lit_len + 2]) {
                    break;
                }
                lit_len++;
            }
            
            if (lit_len == 0) {
                /* Single byte that starts a run */
                if (out_pos + 3 > output_capacity) return -1;
                output[out_pos++] = 0;      /* Literal marker */
                output[out_pos++] = 1;      /* Literal count */
                output[out_pos++] = input[in_pos++];
            } else {
                /* Literal block */
                if (out_pos + 2 + lit_len > output_capacity) return -1;
                output[out_pos++] = 0;      /* Literal marker */
                output[out_pos++] = (uint8_t)lit_len;
                memcpy(output + out_pos, input + in_pos, lit_len);
                out_pos += lit_len;
                in_pos += lit_len;
            }
        }
    }
    
    /* Only return compressed data if it's smaller */
    if (out_pos >= input_size) {
        return -1;  /* Compression not beneficial */
    }
    
    return (int)out_pos;
}

/* ============================================================================
 * Header Validation
 * ============================================================================ */

bool uft_apridisk_validate_header(const apridisk_header_t *header) {
    if (!header) return false;
    return memcmp(header->signature, APRIDISK_SIGNATURE, APRIDISK_SIGNATURE_LEN) == 0;
}

bool uft_apridisk_probe(const uint8_t *data, size_t size, int *confidence) {
    if (!data || size < APRIDISK_HEADER_SIZE) return false;
    
    if (memcmp(data, APRIDISK_SIGNATURE, APRIDISK_SIGNATURE_LEN) == 0) {
        if (confidence) *confidence = 95;
        return true;
    }
    
    return false;
}

/* ============================================================================
 * Read Implementation
 * ============================================================================ */

uft_error_t uft_apridisk_read_mem(const uint8_t *data, size_t size,
                                  uft_disk_image_t **out_disk,
                                  apridisk_read_result_t *result) {
    if (!data || !out_disk || size < APRIDISK_HEADER_SIZE) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    /* Initialize result */
    if (result) {
        memset(result, 0, sizeof(*result));
    }
    
    /* Validate header */
    const apridisk_header_t *header = (const apridisk_header_t *)data;
    if (!uft_apridisk_validate_header(header)) {
        if (result) {
            result->error = UFT_ERR_FORMAT;
            result->error_detail = "Invalid ApriDisk signature";
        }
        return UFT_ERR_FORMAT;
    }
    
    /* First pass: determine disk geometry */
    uint8_t max_cyl = 0, max_head = 0, max_sect = 0;
    uint16_t sector_size = 512;
    uint32_t total_sectors = 0, deleted_sectors = 0, rle_sectors = 0;
    
    size_t pos = APRIDISK_HEADER_SIZE;
    
    while (pos + sizeof(apridisk_record_desc_t) <= size) {
        apridisk_record_desc_t rec;
        memcpy(&rec, data + pos, sizeof(rec));
        
        /* Convert from little-endian */
        rec.type = read_le32((uint8_t*)&rec.type);
        rec.compression = read_le32((uint8_t*)&rec.compression);
        rec.header_size = read_le32((uint8_t*)&rec.header_size);
        rec.data_size = read_le32((uint8_t*)&rec.data_size);
        
        if (rec.header_size < 16) break;  /* Invalid */
        
        pos += rec.header_size;
        
        if (rec.type == APRIDISK_SECTOR || rec.type == APRIDISK_DELETED) {
            if (pos >= size) break;
            
            /* Read sector descriptor (first 8 bytes of data area) */
            apridisk_sector_desc_t sdesc;
            memcpy(&sdesc, data + pos - 8, sizeof(sdesc));
            
            if (sdesc.cylinder > max_cyl) max_cyl = sdesc.cylinder;
            if (sdesc.head > max_head) max_head = sdesc.head;
            if (sdesc.sector > max_sect) max_sect = sdesc.sector;
            sector_size = sector_size_from_code(sdesc.size_code);
            
            total_sectors++;
            if (rec.type == APRIDISK_DELETED) deleted_sectors++;
            if (rec.compression == APRIDISK_COMP_RLE) rle_sectors++;
        }
        
        pos += rec.data_size;
    }
    
    /* Allocate disk image */
    uint16_t tracks = max_cyl + 1;
    uint8_t heads = max_head + 1;
    uint8_t sectors = max_sect;  /* Sectors are 1-based */
    
    if (tracks == 0) tracks = 80;
    if (heads == 0) heads = 2;
    if (sectors == 0) sectors = 9;
    
    uft_disk_image_t *disk = uft_disk_alloc(tracks, heads);
    if (!disk) {
        return UFT_ERR_MEMORY;
    }
    
    disk->format = UFT_FMT_RAW;
    snprintf(disk->format_name, sizeof(disk->format_name), "ApriDisk");
    disk->sectors_per_track = sectors;
    disk->bytes_per_sector = sector_size;
    
    /* Allocate tracks */
    for (uint16_t t = 0; t < tracks; t++) {
        for (uint8_t h = 0; h < heads; h++) {
            size_t idx = t * heads + h;
            uft_track_t *track = uft_track_alloc(sectors, 0);
            if (!track) {
                uft_disk_free(disk);
                return UFT_ERR_MEMORY;
            }
            track->cylinder = t;
            track->head = h;
            track->encoding = UFT_ENC_MFM;
            disk->track_data[idx] = track;
        }
    }
    
    /* Second pass: read sector data */
    pos = APRIDISK_HEADER_SIZE;
    uint8_t *decomp_buffer = malloc(8192);  /* Max sector size */
    if (!decomp_buffer) {
        uft_disk_free(disk);
        return UFT_ERR_MEMORY;
    }
    
    while (pos + sizeof(apridisk_record_desc_t) <= size) {
        apridisk_record_desc_t rec;
        memcpy(&rec, data + pos, sizeof(rec));
        
        rec.type = read_le32((uint8_t*)&rec.type);
        rec.compression = read_le32((uint8_t*)&rec.compression);
        rec.header_size = read_le32((uint8_t*)&rec.header_size);
        rec.data_size = read_le32((uint8_t*)&rec.data_size);
        
        if (rec.header_size < 16) break;
        
        pos += rec.header_size;
        
        if (rec.type == APRIDISK_SECTOR && pos + rec.data_size <= size) {
            /* Parse sector descriptor from header extension */
            apridisk_sector_desc_t sdesc;
            memcpy(&sdesc, data + pos - 8, sizeof(sdesc));
            
            uint16_t sec_size = sector_size_from_code(sdesc.size_code);
            const uint8_t *sec_data = data + pos;
            size_t data_len = rec.data_size;
            
            uint8_t *final_data = (uint8_t*)sec_data;
            
            /* Decompress if needed */
            if (rec.compression == APRIDISK_COMP_RLE) {
                int decomp_len = apridisk_rle_decompress(sec_data, data_len,
                                                         decomp_buffer, sec_size);
                if (decomp_len > 0) {
                    final_data = decomp_buffer;
                    data_len = decomp_len;
                }
            }
            
            /* Store sector */
            if (sdesc.cylinder < tracks && sdesc.head < heads && sdesc.sector > 0) {
                size_t idx = sdesc.cylinder * heads + sdesc.head;
                uft_track_t *track = disk->track_data[idx];
                
                if (track && sdesc.sector <= track->sector_count) {
                    uft_sector_t *sect = &track->sectors[sdesc.sector - 1];
                    sect->id.cylinder = sdesc.cylinder;
                    sect->id.head = sdesc.head;
                    sect->id.sector = sdesc.sector;
                    sect->id.size_code = sdesc.size_code;
                    sect->status = UFT_SECTOR_OK;
                    
                    sect->data = malloc(sec_size);
                    if (sect->data) {
                        if (data_len >= sec_size) {
                            memcpy(sect->data, final_data, sec_size);
                        } else {
                            memcpy(sect->data, final_data, data_len);
                            memset(sect->data + data_len, 0xE5, sec_size - data_len);
                        }
                        sect->data_size = sec_size;
                    }
                }
            }
        } else if (rec.type == APRIDISK_COMMENT && pos + rec.data_size <= size) {
            if (result && !result->comment) {
                result->comment = malloc(rec.data_size + 1);
                if (result->comment) {
                    memcpy(result->comment, data + pos, rec.data_size);
                    result->comment[rec.data_size] = '\0';
                    result->comment_len = rec.data_size;
                }
            }
        }
        
        pos += rec.data_size;
    }
    
    free(decomp_buffer);
    
    /* Fill result */
    if (result) {
        result->success = true;
        result->max_cylinder = max_cyl;
        result->max_head = max_head;
        result->max_sector = max_sect;
        result->sector_size = sector_size;
        result->total_sectors = total_sectors;
        result->deleted_sectors = deleted_sectors;
        result->rle_sectors = rle_sectors;
    }
    
    *out_disk = disk;
    return UFT_OK;
}

uft_error_t uft_apridisk_read(const char *path,
                              uft_disk_image_t **out_disk,
                              apridisk_read_result_t *result) {
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
    
    uft_error_t err = uft_apridisk_read_mem(data, size, out_disk, result);
    free(data);
    
    return err;
}

/* ============================================================================
 * Write Implementation
 * ============================================================================ */

void uft_apridisk_write_options_init(apridisk_write_options_t *opts) {
    if (!opts) return;
    memset(opts, 0, sizeof(*opts));
    opts->use_rle = true;
    opts->comment = NULL;
    opts->creator = "UFT v3.9.0";
}

uft_error_t uft_apridisk_write(const uft_disk_image_t *disk,
                               const char *path,
                               const apridisk_write_options_t *opts) {
    if (!disk || !path) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    apridisk_write_options_t default_opts;
    if (!opts) {
        uft_apridisk_write_options_init(&default_opts);
        opts = &default_opts;
    }
    
    /* Calculate max output size */
    size_t max_size = APRIDISK_HEADER_SIZE;
    max_size += 1024;  /* Comment/creator overhead */
    max_size += (size_t)disk->tracks * disk->heads * disk->sectors_per_track *
                (sizeof(apridisk_record_desc_t) + 8 + disk->bytes_per_sector);
    
    uint8_t *output = malloc(max_size);
    if (!output) {
        return UFT_ERR_MEMORY;
    }
    
    /* Write header */
    memset(output, 0, APRIDISK_HEADER_SIZE);
    memcpy(output, APRIDISK_SIGNATURE, APRIDISK_SIGNATURE_LEN);
    
    size_t out_pos = APRIDISK_HEADER_SIZE;
    
    /* Write creator record */
    if (opts->creator) {
        size_t creator_len = strlen(opts->creator);
        apridisk_record_desc_t rec = {0};
        write_le32((uint8_t*)&rec.type, APRIDISK_CREATOR);
        write_le32((uint8_t*)&rec.compression, APRIDISK_COMP_NONE);
        write_le32((uint8_t*)&rec.header_size, 16);
        write_le32((uint8_t*)&rec.data_size, creator_len);
        memcpy(output + out_pos, &rec, sizeof(rec));
        out_pos += sizeof(rec);
        memcpy(output + out_pos, opts->creator, creator_len);
        out_pos += creator_len;
    }
    
    /* Write comment record */
    if (opts->comment) {
        size_t comment_len = strlen(opts->comment);
        apridisk_record_desc_t rec = {0};
        write_le32((uint8_t*)&rec.type, APRIDISK_COMMENT);
        write_le32((uint8_t*)&rec.compression, APRIDISK_COMP_NONE);
        write_le32((uint8_t*)&rec.header_size, 16);
        write_le32((uint8_t*)&rec.data_size, comment_len);
        memcpy(output + out_pos, &rec, sizeof(rec));
        out_pos += sizeof(rec);
        memcpy(output + out_pos, opts->comment, comment_len);
        out_pos += comment_len;
    }
    
    /* Allocate compression buffer */
    uint8_t *comp_buffer = malloc(disk->bytes_per_sector * 2);
    if (!comp_buffer) {
        free(output);
        return UFT_ERR_MEMORY;
    }
    
    /* Write sector records */
    for (uint16_t t = 0; t < disk->tracks; t++) {
        for (uint8_t h = 0; h < disk->heads; h++) {
            size_t idx = t * disk->heads + h;
            uft_track_t *track = disk->track_data[idx];
            
            for (uint8_t s = 0; s < disk->sectors_per_track; s++) {
                uint8_t *sec_data = NULL;
                size_t sec_size = disk->bytes_per_sector;
                
                if (track && s < track->sector_count && track->sectors[s].data) {
                    sec_data = track->sectors[s].data;
                }
                
                /* Prepare sector descriptor */
                apridisk_sector_desc_t sdesc = {0};
                sdesc.cylinder = t;
                sdesc.head = h;
                sdesc.sector = s + 1;
                sdesc.size_code = code_from_sector_size(sec_size);
                
                /* Try compression if enabled */
                uint32_t compression = APRIDISK_COMP_NONE;
                const uint8_t *write_data = sec_data;
                size_t write_size = sec_size;
                
                if (opts->use_rle && sec_data) {
                    int comp_len = apridisk_rle_compress(sec_data, sec_size,
                                                         comp_buffer, sec_size * 2);
                    if (comp_len > 0 && (size_t)comp_len < sec_size) {
                        compression = APRIDISK_COMP_RLE;
                        write_data = comp_buffer;
                        write_size = comp_len;
                    }
                }
                
                /* Write record */
                apridisk_record_desc_t rec = {0};
                write_le32((uint8_t*)&rec.type, APRIDISK_SECTOR);
                write_le32((uint8_t*)&rec.compression, compression);
                write_le32((uint8_t*)&rec.header_size, 16 + 8);  /* Extra 8 for sector desc */
                write_le32((uint8_t*)&rec.data_size, write_size);
                
                if (out_pos + sizeof(rec) + 8 + write_size > max_size) {
                    free(comp_buffer);
                    free(output);
                    return UFT_ERR_IO;  /* Buffer overflow */
                }
                
                memcpy(output + out_pos, &rec, sizeof(rec));
                out_pos += sizeof(rec);
                memcpy(output + out_pos, &sdesc, sizeof(sdesc));
                out_pos += sizeof(sdesc);
                
                if (write_data) {
                    memcpy(output + out_pos, write_data, write_size);
                } else {
                    memset(output + out_pos, 0xE5, write_size);
                }
                out_pos += write_size;
            }
        }
    }
    
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

static bool apridisk_probe_plugin(const uint8_t *data, size_t size, 
                                  size_t file_size, int *confidence) {
    (void)file_size;
    return uft_apridisk_probe(data, size, confidence);
}

static uft_error_t apridisk_open(uft_disk_t *disk, const char *path, bool read_only) {
    (void)read_only;
    uft_disk_image_t *image = NULL;
    uft_error_t err = uft_apridisk_read(path, &image, NULL);
    if (err == UFT_OK && image) {
        disk->plugin_data = image;
        disk->geometry.cylinders = image->tracks;
        disk->geometry.heads = image->heads;
        disk->geometry.sectors = image->sectors_per_track;
        disk->geometry.sector_size = image->bytes_per_sector;
    }
    return err;
}

static void apridisk_close(uft_disk_t *disk) {
    if (disk && disk->plugin_data) {
        uft_disk_free((uft_disk_image_t*)disk->plugin_data);
        disk->plugin_data = NULL;
    }
}

static uft_error_t apridisk_read_track(uft_disk_t *disk, int cyl, int head, 
                                        uft_track_t *track) {
    uft_disk_image_t *image = (uft_disk_image_t*)disk->plugin_data;
    if (!image || !track) return UFT_ERR_INVALID_PARAM;
    
    size_t idx = cyl * image->heads + head;
    if (idx >= (size_t)(image->tracks * image->heads)) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    uft_track_t *src = image->track_data[idx];
    if (!src) return UFT_ERR_INVALID_PARAM;
    
    /* Copy track data */
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

const uft_format_plugin_t uft_format_plugin_apridisk = {
    .name = "ApriDisk",
    .description = "ApriDisk Image Format",
    .extensions = "dsk",
    .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE,
    .probe = apridisk_probe_plugin,
    .open = apridisk_open,
    .close = apridisk_close,
    .read_track = apridisk_read_track,
};

UFT_REGISTER_FORMAT_PLUGIN(apridisk)
