/**
 * @file uft_bam_editor.c
 * @brief BAM Editor Implementation for C64 D64 Images
 * 
 * @author UFT Project
 * @date 2026-01-16
 */

#include "uft/core/uft_error_compat.h"
#include "uft/formats/c64/uft_bam_editor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * Static Data
 * ============================================================================ */

/** Track offset table (cumulative sectors before each track) */
static int track_offset[43] = {0};
static bool offset_initialized = false;

/** File type names */
static const char *file_type_names[] = {
    "DEL", "SEQ", "PRG", "USR", "REL", "???", "???", "???"
};

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Initialize track offset table
 */
static void init_track_offsets(void)
{
    if (offset_initialized) return;
    
    track_offset[0] = 0;
    for (int t = 1; t <= 42; t++) {
        track_offset[t] = track_offset[t - 1] + bam_sectors_per_track[t - 1];
    }
    
    offset_initialized = true;
}

/**
 * @brief Get sector offset in D64
 */
int bam_sector_offset(int track, int sector)
{
    if (track < 1 || track > 42) return -1;
    if (sector < 0 || sector >= bam_sectors_per_track[track]) return -1;
    
    init_track_offsets();
    
    return (track_offset[track] + sector) * 256;
}

/* ============================================================================
 * Editor Management
 * ============================================================================ */

/**
 * @brief Create BAM editor
 */
bam_editor_t *bam_editor_create(uint8_t *d64_data, size_t d64_size)
{
    if (!d64_data || d64_size < BAM_D64_35_TRACKS) return NULL;
    
    bam_editor_t *editor = calloc(1, sizeof(bam_editor_t));
    if (!editor) return NULL;
    
    editor->d64_data = d64_data;
    editor->d64_size = d64_size;
    
    /* Determine track count and error info */
    if (d64_size >= BAM_D64_40_ERRORS) {
        editor->num_tracks = 40;
        editor->has_errors = true;
    } else if (d64_size >= BAM_D64_40_TRACKS) {
        editor->num_tracks = 40;
        editor->has_errors = false;
    } else if (d64_size >= BAM_D64_35_ERRORS) {
        editor->num_tracks = 35;
        editor->has_errors = true;
    } else {
        editor->num_tracks = 35;
        editor->has_errors = false;
    }
    
    /* Set BAM pointer */
    int bam_offset = bam_sector_offset(BAM_TRACK, BAM_SECTOR);
    editor->bam = (bam_t *)(d64_data + bam_offset);
    
    return editor;
}

/**
 * @brief Free BAM editor
 */
void bam_editor_free(bam_editor_t *editor)
{
    free(editor);
}

/**
 * @brief Load D64 file
 */
int bam_editor_load(const char *filename, bam_editor_t **editor)
{
    if (!filename || !editor) return -1;
    
    FILE *fp = fopen(filename, "rb");
    if (!fp) return -2;
    
    /* Get file size */
    fseek(fp, 0, SEEK_END);
    size_t size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    if (size < BAM_D64_35_TRACKS) {
        fclose(fp);
        return -3;
    }
    
    /* Allocate and read */
    uint8_t *data = malloc(size);
    if (!data) {
        fclose(fp);
        return -4;
    }
    
    if (fread(data, 1, size, fp) != size) {
        free(data);
        fclose(fp);
        return -5;
    }
    
    fclose(fp);
    
    *editor = bam_editor_create(data, size);
    if (!*editor) {
        free(data);
        return -6;
    }
    
    return 0;
}

/**
 * @brief Save D64 file
 */
int bam_editor_save(const bam_editor_t *editor, const char *filename)
{
    if (!editor || !filename) return -1;
    
    FILE *fp = fopen(filename, "wb");
    if (!fp) return -2;
    
    if (fwrite(editor->d64_data, 1, editor->d64_size, fp) != editor->d64_size) {
        fclose(fp);
        return -3;
    }
    if (ferror(fp)) {
        fclose(fp);
        return UFT_ERR_IO;
    }
    
        
    fclose(fp);
return 0;
}

/* ============================================================================
 * Disk Info
 * ============================================================================ */

/**
 * @brief Get disk information
 */
int bam_get_disk_info(const bam_editor_t *editor, disk_info_t *info)
{
    if (!editor || !info) return -1;
    
    memset(info, 0, sizeof(disk_info_t));
    
    /* Extract disk name (convert from PETSCII, remove padding) */
    bam_petscii_to_ascii(editor->bam->disk_name, info->disk_name, 16);
    info->disk_name[16] = '\0';
    
    /* Trim trailing spaces */
    for (int i = 15; i >= 0 && info->disk_name[i] == ' '; i--) {
        info->disk_name[i] = '\0';
    }
    
    /* Disk ID and DOS type */
    info->disk_id[0] = editor->bam->disk_id[0];
    info->disk_id[1] = editor->bam->disk_id[1];
    info->disk_id[2] = '\0';
    
    info->dos_type[0] = editor->bam->dos_type[0];
    info->dos_type[1] = editor->bam->dos_type[1];
    info->dos_type[2] = '\0';
    
    /* Calculate blocks */
    info->num_tracks = editor->num_tracks;
    info->total_blocks = (editor->num_tracks == 40) ? BAM_TOTAL_BLOCKS_40 : BAM_TOTAL_BLOCKS_35;
    info->free_blocks = bam_get_free_blocks(editor);
    info->used_blocks = info->total_blocks - info->free_blocks;
    info->num_files = bam_get_file_count(editor);
    info->has_errors = editor->has_errors;
    
    return 0;
}

/**
 * @brief Set disk name
 */
int bam_set_disk_name(bam_editor_t *editor, const char *name)
{
    if (!editor || !name) return -1;
    
    /* Pad with 0xA0 */
    memset(editor->bam->disk_name, 0xA0, 16);
    
    /* Copy name (convert to PETSCII) */
    bam_ascii_to_petscii(name, editor->bam->disk_name, 16);
    
    editor->modified = true;
    return 0;
}

/**
 * @brief Set disk ID
 */
int bam_set_disk_id(bam_editor_t *editor, const char *id)
{
    if (!editor || !id || strlen(id) < 2) return -1;
    
    editor->bam->disk_id[0] = id[0];
    editor->bam->disk_id[1] = id[1];
    
    editor->modified = true;
    return 0;
}

/**
 * @brief Get free blocks
 */
int bam_get_free_blocks(const bam_editor_t *editor)
{
    if (!editor) return -1;
    
    int free_count = 0;
    
    for (int t = 1; t <= editor->num_tracks; t++) {
        if (t == DIR_TRACK) continue;  /* Skip directory track in count */
        free_count += editor->bam->tracks[t].free_sectors;
    }
    
    return free_count;
}

/* ============================================================================
 * Block Allocation
 * ============================================================================ */

/**
 * @brief Check if block is free
 */
bool bam_is_block_free(const bam_editor_t *editor, int track, int sector)
{
    if (!editor || track < 1 || track > editor->num_tracks) return false;
    if (sector < 0 || sector >= bam_sectors_per_track[track]) return false;
    
    int byte_idx = sector / 8;
    int bit_idx = sector % 8;
    
    return (editor->bam->tracks[track].bitmap[byte_idx] & (1 << bit_idx)) != 0;
}

/**
 * @brief Allocate block
 */
int bam_allocate_block(bam_editor_t *editor, int track, int sector)
{
    if (!editor || track < 1 || track > editor->num_tracks) return -1;
    if (sector < 0 || sector >= bam_sectors_per_track[track]) return -1;
    
    if (!bam_is_block_free(editor, track, sector)) return -2;  /* Already allocated */
    
    int byte_idx = sector / 8;
    int bit_idx = sector % 8;
    
    /* Clear bit (0 = allocated) */
    editor->bam->tracks[track].bitmap[byte_idx] &= ~(1 << bit_idx);
    editor->bam->tracks[track].free_sectors--;
    
    editor->modified = true;
    return 0;
}

/**
 * @brief Free block
 */
int bam_free_block(bam_editor_t *editor, int track, int sector)
{
    if (!editor || track < 1 || track > editor->num_tracks) return -1;
    if (sector < 0 || sector >= bam_sectors_per_track[track]) return -1;
    
    if (bam_is_block_free(editor, track, sector)) return 0;  /* Already free */
    
    int byte_idx = sector / 8;
    int bit_idx = sector % 8;
    
    /* Set bit (1 = free) */
    editor->bam->tracks[track].bitmap[byte_idx] |= (1 << bit_idx);
    editor->bam->tracks[track].free_sectors++;
    
    editor->modified = true;
    return 0;
}

/**
 * @brief Allocate next free block
 */
int bam_allocate_next_free(bam_editor_t *editor, int start_track,
                           int *track, int *sector)
{
    if (!editor || !track || !sector) return -1;
    
    /* Standard 1541 allocation: start from track 18, work outward */
    if (start_track < 1) start_track = DIR_TRACK;
    
    /* Search outward from directory track */
    for (int offset = 1; offset <= editor->num_tracks; offset++) {
        /* Try track below directory */
        int t = DIR_TRACK - offset;
        if (t >= 1) {
            for (int s = 0; s < bam_sectors_per_track[t]; s++) {
                if (bam_is_block_free(editor, t, s)) {
                    bam_allocate_block(editor, t, s);
                    *track = t;
                    *sector = s;
                    return 0;
                }
            }
        }
        
        /* Try track above directory */
        t = DIR_TRACK + offset;
        if (t <= editor->num_tracks && t != DIR_TRACK) {
            for (int s = 0; s < bam_sectors_per_track[t]; s++) {
                if (bam_is_block_free(editor, t, s)) {
                    bam_allocate_block(editor, t, s);
                    *track = t;
                    *sector = s;
                    return 0;
                }
            }
        }
    }
    
    return -1;  /* Disk full */
}

/**
 * @brief Get free sectors on track
 */
int bam_get_track_free(const bam_editor_t *editor, int track)
{
    if (!editor || track < 1 || track > editor->num_tracks) return -1;
    return editor->bam->tracks[track].free_sectors;
}

/* ============================================================================
 * Directory Operations
 * ============================================================================ */

/**
 * @brief Get directory entry pointer
 */
static dir_entry_t *get_dir_entry(const bam_editor_t *editor, int index)
{
    if (!editor || index < 0 || index >= DIR_MAX_ENTRIES) return NULL;
    
    int dir_sector = DIR_FIRST_SECTOR + (index / DIR_ENTRIES_PER_SECTOR);
    int entry_in_sector = index % DIR_ENTRIES_PER_SECTOR;
    
    int offset = bam_sector_offset(DIR_TRACK, dir_sector);
    if (offset < 0) return NULL;
    
    return (dir_entry_t *)(editor->d64_data + offset + entry_in_sector * DIR_ENTRY_SIZE);
}

/**
 * @brief Get file count
 */
int bam_get_file_count(const bam_editor_t *editor)
{
    if (!editor) return 0;
    
    int count = 0;
    
    for (int i = 0; i < DIR_MAX_ENTRIES; i++) {
        dir_entry_t *entry = get_dir_entry(editor, i);
        if (!entry) break;
        
        if ((entry->file_type & 0x07) != FILE_TYPE_DEL) {
            count++;
        }
    }
    
    return count;
}

/**
 * @brief Get file info by index
 */
int bam_get_file_info(const bam_editor_t *editor, int index, file_info_t *info)
{
    if (!editor || !info) return -1;
    
    memset(info, 0, sizeof(file_info_t));
    
    int file_index = 0;
    for (int i = 0; i < DIR_MAX_ENTRIES; i++) {
        dir_entry_t *entry = get_dir_entry(editor, i);
        if (!entry) return -2;
        
        if ((entry->file_type & 0x07) != FILE_TYPE_DEL) {
            if (file_index == index) {
                /* Found the file */
                bam_petscii_to_ascii(entry->filename, info->filename, 16);
                info->filename[16] = '\0';
                
                /* Trim padding */
                for (int j = 15; j >= 0 && (info->filename[j] == ' ' || info->filename[j] == '\0'); j--) {
                    info->filename[j] = '\0';
                }
                
                info->file_type = entry->file_type & 0x07;
                info->blocks = entry->file_size;
                info->first_track = entry->first_track;
                info->first_sector = entry->first_sector;
                info->locked = (entry->file_type & FILE_TYPE_LOCKED) != 0;
                info->closed = (entry->file_type & FILE_TYPE_CLOSED) != 0;
                info->dir_index = i;
                
                return 0;
            }
            file_index++;
        }
    }
    
    return -1;  /* Not found */
}

/**
 * @brief Find file by name
 */
int bam_find_file(const bam_editor_t *editor, const char *filename, file_info_t *info)
{
    if (!editor || !filename) return -1;
    
    char petscii_name[17];
    bam_ascii_to_petscii(filename, petscii_name, 16);
    
    for (int i = 0; i < DIR_MAX_ENTRIES; i++) {
        dir_entry_t *entry = get_dir_entry(editor, i);
        if (!entry) return -2;
        
        if ((entry->file_type & 0x07) != FILE_TYPE_DEL) {
            if (memcmp(entry->filename, petscii_name, 16) == 0) {
                if (info) {
                    /* Fill info */
                    bam_petscii_to_ascii(entry->filename, info->filename, 16);
                    info->filename[16] = '\0';
                    info->file_type = entry->file_type & 0x07;
                    info->blocks = entry->file_size;
                    info->first_track = entry->first_track;
                    info->first_sector = entry->first_sector;
                    info->locked = (entry->file_type & FILE_TYPE_LOCKED) != 0;
                    info->closed = (entry->file_type & FILE_TYPE_CLOSED) != 0;
                    info->dir_index = i;
                }
                return 0;
            }
        }
    }
    
    return -1;  /* Not found */
}

/**
 * @brief Delete file
 */
int bam_delete_file(bam_editor_t *editor, const char *filename)
{
    if (!editor || !filename) return -1;
    
    file_info_t info;
    if (bam_find_file(editor, filename, &info) != 0) {
        return -2;  /* Not found */
    }
    
    /* Free file blocks */
    int track = info.first_track;
    int sector = info.first_sector;
    
    while (track != 0) {
        int offset = bam_sector_offset(track, sector);
        if (offset < 0) break;
        
        /* Get next track/sector from chain */
        int next_track = editor->d64_data[offset];
        int next_sector = editor->d64_data[offset + 1];
        
        /* Free this block */
        bam_free_block(editor, track, sector);
        
        track = next_track;
        sector = next_sector;
    }
    
    /* Mark directory entry as deleted */
    dir_entry_t *entry = get_dir_entry(editor, info.dir_index);
    if (entry) {
        entry->file_type = FILE_TYPE_DEL;
    }
    
    editor->modified = true;
    return 0;
}

/**
 * @brief Rename file
 */
int bam_rename_file(bam_editor_t *editor, const char *old_name, const char *new_name)
{
    if (!editor || !old_name || !new_name) return -1;
    
    file_info_t info;
    if (bam_find_file(editor, old_name, &info) != 0) {
        return -2;  /* Not found */
    }
    
    dir_entry_t *entry = get_dir_entry(editor, info.dir_index);
    if (!entry) return -3;
    
    /* Clear and set new name */
    memset(entry->filename, 0xA0, 16);
    bam_ascii_to_petscii(new_name, entry->filename, 16);
    
    editor->modified = true;
    return 0;
}

/**
 * @brief Lock/unlock file
 */
int bam_set_file_locked(bam_editor_t *editor, const char *filename, bool locked)
{
    if (!editor || !filename) return -1;
    
    file_info_t info;
    if (bam_find_file(editor, filename, &info) != 0) {
        return -2;
    }
    
    dir_entry_t *entry = get_dir_entry(editor, info.dir_index);
    if (!entry) return -3;
    
    if (locked) {
        entry->file_type |= FILE_TYPE_LOCKED;
    } else {
        entry->file_type &= ~FILE_TYPE_LOCKED;
    }
    
    editor->modified = true;
    return 0;
}

/* ============================================================================
 * Validation and Repair
 * ============================================================================ */

/**
 * @brief Validate BAM
 */
int bam_validate(const bam_editor_t *editor, int *errors,
                 char *messages, size_t msg_size)
{
    if (!editor) return -1;
    
    int error_count = 0;
    size_t msg_pos = 0;
    
    /* Check each track's free count matches bitmap */
    for (int t = 1; t <= editor->num_tracks; t++) {
        int counted = 0;
        for (int s = 0; s < bam_sectors_per_track[t]; s++) {
            if (bam_is_block_free(editor, t, s)) counted++;
        }
        
        if (counted != editor->bam->tracks[t].free_sectors) {
            error_count++;
            if (messages && msg_pos < msg_size - 50) {
                msg_pos += snprintf(messages + msg_pos, msg_size - msg_pos,
                                    "Track %d: BAM says %d free, counted %d\n",
                                    t, editor->bam->tracks[t].free_sectors, counted);
            }
        }
    }
    
    if (errors) *errors = error_count;
    return error_count;
}

/**
 * @brief Repair BAM
 */
int bam_repair(bam_editor_t *editor, int *fixed)
{
    if (!editor) return -1;
    
    int fix_count = 0;
    
    /* First, mark all blocks as free */
    for (int t = 1; t <= editor->num_tracks; t++) {
        editor->bam->tracks[t].free_sectors = bam_sectors_per_track[t];
        memset(editor->bam->tracks[t].bitmap, 0xFF, 3);
        
        /* Clear bits for non-existent sectors */
        for (int s = bam_sectors_per_track[t]; s < 24; s++) {
            int byte_idx = s / 8;
            int bit_idx = s % 8;
            editor->bam->tracks[t].bitmap[byte_idx] &= ~(1 << bit_idx);
        }
    }
    
    /* Allocate BAM sector */
    bam_allocate_block(editor, BAM_TRACK, BAM_SECTOR);
    
    /* Allocate directory sectors */
    bam_allocate_block(editor, DIR_TRACK, DIR_FIRST_SECTOR);
    /* Additional directory sectors would be allocated as files are added */
    
    /* Walk directory and allocate file blocks */
    for (int i = 0; i < DIR_MAX_ENTRIES; i++) {
        dir_entry_t *entry = get_dir_entry(editor, i);
        if (!entry) break;
        
        if ((entry->file_type & 0x07) != FILE_TYPE_DEL) {
            /* Follow file chain */
            int track = entry->first_track;
            int sector = entry->first_sector;
            
            while (track != 0 && track <= editor->num_tracks) {
                bam_allocate_block(editor, track, sector);
                fix_count++;
                
                int offset = bam_sector_offset(track, sector);
                if (offset < 0) break;
                
                track = editor->d64_data[offset];
                sector = editor->d64_data[offset + 1];
            }
        }
    }
    
    editor->modified = true;
    if (fixed) *fixed = fix_count;
    return 0;
}

/**
 * @brief Check for cross-links
 */
int bam_check_crosslinks(const bam_editor_t *editor,
                         char *messages, size_t msg_size)
{
    if (!editor) return -1;
    
    /* Track which file uses each block */
    int8_t block_owner[42][21];
    memset(block_owner, -1, sizeof(block_owner));
    
    int crosslinks = 0;
    size_t msg_pos = 0;
    
    /* Walk each file */
    int file_idx = 0;
    for (int i = 0; i < DIR_MAX_ENTRIES; i++) {
        dir_entry_t *entry = get_dir_entry(editor, i);
        if (!entry) break;
        
        if ((entry->file_type & 0x07) != FILE_TYPE_DEL) {
            int track = entry->first_track;
            int sector = entry->first_sector;
            
            while (track != 0 && track <= editor->num_tracks) {
                if (block_owner[track - 1][sector] != -1) {
                    crosslinks++;
                    if (messages && msg_pos < msg_size - 100) {
                        msg_pos += snprintf(messages + msg_pos, msg_size - msg_pos,
                                            "Cross-link at track %d sector %d (files %d and %d)\n",
                                            track, sector, block_owner[track - 1][sector], file_idx);
                    }
                } else {
                    block_owner[track - 1][sector] = file_idx;
                }
                
                int offset = bam_sector_offset(track, sector);
                if (offset < 0) break;
                
                track = editor->d64_data[offset];
                sector = editor->d64_data[offset + 1];
            }
            
            file_idx++;
        }
    }
    
    return crosslinks;
}

/* ============================================================================
 * Formatting
 * ============================================================================ */

/**
 * @brief Format disk
 */
int bam_format_disk(bam_editor_t *editor, const char *disk_name, const char *disk_id)
{
    if (!editor || !disk_name || !disk_id) return -1;
    
    /* Clear BAM sector */
    int bam_offset = bam_sector_offset(BAM_TRACK, BAM_SECTOR);
    memset(editor->d64_data + bam_offset, 0, 256);
    
    /* Set up BAM */
    editor->bam->dir_track = DIR_TRACK;
    editor->bam->dir_sector = DIR_FIRST_SECTOR;
    editor->bam->dos_version = 'A';
    editor->bam->unused1 = 0x00;
    
    /* Initialize track entries */
    for (int t = 1; t <= editor->num_tracks; t++) {
        editor->bam->tracks[t].free_sectors = bam_sectors_per_track[t];
        memset(editor->bam->tracks[t].bitmap, 0xFF, 3);
        
        /* Clear bits for non-existent sectors */
        for (int s = bam_sectors_per_track[t]; s < 24; s++) {
            int byte_idx = s / 8;
            int bit_idx = s % 8;
            editor->bam->tracks[t].bitmap[byte_idx] &= ~(1 << bit_idx);
        }
    }
    
    /* Allocate BAM and directory */
    bam_allocate_block(editor, BAM_TRACK, BAM_SECTOR);
    bam_allocate_block(editor, DIR_TRACK, DIR_FIRST_SECTOR);
    
    /* Set disk name */
    memset(editor->bam->disk_name, 0xA0, 16);
    bam_ascii_to_petscii(disk_name, editor->bam->disk_name, 16);
    
    /* Set padding */
    editor->bam->padding1[0] = 0xA0;
    editor->bam->padding1[1] = 0xA0;
    
    /* Set disk ID */
    editor->bam->disk_id[0] = disk_id[0];
    editor->bam->disk_id[1] = disk_id[1];
    
    /* Set padding and DOS type */
    editor->bam->padding2 = 0xA0;
    editor->bam->dos_type[0] = '2';
    editor->bam->dos_type[1] = 'A';
    
    memset(editor->bam->padding3, 0xA0, 4);
    
    /* Clear directory sector */
    int dir_offset = bam_sector_offset(DIR_TRACK, DIR_FIRST_SECTOR);
    memset(editor->d64_data + dir_offset, 0, 256);
    editor->d64_data[dir_offset + 1] = 0xFF;  /* End of directory chain */
    
    editor->modified = true;
    return 0;
}

/**
 * @brief Create empty D64
 */
int bam_create_d64(int tracks, const char *disk_name, const char *disk_id,
                   uint8_t **d64_data, size_t *d64_size)
{
    if (!d64_data || !d64_size) return -1;
    if (tracks != 35 && tracks != 40) return -2;
    
    size_t size = (tracks == 40) ? BAM_D64_40_TRACKS : BAM_D64_35_TRACKS;
    
    uint8_t *data = calloc(1, size);
    if (!data) return -3;
    
    bam_editor_t *editor = bam_editor_create(data, size);
    if (!editor) {
        free(data);
        return -4;
    }
    
    bam_format_disk(editor, disk_name ? disk_name : "EMPTY DISK",
                    disk_id ? disk_id : "01");
    
    bam_editor_free(editor);
    
    *d64_data = data;
    *d64_size = size;
    return 0;
}

/* ============================================================================
 * Sector I/O
 * ============================================================================ */

/**
 * @brief Read sector
 */
int bam_read_sector(const bam_editor_t *editor, int track, int sector,
                    uint8_t *buffer)
{
    if (!editor || !buffer) return -1;
    
    int offset = bam_sector_offset(track, sector);
    if (offset < 0 || (size_t)(offset + 256) > editor->d64_size) return -2;
    
    memcpy(buffer, editor->d64_data + offset, 256);
    return 0;
}

/**
 * @brief Write sector
 */
int bam_write_sector(bam_editor_t *editor, int track, int sector,
                     const uint8_t *buffer)
{
    if (!editor || !buffer) return -1;
    
    int offset = bam_sector_offset(track, sector);
    if (offset < 0 || (size_t)(offset + 256) > editor->d64_size) return -2;
    
    memcpy(editor->d64_data + offset, buffer, 256);
    editor->modified = true;
    return 0;
}

/* ============================================================================
 * Utilities
 * ============================================================================ */

/**
 * @brief ASCII to PETSCII
 */
void bam_ascii_to_petscii(const char *ascii, char *petscii, size_t len)
{
    if (!ascii || !petscii) return;
    
    for (size_t i = 0; i < len && ascii[i]; i++) {
        char c = ascii[i];
        
        /* Simple conversion: lowercase to uppercase PETSCII */
        if (c >= 'a' && c <= 'z') {
            petscii[i] = c - 32;
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
void bam_petscii_to_ascii(const char *petscii, char *ascii, size_t len)
{
    if (!petscii || !ascii) return;
    
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)petscii[i];
        
        if (c == 0xA0) {
            ascii[i] = ' ';  /* Shifted space */
        } else if (c >= 0x41 && c <= 0x5A) {
            ascii[i] = c;  /* A-Z */
        } else if (c >= 0xC1 && c <= 0xDA) {
            ascii[i] = c - 0x80;  /* Shifted letters */
        } else if (c >= 0x20 && c <= 0x7E) {
            ascii[i] = c;
        } else {
            ascii[i] = '?';
        }
    }
}

/**
 * @brief Get file type name
 */
const char *bam_file_type_name(uint8_t file_type)
{
    int type = file_type & 0x07;
    if (type >= 5) type = 5;
    return file_type_names[type];
}

/**
 * @brief Get sectors per track
 */
int bam_sectors_for_track(int track)
{
    if (track < 1 || track > 42) return 0;
    return bam_sectors_per_track[track];
}

/**
 * @brief Print directory
 */
void bam_print_directory(const bam_editor_t *editor, FILE *fp)
{
    if (!editor || !fp) return;
    
    disk_info_t info;
    bam_get_disk_info(editor, &info);
    
    fprintf(fp, "0 \"%-16s\" %s %s\n", info.disk_name, info.disk_id, info.dos_type);
    
    for (int i = 0; i < DIR_MAX_ENTRIES; i++) {
        file_info_t file;
        if (bam_get_file_info(editor, i, &file) != 0) break;
        
        fprintf(fp, "%-5d \"%-16s\" %s%s%s\n",
                file.blocks,
                file.filename,
                bam_file_type_name(file.file_type),
                file.locked ? "<" : "",
                file.closed ? "" : "*");
    }
    
    fprintf(fp, "%d BLOCKS FREE.\n", info.free_blocks);
}

/**
 * @brief Print BAM map
 */
void bam_print_map(const bam_editor_t *editor, FILE *fp)
{
    if (!editor || !fp) return;
    
    fprintf(fp, "BAM Map:\n");
    fprintf(fp, "Track  Free  Sectors\n");
    fprintf(fp, "-----  ----  -------\n");
    
    for (int t = 1; t <= editor->num_tracks; t++) {
        fprintf(fp, "%5d  %4d  ", t, editor->bam->tracks[t].free_sectors);
        
        for (int s = 0; s < bam_sectors_per_track[t]; s++) {
            fprintf(fp, "%c", bam_is_block_free(editor, t, s) ? '.' : '#');
        }
        
        fprintf(fp, "\n");
    }
}
