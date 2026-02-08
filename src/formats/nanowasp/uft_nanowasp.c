/**
 * @file uft_nanowasp.c
 * @brief NanoWasp floppy image format implementation
 * @version 3.9.0
 * 
 * NanoWasp format from the NanoWasp Microbee emulator.
 * Reference: libdsk drvnwasp.c by John Elliott
 */

#include "uft/formats/uft_nanowasp.h"
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

bool uft_nanowasp_validate_header(const nanowasp_header_t *header) {
    if (!header) return false;
    return memcmp(header->signature, NANOWASP_SIGNATURE, NANOWASP_SIGNATURE_LEN) == 0;
}

bool uft_nanowasp_probe(const uint8_t *data, size_t size, int *confidence) {
    if (!data || size < NANOWASP_HEADER_SIZE) return false;
    
    if (memcmp(data, NANOWASP_SIGNATURE, NANOWASP_SIGNATURE_LEN) == 0) {
        if (confidence) *confidence = 95;
        return true;
    }
    
    return false;
}

/* ============================================================================
 * Read Implementation
 * ============================================================================ */

uft_error_t uft_nanowasp_read_mem(const uint8_t *data, size_t size,
                                  uft_disk_image_t **out_disk,
                                  nanowasp_read_result_t *result) {
    if (!data || !out_disk || size < NANOWASP_HEADER_SIZE) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    /* Initialize result */
    if (result) {
        memset(result, 0, sizeof(*result));
    }
    
    /* Validate header */
    const nanowasp_header_t *header = (const nanowasp_header_t *)data;
    if (!uft_nanowasp_validate_header(header)) {
        if (result) {
            result->error = UFT_ERR_FORMAT;
            result->error_detail = "Invalid NanoWasp signature";
        }
        return UFT_ERR_FORMAT;
    }
    
    /* Extract geometry */
    uint8_t cylinders = header->cylinders;
    uint8_t heads = header->heads;
    uint8_t sectors = header->sectors;
    uint16_t sector_size = read_le16((const uint8_t*)&header->sector_size);
    
    /* Apply defaults if values are zero */
    if (cylinders == 0) cylinders = NANOWASP_DEF_CYLS;
    if (heads == 0) heads = NANOWASP_DEF_HEADS;
    if (sectors == 0) sectors = NANOWASP_DEF_SECTORS;
    if (sector_size == 0) sector_size = NANOWASP_DEF_SECSIZE;
    
    /* Calculate expected data size */
    size_t data_size = (size_t)cylinders * heads * sectors * sector_size;
    size_t available = size - NANOWASP_HEADER_SIZE;
    
    if (result) {
        result->cylinders = cylinders;
        result->heads = heads;
        result->sectors = sectors;
        result->sector_size = sector_size;
        result->image_size = size;
        result->data_size = data_size;
    }
    
    /* Allocate disk image */
    uft_disk_image_t *disk = uft_disk_alloc(cylinders, heads);
    if (!disk) {
        return UFT_ERR_MEMORY;
    }
    
    disk->format = UFT_FMT_RAW;
    snprintf(disk->format_name, sizeof(disk->format_name), "NanoWasp");
    disk->sectors_per_track = sectors;
    disk->bytes_per_sector = sector_size;
    
    /* Read track data */
    const uint8_t *track_data = data + NANOWASP_HEADER_SIZE;
    size_t data_pos = 0;
    uint8_t size_code = code_from_size(sector_size);
    
    for (uint8_t c = 0; c < cylinders; c++) {
        for (uint8_t h = 0; h < heads; h++) {
            size_t idx = c * heads + h;
            
            uft_track_t *track = uft_track_alloc(sectors, 0);
            if (!track) {
                uft_disk_free(disk);
                return UFT_ERR_MEMORY;
            }
            
            track->cylinder = c;
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

uft_error_t uft_nanowasp_read(const char *path,
                              uft_disk_image_t **out_disk,
                              nanowasp_read_result_t *result) {
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
    
    uft_error_t err = uft_nanowasp_read_mem(data, size, out_disk, result);
    free(data);
    
    return err;
}

/* ============================================================================
 * Write Implementation
 * ============================================================================ */

uft_error_t uft_nanowasp_write(const uft_disk_image_t *disk,
                               const char *path) {
    if (!disk || !path) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    /* Calculate output size */
    size_t data_size = (size_t)disk->tracks * disk->heads * 
                       disk->sectors_per_track * disk->bytes_per_sector;
    size_t total_size = NANOWASP_HEADER_SIZE + data_size;
    
    uint8_t *output = malloc(total_size);
    if (!output) {
        return UFT_ERR_MEMORY;
    }
    
    /* Build header */
    nanowasp_header_t *header = (nanowasp_header_t *)output;
    memset(header, 0, sizeof(*header));
    memcpy(header->signature, NANOWASP_SIGNATURE, NANOWASP_SIGNATURE_LEN);
    header->version = 0;
    header->cylinders = disk->tracks;
    header->heads = disk->heads;
    header->sectors = disk->sectors_per_track;
    write_le16((uint8_t*)&header->sector_size, disk->bytes_per_sector);
    
    /* Write track data */
    uint8_t *track_data = output + NANOWASP_HEADER_SIZE;
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

static bool nanowasp_probe_plugin(const uint8_t *data, size_t size,
                                  size_t file_size, int *confidence) {
    (void)file_size;
    return uft_nanowasp_probe(data, size, confidence);
}

static uft_error_t nanowasp_open(uft_disk_t *disk, const char *path, bool read_only) {
    (void)read_only;
    uft_disk_image_t *image = NULL;
    uft_error_t err = uft_nanowasp_read(path, &image, NULL);
    if (err == UFT_OK && image) {
        disk->plugin_data = image;
        disk->geometry.cylinders = image->tracks;
        disk->geometry.heads = image->heads;
        disk->geometry.sectors = image->sectors_per_track;
        disk->geometry.sector_size = image->bytes_per_sector;
    }
    return err;
}

static void nanowasp_close(uft_disk_t *disk) {
    if (disk && disk->plugin_data) {
        uft_disk_free((uft_disk_image_t*)disk->plugin_data);
        disk->plugin_data = NULL;
    }
}

static uft_error_t nanowasp_read_track(uft_disk_t *disk, int cyl, int head,
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

const uft_format_plugin_t uft_format_plugin_nanowasp = {
    .name = "NanoWasp",
    .description = "NanoWasp Microbee Image",
    .extensions = "nw",
    .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE,
    .probe = nanowasp_probe_plugin,
    .open = nanowasp_open,
    .close = nanowasp_close,
    .read_track = nanowasp_read_track,
};

UFT_REGISTER_FORMAT_PLUGIN(nanowasp)
