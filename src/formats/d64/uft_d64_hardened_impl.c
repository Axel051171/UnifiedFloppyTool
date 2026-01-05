/**
 * @file uft_d64_hardened_impl.c
 * @brief Security-Hardened D64 Parser - Full Implementation
 */

#include "uft/formats/d64_hardened.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "uft/core/uft_safe_math.h"

// ============================================================================
// INTERNAL STRUCTURE
// ============================================================================

struct uft_d64_image_hardened {
    FILE       *f;
    size_t      file_size;
    uint8_t     num_tracks;
    bool        has_errors;
    uint8_t    *error_info;
    uint16_t    total_sectors;
    bool        read_only;
    bool        valid;
    bool        closed;
};

// ============================================================================
// STATIC DATA
// ============================================================================

static const uint8_t d64_spt[UFT_D64_TRACKS_MAX] = {
    21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,
    19,19,19,19,19,19,19,
    18,18,18,18,18,18,
    17,17,17,17,17,17,17,17,17,17,17,17
};

static const uint16_t d64_offset[UFT_D64_TRACKS_MAX + 1] = {
    0,21,42,63,84,105,126,147,168,189,210,231,252,273,294,315,336,
    357,376,395,414,433,452,471,490,508,526,544,562,580,598,
    615,632,649,666,683,700,717,734,751,768,785,802
};

// ============================================================================
// ERROR STRINGS
// ============================================================================

const char *uft_d64_error_string(uft_d64_error_t err) {
    switch (err) {
        case UFT_D64_OK:        return "Success";
        case UFT_D64_EINVAL:    return "Invalid argument";
        case UFT_D64_EIO:       return "I/O error";
        case UFT_D64_EFORMAT:   return "Invalid format";
        case UFT_D64_EBOUNDS:   return "Out of bounds";
        case UFT_D64_ENOMEM:    return "Out of memory";
        case UFT_D64_ECLOSED:   return "Image closed";
        case UFT_D64_EPROTECTED:return "Write protected";
        default:                return "Unknown error";
    }
}

// ============================================================================
// HELPERS
// ============================================================================

uint8_t uft_d64_sectors_per_track(uint8_t track) {
    if (track < 1 || track > UFT_D64_TRACKS_MAX) return 0;
    return d64_spt[track - 1];
}

static bool d64_safe_offset(uint8_t track, uint8_t sector, 
                            size_t file_size, size_t *offset_out) {
    if (track < 1 || track > UFT_D64_TRACKS_MAX) return false;
    if (sector >= d64_spt[track - 1]) return false;
    
    size_t off = ((size_t)d64_offset[track - 1] + sector) * UFT_D64_SECTOR_SIZE;
    if (off + UFT_D64_SECTOR_SIZE > file_size) return false;
    
    *offset_out = off;
    return true;
}

static bool d64_safe_sector_index(uint8_t track, uint8_t sector,
                                   uint16_t max_sectors, uint16_t *index_out) {
    if (track < 1 || track > UFT_D64_TRACKS_MAX) return false;
    if (sector >= d64_spt[track - 1]) return false;
    
    uint16_t idx = d64_offset[track - 1] + sector;
    if (idx >= max_sectors) return false;
    
    *index_out = idx;
    return true;
}

// ============================================================================
// OPEN
// ============================================================================

uft_d64_error_t uft_d64_open_safe(const char *path, bool read_only,
                                   uft_d64_image_hardened_t **img_out) {
    if (!path || !img_out) return UFT_D64_EINVAL;
    *img_out = NULL;
    
    uft_d64_image_hardened_t *img = calloc(1, sizeof(*img));
    if (!img) return UFT_D64_ENOMEM;
    
    img->f = fopen(path, read_only ? "rb" : "r+b");
    if (!img->f) {
        free(img);
        return UFT_D64_EIO;
    }
    
    // Get file size
    if (fseek(img->f, 0, SEEK_END) != 0) {
        fclose(img->f);
        free(img);
        return UFT_D64_EIO;
    }
    
    long pos = ftell(img->f);
    if (pos < 0) {
        fclose(img->f);
        free(img);
        return UFT_D64_EIO;
    }
    
    img->file_size = (size_t)pos;
    
    if (fseek(img->f, 0, SEEK_SET) != 0) {
        fclose(img->f);
        free(img);
        return UFT_D64_EIO;
    }
    
    // Detect variant
    switch (img->file_size) {
        case UFT_D64_SIZE_35:
            img->num_tracks = 35; img->has_errors = false; img->total_sectors = 683;
            break;
        case UFT_D64_SIZE_35_ERR:
            img->num_tracks = 35; img->has_errors = true; img->total_sectors = 683;
            break;
        case UFT_D64_SIZE_40:
            img->num_tracks = 40; img->has_errors = false; img->total_sectors = 768;
            break;
        case UFT_D64_SIZE_40_ERR:
            img->num_tracks = 40; img->has_errors = true; img->total_sectors = 768;
            break;
        case UFT_D64_SIZE_42:
            img->num_tracks = 42; img->has_errors = false; img->total_sectors = 802;
            break;
        case UFT_D64_SIZE_42_ERR:
            img->num_tracks = 42; img->has_errors = true; img->total_sectors = 802;
            break;
        default:
            fclose(img->f);
            free(img);
            return UFT_D64_EFORMAT;
    }
    
    // Load error info
    if (img->has_errors) {
        img->error_info = malloc(img->total_sectors);
        if (!img->error_info) {
            fclose(img->f);
            free(img);
            return UFT_D64_ENOMEM;
        }
        
        size_t err_off = (size_t)img->total_sectors * UFT_D64_SECTOR_SIZE;
        if (fseek(img->f, (long)err_off, SEEK_SET) != 0 ||
            if (fread(img->error_info, 1, img->total_sectors, img->f) != img->total_sectors) {
            free(img->error_info) != img->total_sectors) { /* I/O error */ }
            fclose(img->f);
            free(img);
            return UFT_D64_EIO;
        }
    }
    
    img->read_only = read_only;
    img->valid = true;
    img->closed = false;
    *img_out = img;
    
    return UFT_D64_OK;
}

// ============================================================================
// CLOSE
// ============================================================================

void uft_d64_close_safe(uft_d64_image_hardened_t **img) {
    if (!img || !*img) return;
    
    uft_d64_image_hardened_t *p = *img;
    p->closed = true;
    p->valid = false;
    
    if (p->error_info) {
        memset(p->error_info, 0, p->total_sectors);
        free(p->error_info);
        p->error_info = NULL;
    }
    
    if (p->f) {
        fclose(p->f);
        p->f = NULL;
    }
    
    memset(p, 0, sizeof(*p));
    free(p);
    *img = NULL;
}

// ============================================================================
// GETTERS
// ============================================================================

bool uft_d64_is_valid(const uft_d64_image_hardened_t *img) {
    return img && img->valid && !img->closed && img->f;
}

uft_d64_error_t uft_d64_get_geometry(const uft_d64_image_hardened_t *img,
                                      uint8_t *num_tracks,
                                      uint16_t *total_sectors,
                                      bool *has_errors) {
    if (!img) return UFT_D64_EINVAL;
    if (!img->valid || img->closed) return UFT_D64_ECLOSED;
    
    if (num_tracks) *num_tracks = img->num_tracks;
    if (total_sectors) *total_sectors = img->total_sectors;
    if (has_errors) *has_errors = img->has_errors;
    
    return UFT_D64_OK;
}

// ============================================================================
// READ SECTOR
// ============================================================================

uft_d64_error_t uft_d64_read_sector(const uft_d64_image_hardened_t *img,
                                     uint8_t track, uint8_t sector,
                                     uft_d64_sector_t *sector_out) {
    if (!img || !sector_out) return UFT_D64_EINVAL;
    if (!img->valid || img->closed) return UFT_D64_ECLOSED;
    
    memset(sector_out, 0, sizeof(*sector_out));
    
    // Validate track
    if (track < 1 || track > img->num_tracks) {
        return UFT_D64_EBOUNDS;
    }
    
    // Validate sector
    uint8_t max_sector = d64_spt[track - 1];
    if (sector >= max_sector) {
        return UFT_D64_EBOUNDS;
    }
    
    // Get offset
    size_t offset;
    if (!d64_safe_offset(track, sector, img->file_size, &offset)) {
        return UFT_D64_EBOUNDS;
    }
    
    // Read data
    if (fseek(img->f, (long)offset, SEEK_SET) != 0) {
        return UFT_D64_EIO;
    }
    
    if (fread(sector_out->data, 1, UFT_D64_SECTOR_SIZE, img->f) != UFT_D64_SECTOR_SIZE) {
        return UFT_D64_EIO;
    }
    
    // Fill sector info
    sector_out->id.cylinder = track;
    sector_out->id.head = 0;
    sector_out->id.sector = sector;
    sector_out->present = true;
    
    // Get error code
    if (img->has_errors && img->error_info) {
        uint16_t idx;
        if (d64_safe_sector_index(track, sector, img->total_sectors, &idx)) {
            sector_out->error_code = img->error_info[idx];
            sector_out->id.crc_ok = (sector_out->error_code == UFT_D64_DISK_OK);
        } else {
            sector_out->error_code = UFT_D64_DISK_OK;
            sector_out->id.crc_ok = true;
        }
    } else {
        sector_out->error_code = UFT_D64_DISK_OK;
        sector_out->id.crc_ok = true;
    }
    
    return UFT_D64_OK;
}

// ============================================================================
// READ TRACK
// ============================================================================

uft_d64_error_t uft_d64_read_track_safe(const uft_d64_image_hardened_t *img,
                                         uint8_t track,
                                         uft_d64_sector_t *sectors,
                                         size_t sectors_cap,
                                         size_t *out_count) {
    if (!img || !sectors) return UFT_D64_EINVAL;
    if (!img->valid || img->closed) return UFT_D64_ECLOSED;
    
    if (track < 1 || track > img->num_tracks) {
        return UFT_D64_EBOUNDS;
    }
    
    uint8_t num_sectors = d64_spt[track - 1];
    if (sectors_cap < num_sectors) {
        return UFT_D64_EBOUNDS;
    }
    
    for (uint8_t s = 0; s < num_sectors; s++) {
        uft_d64_error_t rc = uft_d64_read_sector(img, track, s, &sectors[s]);
        if (rc != UFT_D64_OK) {
            return rc;
        }
    }
    
    if (out_count) *out_count = num_sectors;
    return UFT_D64_OK;
}

// ============================================================================
// WRITE SECTOR
// ============================================================================

uft_d64_error_t uft_d64_write_sector(uft_d64_image_hardened_t *img,
                                      uint8_t track, uint8_t sector,
                                      const uint8_t *data) {
    if (!img || !data) return UFT_D64_EINVAL;
    if (!img->valid || img->closed) return UFT_D64_ECLOSED;
    if (img->read_only) return UFT_D64_EPROTECTED;
    
    if (track < 1 || track > img->num_tracks) {
        return UFT_D64_EBOUNDS;
    }
    
    if (sector >= d64_spt[track - 1]) {
        return UFT_D64_EBOUNDS;
    }
    
    size_t offset;
    if (!d64_safe_offset(track, sector, img->file_size, &offset)) {
        return UFT_D64_EBOUNDS;
    }
    
    if (fseek(img->f, (long)offset, SEEK_SET) != 0) {
        return UFT_D64_EIO;
    }
    
    if (fwrite(data, 1, UFT_D64_SECTOR_SIZE, img->f) != UFT_D64_SECTOR_SIZE) {
        return UFT_D64_EIO;
    }
    
    fflush(img->f);
    return UFT_D64_OK;
}

// ============================================================================
// GET DISK INFO
// ============================================================================

uft_d64_error_t uft_d64_get_info(const uft_d64_image_hardened_t *img,
                                  uft_d64_disk_info_t *info_out) {
    if (!img || !info_out) return UFT_D64_EINVAL;
    if (!img->valid || img->closed) return UFT_D64_ECLOSED;
    
    memset(info_out, 0, sizeof(*info_out));
    
    // BAM is at track 18, sector 0
    uft_d64_sector_t bam;
    uft_d64_error_t rc = uft_d64_read_sector(img, 18, 0, &bam);
    if (rc != UFT_D64_OK) return rc;
    
    // Disk name at offset 144, 16 bytes
    memcpy(info_out->name, &bam.data[144], 16);
    info_out->name[16] = '\0';
    
    // Trim trailing 0xA0 (shifted space)
    for (int i = 15; i >= 0; i--) {
        if ((uint8_t)info_out->name[i] == 0xA0 || info_out->name[i] == ' ') {
            info_out->name[i] = '\0';
        } else {
            break;
        }
    }
    
    // Disk ID at offset 162-163
    info_out->id[0] = (char)bam.data[162];
    info_out->id[1] = (char)bam.data[163];
    info_out->id[2] = '\0';
    
    // DOS type at offset 165-166
    info_out->dos_type[0] = (char)bam.data[165];
    info_out->dos_type[1] = (char)bam.data[166];
    info_out->dos_type[2] = '\0';
    
    // DOS version at offset 2
    info_out->dos_version = bam.data[2];
    
    // Count free blocks from BAM
    uint16_t free_blocks = 0;
    for (int t = 1; t <= 35; t++) {
        if (t == 18) continue;  // Skip directory track
        int bam_offset = 4 + (t - 1) * 4;
        free_blocks += bam.data[bam_offset];
    }
    info_out->free_blocks = free_blocks;
    
    return UFT_D64_OK;
}
