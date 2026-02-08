/**
 * @file uft_cbm_fs.c
 * @brief Commodore CBM DOS Filesystem Implementation
 * 
 * Complete implementation of CBM DOS filesystem operations for D64/D71/D81.
 * 
 * @version 1.0.0
 * @date 2026-01-04
 * @copyright UFT Project
 */

#include "uft/fs/uft_cbm_fs.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/*============================================================================
 * Internal Constants
 *============================================================================*/

/* D64 sector layout (1541/1570) */
static const uint8_t d64_sectors_per_track[] = {
    0,  /* Track 0 doesn't exist */
    21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,  /* 1-17 */
    19, 19, 19, 19, 19, 19, 19,                                          /* 18-24 */
    18, 18, 18, 18, 18, 18,                                              /* 25-30 */
    17, 17, 17, 17, 17,                                                  /* 31-35 */
    17, 17, 17, 17, 17                                                   /* 36-40 (extended) */
};

/* D81 has constant 40 sectors per track for all 80 tracks */
#define D81_SECTORS_PER_TRACK 40

/* Sector offsets for D64 */
static const uint16_t d64_track_offset[] = {
    0,      /* Track 0 doesn't exist */
    0,      /* Track 1 */
    21,     /* Track 2 */
    42,     /* Track 3 */
    63, 84, 105, 126, 147, 168, 189, 210, 231, 252, 273, 294, 315,  /* 4-17 */
    336, 355, 374, 393, 412, 431, 450,                              /* 18-24 */
    469, 487, 505, 523, 541, 559,                                   /* 25-30 */
    577, 594, 611, 628, 645,                                        /* 31-35 */
    662, 679, 696, 713, 730                                         /* 36-40 */
};

/* Default interleave values */
#define D64_INTERLEAVE_DEFAULT  10
#define D71_INTERLEAVE_DEFAULT  6
#define D81_INTERLEAVE_DEFAULT  1

/* PETSCII shifted space (for padding) */
#define PETSCII_SHIFTED_SPACE   0xA0

/*============================================================================
 * Internal Helper Functions
 *============================================================================*/

static uint8_t get_sectors_for_track(uft_cbm_type_t type, uint8_t track) {
    if (track == 0) return 0;
    
    switch (type) {
        case UFT_CBM_TYPE_D64:
        case UFT_CBM_TYPE_D64_40:
            if (track <= 40) return d64_sectors_per_track[track];
            return 0;
            
        case UFT_CBM_TYPE_D71:
        case UFT_CBM_TYPE_D71_80:
            if (track <= 35) return d64_sectors_per_track[track];
            if (track <= 70) return d64_sectors_per_track[track - 35];
            if (track <= 80) return d64_sectors_per_track[track - 35];
            return 0;
            
        case UFT_CBM_TYPE_D81:
            if (track >= 1 && track <= 80) return D81_SECTORS_PER_TRACK;
            return 0;
            
        default:
            return 0;
    }
}

static size_t get_sector_offset(uft_cbm_type_t type, uint8_t track, uint8_t sector) {
    if (track == 0) return SIZE_MAX;
    
    uint8_t max_sectors = get_sectors_for_track(type, track);
    if (sector >= max_sectors) return SIZE_MAX;
    
    size_t offset = 0;
    
    switch (type) {
        case UFT_CBM_TYPE_D64:
        case UFT_CBM_TYPE_D64_40:
            if (track <= 40) {
                offset = (size_t)d64_track_offset[track] * UFT_CBM_SECTOR_SIZE;
                offset += (size_t)sector * UFT_CBM_SECTOR_SIZE;
            }
            break;
            
        case UFT_CBM_TYPE_D71:
        case UFT_CBM_TYPE_D71_80:
            if (track <= 35) {
                offset = (size_t)d64_track_offset[track] * UFT_CBM_SECTOR_SIZE;
                offset += (size_t)sector * UFT_CBM_SECTOR_SIZE;
            } else if (track <= 70) {
                /* Side 2 starts after side 1 */
                offset = (size_t)d64_track_offset[36] * UFT_CBM_SECTOR_SIZE;  /* End of side 1 */
                offset += (size_t)d64_track_offset[track - 35] * UFT_CBM_SECTOR_SIZE;
                offset += (size_t)sector * UFT_CBM_SECTOR_SIZE;
            }
            break;
            
        case UFT_CBM_TYPE_D81:
            if (track >= 1 && track <= 80) {
                offset = (size_t)(track - 1) * D81_SECTORS_PER_TRACK * UFT_CBM_SECTOR_SIZE;
                offset += (size_t)sector * UFT_CBM_SECTOR_SIZE;
            }
            break;
            
        default:
            return SIZE_MAX;
    }
    
    return offset;
}

static uint16_t get_total_blocks(uft_cbm_type_t type) {
    switch (type) {
        case UFT_CBM_TYPE_D64:    return 683;
        case UFT_CBM_TYPE_D64_40: return 768;
        case UFT_CBM_TYPE_D71:    return 1366;
        case UFT_CBM_TYPE_D71_80: return 1536;
        case UFT_CBM_TYPE_D81:    return 3200;
        default: return 0;
    }
}

static uint8_t get_max_tracks(uft_cbm_type_t type) {
    switch (type) {
        case UFT_CBM_TYPE_D64:    return 35;
        case UFT_CBM_TYPE_D64_40: return 40;
        case UFT_CBM_TYPE_D71:    return 70;
        case UFT_CBM_TYPE_D71_80: return 80;
        case UFT_CBM_TYPE_D81:    return 80;
        default: return 0;
    }
}

static uint8_t get_dir_track(uft_cbm_type_t type) {
    switch (type) {
        case UFT_CBM_TYPE_D64:
        case UFT_CBM_TYPE_D64_40:
        case UFT_CBM_TYPE_D71:
        case UFT_CBM_TYPE_D71_80:
            return UFT_CBM_D64_DIR_TRACK;
        case UFT_CBM_TYPE_D81:
            return UFT_CBM_D81_DIR_TRACK;
        default:
            return 0;
    }
}

static uint8_t get_dir_sector(uft_cbm_type_t type) {
    switch (type) {
        case UFT_CBM_TYPE_D64:
        case UFT_CBM_TYPE_D64_40:
        case UFT_CBM_TYPE_D71:
        case UFT_CBM_TYPE_D71_80:
            return UFT_CBM_D64_DIR_SECTOR;
        case UFT_CBM_TYPE_D81:
            return UFT_CBM_D81_DIR_SECTOR;
        default:
            return 0;
    }
}

/* Parse filename from directory entry, removing trailing shifted spaces */
static uint8_t parse_filename(const uint8_t* raw, uint8_t* out, size_t out_cap) {
    if (out_cap < UFT_CBM_FILENAME_MAX + 1) return 0;
    
    uint8_t len = 0;
    for (int i = 0; i < UFT_CBM_FILENAME_MAX; i++) {
        if (raw[i] == PETSCII_SHIFTED_SPACE) break;
        out[len++] = raw[i];
    }
    out[len] = '\0';
    return len;
}

/* Check if filename matches pattern (supports * and ?) */
static bool filename_matches(const uint8_t* filename, const char* pattern) {
    const uint8_t* f = filename;
    const uint8_t* p = (const uint8_t*)pattern;
    
    while (*f && *p) {
        if (*p == '*') {
            /* Match rest of string */
            return true;
        } else if (*p == '?' || *p == *f) {
            f++;
            p++;
        } else {
            return false;
        }
    }
    
    /* Handle trailing * */
    while (*p == '*') p++;
    
    return (*f == '\0' && *p == '\0');
}

/*============================================================================
 * Lifecycle Functions
 *============================================================================*/

uft_rc_t uft_cbm_fs_create(uft_cbm_fs_t** fs) {
    if (!fs) return UFT_ERR_INVALID_ARG;
    
    *fs = (uft_cbm_fs_t*)calloc(1, sizeof(uft_cbm_fs_t));
    if (!*fs) return UFT_ERR_MEMORY;
    
    (*fs)->type = UFT_CBM_TYPE_UNKNOWN;
    return UFT_SUCCESS;
}

void uft_cbm_fs_destroy(uft_cbm_fs_t** fs) {
    if (!fs || !*fs) return;
    
    uft_cbm_fs_t* f = *fs;
    
    free(f->image);
    free(f->error_table);
    free(f->path);
    
    if (f->bam) {
        free(f->bam->tracks);
        free(f->bam);
    }
    
    if (f->dir) {
        free(f->dir->entries);
        free(f->dir);
    }
    
    free(f);
    *fs = NULL;
}

/*============================================================================
 * Detection Functions
 *============================================================================*/

uft_rc_t uft_cbm_detect_type(const uint8_t* data, size_t size,
                              uft_cbm_type_t* type, bool* has_errors) {
    if (!data || !type) return UFT_ERR_INVALID_ARG;
    
    *type = UFT_CBM_TYPE_UNKNOWN;
    if (has_errors) *has_errors = false;
    
    /* Check by size */
    switch (size) {
        case UFT_CBM_D64_SIZE:
            *type = UFT_CBM_TYPE_D64;
            break;
        case UFT_CBM_D64_SIZE_ERR:
            *type = UFT_CBM_TYPE_D64;
            if (has_errors) *has_errors = true;
            break;
        case UFT_CBM_D64_EXT_SIZE:
            *type = UFT_CBM_TYPE_D64_40;
            break;
        case UFT_CBM_D64_EXT_SIZE_ERR:
            *type = UFT_CBM_TYPE_D64_40;
            if (has_errors) *has_errors = true;
            break;
        case UFT_CBM_D71_SIZE:
            *type = UFT_CBM_TYPE_D71;
            break;
        case UFT_CBM_D71_SIZE_ERR:
            *type = UFT_CBM_TYPE_D71;
            if (has_errors) *has_errors = true;
            break;
        case UFT_CBM_D81_SIZE:
            *type = UFT_CBM_TYPE_D81;
            break;
        case UFT_CBM_D81_SIZE_ERR:
            *type = UFT_CBM_TYPE_D81;
            if (has_errors) *has_errors = true;
            break;
        default:
            return UFT_ERR_FORMAT;
    }
    
    /* Validate BAM location */
    size_t bam_offset;
    if (*type == UFT_CBM_TYPE_D81) {
        bam_offset = get_sector_offset(*type, UFT_CBM_D81_BAM_TRACK, UFT_CBM_D81_BAM_SECTOR);
    } else {
        bam_offset = get_sector_offset(*type, UFT_CBM_D64_BAM_TRACK, UFT_CBM_D64_BAM_SECTOR);
    }
    
    if (bam_offset == SIZE_MAX || bam_offset + UFT_CBM_SECTOR_SIZE > size) {
        return UFT_ERR_FORMAT;
    }
    
    return UFT_SUCCESS;
}

const char* uft_cbm_type_name(uft_cbm_type_t type) {
    switch (type) {
        case UFT_CBM_TYPE_D64:    return "D64 (1541)";
        case UFT_CBM_TYPE_D64_40: return "D64 (40-track)";
        case UFT_CBM_TYPE_D71:    return "D71 (1571)";
        case UFT_CBM_TYPE_D71_80: return "D71 (80-track)";
        case UFT_CBM_TYPE_D81:    return "D81 (1581)";
        case UFT_CBM_TYPE_G64:    return "G64 (GCR)";
        case UFT_CBM_TYPE_G71:    return "G71 (GCR 1571)";
        default:                  return "Unknown";
    }
}

const char* uft_cbm_filetype_name(uft_cbm_filetype_t ft) {
    switch (ft) {
        case UFT_CBM_FT_DEL: return "DEL";
        case UFT_CBM_FT_SEQ: return "SEQ";
        case UFT_CBM_FT_PRG: return "PRG";
        case UFT_CBM_FT_USR: return "USR";
        case UFT_CBM_FT_REL: return "REL";
        case UFT_CBM_FT_CBM: return "CBM";
        case UFT_CBM_FT_DIR: return "DIR";
        default:             return "???";
    }
}

/*============================================================================
 * File Open/Close
 *============================================================================*/

uft_rc_t uft_cbm_fs_open(uft_cbm_fs_t* fs, const char* path, bool writable) {
    if (!fs || !path) return UFT_ERR_INVALID_ARG;
    
    /* Read file */
    FILE* f = fopen(path, "rb");
    if (!f) return UFT_ERR_FILE_NOT_FOUND;
    
    if (fseek(f, 0, SEEK_END) != 0) { /* seek error */ }
    long file_size = ftell(f);
    if (fseek(f, 0, SEEK_SET) != 0) { /* seek error */ }
    if (file_size < 0 || file_size > 10 * 1024 * 1024) {  /* Max 10MB */
        fclose(f);
        return UFT_ERR_FORMAT;
    }
    
    uint8_t* data = (uint8_t*)malloc((size_t)file_size);
    if (!data) {
        fclose(f);
        return UFT_ERR_MEMORY;
    }
    
    size_t read = fread(data, 1, (size_t)file_size, f);
    fclose(f);
    
    if (read != (size_t)file_size) {
        free(data);
        return UFT_ERR_IO;
    }
    
    /* Detect type */
    uft_cbm_type_t type;
    bool has_errors;
    uft_rc_t rc = uft_cbm_detect_type(data, (size_t)file_size, &type, &has_errors);
    if (rc != UFT_SUCCESS) {
        free(data);
        return rc;
    }
    
    /* Close any existing image */
    if (fs->image) {
        free(fs->image);
        fs->image = NULL;
    }
    free(fs->error_table);
    fs->error_table = NULL;
    free(fs->path);
    fs->path = NULL;
    
    /* Store image */
    fs->image = data;
    fs->type = type;
    fs->has_errors = has_errors;
    fs->writable = writable;
    fs->modified = false;
    fs->tracks = get_max_tracks(type);
    fs->path = strdup(path);
    
    /* Calculate data size (without error table) */
    size_t data_size;
    switch (type) {
        case UFT_CBM_TYPE_D64:    data_size = UFT_CBM_D64_SIZE; break;
        case UFT_CBM_TYPE_D64_40: data_size = UFT_CBM_D64_EXT_SIZE; break;
        case UFT_CBM_TYPE_D71:    data_size = UFT_CBM_D71_SIZE; break;
        case UFT_CBM_TYPE_D81:    data_size = UFT_CBM_D81_SIZE; break;
        default:                  data_size = (size_t)file_size; break;
    }
    fs->image_size = data_size;
    
    /* Extract error table if present */
    if (has_errors && (size_t)file_size > data_size) {
        size_t error_size = (size_t)file_size - data_size;
        fs->error_table = (uint8_t*)malloc(error_size);
        if (fs->error_table) {
            memcpy(fs->error_table, data + data_size, error_size);
        }
    }
    
    /* Load BAM and directory */
    uft_cbm_bam_load(fs);
    uft_cbm_dir_load(fs);
    
    return UFT_SUCCESS;
}

uft_rc_t uft_cbm_fs_open_mem(uft_cbm_fs_t* fs, const uint8_t* data, 
                              size_t size, bool writable) {
    if (!fs || !data || size == 0) return UFT_ERR_INVALID_ARG;
    
    /* Detect type */
    uft_cbm_type_t type;
    bool has_errors;
    uft_rc_t rc = uft_cbm_detect_type(data, size, &type, &has_errors);
    if (rc != UFT_SUCCESS) return rc;
    
    /* Allocate and copy */
    uint8_t* copy = (uint8_t*)malloc(size);
    if (!copy) return UFT_ERR_MEMORY;
    memcpy(copy, data, size);
    
    /* Close existing */
    free(fs->image);
    free(fs->error_table);
    free(fs->path);
    
    fs->image = copy;
    fs->image_size = size;
    fs->type = type;
    fs->has_errors = has_errors;
    fs->writable = writable;
    fs->modified = false;
    fs->tracks = get_max_tracks(type);
    fs->path = NULL;
    fs->error_table = NULL;
    
    /* Load BAM and directory */
    uft_cbm_bam_load(fs);
    uft_cbm_dir_load(fs);
    
    return UFT_SUCCESS;
}

uft_rc_t uft_cbm_fs_save(uft_cbm_fs_t* fs) {
    if (!fs || !fs->image) return UFT_ERR_INVALID_ARG;
    if (!fs->path) return UFT_ERR_INVALID_STATE;
    if (!fs->writable) return UFT_ERR_NOT_PERMITTED;
    
    /* Write BAM if modified */
    if (fs->bam && fs->bam->modified) {
        uft_cbm_bam_save(fs);
    }
    
    FILE* f = fopen(fs->path, "wb");
    if (!f) return UFT_ERR_IO;
    
    size_t written = fwrite(fs->image, 1, fs->image_size, f);
    
    /* Write error table if present */
    if (fs->error_table && fs->has_errors) {
        size_t error_size = 0;
        switch (fs->type) {
            case UFT_CBM_TYPE_D64:    error_size = 683; break;
            case UFT_CBM_TYPE_D64_40: error_size = 768; break;
            case UFT_CBM_TYPE_D71:    error_size = 1366; break;
            default: break;
        }
        if (error_size > 0) {
            if (fwrite(fs->error_table, 1, error_size, f) != error_size) { /* I/O error */ }
        }
    }
    
    fclose(f);
    
    if (written != fs->image_size) return UFT_ERR_IO;
    
    fs->modified = false;
    return UFT_SUCCESS;
}

uft_rc_t uft_cbm_fs_save_as(uft_cbm_fs_t* fs, const char* path) {
    if (!fs || !path) return UFT_ERR_INVALID_ARG;
    
    char* old_path = fs->path;
    fs->path = strdup(path);
    if (!fs->path) {
        fs->path = old_path;
        return UFT_ERR_MEMORY;
    }
    
    bool old_writable = fs->writable;
    fs->writable = true;
    
    uft_rc_t rc = uft_cbm_fs_save(fs);
    
    if (rc != UFT_SUCCESS) {
        free(fs->path);
        fs->path = old_path;
        fs->writable = old_writable;
    } else {
        free(old_path);
    }
    
    return rc;
}

uft_rc_t uft_cbm_fs_close(uft_cbm_fs_t* fs) {
    if (!fs) return UFT_ERR_INVALID_ARG;
    
    /* Save if modified and writable */
    if (fs->modified && fs->writable && fs->path) {
        uft_cbm_fs_save(fs);
    }
    
    free(fs->image);
    fs->image = NULL;
    fs->image_size = 0;
    
    free(fs->error_table);
    fs->error_table = NULL;
    
    free(fs->path);
    fs->path = NULL;
    
    if (fs->bam) {
        free(fs->bam->tracks);
        free(fs->bam);
        fs->bam = NULL;
    }
    
    if (fs->dir) {
        free(fs->dir->entries);
        free(fs->dir);
        fs->dir = NULL;
    }
    
    fs->type = UFT_CBM_TYPE_UNKNOWN;
    fs->modified = false;
    
    return UFT_SUCCESS;
}

/*============================================================================
 * Sector Access Functions
 *============================================================================*/

uint8_t uft_cbm_sectors_per_track(uft_cbm_type_t type, uint8_t track) {
    return get_sectors_for_track(type, track);
}

size_t uft_cbm_sector_offset(uft_cbm_type_t type, uint8_t track, uint8_t sector) {
    return get_sector_offset(type, track, sector);
}

uft_rc_t uft_cbm_read_sector(uft_cbm_fs_t* fs, uint8_t track, uint8_t sector,
                              uint8_t buffer[UFT_CBM_SECTOR_SIZE]) {
    if (!fs || !fs->image || !buffer) return UFT_ERR_INVALID_ARG;
    
    size_t offset = get_sector_offset(fs->type, track, sector);
    if (offset == SIZE_MAX || offset + UFT_CBM_SECTOR_SIZE > fs->image_size) {
        return UFT_ERR_INVALID_ARG;
    }
    
    memcpy(buffer, fs->image + offset, UFT_CBM_SECTOR_SIZE);
    return UFT_SUCCESS;
}

uft_rc_t uft_cbm_write_sector(uft_cbm_fs_t* fs, uint8_t track, uint8_t sector,
                               const uint8_t buffer[UFT_CBM_SECTOR_SIZE]) {
    if (!fs || !fs->image || !buffer) return UFT_ERR_INVALID_ARG;
    if (!fs->writable) return UFT_ERR_NOT_PERMITTED;
    
    size_t offset = get_sector_offset(fs->type, track, sector);
    if (offset == SIZE_MAX || offset + UFT_CBM_SECTOR_SIZE > fs->image_size) {
        return UFT_ERR_INVALID_ARG;
    }
    
    memcpy(fs->image + offset, buffer, UFT_CBM_SECTOR_SIZE);
    fs->modified = true;
    return UFT_SUCCESS;
}

uint8_t uft_cbm_sector_error(uft_cbm_fs_t* fs, uint8_t track, uint8_t sector) {
    if (!fs || !fs->error_table) return 0xFF;
    
    /* Calculate error index (linearized sector number) */
    size_t index = 0;
    for (uint8_t t = 1; t < track; t++) {
        index += get_sectors_for_track(fs->type, t);
    }
    index += sector;
    
    /* Check bounds */
    size_t max_errors = get_total_blocks(fs->type);
    if (index >= max_errors) return 0xFF;
    
    return fs->error_table[index];
}
