/**
 * @file uft_c64_bam.c
 * @brief C64 Block Allocation Map (BAM) Editor Implementation
 * 
 * @author UFT Project
 * @date 2026-01-16
 */

#include "uft/formats/c64/uft_c64_bam.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * Static Data
 * ============================================================================ */

/** Sectors per track for D64 */
static const int d64_sectors_per_track[] = {
    0,  /* Track 0 doesn't exist */
    21, 21, 21, 21, 21, 21, 21, 21, 21, 21,  /*  1-10 */
    21, 21, 21, 21, 21, 21, 21, 19, 19, 19,  /* 11-20 */
    19, 19, 19, 19, 18, 18, 18, 18, 18, 18,  /* 21-30 */
    17, 17, 17, 17, 17, 17, 17, 17, 17, 17,  /* 31-40 */
    17, 17                                    /* 41-42 (extended) */
};

/** Track start offsets for D64 (cumulative sectors) */
static const int d64_track_offset[] = {
    0,
    0,    21,   42,   63,   84,   105,  126,  147,  168,  189,   /*  1-10 */
    210,  231,  252,  273,  294,  315,  336,  357,  376,  395,   /* 11-20 */
    414,  433,  452,  471,  490,  508,  526,  544,  562,  580,   /* 21-30 */
    598,  615,  632,  649,  666,  683,  700,  717,  734,  751,   /* 31-40 */
    768,  785                                                     /* 41-42 */
};

/** Format names */
static const char *format_names[] = {
    "D64 (35 tracks)",
    "D64 (40 tracks)",
    "D71 (70 tracks)",
    "D81 (80 tracks)",
    "Unknown"
};

/** File type names */
static const char *file_type_names[] = {
    "DEL", "SEQ", "PRG", "USR", "REL", "CBM"
};

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Get BAM entry offset for track
 */
static int get_bam_entry_offset(int track, bam_format_t format)
{
    if (format == BAM_FORMAT_D64_35 || format == BAM_FORMAT_D64_40) {
        if (track >= 1 && track <= 35) {
            return 4 + (track - 1) * 4;  /* 4 bytes per track */
        } else if (track >= 36 && track <= 40) {
            return 0xAB + (track - 36) * 4;  /* Extended BAM area */
        }
    }
    return -1;
}

/* ============================================================================
 * Context Management
 * ============================================================================ */

/**
 * @brief Detect disk format
 */
bam_format_t bam_detect_format(const uint8_t *data, size_t size)
{
    if (!data) return BAM_FORMAT_UNKNOWN;
    
    if (size == 174848 || size == 175531) {
        return BAM_FORMAT_D64_35;
    } else if (size == 196608 || size == 197376) {
        return BAM_FORMAT_D64_40;
    } else if (size == 349696 || size == 351062) {
        return BAM_FORMAT_D71;
    } else if (size == 819200 || size == 822400) {
        return BAM_FORMAT_D81;
    }
    
    return BAM_FORMAT_UNKNOWN;
}

/**
 * @brief Create BAM context
 */
bam_context_t *bam_create_context(uint8_t *data, size_t size)
{
    if (!data || size == 0) return NULL;
    
    bam_format_t format = bam_detect_format(data, size);
    if (format == BAM_FORMAT_UNKNOWN) return NULL;
    
    bam_context_t *ctx = calloc(1, sizeof(bam_context_t));
    if (!ctx) return NULL;
    
    ctx->data = data;
    ctx->size = size;
    ctx->format = format;
    
    switch (format) {
        case BAM_FORMAT_D64_35:
            ctx->num_tracks = 35;
            ctx->total_sectors = 683;
            break;
        case BAM_FORMAT_D64_40:
            ctx->num_tracks = 40;
            ctx->total_sectors = 768;
            break;
        case BAM_FORMAT_D71:
            ctx->num_tracks = 70;
            ctx->total_sectors = 1366;
            break;
        case BAM_FORMAT_D81:
            ctx->num_tracks = 80;
            ctx->total_sectors = 3200;
            break;
        default:
            free(ctx);
            return NULL;
    }
    
    /* Read BAM */
    bam_read(ctx);
    
    return ctx;
}

/**
 * @brief Free BAM context
 */
void bam_free_context(bam_context_t *ctx)
{
    free(ctx);  /* Does not free data */
}

/* ============================================================================
 * BAM Reading
 * ============================================================================ */

/**
 * @brief Read BAM from image
 */
int bam_read(bam_context_t *ctx)
{
    if (!ctx || !ctx->data) return -1;
    
    int bam_offset = bam_sector_offset(BAM_D64_TRACK, BAM_D64_SECTOR, ctx->format);
    if (bam_offset < 0) return -2;
    
    const uint8_t *bam = ctx->data + bam_offset;
    
    /* Read disk name */
    bam_petscii_to_ascii(bam + 0x90, ctx->disk_name, 16);
    ctx->disk_name[16] = '\0';
    
    /* Trim trailing spaces */
    for (int i = 15; i >= 0 && ctx->disk_name[i] == ' '; i--) {
        ctx->disk_name[i] = '\0';
    }
    
    /* Read disk ID */
    ctx->disk_id[0] = bam[0xA2];
    ctx->disk_id[1] = bam[0xA3];
    ctx->disk_id[2] = '\0';
    
    /* Calculate free sectors */
    ctx->free_sectors = bam_total_free(ctx);
    
    return 0;
}

/**
 * @brief Get BAM sector data
 */
const uint8_t *bam_get_sector(const bam_context_t *ctx)
{
    if (!ctx || !ctx->data) return NULL;
    
    int offset = bam_sector_offset(BAM_D64_TRACK, BAM_D64_SECTOR, ctx->format);
    if (offset < 0) return NULL;
    
    return ctx->data + offset;
}

/**
 * @brief Get disk name
 */
void bam_get_disk_name(const bam_context_t *ctx, char *name)
{
    if (!ctx || !name) return;
    strncpy(name, ctx->disk_name, 17);
}

/**
 * @brief Get disk ID
 */
void bam_get_disk_id(const bam_context_t *ctx, char *id)
{
    if (!ctx || !id) return;
    strncpy(id, ctx->disk_id, 3);
}

/* ============================================================================
 * Block Operations
 * ============================================================================ */

/**
 * @brief Check if block is free
 */
bool bam_is_block_free(const bam_context_t *ctx, int track, int sector)
{
    if (!ctx || !ctx->data) return false;
    if (track < 1 || track > ctx->num_tracks) return false;
    if (sector < 0 || sector >= bam_sectors_per_track(track, ctx->format)) return false;
    
    int bam_offset = bam_sector_offset(BAM_D64_TRACK, BAM_D64_SECTOR, ctx->format);
    if (bam_offset < 0) return false;
    
    int entry_offset = get_bam_entry_offset(track, ctx->format);
    if (entry_offset < 0) return false;
    
    const uint8_t *entry = ctx->data + bam_offset + entry_offset;
    
    /* Bitmap is in bytes 1-3 of entry, sector N is bit N */
    int byte_idx = 1 + (sector / 8);
    int bit_idx = sector % 8;
    
    return (entry[byte_idx] & (1 << bit_idx)) != 0;
}

/**
 * @brief Allocate a block
 */
int bam_allocate_block(bam_context_t *ctx, int track, int sector)
{
    if (!ctx || !ctx->data) return -1;
    if (track < 1 || track > ctx->num_tracks) return -2;
    if (sector < 0 || sector >= bam_sectors_per_track(track, ctx->format)) return -3;
    
    if (!bam_is_block_free(ctx, track, sector)) return -4;  /* Already allocated */
    
    int bam_offset = bam_sector_offset(BAM_D64_TRACK, BAM_D64_SECTOR, ctx->format);
    if (bam_offset < 0) return -5;
    
    int entry_offset = get_bam_entry_offset(track, ctx->format);
    if (entry_offset < 0) return -6;
    
    uint8_t *entry = ctx->data + bam_offset + entry_offset;
    
    /* Clear bit in bitmap */
    int byte_idx = 1 + (sector / 8);
    int bit_idx = sector % 8;
    entry[byte_idx] &= ~(1 << bit_idx);
    
    /* Decrement free count */
    if (entry[0] > 0) entry[0]--;
    
    ctx->free_sectors--;
    ctx->modified = true;
    
    return 0;
}

/**
 * @brief Free a block
 */
int bam_free_block(bam_context_t *ctx, int track, int sector)
{
    if (!ctx || !ctx->data) return -1;
    if (track < 1 || track > ctx->num_tracks) return -2;
    if (sector < 0 || sector >= bam_sectors_per_track(track, ctx->format)) return -3;
    
    if (bam_is_block_free(ctx, track, sector)) return 0;  /* Already free */
    
    int bam_offset = bam_sector_offset(BAM_D64_TRACK, BAM_D64_SECTOR, ctx->format);
    if (bam_offset < 0) return -4;
    
    int entry_offset = get_bam_entry_offset(track, ctx->format);
    if (entry_offset < 0) return -5;
    
    uint8_t *entry = ctx->data + bam_offset + entry_offset;
    
    /* Set bit in bitmap */
    int byte_idx = 1 + (sector / 8);
    int bit_idx = sector % 8;
    entry[byte_idx] |= (1 << bit_idx);
    
    /* Increment free count */
    entry[0]++;
    
    ctx->free_sectors++;
    ctx->modified = true;
    
    return 0;
}

/**
 * @brief Allocate first free block
 */
int bam_allocate_first_free(bam_context_t *ctx, bam_alloc_result_t *result)
{
    if (!ctx || !result) return -1;
    
    memset(result, 0, sizeof(bam_alloc_result_t));
    result->free_before = ctx->free_sectors;
    
    /* Search tracks, skipping directory track */
    for (int track = 1; track <= ctx->num_tracks; track++) {
        if (track == BAM_D64_TRACK) continue;  /* Skip directory track */
        
        int sectors = bam_sectors_per_track(track, ctx->format);
        for (int sector = 0; sector < sectors; sector++) {
            if (bam_is_block_free(ctx, track, sector)) {
                if (bam_allocate_block(ctx, track, sector) == 0) {
                    result->success = true;
                    result->track = track;
                    result->sector = sector;
                    result->free_after = ctx->free_sectors;
                    return 0;
                }
            }
        }
    }
    
    return -2;  /* Disk full */
}

/**
 * @brief Allocate near track
 */
int bam_allocate_near(bam_context_t *ctx, int near_track, bam_alloc_result_t *result)
{
    if (!ctx || !result) return -1;
    
    memset(result, 0, sizeof(bam_alloc_result_t));
    result->free_before = ctx->free_sectors;
    
    /* Search outward from near_track */
    for (int dist = 0; dist <= ctx->num_tracks; dist++) {
        int tracks_to_try[2] = { near_track - dist, near_track + dist };
        
        for (int t = 0; t < 2; t++) {
            int track = tracks_to_try[t];
            if (track < 1 || track > ctx->num_tracks) continue;
            if (track == BAM_D64_TRACK) continue;
            
            int sectors = bam_sectors_per_track(track, ctx->format);
            for (int sector = 0; sector < sectors; sector++) {
                if (bam_is_block_free(ctx, track, sector)) {
                    if (bam_allocate_block(ctx, track, sector) == 0) {
                        result->success = true;
                        result->track = track;
                        result->sector = sector;
                        result->free_after = ctx->free_sectors;
                        return 0;
                    }
                }
            }
        }
    }
    
    return -2;  /* Disk full */
}

/**
 * @brief Get free sectors on track
 */
int bam_free_on_track(const bam_context_t *ctx, int track)
{
    if (!ctx || !ctx->data) return -1;
    if (track < 1 || track > ctx->num_tracks) return -2;
    
    int bam_offset = bam_sector_offset(BAM_D64_TRACK, BAM_D64_SECTOR, ctx->format);
    if (bam_offset < 0) return -3;
    
    int entry_offset = get_bam_entry_offset(track, ctx->format);
    if (entry_offset < 0) return -4;
    
    return ctx->data[bam_offset + entry_offset];
}

/**
 * @brief Get total free sectors
 */
int bam_total_free(const bam_context_t *ctx)
{
    if (!ctx) return 0;
    
    int total = 0;
    for (int track = 1; track <= ctx->num_tracks; track++) {
        if (track == BAM_D64_TRACK) continue;  /* Skip directory track */
        int free = bam_free_on_track(ctx, track);
        if (free > 0) total += free;
    }
    
    return total;
}

/**
 * @brief Get sectors per track
 */
int bam_sectors_per_track(int track, bam_format_t format)
{
    if (format == BAM_FORMAT_D64_35 || format == BAM_FORMAT_D64_40) {
        if (track >= 1 && track <= 42) {
            return d64_sectors_per_track[track];
        }
    } else if (format == BAM_FORMAT_D71) {
        /* D71 is like two D64s */
        if (track >= 1 && track <= 35) {
            return d64_sectors_per_track[track];
        } else if (track >= 36 && track <= 70) {
            return d64_sectors_per_track[track - 35];
        }
    } else if (format == BAM_FORMAT_D81) {
        return 40;  /* D81 has 40 sectors per track */
    }
    
    return 0;
}

/* ============================================================================
 * Directory Operations
 * ============================================================================ */

/**
 * @brief Read directory listing
 */
int bam_read_directory(const bam_context_t *ctx, bam_directory_t *dir)
{
    if (!ctx || !dir) return -1;
    
    memset(dir, 0, sizeof(bam_directory_t));
    strncpy(dir->disk_name, ctx->disk_name, 16);
    strncpy(dir->disk_id, ctx->disk_id, 2);
    dir->blocks_free = ctx->free_sectors;
    
    int track = BAM_DIR_TRACK;
    int sector = BAM_DIR_SECTOR;
    
    while (track != 0 && dir->num_files < 144) {
        uint8_t sector_data[256];
        if (bam_read_sector(ctx, track, sector, sector_data) != 0) break;
        
        /* Process 8 directory entries per sector */
        for (int i = 0; i < 8 && dir->num_files < 144; i++) {
            bam_dir_entry_t *entry = (bam_dir_entry_t *)(sector_data + i * 32);
            
            if (i == 0) {
                /* First entry has next track/sector link */
                track = entry->next_track;
                sector = entry->next_sector;
            }
            
            /* Skip deleted files */
            if ((entry->file_type & 0x07) == BAM_FILE_DEL) continue;
            if (entry->file_type == 0) continue;
            
            bam_file_info_t *info = &dir->files[dir->num_files];
            
            /* Copy filename */
            bam_petscii_to_ascii((uint8_t *)entry->filename, info->filename, 16);
            info->filename[16] = '\0';
            
            /* Trim trailing 0xA0 (shifted space) */
            for (int j = 15; j >= 0; j--) {
                if (info->filename[j] == ' ' || info->filename[j] == '\0') {
                    info->filename[j] = '\0';
                } else {
                    break;
                }
            }
            
            info->file_type = entry->file_type & 0x07;
            info->start_track = entry->start_track;
            info->start_sector = entry->start_sector;
            info->size_sectors = entry->file_size;
            info->size_bytes = info->size_sectors * 254;  /* Approximate */
            info->locked = (entry->file_type & BAM_FILE_LOCKED) != 0;
            info->closed = (entry->file_type & BAM_FILE_CLOSED) != 0;
            
            dir->num_files++;
        }
        
        /* Safety check */
        if (track == BAM_DIR_TRACK && sector == BAM_DIR_SECTOR) break;
    }
    
    return dir->num_files;
}

/**
 * @brief Find file by name
 */
bool bam_find_file(const bam_context_t *ctx, const char *filename, bam_file_info_t *info)
{
    if (!ctx || !filename || !info) return false;
    
    bam_directory_t dir;
    if (bam_read_directory(ctx, &dir) < 0) return false;
    
    for (int i = 0; i < dir.num_files; i++) {
        if (strcmp(dir.files[i].filename, filename) == 0) {
            *info = dir.files[i];
            return true;
        }
    }
    
    return false;
}

/**
 * @brief Get file type name
 */
const char *bam_file_type_name(uint8_t file_type)
{
    int type = file_type & 0x07;
    if (type < 6) {
        return file_type_names[type];
    }
    return "???";
}

/**
 * @brief Delete file
 */
int bam_delete_file(bam_context_t *ctx, const char *filename)
{
    if (!ctx || !filename) return -1;
    
    bam_file_info_t info;
    if (!bam_find_file(ctx, filename, &info)) return -2;
    
    int blocks_freed = 0;
    int track = info.start_track;
    int sector = info.start_sector;
    
    /* Follow file chain and free blocks */
    while (track != 0) {
        uint8_t sector_data[256];
        if (bam_read_sector(ctx, track, sector, sector_data) != 0) break;
        
        int next_track = sector_data[0];
        int next_sector = sector_data[1];
        
        bam_free_block(ctx, track, sector);
        blocks_freed++;
        
        track = next_track;
        sector = next_sector;
        
        if (blocks_freed > 1000) break;  /* Safety limit */
    }
    
    /* Mark directory entry as deleted by scanning directory chain */
    {
        int dir_track = BAM_DIR_TRACK;
        int dir_sector = BAM_DIR_SECTOR;
        bool found = false;
        
        while (dir_track != 0 && !found) {
            uint8_t dir_data[256];
            if (bam_read_sector(ctx, dir_track, dir_sector, dir_data) != 0) break;
            
            for (int i = 0; i < 8; i++) {
                bam_dir_entry_t *entry = (bam_dir_entry_t *)(dir_data + i * 32);
                
                /* Match by start track/sector (unique per file) */
                if (entry->start_track == info.start_track &&
                    entry->start_sector == info.start_sector &&
                    (entry->file_type & 0x07) != BAM_FILE_DEL) {
                    /* Mark as deleted: clear file type, keep filename for recovery */
                    entry->file_type = 0x00;
                    bam_write_sector(ctx, dir_track, dir_sector, dir_data);
                    found = true;
                    break;
                }
            }
            
            if (!found) {
                /* Follow directory chain */
                int next_track = dir_data[0];
                int next_sector = dir_data[1];
                if (next_track == dir_track && next_sector == dir_sector) break;
                dir_track = next_track;
                dir_sector = next_sector;
            }
        }
    }
    
    return blocks_freed;
}

/* ============================================================================
 * BAM Writing
 * ============================================================================ */

/**
 * @brief Write BAM to image
 */
int bam_write(bam_context_t *ctx)
{
    if (!ctx || !ctx->data) return -1;
    
    /* BAM is already in memory, just mark as written */
    ctx->modified = false;
    
    return 0;
}

/**
 * @brief Set disk name
 */
int bam_set_disk_name(bam_context_t *ctx, const char *name)
{
    if (!ctx || !ctx->data || !name) return -1;
    
    int bam_offset = bam_sector_offset(BAM_D64_TRACK, BAM_D64_SECTOR, ctx->format);
    if (bam_offset < 0) return -2;
    
    uint8_t *bam = ctx->data + bam_offset;
    
    /* Convert and write name */
    uint8_t petscii[16];
    memset(petscii, 0xA0, 16);  /* Fill with shifted space */
    bam_ascii_to_petscii(name, petscii, 16);
    memcpy(bam + 0x90, petscii, 16);
    
    /* Update context */
    strncpy(ctx->disk_name, name, 16);
    ctx->disk_name[16] = '\0';
    ctx->modified = true;
    
    return 0;
}

/**
 * @brief Set disk ID
 */
int bam_set_disk_id(bam_context_t *ctx, const char *id)
{
    if (!ctx || !ctx->data || !id) return -1;
    
    int bam_offset = bam_sector_offset(BAM_D64_TRACK, BAM_D64_SECTOR, ctx->format);
    if (bam_offset < 0) return -2;
    
    uint8_t *bam = ctx->data + bam_offset;
    
    bam[0xA2] = id[0];
    bam[0xA3] = (id[1] != '\0') ? id[1] : ' ';
    
    ctx->disk_id[0] = id[0];
    ctx->disk_id[1] = (id[1] != '\0') ? id[1] : ' ';
    ctx->disk_id[2] = '\0';
    ctx->modified = true;
    
    return 0;
}

/* ============================================================================
 * Validation and Repair
 * ============================================================================ */

/**
 * @brief Validate BAM
 */
bool bam_validate(const bam_context_t *ctx, int *errors)
{
    if (!ctx) return false;
    
    int error_count = 0;
    
    for (int track = 1; track <= ctx->num_tracks; track++) {
        if (track == BAM_D64_TRACK) continue;
        
        int reported_free = bam_free_on_track(ctx, track);
        int actual_free = 0;
        int sectors = bam_sectors_per_track(track, ctx->format);
        
        for (int sector = 0; sector < sectors; sector++) {
            if (bam_is_block_free(ctx, track, sector)) {
                actual_free++;
            }
        }
        
        if (reported_free != actual_free) {
            error_count++;
        }
    }
    
    if (errors) *errors = error_count;
    return (error_count == 0);
}

/**
 * @brief Repair BAM
 */
int bam_repair(bam_context_t *ctx, int *blocks_recovered)
{
    if (!ctx || !ctx->data) return -1;
    
    int recovered = 0;
    
    /* Recalculate free counts */
    for (int track = 1; track <= ctx->num_tracks; track++) {
        if (track == BAM_D64_TRACK) continue;
        
        int bam_offset = bam_sector_offset(BAM_D64_TRACK, BAM_D64_SECTOR, ctx->format);
        int entry_offset = get_bam_entry_offset(track, ctx->format);
        
        if (bam_offset < 0 || entry_offset < 0) continue;
        
        uint8_t *entry = ctx->data + bam_offset + entry_offset;
        int old_free = entry[0];
        
        /* Count actual free sectors */
        int actual_free = 0;
        int sectors = bam_sectors_per_track(track, ctx->format);
        
        for (int sector = 0; sector < sectors; sector++) {
            if (bam_is_block_free(ctx, track, sector)) {
                actual_free++;
            }
        }
        
        if (old_free != actual_free) {
            entry[0] = actual_free;
            recovered += (actual_free - old_free);
            ctx->modified = true;
        }
    }
    
    ctx->free_sectors = bam_total_free(ctx);
    
    if (blocks_recovered) *blocks_recovered = recovered;
    return 0;
}

/**
 * @brief Recalculate free
 */
int bam_recalculate_free(bam_context_t *ctx)
{
    if (!ctx) return 0;
    ctx->free_sectors = bam_total_free(ctx);
    return ctx->free_sectors;
}

/* ============================================================================
 * Sector Access
 * ============================================================================ */

/**
 * @brief Get sector offset
 */
int bam_sector_offset(int track, int sector, bam_format_t format)
{
    if (track < 1) return -1;
    
    if (format == BAM_FORMAT_D64_35 || format == BAM_FORMAT_D64_40) {
        if (track > 42) return -2;
        if (sector >= d64_sectors_per_track[track]) return -3;
        
        return (d64_track_offset[track] + sector) * 256;
    } else if (format == BAM_FORMAT_D71) {
        /* D71: two D64s back to back */
        if (track <= 35) {
            return (d64_track_offset[track] + sector) * 256;
        } else if (track <= 70) {
            int side1_offset = 683 * 256;  /* After first side */
            return side1_offset + (d64_track_offset[track - 35] + sector) * 256;
        }
    } else if (format == BAM_FORMAT_D81) {
        if (track > 80 || sector >= 40) return -4;
        return ((track - 1) * 40 + sector) * 256;
    }
    
    return -5;
}

/**
 * @brief Read sector
 */
int bam_read_sector(const bam_context_t *ctx, int track, int sector, uint8_t *buffer)
{
    if (!ctx || !ctx->data || !buffer) return -1;
    
    int offset = bam_sector_offset(track, sector, ctx->format);
    if (offset < 0 || (size_t)offset + 256 > ctx->size) return -2;
    
    memcpy(buffer, ctx->data + offset, 256);
    return 0;
}

/**
 * @brief Write sector
 */
int bam_write_sector(bam_context_t *ctx, int track, int sector, const uint8_t *buffer)
{
    if (!ctx || !ctx->data || !buffer) return -1;
    
    int offset = bam_sector_offset(track, sector, ctx->format);
    if (offset < 0 || (size_t)offset + 256 > ctx->size) return -2;
    
    memcpy(ctx->data + offset, buffer, 256);
    ctx->modified = true;
    return 0;
}

/* ============================================================================
 * Utilities
 * ============================================================================ */

/**
 * @brief Get format name
 */
const char *bam_format_name(bam_format_t format)
{
    if (format < BAM_FORMAT_UNKNOWN) {
        return format_names[format];
    }
    return format_names[BAM_FORMAT_UNKNOWN];
}

/**
 * @brief ASCII to PETSCII
 */
void bam_ascii_to_petscii(const char *ascii, uint8_t *petscii, size_t len)
{
    if (!ascii || !petscii) return;
    
    for (size_t i = 0; i < len && ascii[i] != '\0'; i++) {
        char c = ascii[i];
        
        if (c >= 'a' && c <= 'z') {
            petscii[i] = c - 'a' + 0x41;  /* Lowercase -> uppercase */
        } else if (c >= 'A' && c <= 'Z') {
            petscii[i] = c;
        } else {
            petscii[i] = c;
        }
    }
}

/**
 * @brief PETSCII to ASCII
 */
void bam_petscii_to_ascii(const uint8_t *petscii, char *ascii, size_t len)
{
    if (!petscii || !ascii) return;
    
    for (size_t i = 0; i < len; i++) {
        uint8_t c = petscii[i];
        
        if (c == 0xA0) {
            ascii[i] = ' ';  /* Shifted space */
        } else if (c >= 0x41 && c <= 0x5A) {
            ascii[i] = c;  /* A-Z */
        } else if (c >= 0xC1 && c <= 0xDA) {
            ascii[i] = c - 0xC1 + 'A';  /* Shifted letters */
        } else if (c >= 0x20 && c < 0x7F) {
            ascii[i] = c;
        } else {
            ascii[i] = '?';
        }
    }
    ascii[len] = '\0';
}

/**
 * @brief Print BAM summary
 */
void bam_print_summary(const bam_context_t *ctx, FILE *fp)
{
    if (!ctx || !fp) return;
    
    fprintf(fp, "Disk Format: %s\n", bam_format_name(ctx->format));
    fprintf(fp, "Disk Name:   \"%s\"\n", ctx->disk_name);
    fprintf(fp, "Disk ID:     %s\n", ctx->disk_id);
    fprintf(fp, "Tracks:      %d\n", ctx->num_tracks);
    fprintf(fp, "Total:       %d sectors\n", ctx->total_sectors);
    fprintf(fp, "Free:        %d blocks\n", ctx->free_sectors);
    
    /* Print per-track summary */
    fprintf(fp, "\nBAM per track:\n");
    for (int track = 1; track <= ctx->num_tracks; track++) {
        int free = bam_free_on_track(ctx, track);
        int total = bam_sectors_per_track(track, ctx->format);
        fprintf(fp, "  Track %2d: %2d/%2d free\n", track, free, total);
    }
}

/**
 * @brief Print directory
 */
void bam_print_directory(const bam_directory_t *dir, FILE *fp)
{
    if (!dir || !fp) return;
    
    fprintf(fp, "0 \"%-16s\" %s\n", dir->disk_name, dir->disk_id);
    
    for (int i = 0; i < dir->num_files; i++) {
        const bam_file_info_t *f = &dir->files[i];
        
        fprintf(fp, "%-5d \"%-16s\" %s%s%s\n",
                f->size_sectors,
                f->filename,
                bam_file_type_name(f->file_type),
                f->locked ? "<" : "",
                f->closed ? "" : "*");
    }
    
    fprintf(fp, "%d BLOCKS FREE.\n", dir->blocks_free);
}
