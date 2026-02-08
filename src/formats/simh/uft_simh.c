/**
 * @file uft_simh.c
 * @brief SIMH disc image format implementation
 * @version 3.9.0
 * 
 * SIMH format for historical computer simulation.
 * Reference: libdsk drvsimh.c
 */

#include "uft/formats/uft_simh.h"
#include "uft/uft_format_common.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * Predefined Geometries
 * ============================================================================ */

const simh_geometry_t simh_geometries[] = {
    /* DEC Floppy Formats */
    { SIMH_DISK_RX01,    77,  1, 26, 128,  "DEC RX01 (8\" SS SD)" },
    { SIMH_DISK_RX02,    77,  1, 26, 256,  "DEC RX02 (8\" SS DD)" },
    { SIMH_DISK_RX50,    80,  1, 10, 512,  "DEC RX50 (5.25\" SS)" },
    { SIMH_DISK_RX33,    80,  2, 15, 512,  "DEC RX33 (5.25\" DS HD)" },
    
    /* PC Formats */
    { SIMH_DISK_PC_360K,  40, 2,  9, 512,  "PC 360K (5.25\" DS DD)" },
    { SIMH_DISK_PC_720K,  80, 2,  9, 512,  "PC 720K (3.5\" DS DD)" },
    { SIMH_DISK_PC_1200K, 80, 2, 15, 512,  "PC 1.2M (5.25\" DS HD)" },
    { SIMH_DISK_PC_1440K, 80, 2, 18, 512,  "PC 1.44M (3.5\" DS HD)" },
    
    { SIMH_DISK_UNKNOWN,  0,  0,  0,   0,  NULL }
};

const size_t simh_geometry_count = sizeof(simh_geometries) / sizeof(simh_geometries[0]) - 1;

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

static uint8_t code_from_sector_size(uint16_t size) {
    switch (size) {
        case 128:  return 0;
        case 256:  return 1;
        case 512:  return 2;
        case 1024: return 3;
        default:   return 2;
    }
}

const simh_geometry_t* uft_simh_get_geometry(simh_disk_type_t type) {
    for (size_t i = 0; i < simh_geometry_count; i++) {
        if (simh_geometries[i].type == type) {
            return &simh_geometries[i];
        }
    }
    return NULL;
}

simh_disk_type_t uft_simh_detect_type(size_t file_size) {
    /* Calculate expected sizes for each type */
    for (size_t i = 0; i < simh_geometry_count; i++) {
        const simh_geometry_t *g = &simh_geometries[i];
        size_t expected = (size_t)g->cylinders * g->heads * g->sectors * g->sector_size;
        if (file_size == expected) {
            return g->type;
        }
    }
    
    /* Common sizes */
    switch (file_size) {
        case 256256:   return SIMH_DISK_RX01;   /* 77 * 1 * 26 * 128 */
        case 512512:   return SIMH_DISK_RX02;   /* 77 * 1 * 26 * 256 */
        case 409600:   return SIMH_DISK_RX50;   /* 80 * 1 * 10 * 512 */
        case 368640:   return SIMH_DISK_PC_360K;
        case 737280:   return SIMH_DISK_PC_720K;
        case 1228800:  return SIMH_DISK_PC_1200K;
        case 1474560:  return SIMH_DISK_PC_1440K;
        default:       return SIMH_DISK_UNKNOWN;
    }
}

void uft_simh_read_options_init(simh_read_options_t *opts) {
    if (!opts) return;
    memset(opts, 0, sizeof(*opts));
    opts->disk_type = SIMH_DISK_UNKNOWN;
}

/* ============================================================================
 * Read Implementation
 * ============================================================================ */

uft_error_t uft_simh_read_mem(const uint8_t *data, size_t size,
                              uft_disk_image_t **out_disk,
                              const simh_read_options_t *opts,
                              simh_read_result_t *result) {
    if (!data || !out_disk || size == 0) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    /* Initialize result */
    if (result) {
        memset(result, 0, sizeof(*result));
        result->image_size = size;
    }
    
    /* Determine geometry */
    simh_disk_type_t disk_type = SIMH_DISK_UNKNOWN;
    uint16_t cylinders = 0;
    uint8_t heads = 0, sectors = 0;
    uint16_t sector_size = 512;
    
    if (opts && opts->disk_type != SIMH_DISK_UNKNOWN) {
        /* Use specified type */
        disk_type = opts->disk_type;
        
        if (disk_type == SIMH_DISK_CUSTOM) {
            cylinders = opts->cylinders;
            heads = opts->heads;
            sectors = opts->sectors;
            sector_size = opts->sector_size;
        } else {
            const simh_geometry_t *geom = uft_simh_get_geometry(disk_type);
            if (geom) {
                cylinders = geom->cylinders;
                heads = geom->heads;
                sectors = geom->sectors;
                sector_size = geom->sector_size;
            }
        }
    } else {
        /* Auto-detect from size */
        disk_type = uft_simh_detect_type(size);
        
        if (disk_type != SIMH_DISK_UNKNOWN) {
            const simh_geometry_t *geom = uft_simh_get_geometry(disk_type);
            if (geom) {
                cylinders = geom->cylinders;
                heads = geom->heads;
                sectors = geom->sectors;
                sector_size = geom->sector_size;
            }
        } else {
            /* Try to guess geometry from file size */
            /* Assume 512-byte sectors first */
            size_t total_sectors = size / 512;
            
            if (total_sectors == 720) {
                cylinders = 80; heads = 2; sectors = 9; sector_size = 512;
            } else if (total_sectors == 1440) {
                cylinders = 80; heads = 2; sectors = 18; sector_size = 512;
            } else if (total_sectors == 2880) {
                cylinders = 80; heads = 2; sectors = 36; sector_size = 512;
            } else if (total_sectors % 26 == 0) {
                /* Likely 8" format */
                cylinders = total_sectors / 26;
                heads = 1;
                sectors = 26;
                sector_size = 128;
                if (size / (cylinders * 26) == 256) {
                    sector_size = 256;
                }
            } else {
                /* Default guess */
                sector_size = 512;
                total_sectors = size / sector_size;
                sectors = 9;
                heads = 2;
                cylinders = total_sectors / (sectors * heads);
                if (cylinders == 0) {
                    heads = 1;
                    cylinders = total_sectors / sectors;
                }
            }
            disk_type = SIMH_DISK_CUSTOM;
        }
    }
    
    /* Validate geometry */
    if (cylinders == 0 || sectors == 0 || sector_size == 0) {
        if (result) {
            result->error = UFT_ERR_FORMAT;
            result->error_detail = "Cannot determine SIMH geometry";
        }
        return UFT_ERR_FORMAT;
    }
    
    if (heads == 0) heads = 1;
    
    if (result) {
        result->disk_type = disk_type;
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
    snprintf(disk->format_name, sizeof(disk->format_name), "SIMH");
    disk->sectors_per_track = sectors;
    disk->bytes_per_sector = sector_size;
    
    /* Read track data */
    size_t data_pos = 0;
    uint8_t size_code = code_from_sector_size(sector_size);
    
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
            track->encoding = (sector_size <= 256) ? UFT_ENC_FM : UFT_ENC_MFM;
            
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
                    if (data_pos + sector_size <= size) {
                        memcpy(sect->data, data + data_pos, sector_size);
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

uft_error_t uft_simh_read(const char *path,
                          uft_disk_image_t **out_disk,
                          const simh_read_options_t *opts,
                          simh_read_result_t *result) {
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
    
    uft_error_t err = uft_simh_read_mem(data, size, out_disk, opts, result);
    free(data);
    
    return err;
}

/* ============================================================================
 * Write Implementation
 * ============================================================================ */

uft_error_t uft_simh_write(const uft_disk_image_t *disk,
                           const char *path) {
    if (!disk || !path) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    /* Calculate output size (raw image, no header) */
    size_t data_size = (size_t)disk->tracks * disk->heads * 
                       disk->sectors_per_track * disk->bytes_per_sector;
    
    uint8_t *output = malloc(data_size);
    if (!output) {
        return UFT_ERR_MEMORY;
    }
    
    /* Write track data */
    size_t data_pos = 0;
    
    for (uint16_t c = 0; c < disk->tracks; c++) {
        for (uint8_t h = 0; h < disk->heads; h++) {
            size_t idx = c * disk->heads + h;
            uft_track_t *track = disk->track_data[idx];
            
            for (uint8_t s = 0; s < disk->sectors_per_track; s++) {
                if (track && s < track->sector_count && track->sectors[s].data) {
                    memcpy(output + data_pos, track->sectors[s].data,
                           disk->bytes_per_sector);
                } else {
                    memset(output + data_pos, 0xE5, disk->bytes_per_sector);
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
    
    size_t written = fwrite(output, 1, data_size, fp);
    fclose(fp);
    free(output);
    
    if (written != data_size) {
        return UFT_ERR_IO;
    }
    
    return UFT_OK;
}

/* ============================================================================
 * Format Plugin Registration
 * ============================================================================ */

static bool simh_probe_plugin(const uint8_t *data, size_t size,
                              size_t file_size, int *confidence) {
    (void)data;
    (void)size;
    
    /* SIMH has no signature - detect by file size */
    simh_disk_type_t type = uft_simh_detect_type(file_size);
    
    if (type != SIMH_DISK_UNKNOWN) {
        if (confidence) *confidence = 50;  /* Lower confidence - size-based only */
        return true;
    }
    
    return false;
}

static uft_error_t simh_open(uft_disk_t *disk, const char *path, bool read_only) {
    (void)read_only;
    uft_disk_image_t *image = NULL;
    uft_error_t err = uft_simh_read(path, &image, NULL, NULL);
    if (err == UFT_OK && image) {
        disk->plugin_data = image;
        disk->geometry.cylinders = image->tracks;
        disk->geometry.heads = image->heads;
        disk->geometry.sectors = image->sectors_per_track;
        disk->geometry.sector_size = image->bytes_per_sector;
    }
    return err;
}

static void simh_close(uft_disk_t *disk) {
    if (disk && disk->plugin_data) {
        uft_disk_free((uft_disk_image_t*)disk->plugin_data);
        disk->plugin_data = NULL;
    }
}

static uft_error_t simh_read_track(uft_disk_t *disk, int cyl, int head,
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

const uft_format_plugin_t uft_format_plugin_simh = {
    .name = "SIMH",
    .description = "SIMH Simulator Disc Image",
    .extensions = "dsk,img",
    .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE,
    .probe = simh_probe_plugin,
    .open = simh_open,
    .close = simh_close,
    .read_track = simh_read_track,
};

UFT_REGISTER_FORMAT_PLUGIN(simh)
