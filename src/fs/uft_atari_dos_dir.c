/**
 * @file uft_atari_dos_dir.c
 * @brief Atari DOS directory operations
 * 
 * Directory parsing, listing, file lookup, iteration
 * 
 * @version 1.0.0
 * @date 2026-01-05
 */

#include "uft/fs/uft_atari_dos.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/*===========================================================================
 * Internal Declarations (from core)
 *===========================================================================*/

extern uft_atari_error_t uft_atari_read_sector(uft_atari_ctx_t *ctx,
                                                uint16_t sector,
                                                uint8_t *buffer);
extern uft_atari_error_t uft_atari_write_sector(uft_atari_ctx_t *ctx,
                                                 uint16_t sector,
                                                 const uint8_t *buffer);
extern uint16_t uft_atari_get_sector_size(uft_atari_ctx_t *ctx, uint16_t sector);
extern uft_atari_error_t uft_atari_load_vtoc(uft_atari_ctx_t *ctx);

/*===========================================================================
 * Directory Entry Parsing
 *===========================================================================*/

/**
 * @brief Parse raw directory entry into unified structure
 */
static void parse_entry(const uft_atari_dir_entry_raw_t *raw,
                        uft_atari_entry_t *entry,
                        uint8_t index)
{
    memset(entry, 0, sizeof(*entry));
    
    entry->flags = raw->flags;
    entry->dir_index = index;
    
    /* Parse flags */
    entry->in_use = (raw->flags & UFT_ATARI_FLAG_INUSE) != 0;
    entry->deleted = (raw->flags & UFT_ATARI_FLAG_DELETED) != 0;
    entry->locked = (raw->flags & UFT_ATARI_FLAG_LOCKED) != 0;
    entry->open_for_write = (raw->flags & UFT_ATARI_FLAG_OPEN) != 0;
    
    /* Copy and null-terminate filename */
    memcpy(entry->filename, raw->filename, 8);
    entry->filename[8] = '\0';
    
    /* Trim trailing spaces */
    for (int i = 7; i >= 0 && entry->filename[i] == ' '; i--) {
        entry->filename[i] = '\0';
    }
    
    /* Copy and null-terminate extension */
    memcpy(entry->extension, raw->extension, 3);
    entry->extension[3] = '\0';
    
    /* Trim trailing spaces */
    for (int i = 2; i >= 0 && entry->extension[i] == ' '; i--) {
        entry->extension[i] = '\0';
    }
    
    /* Build full name */
    if (entry->extension[0]) {
        snprintf(entry->full_name, sizeof(entry->full_name), "%s.%s",
                 entry->filename, entry->extension);
    } else {
        snprintf(entry->full_name, sizeof(entry->full_name), "%s",
                 entry->filename);
    }
    
    /* Little-endian sector count and start */
    entry->sector_count = raw->sector_count;
    entry->start_sector = raw->start_sector;
    
    /* File size will be calculated from sector chain */
    entry->file_size = 0;
}

/*===========================================================================
 * Directory Reading
 *===========================================================================*/

uft_atari_error_t uft_atari_read_directory(uft_atari_ctx_t *ctx,
                                            uft_atari_dir_t *dir)
{
    if (!ctx || !dir) {
        return UFT_ATARI_ERR_PARAM;
    }
    
    memset(dir, 0, sizeof(*dir));
    
    /* Read each directory sector (361-368) */
    uint8_t sector_buf[256];
    uint8_t entry_idx = 0;
    
    for (uint8_t sec = 0; sec < UFT_ATARI_DIR_SECTORS; sec++) {
        uint16_t sector_num = UFT_ATARI_DIR_START + sec;
        
        uft_atari_error_t err = uft_atari_read_sector(ctx, sector_num, sector_buf);
        if (err != UFT_ATARI_OK) {
            return err;
        }
        
        uint16_t sec_size = uft_atari_get_sector_size(ctx, sector_num);
        
        /* Each entry is 16 bytes, 8 entries per sector for SD/DD */
        int entries_per_sector = sec_size / UFT_ATARI_ENTRY_SIZE;
        
        for (int e = 0; e < entries_per_sector && entry_idx < UFT_ATARI_MAX_FILES; e++) {
            const uft_atari_dir_entry_raw_t *raw = 
                (const uft_atari_dir_entry_raw_t *)(sector_buf + e * UFT_ATARI_ENTRY_SIZE);
            
            /* Check if entry is in use or deleted */
            if (raw->flags == 0) {
                /* Empty entry - end of directory */
                goto done;
            }
            
            parse_entry(raw, &dir->files[entry_idx], entry_idx);
            
            if (dir->files[entry_idx].in_use && !dir->files[entry_idx].deleted) {
                dir->file_count++;
            } else if (dir->files[entry_idx].deleted) {
                dir->deleted_count++;
            }
            
            entry_idx++;
        }
    }
    
done:
    /* Get free space from VTOC */
    uint16_t free_secs;
    uint32_t free_bytes;
    uft_atari_get_free_space(ctx, &free_secs, &free_bytes);
    dir->free_sectors = free_secs;
    dir->free_bytes = free_bytes;
    
    /* Get total sectors from geometry */
    uft_atari_geometry_t geom;
    if (uft_atari_get_geometry(ctx, &geom) == UFT_ATARI_OK) {
        dir->total_sectors = geom.total_sectors;
    }
    
    return UFT_ATARI_OK;
}

/*===========================================================================
 * File Lookup
 *===========================================================================*/

uft_atari_error_t uft_atari_find_file(uft_atari_ctx_t *ctx,
                                       const char *filename,
                                       uft_atari_entry_t *entry)
{
    if (!ctx || !filename || !entry) {
        return UFT_ATARI_ERR_PARAM;
    }
    
    /* Parse requested filename */
    char search_name[9], search_ext[4];
    uft_atari_error_t err = uft_atari_parse_filename(filename, search_name, search_ext);
    if (err != UFT_ATARI_OK) {
        return err;
    }
    
    /* Read directory */
    uft_atari_dir_t dir;
    err = uft_atari_read_directory(ctx, &dir);
    if (err != UFT_ATARI_OK) {
        return err;
    }
    
    /* Search for file */
    for (size_t i = 0; i < UFT_ATARI_MAX_FILES; i++) {
        if (!dir.files[i].in_use || dir.files[i].deleted) {
            continue;
        }
        
        /* Compare filename (case-insensitive) */
        bool match = true;
        for (int j = 0; j < 8; j++) {
            char c1 = (char)toupper((unsigned char)search_name[j]);
            char c2 = (char)toupper((unsigned char)dir.files[i].filename[j]);
            /* Handle null vs space */
            if (c1 == '\0') c1 = ' ';
            if (c2 == '\0') c2 = ' ';
            if (c1 != c2) {
                match = false;
                break;
            }
        }
        
        if (!match) continue;
        
        /* Compare extension */
        for (int j = 0; j < 3; j++) {
            char c1 = (char)toupper((unsigned char)search_ext[j]);
            char c2 = (char)toupper((unsigned char)dir.files[i].extension[j]);
            if (c1 == '\0') c1 = ' ';
            if (c2 == '\0') c2 = ' ';
            if (c1 != c2) {
                match = false;
                break;
            }
        }
        
        if (match) {
            *entry = dir.files[i];
            return UFT_ATARI_OK;
        }
    }
    
    return UFT_ATARI_ERR_NOTFOUND;
}

/*===========================================================================
 * Directory Iteration
 *===========================================================================*/

uft_atari_error_t uft_atari_foreach_file(uft_atari_ctx_t *ctx,
                                          uft_atari_file_cb callback,
                                          void *user_data)
{
    if (!ctx || !callback) {
        return UFT_ATARI_ERR_PARAM;
    }
    
    uft_atari_dir_t dir;
    uft_atari_error_t err = uft_atari_read_directory(ctx, &dir);
    if (err != UFT_ATARI_OK) {
        return err;
    }
    
    for (size_t i = 0; i < UFT_ATARI_MAX_FILES; i++) {
        if (!dir.files[i].in_use || dir.files[i].deleted) {
            continue;
        }
        
        bool cont = callback(&dir.files[i], user_data);
        if (!cont) {
            break;
        }
    }
    
    return UFT_ATARI_OK;
}

/*===========================================================================
 * Deleted File Listing
 *===========================================================================*/

uft_atari_error_t uft_atari_list_deleted(uft_atari_ctx_t *ctx,
                                          uft_atari_entry_t **entries,
                                          size_t *count)
{
    if (!ctx || !entries || !count) {
        return UFT_ATARI_ERR_PARAM;
    }
    
    *entries = NULL;
    *count = 0;
    
    uft_atari_dir_t dir;
    uft_atari_error_t err = uft_atari_read_directory(ctx, &dir);
    if (err != UFT_ATARI_OK) {
        return err;
    }
    
    if (dir.deleted_count == 0) {
        return UFT_ATARI_OK;
    }
    
    /* Allocate array for deleted entries */
    *entries = malloc(dir.deleted_count * sizeof(uft_atari_entry_t));
    if (!*entries) {
        return UFT_ATARI_ERR_MEMORY;
    }
    
    size_t idx = 0;
    for (size_t i = 0; i < UFT_ATARI_MAX_FILES && idx < dir.deleted_count; i++) {
        if (dir.files[i].deleted) {
            (*entries)[idx++] = dir.files[i];
        }
    }
    
    *count = idx;
    return UFT_ATARI_OK;
}

/*===========================================================================
 * Directory Entry Update (internal helper)
 *===========================================================================*/

/**
 * @brief Update directory entry at given index
 */
static uft_atari_error_t update_dir_entry(uft_atari_ctx_t *ctx,
                                           uint8_t index,
                                           const uft_atari_dir_entry_raw_t *entry)
{
    if (index >= UFT_ATARI_MAX_FILES) {
        return UFT_ATARI_ERR_PARAM;
    }
    
    /* Calculate sector and offset */
    uint16_t sec_size = uft_atari_get_sector_size(ctx, UFT_ATARI_DIR_START);
    int entries_per_sector = sec_size / UFT_ATARI_ENTRY_SIZE;
    
    uint8_t sector_idx = index / entries_per_sector;
    uint8_t entry_offset = (index % entries_per_sector) * UFT_ATARI_ENTRY_SIZE;
    
    uint16_t sector_num = UFT_ATARI_DIR_START + sector_idx;
    
    /* Read sector */
    uint8_t sector_buf[256];
    uft_atari_error_t err = uft_atari_read_sector(ctx, sector_num, sector_buf);
    if (err != UFT_ATARI_OK) {
        return err;
    }
    
    /* Update entry */
    memcpy(sector_buf + entry_offset, entry, UFT_ATARI_ENTRY_SIZE);
    
    /* Write back */
    return uft_atari_write_sector(ctx, sector_num, sector_buf);
}

/**
 * @brief Find first free directory entry
 */
static int find_free_dir_entry(uft_atari_ctx_t *ctx)
{
    uint8_t sector_buf[256];
    int entry_idx = 0;
    
    for (uint8_t sec = 0; sec < UFT_ATARI_DIR_SECTORS; sec++) {
        uint16_t sector_num = UFT_ATARI_DIR_START + sec;
        
        if (uft_atari_read_sector(ctx, sector_num, sector_buf) != UFT_ATARI_OK) {
            return -1;
        }
        
        uint16_t sec_size = uft_atari_get_sector_size(ctx, sector_num);
        int entries_per_sector = sec_size / UFT_ATARI_ENTRY_SIZE;
        
        for (int e = 0; e < entries_per_sector && entry_idx < UFT_ATARI_MAX_FILES; e++) {
            const uft_atari_dir_entry_raw_t *raw = 
                (const uft_atari_dir_entry_raw_t *)(sector_buf + e * UFT_ATARI_ENTRY_SIZE);
            
            /* Empty entry (flags == 0) or deleted entry */
            if (raw->flags == 0 || (raw->flags & UFT_ATARI_FLAG_DELETED)) {
                return entry_idx;
            }
            
            entry_idx++;
        }
    }
    
    return -1; /* Directory full */
}

/*===========================================================================
 * Directory Modification (used by file operations)
 *===========================================================================*/

uft_atari_error_t uft_atari_add_dir_entry(uft_atari_ctx_t *ctx,
                                           const char *filename,
                                           uint16_t start_sector,
                                           uint16_t sector_count,
                                           uint8_t *out_index)
{
    if (!ctx || !filename) {
        return UFT_ATARI_ERR_PARAM;
    }
    
    /* Check if file already exists */
    uft_atari_entry_t existing;
    if (uft_atari_find_file(ctx, filename, &existing) == UFT_ATARI_OK) {
        return UFT_ATARI_ERR_EXISTS;
    }
    
    /* Find free directory entry */
    int index = find_free_dir_entry(ctx);
    if (index < 0) {
        return UFT_ATARI_ERR_DIRFULL;
    }
    
    /* Parse filename */
    char name[9], ext[4];
    uft_atari_error_t err = uft_atari_parse_filename(filename, name, ext);
    if (err != UFT_ATARI_OK) {
        return err;
    }
    
    /* Build directory entry */
    uft_atari_dir_entry_raw_t raw;
    memset(&raw, 0, sizeof(raw));
    
    raw.flags = UFT_ATARI_FLAG_INUSE | UFT_ATARI_FLAG_DOS2;
    raw.sector_count = sector_count;
    raw.start_sector = start_sector;
    memcpy(raw.filename, name, 8);
    memcpy(raw.extension, ext, 3);
    
    /* Write entry */
    err = update_dir_entry(ctx, (uint8_t)index, &raw);
    if (err != UFT_ATARI_OK) {
        return err;
    }
    
    if (out_index) {
        *out_index = (uint8_t)index;
    }
    
    return UFT_ATARI_OK;
}

uft_atari_error_t uft_atari_remove_dir_entry(uft_atari_ctx_t *ctx,
                                              uint8_t index)
{
    if (!ctx || index >= UFT_ATARI_MAX_FILES) {
        return UFT_ATARI_ERR_PARAM;
    }
    
    /* Calculate sector and offset */
    uint16_t sec_size = uft_atari_get_sector_size(ctx, UFT_ATARI_DIR_START);
    int entries_per_sector = sec_size / UFT_ATARI_ENTRY_SIZE;
    
    uint8_t sector_idx = index / entries_per_sector;
    uint8_t entry_offset = (index % entries_per_sector) * UFT_ATARI_ENTRY_SIZE;
    
    uint16_t sector_num = UFT_ATARI_DIR_START + sector_idx;
    
    /* Read sector */
    uint8_t sector_buf[256];
    uft_atari_error_t err = uft_atari_read_sector(ctx, sector_num, sector_buf);
    if (err != UFT_ATARI_OK) {
        return err;
    }
    
    /* Mark entry as deleted (set deleted flag, preserve rest) */
    uft_atari_dir_entry_raw_t *raw = 
        (uft_atari_dir_entry_raw_t *)(sector_buf + entry_offset);
    raw->flags = UFT_ATARI_FLAG_DELETED;
    
    /* Write back */
    return uft_atari_write_sector(ctx, sector_num, sector_buf);
}

uft_atari_error_t uft_atari_update_dir_entry_flags(uft_atari_ctx_t *ctx,
                                                    uint8_t index,
                                                    uint8_t flags)
{
    if (!ctx || index >= UFT_ATARI_MAX_FILES) {
        return UFT_ATARI_ERR_PARAM;
    }
    
    /* Calculate sector and offset */
    uint16_t sec_size = uft_atari_get_sector_size(ctx, UFT_ATARI_DIR_START);
    int entries_per_sector = sec_size / UFT_ATARI_ENTRY_SIZE;
    
    uint8_t sector_idx = index / entries_per_sector;
    uint8_t entry_offset = (index % entries_per_sector) * UFT_ATARI_ENTRY_SIZE;
    
    uint16_t sector_num = UFT_ATARI_DIR_START + sector_idx;
    
    /* Read sector */
    uint8_t sector_buf[256];
    uft_atari_error_t err = uft_atari_read_sector(ctx, sector_num, sector_buf);
    if (err != UFT_ATARI_OK) {
        return err;
    }
    
    /* Update flags */
    sector_buf[entry_offset] = flags;
    
    /* Write back */
    return uft_atari_write_sector(ctx, sector_num, sector_buf);
}

/*===========================================================================
 * Directory Display
 *===========================================================================*/

void uft_atari_print_directory(uft_atari_ctx_t *ctx, FILE *output)
{
    if (!ctx) return;
    if (!output) output = stdout;
    
    uft_atari_dir_t dir;
    if (uft_atari_read_directory(ctx, &dir) != UFT_ATARI_OK) {
        fprintf(output, "Error reading directory\n");
        return;
    }
    
    fprintf(output, "\n");
    fprintf(output, "  Name       Ext  Lock  Start  Count    Size\n");
    fprintf(output, "  --------   ---  ----  -----  -----  ------\n");
    
    for (size_t i = 0; i < UFT_ATARI_MAX_FILES; i++) {
        if (!dir.files[i].in_use || dir.files[i].deleted) {
            continue;
        }
        
        uft_atari_entry_t *f = &dir.files[i];
        
        fprintf(output, "  %-8s   %-3s   %c    %5u  %5u  %6u\n",
                f->filename,
                f->extension,
                f->locked ? '*' : ' ',
                f->start_sector,
                f->sector_count,
                f->sector_count * (uft_atari_get_density(ctx) >= UFT_ATARI_DENSITY_DD ? 253 : 125));
    }
    
    fprintf(output, "\n");
    fprintf(output, "  %zu file(s), %u sectors free (%u bytes)\n",
            dir.file_count, dir.free_sectors, dir.free_bytes);
}

void uft_atari_print_info(uft_atari_ctx_t *ctx, FILE *output)
{
    if (!ctx) return;
    if (!output) output = stdout;
    
    uft_atari_geometry_t geom;
    if (uft_atari_get_geometry(ctx, &geom) != UFT_ATARI_OK) {
        fprintf(output, "Error getting disk info\n");
        return;
    }
    
    fprintf(output, "\nAtari Disk Information:\n");
    fprintf(output, "  DOS Type:    %s\n", uft_atari_dos_name(uft_atari_get_dos_type(ctx)));
    fprintf(output, "  Density:     %s\n", uft_atari_density_name(geom.density));
    fprintf(output, "  Tracks:      %u\n", geom.tracks);
    fprintf(output, "  Sectors/Trk: %u\n", geom.sectors_per_track);
    fprintf(output, "  Sector Size: %u bytes\n", geom.sector_size);
    fprintf(output, "  Total:       %u sectors (%u bytes)\n", 
            geom.total_sectors, geom.total_bytes);
    
    uint16_t free_secs;
    uint32_t free_bytes;
    uft_atari_get_free_space(ctx, &free_secs, &free_bytes);
    fprintf(output, "  Free:        %u sectors (%u bytes)\n", free_secs, free_bytes);
}

/*===========================================================================
 * JSON Export
 *===========================================================================*/

int uft_atari_directory_to_json(uft_atari_ctx_t *ctx,
                                char *buffer,
                                size_t buffer_size)
{
    if (!ctx || !buffer || buffer_size < 64) {
        return -1;
    }
    
    uft_atari_dir_t dir;
    if (uft_atari_read_directory(ctx, &dir) != UFT_ATARI_OK) {
        return -1;
    }
    
    uft_atari_geometry_t geom;
    uft_atari_get_geometry(ctx, &geom);
    
    int pos = 0;
    pos += snprintf(buffer + pos, buffer_size - pos,
                    "{\n  \"dos_type\": \"%s\",\n",
                    uft_atari_dos_name(uft_atari_get_dos_type(ctx)));
    pos += snprintf(buffer + pos, buffer_size - pos,
                    "  \"density\": \"%s\",\n",
                    uft_atari_density_name(geom.density));
    pos += snprintf(buffer + pos, buffer_size - pos,
                    "  \"total_sectors\": %u,\n", geom.total_sectors);
    pos += snprintf(buffer + pos, buffer_size - pos,
                    "  \"free_sectors\": %u,\n", dir.free_sectors);
    pos += snprintf(buffer + pos, buffer_size - pos,
                    "  \"file_count\": %zu,\n", dir.file_count);
    pos += snprintf(buffer + pos, buffer_size - pos,
                    "  \"files\": [\n");
    
    bool first = true;
    for (size_t i = 0; i < UFT_ATARI_MAX_FILES; i++) {
        if (!dir.files[i].in_use || dir.files[i].deleted) {
            continue;
        }
        
        uft_atari_entry_t *f = &dir.files[i];
        
        if (!first) {
            pos += snprintf(buffer + pos, buffer_size - pos, ",\n");
        }
        first = false;
        
        pos += snprintf(buffer + pos, buffer_size - pos,
                        "    {\"name\": \"%s\", \"ext\": \"%s\", \"locked\": %s, "
                        "\"start\": %u, \"sectors\": %u}",
                        f->filename, f->extension,
                        f->locked ? "true" : "false",
                        f->start_sector, f->sector_count);
        
        if (pos >= (int)buffer_size - 64) {
            break;
        }
    }
    
    pos += snprintf(buffer + pos, buffer_size - pos, "\n  ]\n}\n");
    
    return pos;
}
