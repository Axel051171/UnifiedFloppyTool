/**
 * @file uft_cbm_fs_file.c
 * @brief Commodore CBM DOS Filesystem - File Operations Implementation
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
 * File Chain Functions
 *============================================================================*/

uft_rc_t uft_cbm_chain_create(uft_cbm_chain_t** chain) {
    if (!chain) return UFT_ERR_INVALID_ARG;
    
    *chain = (uft_cbm_chain_t*)calloc(1, sizeof(uft_cbm_chain_t));
    if (!*chain) return UFT_ERR_MEMORY;
    
    /* Initial capacity */
    (*chain)->capacity = 256;
    (*chain)->chain = (uft_cbm_ts_t*)calloc((*chain)->capacity, sizeof(uft_cbm_ts_t));
    if (!(*chain)->chain) {
        free(*chain);
        *chain = NULL;
        return UFT_ERR_MEMORY;
    }
    
    return UFT_SUCCESS;
}

void uft_cbm_chain_destroy(uft_cbm_chain_t** chain) {
    if (!chain || !*chain) return;
    
    free((*chain)->chain);
    free(*chain);
    *chain = NULL;
}

uft_rc_t uft_cbm_chain_follow(uft_cbm_fs_t* fs, uint8_t start_track,
                               uint8_t start_sector, uft_cbm_chain_t* chain) {
    if (!fs || !fs->image || !chain) return UFT_ERR_INVALID_ARG;
    
    /* Reset chain */
    chain->count = 0;
    chain->last_bytes = 0;
    chain->total_bytes = 0;
    chain->circular = false;
    chain->broken = false;
    chain->cross_linked = false;
    
    if (start_track == 0) {
        return UFT_SUCCESS;  /* Empty file */
    }
    
    uint8_t track = start_track;
    uint8_t sector = start_sector;
    uint8_t sector_data[UFT_CBM_SECTOR_SIZE];
    
    /* Track visited sectors for circular detection */
    /* Simple approach: check against all previous in chain */
    
    while (track != 0 && chain->count < 1000) {  /* Limit to prevent infinite loop */
        /* Validate track/sector */
        uint8_t max_sectors = uft_cbm_sectors_per_track(fs->type, track);
        if (max_sectors == 0 || sector >= max_sectors) {
            chain->broken = true;
            break;
        }
        
        /* Check for circular reference */
        for (uint16_t i = 0; i < chain->count; i++) {
            if (chain->chain[i].track == track && chain->chain[i].sector == sector) {
                chain->circular = true;
                return UFT_SUCCESS;
            }
        }
        
        /* Expand chain if needed */
        if (chain->count >= chain->capacity) {
            uint16_t new_cap = chain->capacity * 2;
            uft_cbm_ts_t* new_chain = (uft_cbm_ts_t*)realloc(chain->chain, 
                                                              new_cap * sizeof(uft_cbm_ts_t));
            if (!new_chain) return UFT_ERR_MEMORY;
            chain->chain = new_chain;
            chain->capacity = new_cap;
        }
        
        /* Add to chain */
        chain->chain[chain->count].track = track;
        chain->chain[chain->count].sector = sector;
        chain->count++;
        
        /* Read sector */
        uft_rc_t rc = uft_cbm_read_sector(fs, track, sector, sector_data);
        if (rc != UFT_SUCCESS) {
            chain->broken = true;
            break;
        }
        
        /* Get next track/sector from link bytes */
        uint8_t next_track = sector_data[0];
        uint8_t next_sector = sector_data[1];
        
        if (next_track == 0) {
            /* Last sector - next_sector contains bytes used */
            chain->last_bytes = next_sector;
            chain->total_bytes = (uint32_t)(chain->count - 1) * 254 + (next_sector - 1);
            break;
        }
        
        chain->total_bytes += 254;
        track = next_track;
        sector = next_sector;
    }
    
    return UFT_SUCCESS;
}

bool uft_cbm_chain_validate(uft_cbm_fs_t* fs, const uft_cbm_chain_t* chain) {
    if (!fs || !fs->bam || !chain) return false;
    
    if (chain->circular || chain->broken) return false;
    
    /* Check that all sectors in chain are allocated */
    for (uint16_t i = 0; i < chain->count; i++) {
        if (!uft_cbm_bam_is_allocated(fs, chain->chain[i].track, chain->chain[i].sector)) {
            return false;  /* Sector should be allocated but isn't */
        }
    }
    
    return true;
}

/*============================================================================
 * File Extraction
 *============================================================================*/

uft_rc_t uft_cbm_file_extract(uft_cbm_fs_t* fs, const char* filename,
                               const uft_cbm_extract_opts_t* opts,
                               uint8_t** data, size_t* size) {
    if (!fs || !filename || !data || !size) return UFT_ERR_INVALID_ARG;
    
    uft_cbm_dir_entry_t entry;
    uft_rc_t rc = uft_cbm_dir_find(fs, filename, &entry);
    if (rc != UFT_SUCCESS) return rc;
    
    return uft_cbm_file_extract_entry(fs, &entry, opts, data, size);
}

uft_rc_t uft_cbm_file_extract_entry(uft_cbm_fs_t* fs,
                                     const uft_cbm_dir_entry_t* entry,
                                     const uft_cbm_extract_opts_t* opts,
                                     uint8_t** data, size_t* size) {
    if (!fs || !fs->image || !entry || !data || !size) return UFT_ERR_INVALID_ARG;
    
    *data = NULL;
    *size = 0;
    
    uft_cbm_extract_opts_t default_opts = uft_cbm_extract_opts_default();
    if (!opts) opts = &default_opts;
    
    /* Handle empty file */
    if (entry->first_ts.track == 0) {
        *data = (uint8_t*)malloc(1);
        if (!*data) return UFT_ERR_MEMORY;
        *size = 0;
        return UFT_SUCCESS;
    }
    
    /* Follow chain */
    uft_cbm_chain_t* chain = NULL;
    uft_rc_t rc = uft_cbm_chain_create(&chain);
    if (rc != UFT_SUCCESS) return rc;
    
    rc = uft_cbm_chain_follow(fs, entry->first_ts.track, entry->first_ts.sector, chain);
    if (rc != UFT_SUCCESS || chain->broken) {
        uft_cbm_chain_destroy(&chain);
        return (rc != UFT_SUCCESS) ? rc : UFT_ERR_CORRUPT;
    }
    
    /* Check size limit */
    size_t total_size = chain->total_bytes;
    if (opts->max_size > 0 && total_size > opts->max_size) {
        uft_cbm_chain_destroy(&chain);
        return UFT_ERR_BUFFER_TOO_SMALL;
    }
    
    /* Allocate output buffer */
    *data = (uint8_t*)malloc(total_size > 0 ? total_size : 1);
    if (!*data) {
        uft_cbm_chain_destroy(&chain);
        return UFT_ERR_MEMORY;
    }
    
    /* Extract data */
    uint8_t sector_data[UFT_CBM_SECTOR_SIZE];
    size_t out_pos = 0;
    
    for (uint16_t i = 0; i < chain->count; i++) {
        rc = uft_cbm_read_sector(fs, chain->chain[i].track, 
                                  chain->chain[i].sector, sector_data);
        if (rc != UFT_SUCCESS) {
            free(*data);
            *data = NULL;
            uft_cbm_chain_destroy(&chain);
            return rc;
        }
        
        /* Calculate bytes to copy from this sector */
        size_t bytes_to_copy;
        if (i == chain->count - 1) {
            /* Last sector */
            bytes_to_copy = chain->last_bytes > 1 ? chain->last_bytes - 1 : 0;
        } else {
            bytes_to_copy = 254;  /* Data starts at byte 2 */
        }
        
        if (out_pos + bytes_to_copy > total_size) {
            bytes_to_copy = total_size - out_pos;
        }
        
        if (bytes_to_copy > 0) {
            memcpy(*data + out_pos, sector_data + 2, bytes_to_copy);
            out_pos += bytes_to_copy;
        }
    }
    
    *size = out_pos;
    
    /* Convert PETSCII to ASCII for SEQ files if requested */
    if (opts->convert_petscii && entry->file_type == UFT_CBM_FT_SEQ) {
        for (size_t i = 0; i < *size; i++) {
            uint8_t c = (*data)[i];
            if (c >= 0xC1 && c <= 0xDA) {
                (*data)[i] = c - 0x80;  /* Shifted letters to uppercase */
            } else if (c >= 0x41 && c <= 0x5A) {
                (*data)[i] = c + 0x20;  /* Uppercase PETSCII to lowercase */
            }
        }
    }
    
    uft_cbm_chain_destroy(&chain);
    return UFT_SUCCESS;
}

uft_rc_t uft_cbm_file_save(uft_cbm_fs_t* fs, const char* filename,
                            const char* path, const uft_cbm_extract_opts_t* opts) {
    if (!fs || !filename || !path) return UFT_ERR_INVALID_ARG;
    
    uint8_t* data = NULL;
    size_t size = 0;
    
    uft_rc_t rc = uft_cbm_file_extract(fs, filename, opts, &data, &size);
    if (rc != UFT_SUCCESS) return rc;
    
    FILE* f = fopen(path, "wb");
    if (!f) {
        free(data);
        return UFT_ERR_IO;
    }
    
    if (size > 0) {
        size_t written = fwrite(data, 1, size, f);
        if (written != size) {
            fclose(f);
            free(data);
            return UFT_ERR_IO;
        }
    }
    
    fclose(f);
    free(data);
    return UFT_SUCCESS;
}

/*============================================================================
 * File Injection
 *============================================================================*/

uft_rc_t uft_cbm_file_inject(uft_cbm_fs_t* fs, const char* filename,
                              const uint8_t* data, size_t size,
                              const uft_cbm_inject_opts_t* opts) {
    if (!fs || !fs->image || !filename) return UFT_ERR_INVALID_ARG;
    if (!fs->writable) return UFT_ERR_NOT_PERMITTED;
    if (size > 0 && !data) return UFT_ERR_INVALID_ARG;
    
    uft_cbm_inject_opts_t default_opts = uft_cbm_inject_opts_default();
    if (!opts) opts = &default_opts;
    
    /* Check filename length */
    size_t name_len = strlen(filename);
    if (name_len > UFT_CBM_FILENAME_MAX) return UFT_ERR_INVALID_ARG;
    
    /* Check if file exists */
    uft_cbm_dir_entry_t existing;
    bool exists = (uft_cbm_dir_find(fs, filename, &existing) == UFT_SUCCESS);
    
    if (exists && !opts->replace_existing) {
        return UFT_ERR_EXISTS;
    }
    
    /* Delete existing file if replacing */
    if (exists && opts->replace_existing) {
        uft_cbm_file_delete(fs, filename, NULL);
    }
    
    /* Calculate blocks needed */
    uint16_t blocks_needed = (size > 0) ? (uint16_t)((size + 253) / 254) : 0;
    
    /* Check free space */
    if (blocks_needed > uft_cbm_bam_free_blocks(fs)) {
        return UFT_ERR_DISK_FULL;
    }
    
    /* Find free directory entry */
    uint8_t dir_track, dir_sector;
    if (fs->type == UFT_CBM_TYPE_D81) {
        dir_track = 40;
        dir_sector = 3;
    } else {
        dir_track = 18;
        dir_sector = 1;
    }
    
    uint8_t sector_data[UFT_CBM_SECTOR_SIZE];
    uint8_t entry_track = 0, entry_sector = 0, entry_slot = 0;
    bool found_slot = false;
    
    while (dir_track != 0 && !found_slot) {
        uft_cbm_read_sector(fs, dir_track, dir_sector, sector_data);
        
        for (int slot = 0; slot < 8; slot++) {
            uint8_t* entry_raw = &sector_data[slot * 32];
            
            /* Empty or deleted entry */
            if (entry_raw[2] == 0 || (entry_raw[2] & 0x07) == 0) {
                entry_track = dir_track;
                entry_sector = dir_sector;
                entry_slot = (uint8_t)slot;
                found_slot = true;
                break;
            }
        }
        
        if (!found_slot) {
            uint8_t next_track = sector_data[0];
            uint8_t next_sector = sector_data[1];
            
            if (next_track == 0) {
                /* Need to allocate new directory sector */
                uint8_t new_track, new_sector;
                uft_rc_t rc = uft_cbm_bam_alloc_next(fs, dir_track, &new_track, &new_sector, 3);
                if (rc != UFT_SUCCESS) return UFT_ERR_DISK_FULL;
                
                /* Link to new sector */
                sector_data[0] = new_track;
                sector_data[1] = new_sector;
                uft_cbm_write_sector(fs, dir_track, dir_sector, sector_data);
                
                /* Initialize new directory sector */
                memset(sector_data, 0, UFT_CBM_SECTOR_SIZE);
                sector_data[0] = 0;   /* No next sector */
                sector_data[1] = 0xFF;
                uft_cbm_write_sector(fs, new_track, new_sector, sector_data);
                
                entry_track = new_track;
                entry_sector = new_sector;
                entry_slot = 0;
                found_slot = true;
            } else {
                dir_track = next_track;
                dir_sector = next_sector;
            }
        }
    }
    
    if (!found_slot) return UFT_ERR_DISK_FULL;
    
    /* Allocate data sectors */
    uint8_t first_track = 0, first_sector = 0;
    uint8_t prev_track = 0, prev_sector = 0;
    
    const uint8_t* src = data;
    size_t remaining = size;
    
    while (remaining > 0 || (size == 0 && first_track == 0)) {
        uint8_t new_track, new_sector;
        uft_rc_t rc = uft_cbm_bam_alloc_next(fs, prev_track ? prev_track : 1,
                                              &new_track, &new_sector, opts->interleave);
        if (rc != UFT_SUCCESS) {
            /* TODO: Free already allocated sectors */
            return UFT_ERR_DISK_FULL;
        }
        
        if (first_track == 0) {
            first_track = new_track;
            first_sector = new_sector;
        }
        
        /* Link previous sector */
        if (prev_track != 0) {
            uint8_t prev_data[UFT_CBM_SECTOR_SIZE];
            uft_cbm_read_sector(fs, prev_track, prev_sector, prev_data);
            prev_data[0] = new_track;
            prev_data[1] = new_sector;
            uft_cbm_write_sector(fs, prev_track, prev_sector, prev_data);
        }
        
        /* Write data sector */
        memset(sector_data, 0, UFT_CBM_SECTOR_SIZE);
        
        size_t to_write = (remaining > 254) ? 254 : remaining;
        if (to_write > 0 && src) {
            memcpy(sector_data + 2, src, to_write);
            src += to_write;
            remaining -= to_write;
        }
        
        if (remaining == 0) {
            /* Last sector */
            sector_data[0] = 0;
            sector_data[1] = (uint8_t)(to_write + 1);  /* Bytes used + 1 */
        }
        
        uft_cbm_write_sector(fs, new_track, new_sector, sector_data);
        
        prev_track = new_track;
        prev_sector = new_sector;
        
        if (size == 0) break;  /* Empty file, just need one iteration */
    }
    
    /* Write directory entry */
    uft_cbm_read_sector(fs, entry_track, entry_sector, sector_data);
    
    uint8_t* entry_raw = &sector_data[entry_slot * 32];
    memset(entry_raw, 0, 32);
    
    /* File type byte */
    entry_raw[2] = (uint8_t)(opts->file_type | UFT_CBM_FLAG_CLOSED);
    if (opts->lock_file) entry_raw[2] |= UFT_CBM_FLAG_LOCKED;
    
    /* First track/sector */
    entry_raw[3] = first_track;
    entry_raw[4] = first_sector;
    
    /* Filename (PETSCII) */
    uint8_t petscii_name[UFT_CBM_FILENAME_MAX];
    size_t petscii_len = uft_cbm_ascii_to_petscii(filename, petscii_name, UFT_CBM_FILENAME_MAX);
    memcpy(entry_raw + 5, petscii_name, petscii_len);
    uft_cbm_pad_filename(entry_raw + 5, petscii_len, UFT_CBM_FILENAME_MAX);
    
    /* REL record length */
    if (opts->file_type == UFT_CBM_FT_REL) {
        entry_raw[23] = opts->rel_record_len;
    }
    
    /* Block count */
    entry_raw[30] = (uint8_t)(blocks_needed & 0xFF);
    entry_raw[31] = (uint8_t)((blocks_needed >> 8) & 0xFF);
    
    uft_cbm_write_sector(fs, entry_track, entry_sector, sector_data);
    
    /* Save BAM */
    uft_cbm_bam_save(fs);
    
    /* Reload directory */
    uft_cbm_dir_load(fs);
    
    return UFT_SUCCESS;
}

uft_rc_t uft_cbm_file_load(uft_cbm_fs_t* fs, const char* filename,
                            const char* path, const uft_cbm_inject_opts_t* opts) {
    if (!fs || !filename || !path) return UFT_ERR_INVALID_ARG;
    
    /* Read file from disk */
    FILE* f = fopen(path, "rb");
    if (!f) return UFT_ERR_FILE_NOT_FOUND;
    
    if (fseek(f, 0, SEEK_END) != 0) { /* seek error */ }
    long file_size = ftell(f);
    if (fseek(f, 0, SEEK_SET) != 0) { /* seek error */ }
    if (file_size < 0 || file_size > 16 * 1024 * 1024) {  /* Max 16MB */
        fclose(f);
        return UFT_ERR_INVALID_ARG;
    }
    
    uint8_t* data = NULL;
    if (file_size > 0) {
        data = (uint8_t*)malloc((size_t)file_size);
        if (!data) {
            fclose(f);
            return UFT_ERR_MEMORY;
        }
        
        if (fread(data, 1, (size_t)file_size, f) != (size_t)file_size) {
            free(data);
            fclose(f);
            return UFT_ERR_IO;
        }
    }
    fclose(f);
    
    uft_rc_t rc = uft_cbm_file_inject(fs, filename, data, (size_t)file_size, opts);
    free(data);
    return rc;
}

/*============================================================================
 * File Delete/Rename/Copy/Lock
 *============================================================================*/

uft_rc_t uft_cbm_file_delete(uft_cbm_fs_t* fs, const char* filename,
                              uint16_t* deleted) {
    if (!fs || !fs->image || !filename) return UFT_ERR_INVALID_ARG;
    if (!fs->writable) return UFT_ERR_NOT_PERMITTED;
    
    uint16_t del_count = 0;
    
    /* Find all matching files (wildcard support) */
    for (uint16_t i = 0; i < fs->dir->count; i++) {
        uft_cbm_dir_entry_t* e = &fs->dir->entries[i];
        
        /* Skip already deleted */
        if (e->file_type == UFT_CBM_FT_DEL && e->first_ts.track == 0) continue;
        
        /* Check locked */
        if (e->flags & UFT_CBM_FLAG_LOCKED) continue;
        
        /* Check wildcard match */
        char ascii_name[32];
        uft_cbm_petscii_to_ascii(e->filename, e->filename_len, ascii_name, sizeof(ascii_name));
        
        /* Simple wildcard matching (just * at end for now) */
        bool match = false;
        size_t pat_len = strlen(filename);
        if (pat_len > 0 && filename[pat_len - 1] == '*') {
            match = (strncmp(ascii_name, filename, pat_len - 1) == 0);
        } else {
            match = (strcmp(ascii_name, filename) == 0);
        }
        
        if (match) {
            /* Free file sectors */
            if (e->first_ts.track != 0) {
                uft_cbm_chain_t* chain = NULL;
                if (uft_cbm_chain_create(&chain) == UFT_SUCCESS) {
                    uft_cbm_chain_follow(fs, e->first_ts.track, e->first_ts.sector, chain);
                    
                    for (uint16_t j = 0; j < chain->count; j++) {
                        uft_cbm_bam_free(fs, chain->chain[j].track, chain->chain[j].sector);
                    }
                    uft_cbm_chain_destroy(&chain);
                }
            }
            
            /* Clear directory entry */
            uint8_t sector_data[UFT_CBM_SECTOR_SIZE];
            uft_cbm_read_sector(fs, e->entry_ts.track, e->entry_ts.sector, sector_data);
            
            uint8_t* entry_raw = &sector_data[e->entry_offset * 32];
            entry_raw[2] = UFT_CBM_FT_DEL;  /* Mark as deleted */
            entry_raw[3] = 0;  /* Clear T/S */
            entry_raw[4] = 0;
            
            uft_cbm_write_sector(fs, e->entry_ts.track, e->entry_ts.sector, sector_data);
            del_count++;
        }
    }
    
    if (deleted) *deleted = del_count;
    
    if (del_count > 0) {
        uft_cbm_bam_save(fs);
        uft_cbm_dir_load(fs);
    }
    
    return (del_count > 0) ? UFT_SUCCESS : UFT_ERR_NOT_FOUND;
}

uft_rc_t uft_cbm_file_rename(uft_cbm_fs_t* fs, const char* old_name,
                              const char* new_name) {
    if (!fs || !fs->image || !old_name || !new_name) return UFT_ERR_INVALID_ARG;
    if (!fs->writable) return UFT_ERR_NOT_PERMITTED;
    
    /* Check new name length */
    if (strlen(new_name) > UFT_CBM_FILENAME_MAX) return UFT_ERR_INVALID_ARG;
    
    /* Check if new name exists */
    uft_cbm_dir_entry_t check;
    if (uft_cbm_dir_find(fs, new_name, &check) == UFT_SUCCESS) {
        return UFT_ERR_EXISTS;
    }
    
    /* Find old file */
    uft_cbm_dir_entry_t entry;
    uft_rc_t rc = uft_cbm_dir_find(fs, old_name, &entry);
    if (rc != UFT_SUCCESS) return rc;
    
    /* Update directory entry */
    uint8_t sector_data[UFT_CBM_SECTOR_SIZE];
    uft_cbm_read_sector(fs, entry.entry_ts.track, entry.entry_ts.sector, sector_data);
    
    uint8_t* entry_raw = &sector_data[entry.entry_offset * 32];
    
    /* Clear old filename */
    memset(entry_raw + 5, 0xA0, UFT_CBM_FILENAME_MAX);
    
    /* Set new filename */
    uint8_t petscii_name[UFT_CBM_FILENAME_MAX];
    size_t petscii_len = uft_cbm_ascii_to_petscii(new_name, petscii_name, UFT_CBM_FILENAME_MAX);
    memcpy(entry_raw + 5, petscii_name, petscii_len);
    
    uft_cbm_write_sector(fs, entry.entry_ts.track, entry.entry_ts.sector, sector_data);
    
    /* Reload directory */
    uft_cbm_dir_load(fs);
    
    return UFT_SUCCESS;
}

uft_rc_t uft_cbm_file_copy(uft_cbm_fs_t* fs, const char* src_name,
                            const char* dst_name) {
    if (!fs || !src_name || !dst_name) return UFT_ERR_INVALID_ARG;
    
    /* Extract source file */
    uint8_t* data = NULL;
    size_t size = 0;
    
    uft_rc_t rc = uft_cbm_file_extract(fs, src_name, NULL, &data, &size);
    if (rc != UFT_SUCCESS) return rc;
    
    /* Get source file type */
    uft_cbm_dir_entry_t src_entry;
    rc = uft_cbm_dir_find(fs, src_name, &src_entry);
    if (rc != UFT_SUCCESS) {
        free(data);
        return rc;
    }
    
    /* Inject as new file */
    uft_cbm_inject_opts_t opts = uft_cbm_inject_opts_default();
    opts.file_type = src_entry.file_type;
    opts.auto_load_addr = false;  /* Data already includes load address */
    
    rc = uft_cbm_file_inject(fs, dst_name, data, size, &opts);
    free(data);
    
    return rc;
}

uft_rc_t uft_cbm_file_lock(uft_cbm_fs_t* fs, const char* filename, bool locked) {
    if (!fs || !fs->image || !filename) return UFT_ERR_INVALID_ARG;
    if (!fs->writable) return UFT_ERR_NOT_PERMITTED;
    
    uft_cbm_dir_entry_t entry;
    uft_rc_t rc = uft_cbm_dir_find(fs, filename, &entry);
    if (rc != UFT_SUCCESS) return rc;
    
    uint8_t sector_data[UFT_CBM_SECTOR_SIZE];
    uft_cbm_read_sector(fs, entry.entry_ts.track, entry.entry_ts.sector, sector_data);
    
    uint8_t* entry_raw = &sector_data[entry.entry_offset * 32];
    
    if (locked) {
        entry_raw[2] |= UFT_CBM_FLAG_LOCKED;
    } else {
        entry_raw[2] &= ~UFT_CBM_FLAG_LOCKED;
    }
    
    uft_cbm_write_sector(fs, entry.entry_ts.track, entry.entry_ts.sector, sector_data);
    
    uft_cbm_dir_load(fs);
    
    return UFT_SUCCESS;
}

/*============================================================================
 * Validation Functions
 *============================================================================*/

uft_rc_t uft_cbm_validation_create(uft_cbm_validation_t** report) {
    if (!report) return UFT_ERR_INVALID_ARG;
    
    *report = (uft_cbm_validation_t*)calloc(1, sizeof(uft_cbm_validation_t));
    if (!*report) return UFT_ERR_MEMORY;
    
    return UFT_SUCCESS;
}

void uft_cbm_validation_destroy(uft_cbm_validation_t** report) {
    if (!report || !*report) return;
    
    uft_cbm_validation_t* r = *report;
    
    for (uint16_t i = 0; i < r->error_count; i++) {
        free(r->errors[i]);
    }
    free(r->errors);
    
    for (uint16_t i = 0; i < r->warning_count; i++) {
        free(r->warnings[i]);
    }
    free(r->warnings);
    
    free(r);
    *report = NULL;
}

static void add_error(uft_cbm_validation_t* r, const char* fmt, ...) {
    if (r->error_count >= 1000) return;  /* Limit errors */
    
    char** new_errors = (char**)realloc(r->errors, (r->error_count + 1) * sizeof(char*));
    if (!new_errors) return;
    r->errors = new_errors;
    
    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    
    r->errors[r->error_count] = strdup(buf);
    r->error_count++;
}

static void add_warning(uft_cbm_validation_t* r, const char* fmt, ...) {
    if (r->warning_count >= 1000) return;
    
    char** new_warnings = (char**)realloc(r->warnings, (r->warning_count + 1) * sizeof(char*));
    if (!new_warnings) return;
    r->warnings = new_warnings;
    
    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    
    r->warnings[r->warning_count] = strdup(buf);
    r->warning_count++;
}

uft_rc_t uft_cbm_validate(uft_cbm_fs_t* fs, uft_cbm_validation_t* report) {
    if (!fs || !fs->image || !report) return UFT_ERR_INVALID_ARG;
    
    memset(report, 0, sizeof(*report));
    report->type = fs->type;
    report->has_errors = fs->has_errors;
    
    /* Reload BAM and directory for fresh state */
    uft_cbm_bam_load(fs);
    uft_cbm_dir_load(fs);
    
    report->bam_valid = (fs->bam != NULL);
    report->dir_valid = (fs->dir != NULL);
    
    if (!report->bam_valid || !report->dir_valid) {
        add_error(report, "Failed to load BAM or directory");
        return UFT_ERR_VALIDATION;
    }
    
    /* Track sector usage from files */
    uint8_t* used = (uint8_t*)calloc(fs->tracks + 1, 256);  /* Track usage bitmap */
    if (!used) return UFT_ERR_MEMORY;
    
    report->total_files = fs->dir->count;
    report->chains_valid = true;
    
    /* Validate each file */
    for (uint16_t i = 0; i < fs->dir->count; i++) {
        uft_cbm_dir_entry_t* e = &fs->dir->entries[i];
        
        if (e->first_ts.track == 0) continue;  /* Empty file */
        
        uft_cbm_chain_t* chain = NULL;
        if (uft_cbm_chain_create(&chain) != UFT_SUCCESS) continue;
        
        uft_cbm_chain_follow(fs, e->first_ts.track, e->first_ts.sector, chain);
        
        if (chain->broken) {
            char name[32];
            uft_cbm_petscii_to_ascii(e->filename, e->filename_len, name, sizeof(name));
            add_error(report, "File '%s' has broken chain", name);
            report->broken_chains++;
            report->chains_valid = false;
        }
        
        if (chain->circular) {
            char name[32];
            uft_cbm_petscii_to_ascii(e->filename, e->filename_len, name, sizeof(name));
            add_error(report, "File '%s' has circular chain", name);
            report->chains_valid = false;
        }
        
        /* Check for cross-links */
        for (uint16_t j = 0; j < chain->count; j++) {
            uint8_t t = chain->chain[j].track;
            uint8_t s = chain->chain[j].sector;
            
            if (used[t * 256 + s]) {
                report->cross_links++;
                char name[32];
                uft_cbm_petscii_to_ascii(e->filename, e->filename_len, name, sizeof(name));
                add_error(report, "Cross-link at T%d S%d (file '%s')", t, s, name);
            }
            used[t * 256 + s] = 1;
            
            /* Check against BAM */
            if (!uft_cbm_bam_is_allocated(fs, t, s)) {
                report->unallocated_used++;
                add_warning(report, "Sector T%d S%d used but not allocated in BAM", t, s);
            }
        }
        
        uft_cbm_chain_destroy(&chain);
    }
    
    /* Check for orphan sectors (allocated but not used) */
    for (uint8_t t = 1; t <= fs->tracks; t++) {
        uint8_t max_s = uft_cbm_sectors_per_track(fs->type, t);
        for (uint8_t s = 0; s < max_s; s++) {
            if (uft_cbm_bam_is_allocated(fs, t, s) && !used[t * 256 + s]) {
                /* Skip directory track */
                bool is_dir_track = false;
                if ((fs->type == UFT_CBM_TYPE_D64 || fs->type == UFT_CBM_TYPE_D64_40 ||
                     fs->type == UFT_CBM_TYPE_D71 || fs->type == UFT_CBM_TYPE_D71_80) && t == 18) {
                    is_dir_track = true;
                }
                if (fs->type == UFT_CBM_TYPE_D81 && t == 40) {
                    is_dir_track = true;
                }
                
                if (!is_dir_track) {
                    report->orphan_sectors++;
                }
            }
        }
    }
    
    free(used);
    
    report->bam_consistent = (report->unallocated_used == 0);
    
    bool valid = report->bam_valid && report->dir_valid && 
                 report->chains_valid && report->bam_consistent &&
                 report->cross_links == 0;
    
    return valid ? UFT_SUCCESS : UFT_ERR_VALIDATION;
}

uft_rc_t uft_cbm_fix_bam(uft_cbm_fs_t* fs, uint16_t* fixed) {
    if (!fs || !fs->image) return UFT_ERR_INVALID_ARG;
    if (!fs->writable) return UFT_ERR_NOT_PERMITTED;
    
    uint16_t fix_count = 0;
    
    /* Rebuild BAM from file chains */
    /* First, mark all sectors as free */
    for (uint8_t t = 1; t <= fs->tracks; t++) {
        uint8_t max_s = uft_cbm_sectors_per_track(fs->type, t);
        for (uint8_t s = 0; s < max_s; s++) {
            uft_cbm_bam_free(fs, t, s);
        }
    }
    
    /* Mark directory sectors as used */
    uint8_t dir_track, dir_sector;
    if (fs->type == UFT_CBM_TYPE_D81) {
        dir_track = 40;
        dir_sector = 0;
    } else {
        dir_track = 18;
        dir_sector = 0;
    }
    
    /* BAM sector(s) */
    uft_cbm_bam_allocate(fs, dir_track, dir_sector);
    if (fs->type == UFT_CBM_TYPE_D81) {
        uft_cbm_bam_allocate(fs, 40, 1);
        uft_cbm_bam_allocate(fs, 40, 2);
    }
    if (fs->type == UFT_CBM_TYPE_D71 || fs->type == UFT_CBM_TYPE_D71_80) {
        uft_cbm_bam_allocate(fs, 53, 0);
    }
    
    /* Directory sectors */
    uint8_t d_track = (fs->type == UFT_CBM_TYPE_D81) ? 40 : 18;
    uint8_t d_sector = (fs->type == UFT_CBM_TYPE_D81) ? 3 : 1;
    
    while (d_track != 0) {
        uft_cbm_bam_allocate(fs, d_track, d_sector);
        
        uint8_t sector_data[UFT_CBM_SECTOR_SIZE];
        uft_cbm_read_sector(fs, d_track, d_sector, sector_data);
        
        d_track = sector_data[0];
        d_sector = sector_data[1];
    }
    
    /* Mark file sectors as used */
    for (uint16_t i = 0; i < fs->dir->count; i++) {
        uft_cbm_dir_entry_t* e = &fs->dir->entries[i];
        
        if (e->first_ts.track == 0) continue;
        
        uft_cbm_chain_t* chain = NULL;
        if (uft_cbm_chain_create(&chain) != UFT_SUCCESS) continue;
        
        uft_cbm_chain_follow(fs, e->first_ts.track, e->first_ts.sector, chain);
        
        for (uint16_t j = 0; j < chain->count; j++) {
            uft_cbm_bam_allocate(fs, chain->chain[j].track, chain->chain[j].sector);
        }
        
        uft_cbm_chain_destroy(&chain);
    }
    
    uft_cbm_bam_save(fs);
    
    if (fixed) *fixed = fix_count;
    return UFT_SUCCESS;
}

/*============================================================================
 * Format Functions
 *============================================================================*/

uft_rc_t uft_cbm_fs_format(uft_cbm_fs_t* fs, uft_cbm_type_t type,
                            const char* disk_name, const char* disk_id) {
    if (!fs) return UFT_ERR_INVALID_ARG;
    
    /* Determine image size */
    size_t image_size;
    switch (type) {
        case UFT_CBM_TYPE_D64:    image_size = UFT_CBM_D64_SIZE; break;
        case UFT_CBM_TYPE_D64_40: image_size = UFT_CBM_D64_EXT_SIZE; break;
        case UFT_CBM_TYPE_D71:    image_size = UFT_CBM_D71_SIZE; break;
        case UFT_CBM_TYPE_D81:    image_size = UFT_CBM_D81_SIZE; break;
        default: return UFT_ERR_INVALID_ARG;
    }
    
    /* Allocate image */
    uint8_t* image = (uint8_t*)calloc(1, image_size);
    if (!image) return UFT_ERR_MEMORY;
    
    /* Close any existing */
    uft_cbm_fs_close(fs);
    
    fs->image = image;
    fs->image_size = image_size;
    fs->type = type;
    fs->tracks = (type == UFT_CBM_TYPE_D64) ? 35 : 
                 (type == UFT_CBM_TYPE_D64_40) ? 40 :
                 (type == UFT_CBM_TYPE_D71) ? 70 : 80;
    fs->writable = true;
    fs->modified = true;
    
    /* Initialize BAM */
    uint8_t bam_sector[UFT_CBM_SECTOR_SIZE];
    memset(bam_sector, 0, UFT_CBM_SECTOR_SIZE);
    
    if (type == UFT_CBM_TYPE_D64 || type == UFT_CBM_TYPE_D64_40) {
        /* Track 18, sector 0 */
        bam_sector[0] = 18;  /* Directory track */
        bam_sector[1] = 1;   /* Directory sector */
        bam_sector[2] = 0x41;  /* DOS version '2A' */
        bam_sector[3] = 0x00;
        
        /* Initialize BAM entries - all sectors free except track 18 */
        uint8_t max_track = (type == UFT_CBM_TYPE_D64) ? 35 : 40;
        for (uint8_t t = 1; t <= max_track; t++) {
            uint8_t sectors = uft_cbm_sectors_per_track(type, t);
            uint8_t offset = 4 + (t - 1) * 4;
            
            if (t == 18) {
                /* Directory track - allocate BAM and directory */
                bam_sector[offset] = sectors - 2;  /* BAM + 1 dir sector used */
                bam_sector[offset + 1] = 0xFC;     /* Sectors 0,1 used */
                bam_sector[offset + 2] = 0xFF;
                bam_sector[offset + 3] = (sectors > 16) ? 0x1F : 0xFF;
            } else {
                bam_sector[offset] = sectors;
                bam_sector[offset + 1] = 0xFF;
                bam_sector[offset + 2] = 0xFF;
                bam_sector[offset + 3] = (sectors > 16) ? 0x1F : 0xFF;
            }
        }
        
        /* Disk name at 0x90 */
        uint8_t petscii_name[UFT_CBM_FILENAME_MAX];
        memset(&bam_sector[0x90], 0xA0, UFT_CBM_FILENAME_MAX);
        if (disk_name) {
            size_t len = uft_cbm_ascii_to_petscii(disk_name, petscii_name, UFT_CBM_FILENAME_MAX);
            memcpy(&bam_sector[0x90], petscii_name, len);
        }
        
        /* Disk ID at 0xA2 */
        bam_sector[0xA2] = disk_id ? disk_id[0] : '0';
        bam_sector[0xA3] = disk_id && disk_id[0] ? disk_id[1] : '0';
        bam_sector[0xA4] = 0xA0;
        bam_sector[0xA5] = '2';  /* DOS type */
        bam_sector[0xA6] = 'A';
        
        uft_cbm_write_sector(fs, 18, 0, bam_sector);
        
        /* Initialize directory sector */
        memset(bam_sector, 0, UFT_CBM_SECTOR_SIZE);
        bam_sector[0] = 0;     /* No next sector */
        bam_sector[1] = 0xFF;
        uft_cbm_write_sector(fs, 18, 1, bam_sector);
    }
    
    /* Load BAM and directory */
    uft_cbm_bam_load(fs);
    uft_cbm_dir_load(fs);
    
    return UFT_SUCCESS;
}
