/**
 * @file uft_posix.c
 * @brief POSIX disk format implementation
 * @version 3.9.0
 * 
 * Raw disk image with separate geometry file (.geom).
 * Reference: libdsk drvposix.c
 */

#include "uft/formats/uft_posix.h"
#include "uft/uft_format_common.h"
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
        default:   return 2;
    }
}

static char* get_geom_path(const char *path) {
    size_t len = strlen(path);
    size_t geom_len = len + strlen(POSIX_GEOM_EXTENSION) + 1;
    char *geom_path = malloc(geom_len);
    if (geom_path) {
        snprintf(geom_path, geom_len, "%s%s", path, POSIX_GEOM_EXTENSION);
    }
    return geom_path;
}

/* ============================================================================
 * Options Initialization
 * ============================================================================ */

void uft_posix_read_options_init(posix_read_options_t *opts) {
    if (!opts) return;
    memset(opts, 0, sizeof(*opts));
    
    opts->require_geom = false;
    opts->fallback.cylinders = 80;
    opts->fallback.heads = 2;
    opts->fallback.sectors = 9;
    opts->fallback.sector_size = 512;
    opts->fallback.first_sector = 1;
    opts->fallback.encoding = UFT_ENC_MFM;
}

/* ============================================================================
 * Geometry File I/O
 * ============================================================================ */

uft_error_t uft_posix_read_geometry(const char *geom_path,
                                    posix_geometry_t *geometry) {
    if (!geom_path || !geometry) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    FILE *fp = fopen(geom_path, "r");
    if (!fp) {
        return UFT_ERR_IO;
    }
    
    char line[POSIX_GEOM_MAX_LINE];
    if (!fgets(line, sizeof(line), fp)) {
        fclose(fp);
        return UFT_ERR_FORMAT;
    }
    fclose(fp);
    
    /* Parse: cylinders heads sectors secsize [first_sector] */
    int cyls, heads, sects, secsize;
    int first = 1;
    
    int parsed = sscanf(line, "%d %d %d %d %d", &cyls, &heads, &sects, &secsize, &first);
    
    if (parsed < 4) {
        return UFT_ERR_FORMAT;
    }
    
    geometry->cylinders = (uint16_t)cyls;
    geometry->heads = (uint8_t)heads;
    geometry->sectors = (uint8_t)sects;
    geometry->sector_size = (uint16_t)secsize;
    geometry->first_sector = (uint8_t)first;
    geometry->encoding = UFT_ENC_MFM;
    
    return UFT_OK;
}

uft_error_t uft_posix_write_geometry(const char *geom_path,
                                     const posix_geometry_t *geometry) {
    if (!geom_path || !geometry) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    FILE *fp = fopen(geom_path, "w");
    if (!fp) {
        return UFT_ERR_IO;
    }
    
    fprintf(fp, "%d %d %d %d %d\n",
            geometry->cylinders,
            geometry->heads,
            geometry->sectors,
            geometry->sector_size,
            geometry->first_sector);
    
    fclose(fp);
    return UFT_OK;
}

/* ============================================================================
 * Probe Function
 * ============================================================================ */

bool uft_posix_probe(const char *path, int *confidence) {
    if (!path) return false;
    
    /* Check if .geom file exists */
    char *geom_path = get_geom_path(path);
    if (!geom_path) return false;
    
    FILE *fp = fopen(geom_path, "r");
    free(geom_path);
    
    if (fp) {
        fclose(fp);
        if (confidence) *confidence = 80;
        return true;
    }
    
    return false;
}

/* ============================================================================
 * Read Implementation
 * ============================================================================ */

uft_error_t uft_posix_read(const char *path,
                           uft_disk_image_t **out_disk,
                           const posix_read_options_t *opts,
                           posix_read_result_t *result) {
    if (!path || !out_disk) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    /* Initialize result */
    if (result) {
        memset(result, 0, sizeof(*result));
    }
    
    /* Default options */
    posix_read_options_t default_opts;
    if (!opts) {
        uft_posix_read_options_init(&default_opts);
        opts = &default_opts;
    }
    
    /* Try to read geometry file */
    posix_geometry_t geometry;
    char *geom_path = get_geom_path(path);
    bool geom_found = false;
    
    if (geom_path) {
        if (uft_posix_read_geometry(geom_path, &geometry) == UFT_OK) {
            geom_found = true;
        }
        free(geom_path);
    }
    
    if (!geom_found) {
        if (opts->require_geom) {
            if (result) {
                result->error = UFT_ERR_NOT_FOUND;
                result->error_detail = "Geometry file not found";
            }
            return UFT_ERR_NOT_FOUND;
        }
        geometry = opts->fallback;
    }
    
    if (result) {
        result->geom_found = geom_found;
        result->geometry = geometry;
    }
    
    /* Read disk image file */
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        return UFT_ERR_IO;
    }
    
    fseek(fp, 0, SEEK_END);
    size_t size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    if (result) {
        result->image_size = size;
    }
    
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
    
    /* Auto-detect geometry from size if no .geom file */
    if (!geom_found) {
        size_t track_size = (size_t)geometry.sectors * geometry.sector_size;
        if (track_size > 0) {
            size_t total_tracks = size / track_size;
            if (geometry.heads > 0) {
                geometry.cylinders = total_tracks / geometry.heads;
            }
        }
    }
    
    /* Create disk image */
    uft_disk_image_t *disk = uft_disk_alloc(geometry.cylinders, geometry.heads);
    if (!disk) {
        free(data);
        return UFT_ERR_MEMORY;
    }
    
    disk->format = UFT_FMT_RAW;
    snprintf(disk->format_name, sizeof(disk->format_name), "POSIX");
    disk->sectors_per_track = geometry.sectors;
    disk->bytes_per_sector = geometry.sector_size;
    
    /* Read track data */
    size_t data_pos = 0;
    uint8_t size_code = code_from_size(geometry.sector_size);
    
    for (uint16_t c = 0; c < geometry.cylinders; c++) {
        for (uint8_t h = 0; h < geometry.heads; h++) {
            size_t idx = c * geometry.heads + h;
            
            uft_track_t *track = uft_track_alloc(geometry.sectors, 0);
            if (!track) {
                uft_disk_free(disk);
                free(data);
                return UFT_ERR_MEMORY;
            }
            
            track->track_num = c;
            track->head = h;
            track->encoding = geometry.encoding;
            
            for (uint8_t s = 0; s < geometry.sectors; s++) {
                uft_sector_t *sect = &track->sectors[s];
                sect->id.cylinder = c;
                sect->id.head = h;
                sect->id.sector = s + geometry.first_sector;
                sect->id.size_code = size_code;
                sect->status = UFT_SECTOR_OK;
                
                sect->data = malloc(geometry.sector_size);
                sect->data_size = geometry.sector_size;
                
                if (sect->data) {
                    if (data_pos + geometry.sector_size <= size) {
                        memcpy(sect->data, data + data_pos, geometry.sector_size);
                    } else {
                        memset(sect->data, 0xE5, geometry.sector_size);
                    }
                }
                data_pos += geometry.sector_size;
                track->sector_count++;
            }
            
            disk->track_data[idx] = track;
        }
    }
    
    free(data);
    
    if (result) {
        result->success = true;
        result->geometry = geometry;
    }
    
    *out_disk = disk;
    return UFT_OK;
}

/* ============================================================================
 * Write Implementation
 * ============================================================================ */

uft_error_t uft_posix_write(const uft_disk_image_t *disk,
                            const char *path) {
    if (!disk || !path) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    /* Calculate output size */
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
    
    /* Write disk image */
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
    
    /* Write geometry file */
    char *geom_path = get_geom_path(path);
    if (geom_path) {
        posix_geometry_t geometry = {
            .cylinders = disk->tracks,
            .heads = disk->heads,
            .sectors = disk->sectors_per_track,
            .sector_size = disk->bytes_per_sector,
            .first_sector = 1,
            .encoding = UFT_ENC_MFM
        };
        
        uft_posix_write_geometry(geom_path, &geometry);
        free(geom_path);
    }
    
    return UFT_OK;
}

/* ============================================================================
 * Format Plugin Registration
 * ============================================================================ */

static bool posix_probe_plugin(const uint8_t *data, size_t size,
                               size_t file_size, int *confidence) {
    (void)data;
    (void)size;
    (void)file_size;
    
    /* This probe doesn't work without the path - use uft_posix_probe instead */
    if (confidence) *confidence = 0;
    return false;
}

static uft_error_t posix_open(uft_disk_t *disk, const char *path, bool read_only) {
    (void)read_only;
    
    /* Check for .geom file */
    int conf;
    if (!uft_posix_probe(path, &conf)) {
        return UFT_ERR_FORMAT;
    }
    
    uft_disk_image_t *image = NULL;
    uft_error_t err = uft_posix_read(path, &image, NULL, NULL);
    if (err == UFT_OK && image) {
        disk->plugin_data = image;
        disk->geometry.cylinders = image->tracks;
        disk->geometry.heads = image->heads;
        disk->geometry.sectors = image->sectors_per_track;
        disk->geometry.sector_size = image->bytes_per_sector;
    }
    return err;
}

static void posix_close(uft_disk_t *disk) {
    if (disk && disk->plugin_data) {
        uft_disk_free((uft_disk_image_t*)disk->plugin_data);
        disk->plugin_data = NULL;
    }
}

static uft_error_t posix_read_track(uft_disk_t *disk, int cyl, int head,
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

const uft_format_plugin_t uft_format_plugin_posix = {
    .name = "POSIX",
    .description = "POSIX Raw Disk with Geometry File",
    .extensions = "dsk,img,raw",
    .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE,
    .probe = posix_probe_plugin,
    .open = posix_open,
    .close = posix_close,
    .read_track = posix_read_track,
};

UFT_REGISTER_FORMAT_PLUGIN(posix)
