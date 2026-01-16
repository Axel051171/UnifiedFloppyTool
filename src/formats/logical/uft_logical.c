/**
 * @file uft_logical.c
 * @brief Logical Disk format implementation
 * @version 3.9.0
 * 
 * Logical disk format with explicit geometry header.
 * Reference: libdsk drvlogi.c
 */

#include "uft/formats/uft_logical.h"
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
 * Header Validation
 * ============================================================================ */

bool uft_logical_validate_header(const logical_header_t *header) {
    if (!header) return false;
    return memcmp(header->signature, LOGICAL_SIGNATURE, LOGICAL_SIGNATURE_LEN) == 0;
}

bool uft_logical_probe(const uint8_t *data, size_t size, int *confidence) {
    if (!data || size < LOGICAL_HEADER_SIZE) return false;
    
    if (memcmp(data, LOGICAL_SIGNATURE, LOGICAL_SIGNATURE_LEN) == 0) {
        /* Validate geometry values */
        const logical_header_t *hdr = (const logical_header_t *)data;
        uint16_t cyls = read_le16((const uint8_t*)&hdr->cylinders);
        uint16_t heads = read_le16((const uint8_t*)&hdr->heads);
        uint16_t sects = read_le16((const uint8_t*)&hdr->sectors);
        uint16_t secsize = read_le16((const uint8_t*)&hdr->sector_size);
        
        if (cyls > 0 && cyls <= 256 &&
            heads > 0 && heads <= 4 &&
            sects > 0 && sects <= 64 &&
            (secsize == 128 || secsize == 256 || secsize == 512 || secsize == 1024)) {
            if (confidence) *confidence = 95;
            return true;
        }
    }
    
    return false;
}

/* ============================================================================
 * Read Implementation
 * ============================================================================ */

uft_error_t uft_logical_read_mem(const uint8_t *data, size_t size,
                                 uft_disk_image_t **out_disk,
                                 logical_read_result_t *result) {
    if (!data || !out_disk || size < LOGICAL_HEADER_SIZE) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    /* Initialize result */
    if (result) {
        memset(result, 0, sizeof(*result));
    }
    
    /* Validate header */
    const logical_header_t *header = (const logical_header_t *)data;
    if (!uft_logical_validate_header(header)) {
        if (result) {
            result->error = UFT_ERR_FORMAT;
            result->error_detail = "Invalid Logical disk signature";
        }
        return UFT_ERR_FORMAT;
    }
    
    /* Extract geometry */
    uint16_t cylinders = read_le16((const uint8_t*)&header->cylinders);
    uint16_t heads = read_le16((const uint8_t*)&header->heads);
    uint16_t sectors = read_le16((const uint8_t*)&header->sectors);
    uint16_t sector_size = read_le16((const uint8_t*)&header->sector_size);
    uint8_t first_sector = header->first_sector;
    uft_encoding_t encoding = (header->encoding == 0) ? UFT_ENC_FM : UFT_ENC_MFM;
    
    if (cylinders == 0 || heads == 0 || sectors == 0 || sector_size == 0) {
        if (result) {
            result->error = UFT_ERR_FORMAT;
            result->error_detail = "Invalid Logical disk geometry";
        }
        return UFT_ERR_FORMAT;
    }
    
    if (first_sector == 0) first_sector = 1;
    
    if (result) {
        result->cylinders = cylinders;
        result->heads = heads;
        result->sectors = sectors;
        result->sector_size = sector_size;
        result->image_size = size;
    }
    
    /* Allocate disk image */
    uft_disk_image_t *disk = uft_disk_alloc(cylinders, heads);
    if (!disk) {
        return UFT_ERR_MEMORY;
    }
    
    disk->format = UFT_FMT_RAW;
    snprintf(disk->format_name, sizeof(disk->format_name), "Logical");
    disk->sectors_per_track = sectors;
    disk->bytes_per_sector = sector_size;
    
    /* Read track data */
    const uint8_t *track_data = data + LOGICAL_HEADER_SIZE;
    size_t available = size - LOGICAL_HEADER_SIZE;
    size_t data_pos = 0;
    uint8_t size_code = code_from_size(sector_size);
    
    for (uint16_t c = 0; c < cylinders; c++) {
        for (uint16_t h = 0; h < heads; h++) {
            size_t idx = c * heads + h;
            
            uft_track_t *track = uft_track_alloc(sectors, 0);
            if (!track) {
                uft_disk_free(disk);
                return UFT_ERR_MEMORY;
            }
            
            track->track_num = c;
            track->head = h;
            track->encoding = encoding;
            
            for (uint16_t s = 0; s < sectors; s++) {
                uft_sector_t *sect = &track->sectors[s];
                sect->id.cylinder = c;
                sect->id.head = h;
                sect->id.sector = s + first_sector;
                sect->id.size_code = size_code;
                sect->status = UFT_SECTOR_OK;
                
                sect->data = malloc(sector_size);
                sect->data_size = sector_size;
                
                if (sect->data) {
                    if (data_pos + sector_size <= available) {
                        memcpy(sect->data, track_data + data_pos, sector_size);
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

uft_error_t uft_logical_read(const char *path,
                             uft_disk_image_t **out_disk,
                             logical_read_result_t *result) {
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
    
    uft_error_t err = uft_logical_read_mem(data, size, out_disk, result);
    free(data);
    
    return err;
}

/* ============================================================================
 * Write Implementation
 * ============================================================================ */

uft_error_t uft_logical_write(const uft_disk_image_t *disk,
                              const char *path) {
    if (!disk || !path) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    /* Calculate output size */
    size_t data_size = (size_t)disk->tracks * disk->heads *
                       disk->sectors_per_track * disk->bytes_per_sector;
    size_t total_size = LOGICAL_HEADER_SIZE + data_size;
    
    uint8_t *output = malloc(total_size);
    if (!output) {
        return UFT_ERR_MEMORY;
    }
    
    /* Build header */
    logical_header_t *header = (logical_header_t *)output;
    memset(header, 0, sizeof(*header));
    memcpy(header->signature, LOGICAL_SIGNATURE, LOGICAL_SIGNATURE_LEN);
    write_le16((uint8_t*)&header->cylinders, disk->tracks);
    write_le16((uint8_t*)&header->heads, disk->heads);
    write_le16((uint8_t*)&header->sectors, disk->sectors_per_track);
    write_le16((uint8_t*)&header->sector_size, disk->bytes_per_sector);
    header->first_sector = 1;
    header->encoding = 1;  /* MFM */
    write_le16((uint8_t*)&header->data_rate, 250);  /* 250 kbps */
    
    /* Write track data */
    uint8_t *track_data = output + LOGICAL_HEADER_SIZE;
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
 * Format Plugin Registration
 * ============================================================================ */

static bool logical_probe_plugin(const uint8_t *data, size_t size,
                                 size_t file_size, int *confidence) {
    (void)file_size;
    return uft_logical_probe(data, size, confidence);
}

static uft_error_t logical_open(uft_disk_t *disk, const char *path, bool read_only) {
    (void)read_only;
    uft_disk_image_t *image = NULL;
    uft_error_t err = uft_logical_read(path, &image, NULL);
    if (err == UFT_OK && image) {
        disk->plugin_data = image;
        disk->geometry.cylinders = image->tracks;
        disk->geometry.heads = image->heads;
        disk->geometry.sectors = image->sectors_per_track;
        disk->geometry.sector_size = image->bytes_per_sector;
    }
    return err;
}

static void logical_close(uft_disk_t *disk) {
    if (disk && disk->plugin_data) {
        uft_disk_free((uft_disk_image_t*)disk->plugin_data);
        disk->plugin_data = NULL;
    }
}

static uft_error_t logical_read_track(uft_disk_t *disk, int cyl, int head,
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

const uft_format_plugin_t uft_format_plugin_logical = {
    .name = "Logical",
    .description = "Logical Disk Image",
    .extensions = "logical,logi",
    .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE,
    .probe = logical_probe_plugin,
    .open = logical_open,
    .close = logical_close,
    .read_track = logical_read_track,
};

UFT_REGISTER_FORMAT_PLUGIN(logical)
