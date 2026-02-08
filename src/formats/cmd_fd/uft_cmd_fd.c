/**
 * @file uft_cmd_fd.c
 * @brief CMD FD2000/FD4000 Disk Image Format Implementation
 * @version 4.1.0
 * 
 * Implements support for CMD FD series floppy drive images:
 * - D1M: FD2000/FD4000 DD disk (720KB, 80 tracks, 2 sides, 9 sectors)
 * - D2M: FD2000/FD4000 HD disk (1.44MB, 80 tracks, 2 sides, 18 sectors)
 * - D4M: FD4000 ED disk (2.88MB, 80 tracks, 2 sides, 36 sectors)
 * 
 * The CMD FD series drives use standard PC floppy formats with
 * CMD-specific DOS structures for Commodore compatibility.
 * 
 * Reference: VICE emulator, libcbmimage, DirMaster
 */

#include "uft/formats/uft_cmd_fd.h"
#include "uft/core/uft_unified_types.h"
#include "uft/core/uft_error_compat.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * Format Constants
 * ============================================================================ */

/* D1M - 720KB DD format */
#define D1M_TRACKS          80
#define D1M_SIDES           2
#define D1M_SECTORS         9
#define D1M_SECTOR_SIZE     512
#define D1M_TOTAL_SIZE      (D1M_TRACKS * D1M_SIDES * D1M_SECTORS * D1M_SECTOR_SIZE)
#define D1M_EXPECTED_SIZE   737280

/* D2M - 1.44MB HD format */
#define D2M_TRACKS          80
#define D2M_SIDES           2
#define D2M_SECTORS         18
#define D2M_SECTOR_SIZE     512
#define D2M_TOTAL_SIZE      (D2M_TRACKS * D2M_SIDES * D2M_SECTORS * D2M_SECTOR_SIZE)
#define D2M_EXPECTED_SIZE   1474560

/* D4M - 2.88MB ED format */
#define D4M_TRACKS          80
#define D4M_SIDES           2
#define D4M_SECTORS         36
#define D4M_SECTOR_SIZE     512
#define D4M_TOTAL_SIZE      (D4M_TRACKS * D4M_SIDES * D4M_SECTORS * D4M_SECTOR_SIZE)
#define D4M_EXPECTED_SIZE   2949120

/* CMD DOS Constants */
#define CMD_HEADER_TRACK    0
#define CMD_HEADER_SECTOR   0
#define CMD_BAM_TRACK       0
#define CMD_BAM_SECTOR      1
#define CMD_DIR_TRACK       1
#define CMD_DIR_SECTOR      0

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

static cmd_fd_type_t detect_type_by_size(size_t size) {
    if (size == D1M_EXPECTED_SIZE || size == D1M_TOTAL_SIZE) {
        return CMD_FD_D1M;
    }
    if (size == D2M_EXPECTED_SIZE || size == D2M_TOTAL_SIZE) {
        return CMD_FD_D2M;
    }
    if (size == D4M_EXPECTED_SIZE || size == D4M_TOTAL_SIZE) {
        return CMD_FD_D4M;
    }
    return CMD_FD_UNKNOWN;
}

/* ============================================================================
 * Probe Functions
 * ============================================================================ */

bool uft_d1m_probe(const uint8_t *data, size_t size, int *confidence) {
    if (!data) return false;
    
    if (size == D1M_EXPECTED_SIZE || size == D1M_TOTAL_SIZE) {
        /* Check for CMD signature in header sector */
        /* FD2000/FD4000 has specific BAM structure */
        if (confidence) *confidence = 70;
        return true;
    }
    
    return false;
}

bool uft_d2m_probe(const uint8_t *data, size_t size, int *confidence) {
    if (!data) return false;
    
    if (size == D2M_EXPECTED_SIZE || size == D2M_TOTAL_SIZE) {
        if (confidence) *confidence = 70;
        return true;
    }
    
    return false;
}

bool uft_d4m_probe(const uint8_t *data, size_t size, int *confidence) {
    if (!data) return false;
    
    if (size == D4M_EXPECTED_SIZE || size == D4M_TOTAL_SIZE) {
        if (confidence) *confidence = 70;
        return true;
    }
    
    return false;
}

/* ============================================================================
 * Geometry Functions
 * ============================================================================ */

int uft_cmd_fd_get_geometry(cmd_fd_type_t type, cmd_fd_geometry_t *geom) {
    if (!geom) return UFT_ERR_INVALID_ARG;
    
    memset(geom, 0, sizeof(*geom));
    
    switch (type) {
        case CMD_FD_D1M:
            geom->type = CMD_FD_D1M;
            geom->tracks = D1M_TRACKS;
            geom->sides = D1M_SIDES;
            geom->sectors = D1M_SECTORS;
            geom->sector_size = D1M_SECTOR_SIZE;
            geom->total_size = D1M_TOTAL_SIZE;
            geom->name = "D1M";
            geom->description = "CMD FD2000/FD4000 DD (720KB)";
            break;
            
        case CMD_FD_D2M:
            geom->type = CMD_FD_D2M;
            geom->tracks = D2M_TRACKS;
            geom->sides = D2M_SIDES;
            geom->sectors = D2M_SECTORS;
            geom->sector_size = D2M_SECTOR_SIZE;
            geom->total_size = D2M_TOTAL_SIZE;
            geom->name = "D2M";
            geom->description = "CMD FD2000/FD4000 HD (1.44MB)";
            break;
            
        case CMD_FD_D4M:
            geom->type = CMD_FD_D4M;
            geom->tracks = D4M_TRACKS;
            geom->sides = D4M_SIDES;
            geom->sectors = D4M_SECTORS;
            geom->sector_size = D4M_SECTOR_SIZE;
            geom->total_size = D4M_TOTAL_SIZE;
            geom->name = "D4M";
            geom->description = "CMD FD4000 ED (2.88MB)";
            break;
            
        default:
            return UFT_ERR_INVALID_ARG;
    }
    
    return UFT_OK;
}

/* ============================================================================
 * Read Functions
 * ============================================================================ */

int uft_cmd_fd_read(const char *path, cmd_fd_type_t expected_type, 
                    uft_disk_image_t **out) {
    if (!path || !out) return UFT_ERR_INVALID_ARG;
    
    FILE *f = fopen(path, "rb");
    if (!f) return UFT_ERR_FILE_OPEN;
    
    /* Get file size */
    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    /* Detect type */
    cmd_fd_type_t type = detect_type_by_size(size);
    if (type == CMD_FD_UNKNOWN) {
        fclose(f);
        return UFT_ERR_FORMAT;
    }
    
    /* If expected type specified, verify */
    if (expected_type != CMD_FD_UNKNOWN && expected_type != type) {
        fclose(f);
        return UFT_ERR_FORMAT;
    }
    
    /* Get geometry */
    cmd_fd_geometry_t geom;
    if (uft_cmd_fd_get_geometry(type, &geom) != UFT_OK) {
        fclose(f);
        return UFT_ERR_FORMAT;
    }
    
    /* Allocate disk image */
    uft_disk_image_t *disk = calloc(1, sizeof(uft_disk_image_t));
    if (!disk) {
        fclose(f);
        return UFT_ERR_MEMORY;
    }
    
    disk->tracks = geom.tracks;
    disk->heads = geom.sides;
    disk->sectors_per_track = geom.sectors;
    disk->bytes_per_sector = geom.sector_size;
    /* disk->encoding set to MFM default */
    disk->track_count = disk->tracks * disk->heads;
    
    /* Allocate track array */
    disk->track_data = calloc(disk->track_count, sizeof(void*));
    if (!disk->track_data) {
        free(disk);
        fclose(f);
        return UFT_ERR_MEMORY;
    }
    
    /* Read all tracks */
    uint8_t size_code = code_from_size(geom.sector_size);
    
    for (int cyl = 0; cyl < geom.tracks; cyl++) {
        for (int head = 0; head < geom.sides; head++) {
            int idx = cyl * geom.sides + head;
            
            uft_track_t *track = uft_track_alloc(geom.sectors, 0);
            if (!track) {
                /* Cleanup and fail */
                for (int j = 0; j < idx; j++) {
                    if (disk->track_data[j]) {
                        uft_track_free(disk->track_data[j]);
                    }
                }
                free(disk->track_data);
                free(disk);
                fclose(f);
                return UFT_ERR_MEMORY;
            }
            
            track->track_num = cyl;
            track->head = head;
            track->encoding = UFT_ENCODING_MFM;
            
            /* Read sectors */
            for (int s = 0; s < geom.sectors; s++) {
                uft_sector_t *sect = &track->sectors[s];
                sect->id.track = cyl;
                sect->id.head = head;
                sect->id.sector = s + 1;  /* 1-based sectors */
                sect->id.size_code = size_code;
                
                sect->data = malloc(geom.sector_size);
                sect->data_len = geom.sector_size;
                
                if (sect->data) {
                    if (fread(sect->data, 1, geom.sector_size, f) != geom.sector_size) {
                        sect->id.status = UFT_SECTOR_CRC_ERROR;
                    } else {
                        sect->id.status = UFT_SECTOR_OK;
                    }
                }
                
                track->sector_count++;
            }
            
            disk->track_data[idx] = track;
        }
    }
    
    fclose(f);
    *out = disk;
    return UFT_OK;
}

/* Convenience wrappers */
int uft_d1m_read(const char *path, uft_disk_image_t **out) {
    return uft_cmd_fd_read(path, CMD_FD_D1M, out);
}

int uft_d2m_read(const char *path, uft_disk_image_t **out) {
    return uft_cmd_fd_read(path, CMD_FD_D2M, out);
}

int uft_d4m_read(const char *path, uft_disk_image_t **out) {
    return uft_cmd_fd_read(path, CMD_FD_D4M, out);
}

/* ============================================================================
 * Write Functions
 * ============================================================================ */

int uft_cmd_fd_write(const char *path, const uft_disk_image_t *disk,
                     cmd_fd_type_t type) {
    if (!path || !disk) return UFT_ERR_INVALID_ARG;
    
    /* Get geometry */
    cmd_fd_geometry_t geom;
    if (uft_cmd_fd_get_geometry(type, &geom) != UFT_OK) {
        return UFT_ERR_INVALID_ARG;
    }
    
    FILE *f = fopen(path, "wb");
    if (!f) return UFT_ERR_FILE_CREATE;
    
    /* Write all tracks in order */
    uint8_t *empty_sector = calloc(1, geom.sector_size);
    
    for (int cyl = 0; cyl < geom.tracks; cyl++) {
        for (int head = 0; head < geom.sides; head++) {
            int idx = cyl * geom.sides + head;
            
            uft_track_t *track = NULL;
            if (idx < (int)disk->track_count) {
                track = disk->track_data[idx];
            }
            
            for (int s = 0; s < geom.sectors; s++) {
                if (track && s < track->sector_count && 
                    track->sectors[s].data) {
                    fwrite(track->sectors[s].data, 1, geom.sector_size, f);
                } else {
                    fwrite(empty_sector, 1, geom.sector_size, f);
                }
            }
        }
    }
    
    free(empty_sector);
    fclose(f);
    return UFT_OK;
}

int uft_d1m_write(const char *path, const uft_disk_image_t *disk) {
    return uft_cmd_fd_write(path, disk, CMD_FD_D1M);
}

int uft_d2m_write(const char *path, const uft_disk_image_t *disk) {
    return uft_cmd_fd_write(path, disk, CMD_FD_D2M);
}

int uft_d4m_write(const char *path, const uft_disk_image_t *disk) {
    return uft_cmd_fd_write(path, disk, CMD_FD_D4M);
}

/* ============================================================================
 * Create Blank Functions
 * ============================================================================ */

int uft_cmd_fd_create_blank(cmd_fd_type_t type, uft_disk_image_t **out) {
    if (!out) return UFT_ERR_INVALID_ARG;
    
    cmd_fd_geometry_t geom;
    if (uft_cmd_fd_get_geometry(type, &geom) != UFT_OK) {
        return UFT_ERR_INVALID_ARG;
    }
    
    /* Allocate disk image */
    uft_disk_image_t *disk = calloc(1, sizeof(uft_disk_image_t));
    if (!disk) return UFT_ERR_MEMORY;
    
    disk->tracks = geom.tracks;
    disk->heads = geom.sides;
    disk->sectors_per_track = geom.sectors;
    disk->bytes_per_sector = geom.sector_size;
    /* disk->encoding set to MFM default */
    disk->track_count = disk->tracks * disk->heads;
    
    disk->track_data = calloc(disk->track_count, sizeof(void*));
    if (!disk->track_data) {
        free(disk);
        return UFT_ERR_MEMORY;
    }
    
    uint8_t size_code = code_from_size(geom.sector_size);
    
    for (int cyl = 0; cyl < geom.tracks; cyl++) {
        for (int head = 0; head < geom.sides; head++) {
            int idx = cyl * geom.sides + head;
            
            uft_track_t *track = uft_track_alloc(geom.sectors, 0);
            if (!track) continue;
            
            track->track_num = cyl;
            track->head = head;
            track->encoding = UFT_ENCODING_MFM;
            
            for (int s = 0; s < geom.sectors; s++) {
                uft_sector_t *sect = &track->sectors[s];
                sect->id.track = cyl;
                sect->id.head = head;
                sect->id.sector = s + 1;
                sect->id.size_code = size_code;
                sect->data = calloc(1, geom.sector_size);
                sect->data_len = geom.sector_size;
                sect->id.status = UFT_SECTOR_OK;
                track->sector_count++;
            }
            
            disk->track_data[idx] = track;
        }
    }
    
    *out = disk;
    return UFT_OK;
}

/* ============================================================================
 * Info Functions
 * ============================================================================ */

int uft_cmd_fd_get_info(const char *path, char *buf, size_t buf_size) {
    if (!path || !buf) return UFT_ERR_INVALID_ARG;
    
    FILE *f = fopen(path, "rb");
    if (!f) return UFT_ERR_FILE_OPEN;
    
    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    fclose(f);
    
    cmd_fd_type_t type = detect_type_by_size(size);
    if (type == CMD_FD_UNKNOWN) {
        return UFT_ERR_FORMAT;
    }
    
    cmd_fd_geometry_t geom;
    uft_cmd_fd_get_geometry(type, &geom);
    
    snprintf(buf, buf_size,
        "Format: %s\n"
        "Description: %s\n"
        "Tracks: %d\n"
        "Sides: %d\n"
        "Sectors/Track: %d\n"
        "Sector Size: %d bytes\n"
        "Total Size: %zu bytes\n"
        "Encoding: MFM\n",
        geom.name, geom.description,
        geom.tracks, geom.sides, geom.sectors,
        geom.sector_size, geom.total_size);
    
    return UFT_OK;
}
