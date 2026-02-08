/**
 * @file uft_x68k.c
 * @brief Sharp X68000 disk image format implementation
 * @version 3.9.0
 * 
 * Supports X68000 XDF and DIM formats.
 * Reference: x68000-floppy-tools by leaded-solder
 */

#include "uft/formats/uft_x68k.h"
#include "uft/core/uft_unified_types.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

static uint8_t code_from_size(uint16_t size) {
    switch (size) {
        case 128:  return 0;
        case 256:  return 1;
        case 512:  return 2;
        case 1024: return 3;
        case 2048: return 4;
        default:   return 2;
    }
}

/* ============================================================================
 * Media Type Detection
 * ============================================================================ */

x68k_media_type_t x68k_detect_media_type(size_t image_size) {
    /* Check for DIM format (with header) */
    if (image_size == DIM_HEADER_SIZE + X68K_2HD_SIZE) {
        return X68K_MEDIA_2HD;
    }
    if (image_size == DIM_HEADER_SIZE + X68K_2DD_SIZE) {
        return X68K_MEDIA_2DD;
    }
    
    /* Check for raw XDF format */
    if (image_size == X68K_2HD_SIZE) {
        return X68K_MEDIA_2HD;
    }
    if (image_size == X68K_2DD_SIZE) {
        return X68K_MEDIA_2DD;
    }
    
    /* 1.44MB format (rare) */
    if (image_size == 80 * 2 * 18 * 512 || 
        image_size == DIM_HEADER_SIZE + 80 * 2 * 18 * 512) {
        return X68K_MEDIA_2HQ;
    }
    
    /* Default to 2HD for unknown sizes */
    return X68K_MEDIA_2HD;
}

bool uft_x68k_xdf_probe(const uint8_t *data, size_t size, int *confidence) {
    if (!data) return false;
    
    /* Check for exact X68000 sizes */
    if (size == X68K_2HD_SIZE) {
        /* Check for Human68k boot signature */
        if (data[0] == 0x60 && data[1] == 0x1C) {
            if (confidence) *confidence = 85;
            return true;
        }
        if (confidence) *confidence = 60;
        return true;
    }
    
    if (size == X68K_2DD_SIZE) {
        if (confidence) *confidence = 50;
        return true;
    }
    
    return false;
}

bool uft_x68k_dim_probe(const uint8_t *data, size_t size, int *confidence) {
    if (!data || size < DIM_HEADER_SIZE) return false;
    
    /* DIM has header + data */
    size_t data_size = size - DIM_HEADER_SIZE;
    
    if (data_size == X68K_2HD_SIZE || data_size == X68K_2DD_SIZE) {
        /* Check media type byte */
        if (data[0] == 0x00 || data[0] == 0x01 || data[0] == 0x02) {
            if (confidence) *confidence = 75;
            return true;
        }
    }
    
    return false;
}

/* ============================================================================
 * Read Implementation
 * ============================================================================ */

uft_error_t uft_x68k_read_mem(const uint8_t *data, size_t size,
                              uft_disk_image_t **out_disk,
                              x68k_read_result_t *result) {
    if (!data || !out_disk) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    /* Initialize result */
    if (result) {
        memset(result, 0, sizeof(*result));
    }
    
    /* Detect format */
    bool is_dim = false;
    const uint8_t *disk_data = data;
    size_t disk_size = size;
    
    /* Check if DIM format (has header) */
    if (size > DIM_HEADER_SIZE) {
        size_t data_only = size - DIM_HEADER_SIZE;
        if (data_only == X68K_2HD_SIZE || data_only == X68K_2DD_SIZE ||
            data_only == 80 * 2 * 18 * 512) {
            is_dim = true;
            disk_data = data + DIM_HEADER_SIZE;
            disk_size = data_only;
        }
    }
    
    /* Determine geometry */
    x68k_media_type_t media_type = x68k_detect_media_type(size);
    uint8_t cyls, heads, sectors;
    uint16_t sector_size;
    
    switch (media_type) {
        case X68K_MEDIA_2HD:
            cyls = X68K_2HD_CYLS;
            heads = X68K_2HD_HEADS;
            sectors = X68K_2HD_SECTORS;
            sector_size = X68K_2HD_SECSIZE;
            break;
        case X68K_MEDIA_2DD:
            cyls = X68K_2DD_CYLS;
            heads = X68K_2DD_HEADS;
            sectors = X68K_2DD_SECTORS;
            sector_size = X68K_2DD_SECSIZE;
            break;
        case X68K_MEDIA_2HQ:
            cyls = 80;
            heads = 2;
            sectors = 18;
            sector_size = 512;
            break;
        default:
            return UFT_ERR_FORMAT;
    }
    
    if (result) {
        result->media_type = media_type;
        result->cylinders = cyls;
        result->heads = heads;
        result->sectors = sectors;
        result->sector_size = sector_size;
        result->is_dim = is_dim;
        result->is_xdf = !is_dim;
        result->image_size = size;
    }
    
    /* Allocate disk image */
    uft_disk_image_t *disk = uft_disk_alloc(cyls, heads);
    if (!disk) {
        return UFT_ERR_MEMORY;
    }
    
    disk->format = UFT_FMT_RAW;
    snprintf(disk->format_name, sizeof(disk->format_name), 
             is_dim ? "X68K-DIM" : "X68K-XDF");
    disk->sectors_per_track = sectors;
    disk->bytes_per_sector = sector_size;
    
    /* Read track data */
    size_t data_pos = 0;
    uint8_t size_code = code_from_size(sector_size);
    
    for (uint8_t c = 0; c < cyls; c++) {
        for (uint8_t h = 0; h < heads; h++) {
            size_t idx = c * heads + h;
            
            uft_track_t *track = uft_track_alloc(sectors, 0);
            if (!track) {
                uft_disk_free(disk);
                return UFT_ERR_MEMORY;
            }
            
            track->track_num = c;
            track->head = h;
            track->encoding = UFT_ENC_MFM;
            
            for (uint8_t s = 0; s < sectors; s++) {
                uft_sector_t *sect = &track->sectors[s];
                sect->id.cylinder = c;
                sect->id.head = h;
                sect->id.sector = s + 1;
                sect->id.size_code = size_code;
                sect->status = UFT_SECTOR_OK;
                
                sect->data = malloc(sector_size);
                sect->data_size = sector_size;
                
                if (sect->data) {
                    if (data_pos + sector_size <= disk_size) {
                        memcpy(sect->data, disk_data + data_pos, sector_size);
                    } else {
                        memset(sect->data, 0xE5, sector_size);
                    }
                }
                data_pos += sector_size;
                
                track->sector_count++;
            }
            
            disk->track_data[idx] = track;
        }
    }
    
    /* Check for Human68k filesystem */
    if (result && disk_size >= sector_size) {
        /* Human68k boot sector starts with 0x60 0x1C (BRA.S instruction) */
        if (disk_data[0] == 0x60 && disk_data[1] == 0x1C) {
            result->has_human68k = true;
        }
    }
    
    if (result) {
        result->success = true;
    }
    
    *out_disk = disk;
    return UFT_OK;
}

uft_error_t uft_x68k_read(const char *path,
                          uft_disk_image_t **out_disk,
                          x68k_read_result_t *result) {
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
    
    uft_error_t err = uft_x68k_read_mem(data, size, out_disk, result);
    free(data);
    
    return err;
}

/* ============================================================================
 * Write Implementation
 * ============================================================================ */

void uft_x68k_write_options_init(x68k_write_options_t *opts) {
    if (!opts) return;
    memset(opts, 0, sizeof(*opts));
    opts->write_dim_header = false;
    opts->media_type = X68K_MEDIA_2HD;
}

uft_error_t uft_x68k_write(const uft_disk_image_t *disk,
                           const char *path,
                           const x68k_write_options_t *opts) {
    if (!disk || !path) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    x68k_write_options_t default_opts;
    if (!opts) {
        uft_x68k_write_options_init(&default_opts);
        opts = &default_opts;
    }
    
    /* Calculate sizes */
    size_t track_size = (size_t)disk->sectors_per_track * disk->bytes_per_sector;
    size_t data_size = (size_t)disk->tracks * disk->heads * track_size;
    size_t header_size = opts->write_dim_header ? DIM_HEADER_SIZE : 0;
    size_t total_size = header_size + data_size;
    
    uint8_t *output = malloc(total_size);
    if (!output) {
        return UFT_ERR_MEMORY;
    }
    
    /* Write DIM header if requested */
    if (opts->write_dim_header) {
        dim_header_t *header = (dim_header_t *)output;
        memset(header, 0, sizeof(*header));
        header->media_type = (uint8_t)opts->media_type;
        
        /* Set track map (all tracks used) */
        memset(header->track_map, 0x01, 
               disk->tracks < 154 ? disk->tracks : 154);
    }
    
    /* Write track data */
    uint8_t *track_data = output + header_size;
    size_t data_pos = 0;
    
    for (uint16_t c = 0; c < disk->tracks; c++) {
        for (uint8_t h = 0; h < disk->heads; h++) {
            size_t idx = c * disk->heads + h;
            uft_track_t *track = disk->track_data[idx];
            
            for (uint8_t s = 0; s < disk->sectors_per_track; s++) {
                if (track && s < track->sector_count && track->sectors[s].data) {
                    memcpy(track_data + data_pos, track->sectors[s].data,
                           disk->bytes_per_sector);
                } else {
                    memset(track_data + data_pos, 0xE5, disk->bytes_per_sector);
                }
                data_pos += disk->bytes_per_sector;
            }
        }
    }
    
    /* Write file */
    FILE *fp = fopen(path, "wb");
    if (!fp) {
        free(output);
        return UFT_ERR_IO;
    }
    
    size_t written = fwrite(output, 1, total_size, fp);
    fclose(fp);
    free(output);
    
    if (written != total_size) {
        return UFT_ERR_IO;
    }
    
    return UFT_OK;
}

/* ============================================================================
 * Conversion Functions
 * ============================================================================ */

uft_error_t uft_x68k_dim_to_xdf(const uint8_t *dim_data, size_t dim_size,
                                uint8_t **out_xdf, size_t *out_size) {
    if (!dim_data || !out_xdf || !out_size) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    if (dim_size <= DIM_HEADER_SIZE) {
        return UFT_ERR_FORMAT;
    }
    
    size_t xdf_size = dim_size - DIM_HEADER_SIZE;
    uint8_t *xdf = malloc(xdf_size);
    if (!xdf) {
        return UFT_ERR_MEMORY;
    }
    
    memcpy(xdf, dim_data + DIM_HEADER_SIZE, xdf_size);
    
    *out_xdf = xdf;
    *out_size = xdf_size;
    return UFT_OK;
}

uft_error_t uft_x68k_xdf_to_dim(const uint8_t *xdf_data, size_t xdf_size,
                                x68k_media_type_t media_type,
                                uint8_t **out_dim, size_t *out_size) {
    if (!xdf_data || !out_dim || !out_size) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    size_t dim_size = DIM_HEADER_SIZE + xdf_size;
    uint8_t *dim = malloc(dim_size);
    if (!dim) {
        return UFT_ERR_MEMORY;
    }
    
    /* Create header */
    dim_header_t *header = (dim_header_t *)dim;
    memset(header, 0, sizeof(*header));
    header->media_type = (uint8_t)media_type;
    
    /* Calculate tracks based on media type */
    uint8_t tracks;
    switch (media_type) {
        case X68K_MEDIA_2HD: tracks = X68K_2HD_CYLS; break;
        case X68K_MEDIA_2DD: tracks = X68K_2DD_CYLS; break;
        case X68K_MEDIA_2HQ: tracks = 80; break;
        default: tracks = 80; break;
    }
    
    /* Set track map */
    memset(header->track_map, 0x01, tracks < 154 ? tracks : 154);
    
    /* Copy data */
    memcpy(dim + DIM_HEADER_SIZE, xdf_data, xdf_size);
    
    *out_dim = dim;
    *out_size = dim_size;
    return UFT_OK;
}

/* ============================================================================
 * Human68k Filesystem Support
 * ============================================================================ */

bool uft_x68k_has_human68k(const uft_disk_image_t *disk) {
    if (!disk || !disk->track_data || !disk->track_data[0]) {
        return false;
    }
    
    uft_track_t *track0 = disk->track_data[0];
    if (track0->sector_count == 0 || !track0->sectors[0].data) {
        return false;
    }
    
    /* Human68k boot sector signature: 0x60 0x1C (BRA.S +0x1C) */
    uint8_t *boot = track0->sectors[0].data;
    return (boot[0] == 0x60 && boot[1] == 0x1C);
}

const char* uft_x68k_get_volume_label(const uft_disk_image_t *disk,
                                       char *buffer, size_t buffer_size) {
    if (!disk || !buffer || buffer_size < 12) {
        return NULL;
    }
    
    if (!uft_x68k_has_human68k(disk)) {
        return NULL;
    }
    
    uft_track_t *track0 = disk->track_data[0];
    uint8_t *boot = track0->sectors[0].data;
    
    /* Volume label at offset 0x02 (11 bytes, space-padded) */
    memcpy(buffer, boot + 0x02, 11);
    buffer[11] = '\0';
    
    /* Trim trailing spaces */
    for (int i = 10; i >= 0 && buffer[i] == ' '; i--) {
        buffer[i] = '\0';
    }
    
    return buffer;
}

/* ============================================================================
 * Format Plugin Registration
 * ============================================================================ */

static bool x68k_probe_plugin(const uint8_t *data, size_t size,
                              size_t file_size, int *confidence) {
    (void)file_size;
    
    /* Try DIM format first */
    if (uft_x68k_dim_probe(data, size, confidence)) {
        return true;
    }
    
    /* Then try raw XDF */
    return uft_x68k_xdf_probe(data, size, confidence);
}

static uft_error_t x68k_open(uft_disk_t *disk, const char *path, bool read_only) {
    (void)read_only;
    uft_disk_image_t *image = NULL;
    uft_error_t err = uft_x68k_read(path, &image, NULL);
    if (err == UFT_OK && image) {
        disk->plugin_data = image;
        disk->geometry.cylinders = image->tracks;
        disk->geometry.heads = image->heads;
        disk->geometry.sectors = image->sectors_per_track;
        disk->geometry.sector_size = image->bytes_per_sector;
    }
    return err;
}

static void x68k_close(uft_disk_t *disk) {
    if (disk && disk->plugin_data) {
        uft_disk_free((uft_disk_image_t*)disk->plugin_data);
        disk->plugin_data = NULL;
    }
}

static uft_error_t x68k_read_track(uft_disk_t *disk, int cyl, int head,
                                    uft_track_t *track) {
    uft_disk_image_t *image = (uft_disk_image_t*)disk->plugin_data;
    if (!image || !track) return UFT_ERR_INVALID_PARAM;
    
    size_t idx = cyl * image->heads + head;
    if (idx >= (size_t)(image->tracks * image->heads)) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    uft_track_t *src = image->track_data[idx];
    if (!src) return UFT_ERR_INVALID_PARAM;
    
    track->track_num = cyl;
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

const uft_format_plugin_t uft_format_plugin_x68k = {
    .name = "X68000",
    .description = "Sharp X68000 XDF/DIM Image",
    .extensions = "xdf,dim,2hd",
    .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE,
    .probe = x68k_probe_plugin,
    .open = x68k_open,
    .close = x68k_close,
    .read_track = x68k_read_track,
};

UFT_REGISTER_FORMAT_PLUGIN(x68k)
