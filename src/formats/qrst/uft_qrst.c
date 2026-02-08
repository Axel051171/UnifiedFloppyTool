/**
 * @file uft_qrst.c
 * @brief QRST (Compaq Quick Release Sector Transfer) format implementation
 * @version 3.9.0
 * 
 * QRST format with RLE compression support.
 * Reference: libdsk drvqrst.c by John Elliott
 */

#include "uft/formats/uft_qrst.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

static uint16_t read_le16(const uint8_t *p) {
    return p[0] | (p[1] << 8);
}

static uint32_t read_le32(const uint8_t *p) {
    return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
}

static void write_le16(uint8_t *p, uint16_t v) {
    p[0] = v & 0xFF;
    p[1] = (v >> 8) & 0xFF;
}

static void write_le32(uint8_t *p, uint32_t v) {
    p[0] = v & 0xFF;
    p[1] = (v >> 8) & 0xFF;
    p[2] = (v >> 16) & 0xFF;
    p[3] = (v >> 24) & 0xFF;
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
 * RLE Compression/Decompression
 * 
 * QRST RLE format:
 * - 0x00 count value: repeat 'value' count times
 * - Other: literal byte
 * ============================================================================ */

int qrst_rle_decompress(const uint8_t *input, size_t input_size,
                        uint8_t *output, size_t output_size) {
    size_t in_pos = 0;
    size_t out_pos = 0;
    
    while (in_pos < input_size && out_pos < output_size) {
        uint8_t byte = input[in_pos++];
        
        if (byte == 0x00 && in_pos + 2 <= input_size) {
            /* RLE sequence: 0x00 count value */
            uint8_t count = input[in_pos++];
            uint8_t value = input[in_pos++];
            
            for (int i = 0; i < count && out_pos < output_size; i++) {
                output[out_pos++] = value;
            }
        } else {
            /* Literal byte */
            output[out_pos++] = byte;
        }
    }
    
    return (int)out_pos;
}

int qrst_rle_compress(const uint8_t *input, size_t input_size,
                      uint8_t *output, size_t output_capacity) {
    size_t in_pos = 0;
    size_t out_pos = 0;
    
    while (in_pos < input_size) {
        /* Check for a run */
        uint8_t run_byte = input[in_pos];
        size_t run_len = 1;
        
        while (in_pos + run_len < input_size &&
               input[in_pos + run_len] == run_byte &&
               run_len < 255) {
            run_len++;
        }
        
        if (run_len >= 4 || (run_byte == 0x00 && run_len >= 1)) {
            /* Encode as RLE */
            if (out_pos + 3 > output_capacity) return -1;
            output[out_pos++] = 0x00;
            output[out_pos++] = (uint8_t)run_len;
            output[out_pos++] = run_byte;
            in_pos += run_len;
        } else {
            /* Literal byte */
            if (out_pos + 1 > output_capacity) return -1;
            
            /* If byte is 0x00, encode as RLE to avoid confusion */
            if (run_byte == 0x00) {
                if (out_pos + 3 > output_capacity) return -1;
                output[out_pos++] = 0x00;
                output[out_pos++] = 1;
                output[out_pos++] = 0x00;
            } else {
                output[out_pos++] = run_byte;
            }
            in_pos++;
        }
    }
    
    /* Only return if compression is beneficial */
    if (out_pos >= input_size) {
        return -1;
    }
    
    return (int)out_pos;
}

/* ============================================================================
 * Header Validation
 * ============================================================================ */

bool uft_qrst_validate_header(const qrst_header_t *header) {
    if (!header) return false;
    return memcmp(header->signature, QRST_SIGNATURE, QRST_SIGNATURE_LEN) == 0;
}

bool uft_qrst_probe(const uint8_t *data, size_t size, int *confidence) {
    if (!data || size < QRST_HEADER_SIZE) return false;
    
    if (memcmp(data, QRST_SIGNATURE, QRST_SIGNATURE_LEN) == 0) {
        if (confidence) *confidence = 95;
        return true;
    }
    
    return false;
}

/* ============================================================================
 * Read Implementation
 * ============================================================================ */

uft_error_t uft_qrst_read_mem(const uint8_t *data, size_t size,
                              uft_disk_image_t **out_disk,
                              qrst_read_result_t *result) {
    if (!data || !out_disk || size < QRST_HEADER_SIZE) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    /* Initialize result */
    if (result) {
        memset(result, 0, sizeof(*result));
    }
    
    /* Validate header */
    const qrst_header_t *header = (const qrst_header_t *)data;
    if (!uft_qrst_validate_header(header)) {
        if (result) {
            result->error = UFT_ERR_FORMAT;
            result->error_detail = "Invalid QRST signature";
        }
        return UFT_ERR_FORMAT;
    }
    
    /* Extract geometry */
    uint16_t cylinders = read_le16((const uint8_t*)&header->cylinders);
    uint16_t heads = read_le16((const uint8_t*)&header->heads);
    uint16_t sectors = read_le16((const uint8_t*)&header->sectors);
    uint16_t sector_size = read_le16((const uint8_t*)&header->sector_size);
    
    if (cylinders == 0 || heads == 0 || sectors == 0 || sector_size == 0) {
        if (result) {
            result->error = UFT_ERR_FORMAT;
            result->error_detail = "Invalid QRST geometry";
        }
        return UFT_ERR_FORMAT;
    }
    
    if (result) {
        result->cylinders = cylinders;
        result->heads = heads;
        result->sectors = sectors;
        result->sector_size = sector_size;
    }
    
    /* Allocate disk image */
    uft_disk_image_t *disk = uft_disk_alloc(cylinders, heads);
    if (!disk) {
        return UFT_ERR_MEMORY;
    }
    
    disk->format = UFT_FMT_RAW;
    snprintf(disk->format_name, sizeof(disk->format_name), "QRST");
    disk->sectors_per_track = sectors;
    disk->bytes_per_sector = sector_size;
    
    /* Allocate tracks */
    for (uint16_t c = 0; c < cylinders; c++) {
        for (uint16_t h = 0; h < heads; h++) {
            size_t idx = c * heads + h;
            uft_track_t *track = uft_track_alloc(sectors, 0);
            if (!track) {
                uft_disk_free(disk);
                return UFT_ERR_MEMORY;
            }
            track->cylinder = c;
            track->head = h;
            track->encoding = UFT_ENC_MFM;
            disk->track_data[idx] = track;
        }
    }
    
    /* Read track data */
    size_t pos = QRST_HEADER_SIZE;
    size_t track_size = (size_t)sectors * sector_size;
    uint8_t *decomp_buffer = malloc(track_size);
    uint32_t total_tracks = 0, compressed_tracks = 0;
    
    if (!decomp_buffer) {
        uft_disk_free(disk);
        return UFT_ERR_MEMORY;
    }
    
    while (pos + sizeof(qrst_track_header_t) <= size) {
        qrst_track_header_t thdr;
        memcpy(&thdr, data + pos, sizeof(thdr));
        pos += sizeof(thdr);
        
        uint16_t cyl = read_le16((uint8_t*)&thdr.cylinder);
        uint8_t head = thdr.head;
        uint8_t compressed = thdr.compressed;
        uint32_t data_size = read_le32((uint8_t*)&thdr.data_size);
        
        if (pos + data_size > size) break;
        
        if (cyl >= cylinders || head >= heads) {
            pos += data_size;
            continue;
        }
        
        total_tracks++;
        if (compressed) compressed_tracks++;
        
        size_t idx = cyl * heads + head;
        uft_track_t *track = disk->track_data[idx];
        
        const uint8_t *track_data = data + pos;
        uint8_t *final_data = (uint8_t*)track_data;
        size_t final_size = data_size;
        
        /* Decompress if needed */
        if (compressed) {
            int decomp_len = qrst_rle_decompress(track_data, data_size,
                                                  decomp_buffer, track_size);
            if (decomp_len > 0) {
                final_data = decomp_buffer;
                final_size = decomp_len;
            }
        }
        
        /* Copy sector data */
        uint8_t size_code = code_from_size(sector_size);
        size_t data_pos = 0;
        
        for (uint16_t s = 0; s < sectors; s++) {
            uft_sector_t *sect = &track->sectors[s];
            sect->id.cylinder = cyl;
            sect->id.head = head;
            sect->id.sector = s + 1;
            sect->id.size_code = size_code;
            sect->status = UFT_SECTOR_OK;
            
            sect->data = malloc(sector_size);
            sect->data_size = sector_size;
            
            if (sect->data) {
                if (data_pos + sector_size <= final_size) {
                    memcpy(sect->data, final_data + data_pos, sector_size);
                } else {
                    memset(sect->data, 0xE5, sector_size);
                }
            }
            data_pos += sector_size;
            track->sector_count++;
        }
        
        pos += data_size;
    }
    
    free(decomp_buffer);
    
    if (result) {
        result->success = true;
        result->total_tracks = total_tracks;
        result->compressed_tracks = compressed_tracks;
    }
    
    *out_disk = disk;
    return UFT_OK;
}

uft_error_t uft_qrst_read(const char *path,
                          uft_disk_image_t **out_disk,
                          qrst_read_result_t *result) {
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
    
    uft_error_t err = uft_qrst_read_mem(data, size, out_disk, result);
    free(data);
    
    return err;
}

/* ============================================================================
 * Write Implementation
 * ============================================================================ */

void uft_qrst_write_options_init(qrst_write_options_t *opts) {
    if (!opts) return;
    memset(opts, 0, sizeof(*opts));
    opts->use_compression = true;
}

uft_error_t uft_qrst_write(const uft_disk_image_t *disk,
                           const char *path,
                           const qrst_write_options_t *opts) {
    if (!disk || !path) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    qrst_write_options_t default_opts;
    if (!opts) {
        uft_qrst_write_options_init(&default_opts);
        opts = &default_opts;
    }
    
    /* Calculate max output size */
    size_t track_size = (size_t)disk->sectors_per_track * disk->bytes_per_sector;
    size_t max_size = QRST_HEADER_SIZE +
                      (size_t)disk->tracks * disk->heads * 
                      (sizeof(qrst_track_header_t) + track_size + 256);
    
    uint8_t *output = malloc(max_size);
    if (!output) {
        return UFT_ERR_MEMORY;
    }
    
    /* Build header */
    qrst_header_t *header = (qrst_header_t *)output;
    memset(header, 0, sizeof(*header));
    memcpy(header->signature, QRST_SIGNATURE, QRST_SIGNATURE_LEN);
    write_le16((uint8_t*)&header->version, 1);
    write_le16((uint8_t*)&header->cylinders, disk->tracks);
    write_le16((uint8_t*)&header->heads, disk->heads);
    write_le16((uint8_t*)&header->sectors, disk->sectors_per_track);
    write_le16((uint8_t*)&header->sector_size, disk->bytes_per_sector);
    header->compression = opts->use_compression ? QRST_COMP_RLE : QRST_COMP_NONE;
    
    size_t out_pos = QRST_HEADER_SIZE;
    
    /* Allocate buffers */
    uint8_t *track_buffer = malloc(track_size);
    uint8_t *comp_buffer = malloc(track_size + 256);
    
    if (!track_buffer || !comp_buffer) {
        free(track_buffer);
        free(comp_buffer);
        free(output);
        return UFT_ERR_MEMORY;
    }
    
    /* Write track data */
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
            
            /* Try compression */
            const uint8_t *write_data = track_buffer;
            size_t write_size = track_size;
            uint8_t compressed = 0;
            
            if (opts->use_compression) {
                int comp_len = qrst_rle_compress(track_buffer, track_size,
                                                  comp_buffer, track_size + 256);
                if (comp_len > 0 && (size_t)comp_len < track_size) {
                    write_data = comp_buffer;
                    write_size = comp_len;
                    compressed = 1;
                }
            }
            
            /* Write track header */
            qrst_track_header_t thdr = {0};
            write_le16((uint8_t*)&thdr.cylinder, c);
            thdr.head = h;
            thdr.compressed = compressed;
            write_le32((uint8_t*)&thdr.data_size, write_size);
            
            if (out_pos + sizeof(thdr) + write_size > max_size) {
                free(track_buffer);
                free(comp_buffer);
                free(output);
                return UFT_ERR_IO;
            }
            
            memcpy(output + out_pos, &thdr, sizeof(thdr));
            out_pos += sizeof(thdr);
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

static bool qrst_probe_plugin(const uint8_t *data, size_t size,
                              size_t file_size, int *confidence) {
    (void)file_size;
    return uft_qrst_probe(data, size, confidence);
}

static uft_error_t qrst_open(uft_disk_t *disk, const char *path, bool read_only) {
    (void)read_only;
    uft_disk_image_t *image = NULL;
    uft_error_t err = uft_qrst_read(path, &image, NULL);
    if (err == UFT_OK && image) {
        disk->plugin_data = image;
        disk->geometry.cylinders = image->tracks;
        disk->geometry.heads = image->heads;
        disk->geometry.sectors = image->sectors_per_track;
        disk->geometry.sector_size = image->bytes_per_sector;
    }
    return err;
}

static void qrst_close(uft_disk_t *disk) {
    if (disk && disk->plugin_data) {
        uft_disk_free((uft_disk_image_t*)disk->plugin_data);
        disk->plugin_data = NULL;
    }
}

static uft_error_t qrst_read_track(uft_disk_t *disk, int cyl, int head,
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

const uft_format_plugin_t uft_format_plugin_qrst = {
    .name = "QRST",
    .description = "Compaq Quick Release Sector Transfer",
    .extensions = "qrst",
    .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE,
    .probe = qrst_probe_plugin,
    .open = qrst_open,
    .close = qrst_close,
    .read_track = qrst_read_track,
};

UFT_REGISTER_FORMAT_PLUGIN(qrst)
