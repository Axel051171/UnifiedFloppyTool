/**
 * @file uft_apple_dos33.c
 * @brief Apple DOS 3.3 Filesystem Implementation
 * @version 3.7.0
 * 
 * VTOC management, catalog operations, T/S list handling.
 */

#include "uft/fs/uft_apple_dos.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/*===========================================================================
 * Internal Helpers
 *===========================================================================*/

/**
 * @brief Clear high bit from DOS 3.3 filename character
 */
static char clear_high_bit(char c) {
    return c & 0x7F;
}

/**
 * @brief Set high bit on DOS 3.3 filename character
 */
static char set_high_bit(char c) {
    return c | 0x80;
}

/**
 * @brief Extract filename from DOS 3.3 catalog entry
 */
static void extract_dos33_filename(const uft_dos33_entry_t *entry, char *name) {
    int len = 0;
    
    for (int i = 0; i < 30 && entry->filename[i]; i++) {
        char c = clear_high_bit(entry->filename[i]);
        if (c == ' ' && len == 0) continue;  /* Skip leading spaces */
        name[len++] = c;
    }
    
    /* Trim trailing spaces */
    while (len > 0 && name[len - 1] == ' ') len--;
    name[len] = '\0';
}

/**
 * @brief Encode filename for DOS 3.3 catalog entry
 */
static void encode_dos33_filename(const char *name, char *dest) {
    memset(dest, set_high_bit(' '), 30);  /* Fill with spaces */
    
    int len = strlen(name);
    if (len > 30) len = 30;
    
    for (int i = 0; i < len; i++) {
        dest[i] = set_high_bit(toupper(name[i]));
    }
}

/*===========================================================================
 * VTOC Bitmap Operations
 *===========================================================================*/

/**
 * @brief Check if sector is free in VTOC bitmap
 * 
 * Bitmap layout: 4 bytes per track (tracks 0-34 = 35*4 = 140 bytes)
 * Each 4-byte group: byte0-1 = sectors 0-15, byte2-3 = unused
 * Bit = 1 means free, bit = 0 means used
 */
static bool is_sector_free(const uft_dos33_vtoc_t *vtoc, uint8_t track, uint8_t sector) {
    if (track >= 35 || sector >= 16) return false;
    
    int byte_offset = track * 4 + (sector >> 3);
    int bit = 7 - (sector & 7);
    
    return (vtoc->bitmap[byte_offset] >> bit) & 1;
}

/**
 * @brief Mark sector as used in VTOC bitmap
 */
static void mark_sector_used(uft_dos33_vtoc_t *vtoc, uint8_t track, uint8_t sector) {
    if (track >= 35 || sector >= 16) return;
    
    int byte_offset = track * 4 + (sector >> 3);
    int bit = 7 - (sector & 7);
    
    vtoc->bitmap[byte_offset] &= ~(1 << bit);
}

/**
 * @brief Mark sector as free in VTOC bitmap
 */
static void mark_sector_free(uft_dos33_vtoc_t *vtoc, uint8_t track, uint8_t sector) {
    if (track >= 35 || sector >= 16) return;
    
    int byte_offset = track * 4 + (sector >> 3);
    int bit = 7 - (sector & 7);
    
    vtoc->bitmap[byte_offset] |= (1 << bit);
}

/*===========================================================================
 * Sector Allocation
 *===========================================================================*/

int uft_apple_alloc_sector(uft_apple_ctx_t *ctx, uint8_t *track, uint8_t *sector) {
    if (!ctx || !track || !sector) return UFT_APPLE_ERR_INVALID;
    
    if (ctx->fs_type != UFT_APPLE_FS_DOS33 && ctx->fs_type != UFT_APPLE_FS_DOS32) {
        return UFT_APPLE_ERR_BADTYPE;
    }
    
    uft_dos33_vtoc_t *vtoc = &ctx->vtoc;
    
    /* Start from last allocated track, moving in current direction */
    uint8_t start_track = vtoc->last_track_alloc;
    int8_t direction = vtoc->alloc_direction;
    
    if (direction == 0) direction = -1;  /* Default: allocate toward track 0 */
    
    /* Search for free sector */
    for (int t = 0; t < 35; t++) {
        int current_track = start_track + (t * direction);
        
        /* Wrap around */
        if (current_track < 0) {
            current_track += 35;
            direction = -direction;
        } else if (current_track >= 35) {
            current_track -= 35;
            direction = -direction;
        }
        
        /* Skip catalog track (17) - it's reserved */
        if (current_track == 17) continue;
        
        /* Search sectors on this track */
        for (int s = 0; s < 16; s++) {
            if (is_sector_free(vtoc, current_track, s)) {
                mark_sector_used(vtoc, current_track, s);
                vtoc->last_track_alloc = current_track;
                vtoc->alloc_direction = direction;
                
                *track = current_track;
                *sector = s;
                ctx->is_modified = true;
                return 0;
            }
        }
    }
    
    return UFT_APPLE_ERR_DISKFULL;
}

int uft_apple_free_sector(uft_apple_ctx_t *ctx, uint8_t track, uint8_t sector) {
    if (!ctx) return UFT_APPLE_ERR_INVALID;
    
    if (ctx->fs_type != UFT_APPLE_FS_DOS33 && ctx->fs_type != UFT_APPLE_FS_DOS32) {
        return UFT_APPLE_ERR_BADTYPE;
    }
    
    if (track >= 35 || sector >= 16) return UFT_APPLE_ERR_INVALID;
    
    mark_sector_free(&ctx->vtoc, track, sector);
    ctx->is_modified = true;
    
    return 0;
}

/*===========================================================================
 * Free Space Counting
 *===========================================================================*/

int uft_apple_get_free(const uft_apple_ctx_t *ctx, uint16_t *free_count) {
    if (!ctx || !free_count) return UFT_APPLE_ERR_INVALID;
    
    *free_count = 0;
    
    if (ctx->fs_type == UFT_APPLE_FS_DOS33 || ctx->fs_type == UFT_APPLE_FS_DOS32) {
        for (int t = 0; t < 35; t++) {
            for (int s = 0; s < 16; s++) {
                if (is_sector_free(&ctx->vtoc, t, s)) {
                    (*free_count)++;
                }
            }
        }
    } else if (ctx->fs_type == UFT_APPLE_FS_PRODOS) {
        /* ProDOS bitmap counting - implemented in prodos module */
        return UFT_APPLE_ERR_BADTYPE;  /* Let ProDOS handle it */
    }
    
    return 0;
}

/*===========================================================================
 * Catalog Operations
 *===========================================================================*/

/**
 * @brief Read catalog and build directory listing
 */
int uft_dos33_read_catalog(uft_apple_ctx_t *ctx, uft_apple_dir_t *dir) {
    if (!ctx || !dir) return UFT_APPLE_ERR_INVALID;
    
    if (ctx->fs_type != UFT_APPLE_FS_DOS33 && ctx->fs_type != UFT_APPLE_FS_DOS32) {
        return UFT_APPLE_ERR_BADTYPE;
    }
    
    uft_apple_dir_init(dir);
    
    uint8_t cat_track = ctx->vtoc.catalog_track;
    uint8_t cat_sector = ctx->vtoc.catalog_sector;
    uint8_t sector_data[UFT_APPLE_SECTOR_SIZE];
    
    /* Traverse catalog chain */
    int safety = 50;  /* Prevent infinite loop */
    
    while (cat_track != 0 && safety-- > 0) {
        int ret = uft_apple_read_sector(ctx, cat_track, cat_sector, sector_data);
        if (ret < 0) return ret;
        
        const uft_dos33_catalog_t *catalog = (const uft_dos33_catalog_t *)sector_data;
        
        /* Process entries in this sector */
        for (int i = 0; i < 7; i++) {
            const uft_dos33_entry_t *entry = &catalog->entries[i];
            
            /* Skip deleted entries (track = 0xFF) */
            if (entry->ts_list_track == 0xFF) continue;
            
            /* Skip empty entries (track = 0x00) */
            if (entry->ts_list_track == 0x00) continue;
            
            /* Expand directory if needed */
            if (dir->count >= dir->capacity) {
                size_t new_cap = dir->capacity ? dir->capacity * 2 : 16;
                uft_apple_entry_t *new_entries = realloc(dir->entries, 
                                                         new_cap * sizeof(uft_apple_entry_t));
                if (!new_entries) return UFT_APPLE_ERR_NOMEM;
                dir->entries = new_entries;
                dir->capacity = new_cap;
            }
            
            /* Fill entry */
            uft_apple_entry_t *e = &dir->entries[dir->count++];
            memset(e, 0, sizeof(*e));
            
            extract_dos33_filename(entry, e->name);
            e->file_type = entry->file_type & 0x7F;
            e->is_locked = (entry->file_type & 0x80) != 0;
            e->is_directory = false;
            e->ts_track = entry->ts_list_track;
            e->ts_sector = entry->ts_list_sector;
            e->sector_count = entry->sector_count;
            
            /* Calculate approximate size */
            /* First 2 bytes of B files = load address, next 2 = length */
            e->size = (entry->sector_count > 0) ? 
                      (entry->sector_count - 1) * 256 : 0;
        }
        
        /* Move to next catalog sector */
        cat_track = catalog->next_track;
        cat_sector = catalog->next_sector;
    }
    
    return 0;
}

/**
 * @brief Find entry in catalog by name
 */
int uft_dos33_find_entry(const uft_apple_ctx_t *ctx, const char *name, 
                         uft_apple_entry_t *entry,
                         uint8_t *cat_track, uint8_t *cat_sector, int *cat_index) {
    if (!ctx || !name) return UFT_APPLE_ERR_INVALID;
    
    if (ctx->fs_type != UFT_APPLE_FS_DOS33 && ctx->fs_type != UFT_APPLE_FS_DOS32) {
        return UFT_APPLE_ERR_BADTYPE;
    }
    
    /* Convert search name to uppercase for comparison */
    char search_name[32];
    int len = 0;
    for (int i = 0; name[i] && len < 30; i++) {
        search_name[len++] = toupper(name[i]);
    }
    search_name[len] = '\0';
    
    uint8_t track = ctx->vtoc.catalog_track;
    uint8_t sector = ctx->vtoc.catalog_sector;
    uint8_t sector_data[UFT_APPLE_SECTOR_SIZE];
    
    int safety = 50;
    
    while (track != 0 && safety-- > 0) {
        int ret = uft_apple_read_sector(ctx, track, sector, sector_data);
        if (ret < 0) return ret;
        
        const uft_dos33_catalog_t *catalog = (const uft_dos33_catalog_t *)sector_data;
        
        for (int i = 0; i < 7; i++) {
            const uft_dos33_entry_t *e = &catalog->entries[i];
            
            if (e->ts_list_track == 0xFF || e->ts_list_track == 0x00) continue;
            
            char entry_name[32];
            extract_dos33_filename(e, entry_name);
            
            if (strcmp(entry_name, search_name) == 0) {
                /* Found it */
                if (entry) {
                    memset(entry, 0, sizeof(*entry));
                    strncpy(entry->name, entry_name, 31); entry->name[31] = '\0';
                    entry->file_type = e->file_type & 0x7F;
                    entry->is_locked = (e->file_type & 0x80) != 0;
                    entry->ts_track = e->ts_list_track;
                    entry->ts_sector = e->ts_list_sector;
                    entry->sector_count = e->sector_count;
                }
                if (cat_track) *cat_track = track;
                if (cat_sector) *cat_sector = sector;
                if (cat_index) *cat_index = i;
                
                return 0;
            }
        }
        
        track = catalog->next_track;
        sector = catalog->next_sector;
    }
    
    return UFT_APPLE_ERR_NOTFOUND;
}

/*===========================================================================
 * T/S List Operations
 *===========================================================================*/

/**
 * @brief Read file data by following T/S list
 */
int uft_dos33_read_file_data(const uft_apple_ctx_t *ctx, uint8_t ts_track, uint8_t ts_sector,
                             uint8_t **data_out, size_t *size_out) {
    if (!ctx || !data_out || !size_out) return UFT_APPLE_ERR_INVALID;
    
    *data_out = NULL;
    *size_out = 0;
    
    /* First pass: count sectors */
    uint8_t sector_data[UFT_APPLE_SECTOR_SIZE];
    size_t total_sectors = 0;
    uint8_t t = ts_track;
    uint8_t s = ts_sector;
    int safety = 500;  /* Max sectors in a file */
    
    while (t != 0 && safety-- > 0) {
        int ret = uft_apple_read_sector(ctx, t, s, sector_data);
        if (ret < 0) return ret;
        
        const uft_dos33_tslist_t *tslist = (const uft_dos33_tslist_t *)sector_data;
        
        /* Count valid T/S pairs */
        for (int i = 0; i < 122; i++) {
            if (tslist->pairs[i].track == 0 && tslist->pairs[i].sector == 0) {
                continue;  /* Sparse file hole or end */
            }
            total_sectors++;
        }
        
        t = tslist->next_track;
        s = tslist->next_sector;
    }
    
    if (total_sectors == 0) {
        *data_out = NULL;
        *size_out = 0;
        return 0;
    }
    
    /* Allocate buffer */
    uint8_t *data = uft_safe_malloc_array(total_sectors, UFT_APPLE_SECTOR_SIZE);
    if (!data) return UFT_APPLE_ERR_NOMEM;
    
    /* Second pass: read data */
    t = ts_track;
    s = ts_sector;
    size_t offset = 0;
    safety = 500;
    
    while (t != 0 && safety-- > 0) {
        int ret = uft_apple_read_sector(ctx, t, s, sector_data);
        if (ret < 0) {
            free(data);
            return ret;
        }
        
        const uft_dos33_tslist_t *tslist = (const uft_dos33_tslist_t *)sector_data;
        
        for (int i = 0; i < 122; i++) {
            uint8_t dt = tslist->pairs[i].track;
            uint8_t ds = tslist->pairs[i].sector;
            
            if (dt == 0 && ds == 0) {
                /* Zero-fill for sparse region */
                memset(data + offset, 0, UFT_APPLE_SECTOR_SIZE);
            } else {
                ret = uft_apple_read_sector(ctx, dt, ds, data + offset);
                if (ret < 0) {
                    free(data);
                    return ret;
                }
            }
            offset += UFT_APPLE_SECTOR_SIZE;
            
            if (offset >= total_sectors * UFT_APPLE_SECTOR_SIZE) break;
        }
        
        t = tslist->next_track;
        s = tslist->next_sector;
    }
    
    *data_out = data;
    *size_out = total_sectors * UFT_APPLE_SECTOR_SIZE;
    
    return 0;
}

/**
 * @brief Create T/S list for new file
 */
int uft_dos33_create_ts_list(uft_apple_ctx_t *ctx, const uint8_t *data, size_t size,
                             uint8_t *ts_track, uint8_t *ts_sector,
                             uint16_t *sector_count) {
    if (!ctx || !ts_track || !ts_sector || !sector_count) {
        return UFT_APPLE_ERR_INVALID;
    }
    
    /* Calculate sectors needed */
    size_t data_sectors = (size + UFT_APPLE_SECTOR_SIZE - 1) / UFT_APPLE_SECTOR_SIZE;
    size_t ts_sectors = (data_sectors + 121) / 122;  /* 122 pairs per T/S list */
    
    if (data_sectors == 0) data_sectors = 1;  /* At least one sector */
    if (ts_sectors == 0) ts_sectors = 1;
    
    /* Allocate T/S list sector */
    uint8_t first_ts_track, first_ts_sector;
    int ret = uft_apple_alloc_sector(ctx, &first_ts_track, &first_ts_sector);
    if (ret < 0) return ret;
    
    uint8_t ts_data[UFT_APPLE_SECTOR_SIZE];
    memset(ts_data, 0, sizeof(ts_data));
    uft_dos33_tslist_t *tslist = (uft_dos33_tslist_t *)ts_data;
    
    size_t written_sectors = 0;
    size_t data_offset = 0;
    
    uint8_t cur_ts_track = first_ts_track;
    uint8_t cur_ts_sector = first_ts_sector;
    
    while (written_sectors < data_sectors) {
        int pair_idx = 0;
        
        while (pair_idx < 122 && written_sectors < data_sectors) {
            /* Allocate data sector */
            uint8_t data_track, data_sector;
            ret = uft_apple_alloc_sector(ctx, &data_track, &data_sector);
            if (ret < 0) {
                /* TODO: cleanup already allocated sectors */
                return ret;
            }
            
            /* Write data to sector */
            uint8_t sector_buf[UFT_APPLE_SECTOR_SIZE] = {0};
            size_t to_copy = size - data_offset;
            if (to_copy > UFT_APPLE_SECTOR_SIZE) to_copy = UFT_APPLE_SECTOR_SIZE;
            
            if (data && to_copy > 0) {
                memcpy(sector_buf, data + data_offset, to_copy);
            }
            
            ret = uft_apple_write_sector(ctx, data_track, data_sector, sector_buf);
            if (ret < 0) return ret;
            
            /* Add to T/S list */
            tslist->pairs[pair_idx].track = data_track;
            tslist->pairs[pair_idx].sector = data_sector;
            
            pair_idx++;
            written_sectors++;
            data_offset += UFT_APPLE_SECTOR_SIZE;
        }
        
        /* Need another T/S list sector? */
        if (written_sectors < data_sectors) {
            uint8_t next_ts_track, next_ts_sector;
            ret = uft_apple_alloc_sector(ctx, &next_ts_track, &next_ts_sector);
            if (ret < 0) return ret;
            
            tslist->next_track = next_ts_track;
            tslist->next_sector = next_ts_sector;
        }
        
        /* Write T/S list sector */
        ret = uft_apple_write_sector(ctx, cur_ts_track, cur_ts_sector, ts_data);
        if (ret < 0) return ret;
        
        /* Prepare next T/S list */
        if (written_sectors < data_sectors) {
            cur_ts_track = tslist->next_track;
            cur_ts_sector = tslist->next_sector;
            memset(ts_data, 0, sizeof(ts_data));
        }
    }
    
    *ts_track = first_ts_track;
    *ts_sector = first_ts_sector;
    *sector_count = written_sectors + ts_sectors;
    
    return 0;
}

/**
 * @brief Free all sectors in a T/S list chain
 */
int uft_dos33_free_file_sectors(uft_apple_ctx_t *ctx, uint8_t ts_track, uint8_t ts_sector) {
    if (!ctx) return UFT_APPLE_ERR_INVALID;
    
    uint8_t sector_data[UFT_APPLE_SECTOR_SIZE];
    uint8_t t = ts_track;
    uint8_t s = ts_sector;
    int safety = 500;
    
    while (t != 0 && safety-- > 0) {
        int ret = uft_apple_read_sector(ctx, t, s, sector_data);
        if (ret < 0) return ret;
        
        const uft_dos33_tslist_t *tslist = (const uft_dos33_tslist_t *)sector_data;
        
        /* Free data sectors */
        for (int i = 0; i < 122; i++) {
            if (tslist->pairs[i].track != 0 || tslist->pairs[i].sector != 0) {
                uft_apple_free_sector(ctx, tslist->pairs[i].track, tslist->pairs[i].sector);
            }
        }
        
        /* Remember next T/S list before freeing current */
        uint8_t next_t = tslist->next_track;
        uint8_t next_s = tslist->next_sector;
        
        /* Free T/S list sector */
        uft_apple_free_sector(ctx, t, s);
        
        t = next_t;
        s = next_s;
    }
    
    return 0;
}

/*===========================================================================
 * Catalog Entry Management
 *===========================================================================*/

/**
 * @brief Add entry to catalog
 */
int uft_dos33_add_catalog_entry(uft_apple_ctx_t *ctx, const char *name, uint8_t file_type,
                                uint8_t ts_track, uint8_t ts_sector, uint16_t sector_count) {
    if (!ctx || !name) return UFT_APPLE_ERR_INVALID;
    
    /* Check if name already exists */
    int ret = uft_dos33_find_entry(ctx, name, NULL, NULL, NULL, NULL);
    if (ret == 0) return UFT_APPLE_ERR_EXISTS;
    
    /* Find free catalog slot */
    uint8_t cat_track = ctx->vtoc.catalog_track;
    uint8_t cat_sector = ctx->vtoc.catalog_sector;
    uint8_t sector_data[UFT_APPLE_SECTOR_SIZE];
    
    int safety = 50;
    
    while (cat_track != 0 && safety-- > 0) {
        ret = uft_apple_read_sector(ctx, cat_track, cat_sector, sector_data);
        if (ret < 0) return ret;
        
        uft_dos33_catalog_t *catalog = (uft_dos33_catalog_t *)sector_data;
        
        /* Look for empty or deleted slot */
        for (int i = 0; i < 7; i++) {
            uft_dos33_entry_t *entry = &catalog->entries[i];
            
            if (entry->ts_list_track == 0x00 || entry->ts_list_track == 0xFF) {
                /* Found free slot */
                memset(entry, 0, sizeof(*entry));
                entry->ts_list_track = ts_track;
                entry->ts_list_sector = ts_sector;
                entry->file_type = file_type;
                encode_dos33_filename(name, entry->filename);
                entry->sector_count = sector_count;
                
                ret = uft_apple_write_sector(ctx, cat_track, cat_sector, sector_data);
                return ret;
            }
        }
        
        cat_track = catalog->next_track;
        cat_sector = catalog->next_sector;
    }
    
    return UFT_APPLE_ERR_DISKFULL;  /* No free catalog slots */
}

/**
 * @brief Delete catalog entry
 */
int uft_dos33_delete_catalog_entry(uft_apple_ctx_t *ctx, uint8_t cat_track, 
                                   uint8_t cat_sector, int cat_index) {
    if (!ctx) return UFT_APPLE_ERR_INVALID;
    
    uint8_t sector_data[UFT_APPLE_SECTOR_SIZE];
    
    int ret = uft_apple_read_sector(ctx, cat_track, cat_sector, sector_data);
    if (ret < 0) return ret;
    
    uft_dos33_catalog_t *catalog = (uft_dos33_catalog_t *)sector_data;
    
    /* Mark as deleted */
    catalog->entries[cat_index].ts_list_track = 0xFF;
    
    ret = uft_apple_write_sector(ctx, cat_track, cat_sector, sector_data);
    return ret;
}

/*===========================================================================
 * Directory Operations
 *===========================================================================*/

void uft_apple_dir_init(uft_apple_dir_t *dir) {
    if (!dir) return;
    memset(dir, 0, sizeof(*dir));
}

void uft_apple_dir_free(uft_apple_dir_t *dir) {
    if (!dir) return;
    free(dir->entries);
    dir->entries = NULL;
    dir->count = 0;
    dir->capacity = 0;
}

int uft_apple_read_dir(const uft_apple_ctx_t *ctx, const char *path, uft_apple_dir_t *dir) {
    if (!ctx || !dir) return UFT_APPLE_ERR_INVALID;
    
    (void)path;  /* DOS 3.3 has flat directory */
    
    if (ctx->fs_type == UFT_APPLE_FS_DOS33 || ctx->fs_type == UFT_APPLE_FS_DOS32) {
        return uft_dos33_read_catalog((uft_apple_ctx_t *)ctx, dir);
    }
    
    /* ProDOS handled elsewhere */
    return UFT_APPLE_ERR_BADTYPE;
}

int uft_apple_find(const uft_apple_ctx_t *ctx, const char *path, uft_apple_entry_t *entry) {
    if (!ctx || !path) return UFT_APPLE_ERR_INVALID;
    
    if (ctx->fs_type == UFT_APPLE_FS_DOS33 || ctx->fs_type == UFT_APPLE_FS_DOS32) {
        return uft_dos33_find_entry(ctx, path, entry, NULL, NULL, NULL);
    }
    
    return UFT_APPLE_ERR_BADTYPE;
}

int uft_apple_foreach(const uft_apple_ctx_t *ctx, const char *path,
                      uft_apple_dir_callback_t callback, void *user_data) {
    if (!ctx || !callback) return UFT_APPLE_ERR_INVALID;
    
    uft_apple_dir_t dir;
    uft_apple_dir_init(&dir);
    
    int ret = uft_apple_read_dir(ctx, path, &dir);
    if (ret < 0) {
        uft_apple_dir_free(&dir);
        return ret;
    }
    
    for (size_t i = 0; i < dir.count; i++) {
        ret = callback(&dir.entries[i], user_data);
        if (ret != 0) break;
    }
    
    uft_apple_dir_free(&dir);
    return 0;
}

/*===========================================================================
 * Print Directory
 *===========================================================================*/

void uft_apple_print_dir(const uft_apple_ctx_t *ctx, const char *path, FILE *fp) {
    if (!ctx || !fp) return;
    
    uft_apple_dir_t dir;
    uft_apple_dir_init(&dir);
    
    if (uft_apple_read_dir(ctx, path, &dir) < 0) {
        fprintf(fp, "Error reading directory\n");
        return;
    }
    
    char vol_name[64];
    uft_apple_get_volume_name(ctx, vol_name, sizeof(vol_name));
    
    if (ctx->fs_type == UFT_APPLE_FS_DOS33 || ctx->fs_type == UFT_APPLE_FS_DOS32) {
        fprintf(fp, "\n%s\n\n", vol_name);
        
        for (size_t i = 0; i < dir.count; i++) {
            const uft_apple_entry_t *e = &dir.entries[i];
            
            fprintf(fp, "%c%c %-30s  %3d\n",
                    e->is_locked ? '*' : ' ',
                    uft_dos33_type_char(e->file_type),
                    e->name,
                    e->sector_count);
        }
        
        uint16_t free_sectors;
        uft_apple_get_free(ctx, &free_sectors);
        fprintf(fp, "\nFREE SECTORS: %d\n", free_sectors);
    }
    
    uft_apple_dir_free(&dir);
}
