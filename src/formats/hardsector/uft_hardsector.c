/**
 * @file uft_hardsector.c
 * @brief Hard-sector floppy disk format implementation
 * @version 3.9.0
 * 
 * Supports 8" and 5.25" hard-sectored disk formats.
 * Reference: IBM 3740/System 34 specs, Catweasel hard-sector support
 */

#include "uft/formats/uft_hardsector.h"
#include "uft/core/uft_unified_types.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * Geometry Functions
 * ============================================================================ */

void hardsector_get_geometry(hardsector_type_t type, 
                             hardsector_geometry_t *geometry) {
    if (!geometry) return;
    memset(geometry, 0, sizeof(*geometry));
    
    geometry->type = type;
    geometry->first_sector = 1;
    geometry->double_step = false;
    
    switch (type) {
        case HS_TYPE_8IN_SSSD:
            geometry->cylinders = HS_8IN_SSSD_CYLS;
            geometry->heads = HS_8IN_SSSD_HEADS;
            geometry->sectors = HS_8IN_SSSD_SECTORS;
            geometry->sector_size = HS_8IN_SSSD_SECSIZE;
            geometry->encoding = HS_ENC_FM;
            break;
            
        case HS_TYPE_8IN_DSSD:
            geometry->cylinders = HS_8IN_DSSD_CYLS;
            geometry->heads = HS_8IN_DSSD_HEADS;
            geometry->sectors = HS_8IN_DSSD_SECTORS;
            geometry->sector_size = HS_8IN_DSSD_SECSIZE;
            geometry->encoding = HS_ENC_FM;
            break;
            
        case HS_TYPE_8IN_DSDD:
            geometry->cylinders = HS_8IN_DSDD_CYLS;
            geometry->heads = HS_8IN_DSDD_HEADS;
            geometry->sectors = HS_8IN_DSDD_SECTORS;
            geometry->sector_size = HS_8IN_DSDD_SECSIZE;
            geometry->encoding = HS_ENC_MFM;
            break;
            
        case HS_TYPE_525_10SEC:
            geometry->cylinders = HS_525_10SEC_CYLS;
            geometry->heads = HS_525_10SEC_HEADS;
            geometry->sectors = HS_525_10SEC_SECTORS;
            geometry->sector_size = HS_525_10SEC_SECSIZE;
            geometry->encoding = HS_ENC_FM;
            break;
            
        case HS_TYPE_525_16SEC:
            geometry->cylinders = HS_525_16SEC_CYLS;
            geometry->heads = HS_525_16SEC_HEADS;
            geometry->sectors = HS_525_16SEC_SECTORS;
            geometry->sector_size = HS_525_16SEC_SECSIZE;
            geometry->encoding = HS_ENC_FM;
            break;
            
        default:
            /* Custom - leave zeroed for user to fill */
            geometry->type = HS_TYPE_CUSTOM;
            break;
    }
}

hardsector_type_t hardsector_detect_type(size_t image_size) {
    /* Check exact sizes for standard formats */
    if (image_size == HS_8IN_SSSD_SIZE) return HS_TYPE_8IN_SSSD;
    if (image_size == HS_8IN_DSSD_SIZE) return HS_TYPE_8IN_DSSD;
    if (image_size == HS_8IN_DSDD_SIZE) return HS_TYPE_8IN_DSDD;
    
    /* 5.25" hard-sector sizes */
    size_t size_10sec = (size_t)HS_525_10SEC_CYLS * HS_525_10SEC_HEADS * 
                        HS_525_10SEC_SECTORS * HS_525_10SEC_SECSIZE;
    if (image_size == size_10sec) return HS_TYPE_525_10SEC;
    
    size_t size_16sec = (size_t)HS_525_16SEC_CYLS * HS_525_16SEC_HEADS * 
                        HS_525_16SEC_SECTORS * HS_525_16SEC_SECSIZE;
    if (image_size == size_16sec) return HS_TYPE_525_16SEC;
    
    return HS_TYPE_CUSTOM;
}

size_t hardsector_calc_size(const hardsector_geometry_t *geometry) {
    if (!geometry) return 0;
    return (size_t)geometry->cylinders * geometry->heads * 
           geometry->sectors * geometry->sector_size;
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

static uint8_t code_from_size(uint16_t size) {
    switch (size) {
        case 128:  return 0;
        case 256:  return 1;
        case 512:  return 2;
        case 1024: return 3;
        default:   return 0;
    }
}

static uft_encoding_t convert_encoding(hardsector_encoding_t enc) {
    switch (enc) {
        case HS_ENC_FM:  return UFT_ENC_FM;
        case HS_ENC_MFM: return UFT_ENC_MFM;
        case HS_ENC_GCR: return UFT_ENC_GCR;
        default:         return UFT_ENC_FM;
    }
}

/* ============================================================================
 * Read Implementation
 * ============================================================================ */

uft_error_t uft_hardsector_read_mem(const uint8_t *data, size_t size,
                                    const hardsector_geometry_t *geometry,
                                    uft_disk_image_t **out_disk,
                                    hardsector_read_result_t *result) {
    if (!data || !out_disk) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    /* Initialize result */
    if (result) {
        memset(result, 0, sizeof(*result));
    }
    
    /* Get geometry - either provided or detected */
    hardsector_geometry_t geo;
    if (geometry) {
        memcpy(&geo, geometry, sizeof(geo));
    } else {
        hardsector_type_t type = hardsector_detect_type(size);
        if (type == HS_TYPE_CUSTOM) {
            if (result) {
                result->error = UFT_ERR_FORMAT;
                result->error_detail = "Cannot detect hard-sector geometry";
            }
            return UFT_ERR_FORMAT;
        }
        hardsector_get_geometry(type, &geo);
    }
    
    if (result) {
        memcpy(&result->geometry, &geo, sizeof(geo));
        result->image_size = size;
        result->total_sectors = geo.cylinders * geo.heads * geo.sectors;
    }
    
    /* Validate size */
    size_t expected_size = hardsector_calc_size(&geo);
    if (size < expected_size) {
        if (result) {
            result->error = UFT_ERR_FORMAT;
            result->error_detail = "Image size too small for geometry";
        }
        return UFT_ERR_FORMAT;
    }
    
    /* Allocate disk image */
    uft_disk_image_t *disk = uft_disk_alloc(geo.cylinders, geo.heads);
    if (!disk) {
        return UFT_ERR_MEMORY;
    }
    
    const char *type_name;
    switch (geo.type) {
        case HS_TYPE_8IN_SSSD: type_name = "8in-SSSD"; break;
        case HS_TYPE_8IN_DSSD: type_name = "8in-DSSD"; break;
        case HS_TYPE_8IN_DSDD: type_name = "8in-DSDD"; break;
        case HS_TYPE_525_10SEC: type_name = "5.25-10sec"; break;
        case HS_TYPE_525_16SEC: type_name = "5.25-16sec"; break;
        default: type_name = "HardSector"; break;
    }
    
    disk->format = UFT_FMT_RAW;
    snprintf(disk->format_name, sizeof(disk->format_name), "%s", type_name);
    disk->sectors_per_track = geo.sectors;
    disk->bytes_per_sector = geo.sector_size;
    
    /* Read track data */
    size_t data_pos = 0;
    uint8_t size_code = code_from_size(geo.sector_size);
    uft_encoding_t encoding = convert_encoding(geo.encoding);
    
    for (uint8_t c = 0; c < geo.cylinders; c++) {
        for (uint8_t h = 0; h < geo.heads; h++) {
            size_t idx = c * geo.heads + h;
            
            uft_track_t *track = uft_track_alloc(geo.sectors, 0);
            if (!track) {
                uft_disk_free(disk);
                return UFT_ERR_MEMORY;
            }
            
            track->track_num = c;
            track->head = h;
            track->encoding = encoding;
            
            for (uint8_t s = 0; s < geo.sectors; s++) {
                uft_sector_t *sect = &track->sectors[s];
                sect->id.cylinder = c;
                sect->id.head = h;
                sect->id.sector = s + geo.first_sector;
                sect->id.size_code = size_code;
                sect->status = UFT_SECTOR_OK;
                
                sect->data = malloc(geo.sector_size);
                sect->data_size = geo.sector_size;
                
                if (sect->data) {
                    if (data_pos + geo.sector_size <= size) {
                        memcpy(sect->data, data + data_pos, geo.sector_size);
                    } else {
                        memset(sect->data, 0xE5, geo.sector_size);
                        if (result) result->bad_sectors++;
                    }
                }
                data_pos += geo.sector_size;
                
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

uft_error_t uft_hardsector_read(const char *path,
                                uft_disk_image_t **out_disk,
                                hardsector_read_result_t *result) {
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
    
    uft_error_t err = uft_hardsector_read_mem(data, size, NULL, out_disk, result);
    free(data);
    
    return err;
}

/* ============================================================================
 * Write Implementation
 * ============================================================================ */

void uft_hardsector_write_options_init(hardsector_write_options_t *opts,
                                       hardsector_type_t type) {
    if (!opts) return;
    memset(opts, 0, sizeof(*opts));
    hardsector_get_geometry(type, &opts->geometry);
    opts->fill_byte = 0xE5;
    opts->create_index_marks = false;
}

uft_error_t uft_hardsector_write(const uft_disk_image_t *disk,
                                 const char *path,
                                 const hardsector_write_options_t *opts) {
    if (!disk || !path) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    hardsector_write_options_t default_opts;
    if (!opts) {
        /* Determine type from disk geometry */
        hardsector_type_t type = HS_TYPE_CUSTOM;
        if (disk->tracks == 77 && disk->heads == 1 && 
            disk->sectors_per_track == 26 && disk->bytes_per_sector == 128) {
            type = HS_TYPE_8IN_SSSD;
        } else if (disk->tracks == 77 && disk->heads == 2 &&
                   disk->sectors_per_track == 26 && disk->bytes_per_sector == 128) {
            type = HS_TYPE_8IN_DSSD;
        } else if (disk->tracks == 77 && disk->heads == 2 &&
                   disk->sectors_per_track == 26 && disk->bytes_per_sector == 256) {
            type = HS_TYPE_8IN_DSDD;
        }
        uft_hardsector_write_options_init(&default_opts, type);
        opts = &default_opts;
    }
    
    /* Calculate output size */
    size_t track_size = (size_t)disk->sectors_per_track * disk->bytes_per_sector;
    size_t total_size = (size_t)disk->tracks * disk->heads * track_size;
    
    uint8_t *output = malloc(total_size);
    if (!output) {
        return UFT_ERR_MEMORY;
    }
    
    /* Fill with default byte */
    memset(output, opts->fill_byte, total_size);
    
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
 * IBM 3740 Specific Functions
 * ============================================================================ */

uft_error_t uft_ibm3740_read(const char *path,
                             uft_disk_image_t **out_disk,
                             hardsector_read_result_t *result) {
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
    
    hardsector_geometry_t geo;
    hardsector_get_geometry(HS_TYPE_8IN_SSSD, &geo);
    
    uft_error_t err = uft_hardsector_read_mem(data, size, &geo, out_disk, result);
    free(data);
    
    return err;
}

uft_error_t uft_ibm3740_write(const uft_disk_image_t *disk,
                              const char *path) {
    hardsector_write_options_t opts;
    uft_hardsector_write_options_init(&opts, HS_TYPE_8IN_SSSD);
    return uft_hardsector_write(disk, path, &opts);
}

/* ============================================================================
 * Format Probe
 * ============================================================================ */

bool uft_hardsector_probe(const uint8_t *data, size_t size, int *confidence) {
    (void)data;  /* Content doesn't matter for hard-sector detection */
    
    hardsector_type_t type = hardsector_detect_type(size);
    
    if (type != HS_TYPE_CUSTOM) {
        /* Known hard-sector size */
        if (confidence) {
            /* Lower confidence because these sizes could match other formats */
            *confidence = 50;
        }
        return true;
    }
    
    return false;
}

/* ============================================================================
 * Format Plugin Registration
 * ============================================================================ */

static bool hardsector_probe_plugin(const uint8_t *data, size_t size,
                                    size_t file_size, int *confidence) {
    (void)file_size;
    return uft_hardsector_probe(data, size, confidence);
}

static uft_error_t hardsector_open(uft_disk_t *disk, const char *path, bool read_only) {
    (void)read_only;
    uft_disk_image_t *image = NULL;
    uft_error_t err = uft_hardsector_read(path, &image, NULL);
    if (err == UFT_OK && image) {
        disk->plugin_data = image;
        disk->geometry.cylinders = image->tracks;
        disk->geometry.heads = image->heads;
        disk->geometry.sectors = image->sectors_per_track;
        disk->geometry.sector_size = image->bytes_per_sector;
    }
    return err;
}

static void hardsector_close(uft_disk_t *disk) {
    if (disk && disk->plugin_data) {
        uft_disk_free((uft_disk_image_t*)disk->plugin_data);
        disk->plugin_data = NULL;
    }
}

static uft_error_t hardsector_read_track(uft_disk_t *disk, int cyl, int head,
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

const uft_format_plugin_t uft_format_plugin_hardsector = {
    .name = "HardSector",
    .description = "Hard-Sector 8\" and 5.25\" Disk Image",
    .extensions = "img,ima,8in",
    .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE,
    .probe = hardsector_probe_plugin,
    .open = hardsector_open,
    .close = hardsector_close,
    .read_track = hardsector_read_track,
};

UFT_REGISTER_FORMAT_PLUGIN(hardsector)
