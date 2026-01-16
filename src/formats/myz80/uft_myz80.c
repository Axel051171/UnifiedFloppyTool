/**
 * @file uft_myz80.c
 * @brief MYZ80 Hard Drive Image format implementation
 * @version 3.9.0
 * 
 * MYZ80 CP/M emulator hard drive format with 256-byte header.
 * Reference: libdsk drvmyz80.c
 */

#include "uft/formats/uft_myz80.h"
#include "uft/uft_format_common.h"
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

static uint8_t size_code(uint16_t size) {
    switch (size) {
        case 128:  return 0;
        case 256:  return 1;
        case 512:  return 2;
        case 1024: return 3;
        default:   return 0;
    }
}

/* ============================================================================
 * Options Initialization
 * ============================================================================ */

void uft_myz80_read_options_init(myz80_read_options_t *opts) {
    if (!opts) return;
    memset(opts, 0, sizeof(*opts));
    opts->ignore_header = false;
}

void uft_myz80_write_options_init(myz80_write_options_t *opts) {
    if (!opts) return;
    memset(opts, 0, sizeof(*opts));
    strcpy(opts->label, "UFT DISK");
}

/* ============================================================================
 * Header Validation
 * ============================================================================ */

bool uft_myz80_validate_header(const myz80_header_t *header) {
    if (!header) return false;
    
    /* Check magic */
    if (memcmp(header->magic, MYZ80_MAGIC, MYZ80_MAGIC_LEN) != 0) {
        return false;
    }
    
    /* Sanity check geometry */
    uint16_t cyls = read_le16((const uint8_t*)&header->cylinders);
    uint16_t secsize = read_le16((const uint8_t*)&header->sector_size);
    
    if (cyls == 0 || cyls > 1024) return false;
    if (header->heads == 0 || header->heads > 16) return false;
    if (header->sectors == 0 || header->sectors > 255) return false;
    if (secsize != 128 && secsize != 256 && secsize != 512 && secsize != 1024) {
        return false;
    }
    
    return true;
}

bool uft_myz80_probe(const uint8_t *data, size_t size, int *confidence) {
    if (!data || size < MYZ80_HEADER_SIZE) return false;
    
    const myz80_header_t *header = (const myz80_header_t *)data;
    
    if (uft_myz80_validate_header(header)) {
        if (confidence) *confidence = 90;
        return true;
    }
    
    /* Check if it could be headerless MYZ80 by size */
    /* Standard 8" SSSD: 77 * 1 * 26 * 128 = 256,256 bytes */
    /* Standard 8" DSDD: 77 * 2 * 26 * 256 = 1,025,024 bytes */
    if (size == 256256 || size == 256256 + MYZ80_HEADER_SIZE ||
        size == 1025024 || size == 1025024 + MYZ80_HEADER_SIZE) {
        if (confidence) *confidence = 30;
        return true;
    }
    
    return false;
}

/* ============================================================================
 * Read Implementation
 * ============================================================================ */

uft_error_t uft_myz80_read_mem(const uint8_t *data, size_t size,
                               uft_disk_image_t **out_disk,
                               const myz80_read_options_t *opts,
                               myz80_read_result_t *result) {
    if (!data || !out_disk || size < MYZ80_HEADER_SIZE) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    /* Initialize result */
    if (result) {
        memset(result, 0, sizeof(*result));
        result->image_size = size;
    }
    
    /* Check header */
    const myz80_header_t *header = (const myz80_header_t *)data;
    bool has_header = uft_myz80_validate_header(header);
    
    uint16_t cylinders, sector_size;
    uint8_t heads, sectors, first_sector;
    const uint8_t *disk_data;
    size_t disk_data_size;
    
    if (has_header && !(opts && opts->ignore_header)) {
        /* Use header geometry */
        cylinders = read_le16((const uint8_t*)&header->cylinders);
        heads = header->heads;
        sectors = header->sectors;
        sector_size = read_le16((const uint8_t*)&header->sector_size);
        first_sector = header->first_sector ? header->first_sector : 1;
        
        disk_data = data + MYZ80_HEADER_SIZE;
        disk_data_size = size - MYZ80_HEADER_SIZE;
        
        if (result) {
            result->has_valid_header = true;
            memcpy(result->label, header->label, sizeof(header->label));
            memcpy(result->comment, header->comment, sizeof(header->comment));
        }
    } else {
        /* Guess geometry from file size */
        disk_data = data;
        disk_data_size = size;
        first_sector = 1;
        
        /* Try common CP/M geometries */
        if (size == 256256 || size == 256256 + MYZ80_HEADER_SIZE) {
            /* 8" SSSD */
            cylinders = 77;
            heads = 1;
            sectors = 26;
            sector_size = 128;
            if (size == 256256 + MYZ80_HEADER_SIZE) {
                disk_data = data + MYZ80_HEADER_SIZE;
                disk_data_size = 256256;
            }
        } else if (size == 1025024 || size == 1025024 + MYZ80_HEADER_SIZE) {
            /* 8" DSDD */
            cylinders = 77;
            heads = 2;
            sectors = 26;
            sector_size = 256;
            if (size == 1025024 + MYZ80_HEADER_SIZE) {
                disk_data = data + MYZ80_HEADER_SIZE;
                disk_data_size = 1025024;
            }
        } else {
            /* Default to 77/2/26/128 */
            cylinders = MYZ80_DEFAULT_CYLINDERS;
            heads = MYZ80_DEFAULT_HEADS;
            sectors = MYZ80_DEFAULT_SECTORS;
            sector_size = MYZ80_DEFAULT_SECSIZE;
        }
        
        if (result) {
            result->has_valid_header = false;
        }
    }
    
    if (result) {
        result->cylinders = cylinders;
        result->heads = heads;
        result->sectors = sectors;
        result->sector_size = sector_size;
    }
    
    /* Create disk image */
    uft_disk_image_t *disk = uft_disk_alloc(cylinders, heads);
    if (!disk) {
        return UFT_ERR_MEMORY;
    }
    
    disk->format = UFT_FMT_RAW;
    snprintf(disk->format_name, sizeof(disk->format_name), "MYZ80");
    disk->sectors_per_track = sectors;
    disk->bytes_per_sector = sector_size;
    
    /* Read disk data */
    size_t data_pos = 0;
    uint8_t sz_code = size_code(sector_size);
    
    for (uint16_t c = 0; c < cylinders; c++) {
        for (uint8_t h = 0; h < heads; h++) {
            size_t idx = c * heads + h;
            
            uft_track_t *track = uft_track_alloc(sectors, 0);
            if (!track) {
                uft_disk_free(disk);
                return UFT_ERR_MEMORY;
            }
            
            track->track_num = c;
            track->head = h;
            track->encoding = UFT_ENC_FM;  /* CP/M typically uses FM */
            
            for (uint8_t s = 0; s < sectors; s++) {
                uft_sector_t *sect = &track->sectors[s];
                sect->id.cylinder = c;
                sect->id.head = h;
                sect->id.sector = s + first_sector;
                sect->id.size_code = sz_code;
                sect->status = UFT_SECTOR_OK;
                
                sect->data = malloc(sector_size);
                sect->data_size = sector_size;
                
                if (sect->data) {
                    if (data_pos + sector_size <= disk_data_size) {
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
    
    if (result) {
        result->success = true;
    }
    
    *out_disk = disk;
    return UFT_OK;
}

uft_error_t uft_myz80_read(const char *path,
                           uft_disk_image_t **out_disk,
                           const myz80_read_options_t *opts,
                           myz80_read_result_t *result) {
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
    
    uft_error_t err = uft_myz80_read_mem(data, size, out_disk, opts, result);
    free(data);
    
    return err;
}

/* ============================================================================
 * Write Implementation
 * ============================================================================ */

uft_error_t uft_myz80_write(const uft_disk_image_t *disk,
                            const char *path,
                            const myz80_write_options_t *opts) {
    if (!disk || !path) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    /* Calculate data size */
    size_t disk_data_size = (size_t)disk->tracks * disk->heads *
                            disk->sectors_per_track * disk->bytes_per_sector;
    size_t total_size = MYZ80_HEADER_SIZE + disk_data_size;
    
    uint8_t *output = malloc(total_size);
    if (!output) {
        return UFT_ERR_MEMORY;
    }
    memset(output, 0, total_size);
    
    /* Build header */
    myz80_header_t *header = (myz80_header_t *)output;
    memcpy(header->magic, MYZ80_MAGIC, MYZ80_MAGIC_LEN);
    header->version = 1;
    header->flags = 0;
    write_le16((uint8_t*)&header->cylinders, disk->tracks);
    header->heads = disk->heads;
    header->sectors = disk->sectors_per_track;
    write_le16((uint8_t*)&header->sector_size, disk->bytes_per_sector);
    header->first_sector = 1;
    
    if (opts) {
        strncpy(header->label, opts->label, sizeof(header->label) - 1);
        strncpy(header->comment, opts->comment, sizeof(header->comment) - 1);
    } else {
        strcpy(header->label, "UFT DISK");
    }
    
    /* Write disk data */
    uint8_t *data_ptr = output + MYZ80_HEADER_SIZE;
    
    for (uint16_t c = 0; c < disk->tracks; c++) {
        for (uint8_t h = 0; h < disk->heads; h++) {
            size_t idx = c * disk->heads + h;
            uft_track_t *track = disk->track_data[idx];
            
            for (uint8_t s = 0; s < disk->sectors_per_track; s++) {
                if (track && s < track->sector_count && track->sectors[s].data) {
                    memcpy(data_ptr, track->sectors[s].data, disk->bytes_per_sector);
                } else {
                    memset(data_ptr, 0xE5, disk->bytes_per_sector);
                }
                data_ptr += disk->bytes_per_sector;
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
 * Format Plugin Registration
 * ============================================================================ */

static bool myz80_probe_plugin(const uint8_t *data, size_t size,
                               size_t file_size, int *confidence) {
    (void)file_size;
    return uft_myz80_probe(data, size, confidence);
}

static uft_error_t myz80_open(uft_disk_t *disk, const char *path, bool read_only) {
    (void)read_only;
    uft_disk_image_t *image = NULL;
    uft_error_t err = uft_myz80_read(path, &image, NULL, NULL);
    if (err == UFT_OK && image) {
        disk->plugin_data = image;
        disk->geometry.cylinders = image->tracks;
        disk->geometry.heads = image->heads;
        disk->geometry.sectors = image->sectors_per_track;
        disk->geometry.sector_size = image->bytes_per_sector;
    }
    return err;
}

static void myz80_close(uft_disk_t *disk) {
    if (disk && disk->plugin_data) {
        uft_disk_free((uft_disk_image_t*)disk->plugin_data);
        disk->plugin_data = NULL;
    }
}

static uft_error_t myz80_read_track(uft_disk_t *disk, int cyl, int head,
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

const uft_format_plugin_t uft_format_plugin_myz80 = {
    .name = "MYZ80",
    .description = "MYZ80 CP/M Emulator Hard Drive Image",
    .extensions = "myz80,myz",
    .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE,
    .probe = myz80_probe_plugin,
    .open = myz80_open,
    .close = myz80_close,
    .read_track = myz80_read_track,
};

UFT_REGISTER_FORMAT_PLUGIN(myz80)
