/**
 * @file uft_cbm_fs_bam.c
 * @brief Commodore CBM DOS Filesystem - BAM and Directory Implementation
 * 
 * @version 1.0.0
 * @date 2026-01-04
 * @copyright UFT Project
 */

#include "uft/fs/uft_cbm_fs.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/*============================================================================
 * External References (from uft_cbm_fs.c)
 *============================================================================*/

extern uint8_t uft_cbm_sectors_per_track(uft_cbm_type_t type, uint8_t track);
extern size_t uft_cbm_sector_offset(uft_cbm_type_t type, uint8_t track, uint8_t sector);

/*============================================================================
 * BAM Functions
 *============================================================================*/

uft_rc_t uft_cbm_bam_load(uft_cbm_fs_t* fs) {
    if (!fs || !fs->image) return UFT_ERR_INVALID_ARG;
    
    /* Allocate BAM if needed */
    if (!fs->bam) {
        fs->bam = (uft_cbm_bam_t*)calloc(1, sizeof(uft_cbm_bam_t));
        if (!fs->bam) return UFT_ERR_MEMORY;
    }
    
    fs->bam->type = fs->type;
    fs->bam->total_tracks = fs->tracks;
    fs->bam->modified = false;
    
    /* Allocate track entries */
    if (fs->bam->tracks) free(fs->bam->tracks);
    fs->bam->tracks = (uft_cbm_bam_track_t*)calloc(fs->tracks + 1, sizeof(uft_cbm_bam_track_t));
    if (!fs->bam->tracks) return UFT_ERR_MEMORY;
    
    uint8_t sector[UFT_CBM_SECTOR_SIZE];
    
    switch (fs->type) {
        case UFT_CBM_TYPE_D64:
        case UFT_CBM_TYPE_D64_40: {
            /* Read BAM sector (track 18, sector 0) */
            uft_rc_t rc = uft_cbm_read_sector(fs, 18, 0, sector);
            if (rc != UFT_SUCCESS) return rc;
            
            /* BAM entries start at offset 4 */
            /* Each entry: free count (1 byte) + bitmap (3 bytes) = 4 bytes per track */
            uint16_t total_free = 0;
            uint8_t max_track = (fs->type == UFT_CBM_TYPE_D64) ? 35 : 40;
            
            for (uint8_t t = 1; t <= max_track; t++) {
                uint8_t offset = 4 + (t - 1) * 4;
                
                fs->bam->tracks[t].track = t;
                fs->bam->tracks[t].free_sectors = sector[offset];
                fs->bam->tracks[t].bitmap[0] = sector[offset + 1];
                fs->bam->tracks[t].bitmap[1] = sector[offset + 2];
                fs->bam->tracks[t].bitmap[2] = sector[offset + 3];
                
                /* Don't count directory track in free blocks */
                if (t != 18) {
                    total_free += fs->bam->tracks[t].free_sectors;
                }
            }
            
            fs->bam->total_free = total_free;
            break;
        }
        
        case UFT_CBM_TYPE_D71:
        case UFT_CBM_TYPE_D71_80: {
            /* Side 1 BAM at track 18, sector 0 */
            uft_rc_t rc = uft_cbm_read_sector(fs, 18, 0, sector);
            if (rc != UFT_SUCCESS) return rc;
            
            uint16_t total_free = 0;
            
            /* Side 1: tracks 1-35 */
            for (uint8_t t = 1; t <= 35; t++) {
                uint8_t offset = 4 + (t - 1) * 4;
                
                fs->bam->tracks[t].track = t;
                fs->bam->tracks[t].free_sectors = sector[offset];
                fs->bam->tracks[t].bitmap[0] = sector[offset + 1];
                fs->bam->tracks[t].bitmap[1] = sector[offset + 2];
                fs->bam->tracks[t].bitmap[2] = sector[offset + 3];
                
                if (t != 18) {
                    total_free += fs->bam->tracks[t].free_sectors;
                }
            }
            
            /* Side 2 BAM at track 53, sector 0 */
            rc = uft_cbm_read_sector(fs, 53, 0, sector);
            if (rc != UFT_SUCCESS) return rc;
            
            /* Side 2: tracks 36-70 */
            for (uint8_t t = 36; t <= 70; t++) {
                uint8_t offset = (t - 36) * 3;  /* No free count byte in D71 side 2 */
                
                fs->bam->tracks[t].track = t;
                fs->bam->tracks[t].bitmap[0] = sector[offset];
                fs->bam->tracks[t].bitmap[1] = sector[offset + 1];
                fs->bam->tracks[t].bitmap[2] = sector[offset + 2];
                
                /* Count free sectors from bitmap */
                uint8_t free_count = 0;
                uint8_t sectors = uft_cbm_sectors_per_track(fs->type, t);
                for (uint8_t s = 0; s < sectors; s++) {
                    uint8_t byte_idx = s / 8;
                    uint8_t bit_idx = s % 8;
                    if (fs->bam->tracks[t].bitmap[byte_idx] & (1 << bit_idx)) {
                        free_count++;
                    }
                }
                fs->bam->tracks[t].free_sectors = free_count;
                
                if (t != 53) {
                    total_free += free_count;
                }
            }
            
            fs->bam->total_free = total_free;
            break;
        }
        
        case UFT_CBM_TYPE_D81: {
            /* D81 BAM at track 40, sectors 1-2 */
            uint8_t bam1[UFT_CBM_SECTOR_SIZE], bam2[UFT_CBM_SECTOR_SIZE];
            
            uft_rc_t rc = uft_cbm_read_sector(fs, 40, 1, bam1);
            if (rc != UFT_SUCCESS) return rc;
            
            rc = uft_cbm_read_sector(fs, 40, 2, bam2);
            if (rc != UFT_SUCCESS) return rc;
            
            uint16_t total_free = 0;
            
            /* D81: 6 bytes per track (free count + 5 bitmap bytes for 40 sectors) */
            for (uint8_t t = 1; t <= 40; t++) {
                uint8_t offset = 16 + (t - 1) * 6;
                
                fs->bam->tracks[t].track = t;
                fs->bam->tracks[t].free_sectors = bam1[offset];
                /* Note: D81 uses 5 bytes for bitmap, we only store first 3 */
                fs->bam->tracks[t].bitmap[0] = bam1[offset + 1];
                fs->bam->tracks[t].bitmap[1] = bam1[offset + 2];
                fs->bam->tracks[t].bitmap[2] = bam1[offset + 3];
                
                if (t != 40) {
                    total_free += fs->bam->tracks[t].free_sectors;
                }
            }
            
            for (uint8_t t = 41; t <= 80; t++) {
                uint8_t offset = 16 + (t - 41) * 6;
                
                fs->bam->tracks[t].track = t;
                fs->bam->tracks[t].free_sectors = bam2[offset];
                fs->bam->tracks[t].bitmap[0] = bam2[offset + 1];
                fs->bam->tracks[t].bitmap[1] = bam2[offset + 2];
                fs->bam->tracks[t].bitmap[2] = bam2[offset + 3];
                
                total_free += fs->bam->tracks[t].free_sectors;
            }
            
            fs->bam->total_free = total_free;
            break;
        }
        
        default:
            return UFT_ERR_FORMAT;
    }
    
    /* Calculate total blocks */
    fs->bam->total_blocks = 0;
    for (uint8_t t = 1; t <= fs->tracks; t++) {
        fs->bam->total_blocks += uft_cbm_sectors_per_track(fs->type, t);
    }
    
    return UFT_SUCCESS;
}

uft_rc_t uft_cbm_bam_save(uft_cbm_fs_t* fs) {
    if (!fs || !fs->image || !fs->bam) return UFT_ERR_INVALID_ARG;
    if (!fs->writable) return UFT_ERR_NOT_PERMITTED;
    
    uint8_t sector[UFT_CBM_SECTOR_SIZE];
    
    switch (fs->type) {
        case UFT_CBM_TYPE_D64:
        case UFT_CBM_TYPE_D64_40: {
            /* Read existing BAM to preserve header */
            uft_cbm_read_sector(fs, 18, 0, sector);
            
            uint8_t max_track = (fs->type == UFT_CBM_TYPE_D64) ? 35 : 40;
            for (uint8_t t = 1; t <= max_track; t++) {
                uint8_t offset = 4 + (t - 1) * 4;
                sector[offset] = fs->bam->tracks[t].free_sectors;
                sector[offset + 1] = fs->bam->tracks[t].bitmap[0];
                sector[offset + 2] = fs->bam->tracks[t].bitmap[1];
                sector[offset + 3] = fs->bam->tracks[t].bitmap[2];
            }
            
            uft_cbm_write_sector(fs, 18, 0, sector);
            break;
        }
        
        case UFT_CBM_TYPE_D71:
        case UFT_CBM_TYPE_D71_80: {
            /* Side 1 */
            uft_cbm_read_sector(fs, 18, 0, sector);
            for (uint8_t t = 1; t <= 35; t++) {
                uint8_t offset = 4 + (t - 1) * 4;
                sector[offset] = fs->bam->tracks[t].free_sectors;
                sector[offset + 1] = fs->bam->tracks[t].bitmap[0];
                sector[offset + 2] = fs->bam->tracks[t].bitmap[1];
                sector[offset + 3] = fs->bam->tracks[t].bitmap[2];
            }
            uft_cbm_write_sector(fs, 18, 0, sector);
            
            /* Side 2 */
            uft_cbm_read_sector(fs, 53, 0, sector);
            for (uint8_t t = 36; t <= 70; t++) {
                uint8_t offset = (t - 36) * 3;
                sector[offset] = fs->bam->tracks[t].bitmap[0];
                sector[offset + 1] = fs->bam->tracks[t].bitmap[1];
                sector[offset + 2] = fs->bam->tracks[t].bitmap[2];
            }
            uft_cbm_write_sector(fs, 53, 0, sector);
            break;
        }
        
        case UFT_CBM_TYPE_D81: {
            uint8_t bam1[UFT_CBM_SECTOR_SIZE], bam2[UFT_CBM_SECTOR_SIZE];
            
            uft_cbm_read_sector(fs, 40, 1, bam1);
            uft_cbm_read_sector(fs, 40, 2, bam2);
            
            for (uint8_t t = 1; t <= 40; t++) {
                uint8_t offset = 16 + (t - 1) * 6;
                bam1[offset] = fs->bam->tracks[t].free_sectors;
                bam1[offset + 1] = fs->bam->tracks[t].bitmap[0];
                bam1[offset + 2] = fs->bam->tracks[t].bitmap[1];
                bam1[offset + 3] = fs->bam->tracks[t].bitmap[2];
            }
            
            for (uint8_t t = 41; t <= 80; t++) {
                uint8_t offset = 16 + (t - 41) * 6;
                bam2[offset] = fs->bam->tracks[t].free_sectors;
                bam2[offset + 1] = fs->bam->tracks[t].bitmap[0];
                bam2[offset + 2] = fs->bam->tracks[t].bitmap[1];
                bam2[offset + 3] = fs->bam->tracks[t].bitmap[2];
            }
            
            uft_cbm_write_sector(fs, 40, 1, bam1);
            uft_cbm_write_sector(fs, 40, 2, bam2);
            break;
        }
        
        default:
            return UFT_ERR_FORMAT;
    }
    
    fs->bam->modified = false;
    return UFT_SUCCESS;
}

bool uft_cbm_bam_is_allocated(uft_cbm_fs_t* fs, uint8_t track, uint8_t sector) {
    if (!fs || !fs->bam || track == 0 || track > fs->tracks) return true;
    
    uint8_t max_sectors = uft_cbm_sectors_per_track(fs->type, track);
    if (sector >= max_sectors) return true;
    
    uint8_t byte_idx = sector / 8;
    uint8_t bit_idx = sector % 8;
    
    /* In CBM BAM: 1 = free, 0 = allocated */
    return !(fs->bam->tracks[track].bitmap[byte_idx] & (1 << bit_idx));
}

uft_rc_t uft_cbm_bam_allocate(uft_cbm_fs_t* fs, uint8_t track, uint8_t sector) {
    if (!fs || !fs->bam || track == 0 || track > fs->tracks) return UFT_ERR_INVALID_ARG;
    if (!fs->writable) return UFT_ERR_NOT_PERMITTED;
    
    uint8_t max_sectors = uft_cbm_sectors_per_track(fs->type, track);
    if (sector >= max_sectors) return UFT_ERR_INVALID_ARG;
    
    uint8_t byte_idx = sector / 8;
    uint8_t bit_idx = sector % 8;
    
    /* Check if already allocated */
    if (!(fs->bam->tracks[track].bitmap[byte_idx] & (1 << bit_idx))) {
        return UFT_SUCCESS;  /* Already allocated */
    }
    
    /* Clear bit to allocate */
    fs->bam->tracks[track].bitmap[byte_idx] &= ~(1 << bit_idx);
    fs->bam->tracks[track].free_sectors--;
    fs->bam->total_free--;
    fs->bam->modified = true;
    
    return UFT_SUCCESS;
}

uft_rc_t uft_cbm_bam_free(uft_cbm_fs_t* fs, uint8_t track, uint8_t sector) {
    if (!fs || !fs->bam || track == 0 || track > fs->tracks) return UFT_ERR_INVALID_ARG;
    if (!fs->writable) return UFT_ERR_NOT_PERMITTED;
    
    uint8_t max_sectors = uft_cbm_sectors_per_track(fs->type, track);
    if (sector >= max_sectors) return UFT_ERR_INVALID_ARG;
    
    uint8_t byte_idx = sector / 8;
    uint8_t bit_idx = sector % 8;
    
    /* Check if already free */
    if (fs->bam->tracks[track].bitmap[byte_idx] & (1 << bit_idx)) {
        return UFT_SUCCESS;  /* Already free */
    }
    
    /* Set bit to free */
    fs->bam->tracks[track].bitmap[byte_idx] |= (1 << bit_idx);
    fs->bam->tracks[track].free_sectors++;
    fs->bam->total_free++;
    fs->bam->modified = true;
    
    return UFT_SUCCESS;
}

uft_rc_t uft_cbm_bam_alloc_next(uft_cbm_fs_t* fs, uint8_t near_track,
                                 uint8_t* track, uint8_t* sector,
                                 uint8_t interleave) {
    if (!fs || !fs->bam || !track || !sector) return UFT_ERR_INVALID_ARG;
    if (!fs->writable) return UFT_ERR_NOT_PERMITTED;
    
    if (interleave == 0) {
        switch (fs->type) {
            case UFT_CBM_TYPE_D64:
            case UFT_CBM_TYPE_D64_40:
                interleave = 10;
                break;
            case UFT_CBM_TYPE_D71:
            case UFT_CBM_TYPE_D71_80:
                interleave = 6;
                break;
            case UFT_CBM_TYPE_D81:
                interleave = 1;
                break;
            default:
                interleave = 10;
        }
    }
    
    /* Start from preferred track */
    if (near_track == 0 || near_track > fs->tracks) {
        near_track = 1;
    }
    
    /* Search outward from preferred track */
    for (uint8_t delta = 0; delta <= fs->tracks; delta++) {
        for (int dir = -1; dir <= 1; dir += 2) {
            int t = near_track + delta * dir;
            if (t < 1 || t > fs->tracks) continue;
            
            /* Skip directory track */
            if ((fs->type == UFT_CBM_TYPE_D64 || fs->type == UFT_CBM_TYPE_D64_40 ||
                 fs->type == UFT_CBM_TYPE_D71 || fs->type == UFT_CBM_TYPE_D71_80) && t == 18) {
                continue;
            }
            if (fs->type == UFT_CBM_TYPE_D81 && t == 40) {
                continue;
            }
            
            if (fs->bam->tracks[t].free_sectors > 0) {
                /* Find free sector with interleave */
                uint8_t max_sectors = uft_cbm_sectors_per_track(fs->type, (uint8_t)t);
                for (uint8_t s = 0; s < max_sectors; s++) {
                    if (!uft_cbm_bam_is_allocated(fs, (uint8_t)t, s)) {
                        *track = (uint8_t)t;
                        *sector = s;
                        return uft_cbm_bam_allocate(fs, *track, *sector);
                    }
                }
            }
            
            if (delta == 0) break;  /* Only check once for delta=0 */
        }
    }
    
    return UFT_ERR_DISK_FULL;
}

uint16_t uft_cbm_bam_free_blocks(uft_cbm_fs_t* fs) {
    if (!fs || !fs->bam) return 0;
    return fs->bam->total_free;
}

uint16_t uft_cbm_bam_total_blocks(uft_cbm_fs_t* fs) {
    if (!fs || !fs->bam) return 0;
    return fs->bam->total_blocks;
}

/*============================================================================
 * Directory Functions
 *============================================================================*/

/* Helper to parse a single directory entry */
static void parse_dir_entry(const uint8_t* raw, uint8_t track, uint8_t sector,
                            uint8_t slot, uft_cbm_dir_entry_t* entry) {
    memset(entry, 0, sizeof(*entry));
    
    /* Raw entry is 32 bytes */
    entry->type_byte = raw[2];
    entry->file_type = (uft_cbm_filetype_t)(raw[2] & 0x07);
    entry->flags = raw[2] & 0xF8;
    
    entry->first_ts.track = raw[3];
    entry->first_ts.sector = raw[4];
    
    /* Parse filename */
    entry->filename_len = 0;
    for (int i = 0; i < UFT_CBM_FILENAME_MAX; i++) {
        if (raw[5 + i] == 0xA0) break;  /* Shifted space = end */
        entry->filename[entry->filename_len++] = raw[5 + i];
    }
    entry->filename[entry->filename_len] = '\0';
    
    /* REL file info */
    entry->side_ts.track = raw[21];
    entry->side_ts.sector = raw[22];
    entry->rel_record_len = raw[23];
    
    /* GEOS info */
    entry->geos_type = (uft_geos_type_t)raw[24];
    entry->geos_struct = (uft_geos_struct_t)raw[25];
    entry->geos_info_ts.track = raw[26];
    entry->geos_info_ts.sector = raw[27];
    
    /* Block count */
    entry->blocks = raw[30] | ((uint16_t)raw[31] << 8);
    
    /* Entry location */
    entry->entry_ts.track = track;
    entry->entry_ts.sector = sector;
    entry->entry_offset = slot;
}

uft_rc_t uft_cbm_dir_load(uft_cbm_fs_t* fs) {
    if (!fs || !fs->image) return UFT_ERR_INVALID_ARG;
    
    /* Allocate directory if needed */
    if (!fs->dir) {
        fs->dir = (uft_cbm_directory_t*)calloc(1, sizeof(uft_cbm_directory_t));
        if (!fs->dir) return UFT_ERR_MEMORY;
    }
    
    /* Free existing entries */
    free(fs->dir->entries);
    fs->dir->entries = NULL;
    fs->dir->count = 0;
    
    /* Determine max entries */
    uint16_t max_entries;
    switch (fs->type) {
        case UFT_CBM_TYPE_D64:
        case UFT_CBM_TYPE_D64_40:
            max_entries = UFT_CBM_D64_MAX_ENTRIES;
            break;
        case UFT_CBM_TYPE_D71:
        case UFT_CBM_TYPE_D71_80:
            max_entries = UFT_CBM_D71_MAX_ENTRIES;
            break;
        case UFT_CBM_TYPE_D81:
            max_entries = UFT_CBM_D81_MAX_ENTRIES;
            break;
        default:
            return UFT_ERR_FORMAT;
    }
    
    fs->dir->entries = (uft_cbm_dir_entry_t*)calloc(max_entries, sizeof(uft_cbm_dir_entry_t));
    if (!fs->dir->entries) return UFT_ERR_MEMORY;
    fs->dir->capacity = max_entries;
    
    /* Read header/BAM sector for disk name/ID */
    uint8_t bam_sector[UFT_CBM_SECTOR_SIZE];
    uint8_t bam_track, bam_sec;
    
    if (fs->type == UFT_CBM_TYPE_D81) {
        bam_track = 40;
        bam_sec = 0;
    } else {
        bam_track = 18;
        bam_sec = 0;
    }
    
    uft_cbm_read_sector(fs, bam_track, bam_sec, bam_sector);
    
    /* Extract disk name and ID */
    if (fs->type == UFT_CBM_TYPE_D81) {
        /* D81: disk name at offset 4, ID at 22 */
        for (int i = 0; i < UFT_CBM_FILENAME_MAX; i++) {
            if (bam_sector[4 + i] == 0xA0) break;
            fs->dir->disk_name[i] = bam_sector[4 + i];
        }
        fs->dir->disk_id[0] = bam_sector[22];
        fs->dir->disk_id[1] = bam_sector[23];
        fs->dir->dos_type[0] = bam_sector[25];
        fs->dir->dos_type[1] = bam_sector[26];
    } else {
        /* D64/D71: disk name at offset 0x90, ID at 0xA2 */
        for (int i = 0; i < UFT_CBM_FILENAME_MAX; i++) {
            if (bam_sector[0x90 + i] == 0xA0) break;
            fs->dir->disk_name[i] = bam_sector[0x90 + i];
        }
        fs->dir->disk_id[0] = bam_sector[0xA2];
        fs->dir->disk_id[1] = bam_sector[0xA3];
        fs->dir->dos_type[0] = bam_sector[0xA5];
        fs->dir->dos_type[1] = bam_sector[0xA6];
    }
    fs->dir->disk_name[UFT_CBM_FILENAME_MAX] = '\0';
    fs->dir->disk_id[2] = '\0';
    fs->dir->dos_type[2] = '\0';
    
    /* Follow directory chain */
    uint8_t dir_track, dir_sector;
    if (fs->type == UFT_CBM_TYPE_D81) {
        dir_track = 40;
        dir_sector = 3;
    } else {
        dir_track = 18;
        dir_sector = 1;
    }
    
    uint16_t entry_idx = 0;
    uint16_t sector_count = 0;
    
    while (dir_track != 0 && sector_count < 100) {  /* Limit to prevent infinite loop */
        uint8_t sector_data[UFT_CBM_SECTOR_SIZE];
        uft_rc_t rc = uft_cbm_read_sector(fs, dir_track, dir_sector, sector_data);
        if (rc != UFT_SUCCESS) break;
        
        /* Parse 8 directory entries per sector */
        for (int slot = 0; slot < 8 && entry_idx < max_entries; slot++) {
            uint8_t* entry_raw = &sector_data[slot * 32];
            
            /* Skip empty entries (file type 0) */
            if (entry_raw[2] == 0) continue;
            
            uft_cbm_dir_entry_t* entry = &fs->dir->entries[entry_idx];
            entry->index = entry_idx;
            
            parse_dir_entry(entry_raw, dir_track, dir_sector, (uint8_t)slot, entry);
            entry_idx++;
        }
        
        /* Follow chain */
        uint8_t next_track = sector_data[0];
        uint8_t next_sector = sector_data[1];
        
        if (next_track == 0) break;
        if (next_track == dir_track && next_sector == dir_sector) break;  /* Circular */
        
        dir_track = next_track;
        dir_sector = next_sector;
        sector_count++;
    }
    
    fs->dir->count = entry_idx;
    fs->dir->blocks_free = uft_cbm_bam_free_blocks(fs);
    fs->dir->blocks_total = uft_cbm_bam_total_blocks(fs);
    
    return UFT_SUCCESS;
}

const uft_cbm_directory_t* uft_cbm_dir_get(uft_cbm_fs_t* fs) {
    if (!fs) return NULL;
    return fs->dir;
}

uft_rc_t uft_cbm_dir_foreach(uft_cbm_fs_t* fs, uft_cbm_dir_callback_t callback,
                              void* user_data) {
    if (!fs || !fs->dir || !callback) return UFT_ERR_INVALID_ARG;
    
    for (uint16_t i = 0; i < fs->dir->count; i++) {
        if (!callback(&fs->dir->entries[i], user_data)) {
            break;
        }
    }
    
    return UFT_SUCCESS;
}

/* Helper for wildcard matching */
static bool filename_matches(const uint8_t* filename, const char* pattern) {
    const uint8_t* f = filename;
    const uint8_t* p = (const uint8_t*)pattern;
    
    while (*f && *p) {
        if (*p == '*') {
            return true;
        } else if (*p == '?' || *p == *f) {
            f++;
            p++;
        } else {
            return false;
        }
    }
    
    while (*p == '*') p++;
    
    return (*f == '\0' && *p == '\0');
}

uft_rc_t uft_cbm_dir_find(uft_cbm_fs_t* fs, const char* filename,
                           uft_cbm_dir_entry_t* entry) {
    if (!fs || !fs->dir || !filename || !entry) return UFT_ERR_INVALID_ARG;
    
    for (uint16_t i = 0; i < fs->dir->count; i++) {
        if (filename_matches(fs->dir->entries[i].filename, filename)) {
            memcpy(entry, &fs->dir->entries[i], sizeof(*entry));
            return UFT_SUCCESS;
        }
    }
    
    return UFT_ERR_NOT_FOUND;
}

uft_rc_t uft_cbm_dir_get_entry(uft_cbm_fs_t* fs, uint16_t index,
                                uft_cbm_dir_entry_t* entry) {
    if (!fs || !fs->dir || !entry) return UFT_ERR_INVALID_ARG;
    if (index >= fs->dir->count) return UFT_ERR_INVALID_ARG;
    
    memcpy(entry, &fs->dir->entries[index], sizeof(*entry));
    return UFT_SUCCESS;
}

uint16_t uft_cbm_dir_count(uft_cbm_fs_t* fs) {
    if (!fs || !fs->dir) return 0;
    return fs->dir->count;
}

/*============================================================================
 * PETSCII Conversion
 *============================================================================*/

size_t uft_cbm_petscii_to_ascii(const uint8_t* petscii, size_t petscii_len,
                                 char* ascii, size_t ascii_cap) {
    if (!petscii || !ascii || ascii_cap == 0) return 0;
    
    size_t out_len = 0;
    for (size_t i = 0; i < petscii_len && out_len + 1 < ascii_cap; i++) {
        uint8_t c = petscii[i];
        
        if (c == 0xA0) {
            /* Shifted space = end of filename */
            break;
        } else if (c >= 0x41 && c <= 0x5A) {
            /* Uppercase PETSCII -> lowercase ASCII */
            ascii[out_len++] = (char)(c + 32);
        } else if (c >= 0xC1 && c <= 0xDA) {
            /* Shifted letters -> uppercase ASCII */
            ascii[out_len++] = (char)(c - 0x80);
        } else if (c >= 0x20 && c <= 0x7E) {
            /* Printable ASCII range */
            ascii[out_len++] = (char)c;
        } else {
            /* Non-printable -> underscore */
            ascii[out_len++] = '_';
        }
    }
    
    ascii[out_len] = '\0';
    return out_len;
}

size_t uft_cbm_ascii_to_petscii(const char* ascii, uint8_t* petscii,
                                 size_t petscii_cap) {
    if (!ascii || !petscii || petscii_cap == 0) return 0;
    
    size_t out_len = 0;
    for (size_t i = 0; ascii[i] && out_len < petscii_cap; i++) {
        char c = ascii[i];
        
        if (c >= 'a' && c <= 'z') {
            /* Lowercase ASCII -> uppercase PETSCII */
            petscii[out_len++] = (uint8_t)(c - 32);
        } else if (c >= 'A' && c <= 'Z') {
            /* Uppercase ASCII -> shifted PETSCII */
            petscii[out_len++] = (uint8_t)(c + 0x80);
        } else if (c >= 0x20 && c <= 0x7E) {
            /* Other printable */
            petscii[out_len++] = (uint8_t)c;
        }
    }
    
    return out_len;
}

void uft_cbm_pad_filename(uint8_t* filename, size_t current_len, size_t max_len) {
    if (!filename) return;
    
    for (size_t i = current_len; i < max_len; i++) {
        filename[i] = 0xA0;  /* Shifted space */
    }
}

/*============================================================================
 * Utility Functions
 *============================================================================*/

uft_cbm_extract_opts_t uft_cbm_extract_opts_default(void) {
    uft_cbm_extract_opts_t opts = {
        .include_load_addr = true,
        .convert_petscii = false,
        .handle_geos_vlir = true,
        .max_size = 0
    };
    return opts;
}

uft_cbm_inject_opts_t uft_cbm_inject_opts_default(void) {
    uft_cbm_inject_opts_t opts = {
        .file_type = UFT_CBM_FT_PRG,
        .load_address = 0x0801,
        .auto_load_addr = true,
        .rel_record_len = 0,
        .replace_existing = false,
        .lock_file = false,
        .interleave = 0
    };
    return opts;
}

size_t uft_cbm_blocks_free_msg(uft_cbm_fs_t* fs, char* buffer, size_t cap) {
    if (!fs || !buffer || cap == 0) return 0;
    
    return (size_t)snprintf(buffer, cap, "%u BLOCKS FREE.", 
                            (unsigned)uft_cbm_bam_free_blocks(fs));
}

uft_rc_t uft_cbm_print_directory(uft_cbm_fs_t* fs, void* stream) {
    if (!fs || !fs->dir || !stream) return UFT_ERR_INVALID_ARG;
    
    FILE* out = (FILE*)stream;
    
    /* Print header */
    char disk_name_ascii[32];
    uft_cbm_petscii_to_ascii(fs->dir->disk_name, UFT_CBM_FILENAME_MAX,
                              disk_name_ascii, sizeof(disk_name_ascii));
    
    fprintf(out, "0 \"%-16s\" %s %s\n", 
            disk_name_ascii, fs->dir->disk_id, fs->dir->dos_type);
    
    /* Print entries */
    for (uint16_t i = 0; i < fs->dir->count; i++) {
        const uft_cbm_dir_entry_t* e = &fs->dir->entries[i];
        
        /* Skip deleted files unless they have content */
        if (e->file_type == UFT_CBM_FT_DEL && e->first_ts.track == 0) continue;
        
        char filename_ascii[32];
        uft_cbm_petscii_to_ascii(e->filename, e->filename_len,
                                  filename_ascii, sizeof(filename_ascii));
        
        const char* type_str = uft_cbm_filetype_name(e->file_type);
        char flags_str[4] = "   ";
        
        if (!(e->flags & UFT_CBM_FLAG_CLOSED)) flags_str[0] = '*';  /* Splat file */
        if (e->flags & UFT_CBM_FLAG_LOCKED) flags_str[2] = '<';     /* Locked */
        
        fprintf(out, "%-5u \"%-16s\" %s%s%s\n",
                (unsigned)e->blocks, filename_ascii, 
                flags_str[0] == '*' ? "*" : " ",
                type_str,
                flags_str[2] == '<' ? "<" : "");
    }
    
    /* Print footer */
    fprintf(out, "%u BLOCKS FREE.\n", (unsigned)fs->dir->blocks_free);
    
    return UFT_SUCCESS;
}
