/**
 * @file uft_trsdos_dir.c
 * @brief TRSDOS/LDOS/NewDOS Filesystem - Directory and File Operations
 * 
 * Directory parsing, file extraction, injection, delete, rename
 */

#include "uft/fs/uft_trsdos.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/*===========================================================================
 * Directory Parsing - TRSDOS 2.3
 *===========================================================================*/

/**
 * @brief Parse TRSDOS 2.3 directory entry
 */
static bool parse_trsdos23_entry(const uint8_t *raw, uft_trsdos_entry_t *entry) {
    const uft_trsdos23_dir_entry_t *de = (const uft_trsdos23_dir_entry_t *)raw;
    
    /* Check if entry is valid */
    if (de->attr == 0x00 || de->attr == 0xFF) {
        return false; /* Empty or end marker */
    }
    
    memset(entry, 0, sizeof(*entry));
    entry->version = UFT_TRSDOS_VERSION_23;
    
    /* Copy filename (bytes 8-15) */
    memcpy(entry->name, &raw[8], 8);
    entry->name[8] = '\0';
    
    /* Copy extension (bytes 16-18) */
    memcpy(entry->ext, &raw[16], 3);
    entry->ext[3] = '\0';
    
    /* Parse attributes */
    uint8_t attr = de->attr;
    entry->attrib.visibility = (attr >> 6) & 0x03;
    entry->attrib.protection = (attr >> 3) & 0x07;
    entry->attrib.is_system = (attr & 0x04) != 0;
    
    /* Password check */
    entry->attrib.has_password = (de->password[0] != 0 || de->password[1] != 0);
    
    /* EOF and LRL */
    entry->lrl = de->lrl;
    
    /* Date (ASCII digits) */
    if (de->month >= '0' && de->month <= '9' &&
        de->day >= '0' && de->day <= '9' &&
        de->year >= '0' && de->year <= '9') {
        entry->has_date = true;
        entry->date.month = de->month - '0';
        entry->date.day = de->day - '0';
        entry->date.year = 70 + (de->year - '0'); /* 1970s */
    }
    
    /* Parse extents (bytes 24-47) */
    entry->extent_count = 0;
    uint32_t total_granules = 0;
    
    for (int i = 0; i < 4; i++) {
        uint8_t start = de->extents[i].start_granule;
        uint8_t count = de->extents[i].num_granules;
        
        if (count == 0) break;
        
        entry->extents[entry->extent_count].start_granule = start;
        entry->extents[entry->extent_count].num_granules = count;
        entry->extent_count++;
        total_granules += count;
    }
    
    entry->granules = (uint8_t)total_granules;
    
    /* Calculate size: granules * sectors_per_granule * 256 - (256 - eof_offset) */
    uint32_t full_sectors = total_granules * 5; /* 5 sectors per granule */
    if (de->eof_offset > 0) {
        entry->size = (full_sectors - 1) * 256 + de->eof_offset;
    } else {
        entry->size = full_sectors * 256;
    }
    
    entry->sectors = (uint16_t)full_sectors;
    
    return true;
}

/*===========================================================================
 * Directory Parsing - TRSDOS 6 / LDOS
 *===========================================================================*/

/**
 * @brief Parse TRSDOS 6/LDOS directory entry
 */
static bool parse_trsdos6_entry(const uint8_t *raw, uft_trsdos_entry_t *entry) {
    const uft_trsdos6_dir_entry_t *de = (const uft_trsdos6_dir_entry_t *)raw;
    
    /* Check attribute byte */
    if (de->attr == 0x00 || de->attr == 0xFF) {
        return false; /* Deleted or end */
    }
    
    memset(entry, 0, sizeof(*entry));
    entry->version = UFT_TRSDOS_VERSION_6;
    
    /* Copy filename and extension */
    memcpy(entry->name, de->name, 8);
    entry->name[8] = '\0';
    memcpy(entry->ext, de->ext, 3);
    entry->ext[3] = '\0';
    
    /* Parse attributes */
    uint8_t attr = de->attr;
    entry->attrib.visibility = (attr >> 6) & 0x03;
    entry->attrib.protection = (attr >> 3) & 0x07;
    entry->attrib.is_system = (attr & 0x04) != 0;
    
    /* Password check */
    entry->attrib.has_password = (de->update_password[0] != 0 || 
                                   de->update_password[1] != 0 ||
                                   de->access_password[0] != 0 ||
                                   de->access_password[1] != 0);
    
    /* LRL */
    entry->lrl = de->lrl;
    
    /* Date */
    if (de->date.year > 0 || de->date.month > 0 || de->date.day > 0) {
        entry->has_date = true;
        entry->date.year = de->date.year;
        entry->date.month = de->date.month;
        entry->date.day = de->date.day;
    }
    
    /* EOF position */
    uint16_t eof = de->eof;
    
    /* Parse extent data elements (FXDE) */
    /* FXDE format: first byte is starting granule, rest is extent info */
    entry->extent_count = de->fde_cnt;
    if (entry->extent_count > 16) entry->extent_count = 16;
    
    /* For now, simplified: use first FXDE byte as granule count estimate */
    uint8_t gran_count = de->fxde[0];
    if (gran_count == 0) gran_count = 1;
    
    entry->granules = gran_count;
    
    /* Calculate size */
    entry->size = gran_count * 6 * 256; /* 6 sectors per granule */
    if (eof > 0 && eof < 256) {
        entry->size -= (256 - eof);
    }
    
    entry->sectors = gran_count * 6;
    
    return true;
}

/*===========================================================================
 * Directory Parsing - RS-DOS (CoCo)
 *===========================================================================*/

/**
 * @brief Parse RS-DOS directory entry
 */
static bool parse_rsdos_entry(const uint8_t *raw, uft_trsdos_entry_t *entry) {
    const uft_rsdos_dir_entry_t *de = (const uft_rsdos_dir_entry_t *)raw;
    
    /* Check if entry is valid */
    if (de->name[0] == 0x00 || de->name[0] == 0xFF) {
        return false;
    }
    
    memset(entry, 0, sizeof(*entry));
    entry->version = UFT_TRSDOS_VERSION_RSDOS;
    
    /* Copy filename and extension */
    memcpy(entry->name, de->name, 8);
    entry->name[8] = '\0';
    memcpy(entry->ext, de->ext, 3);
    entry->ext[3] = '\0';
    
    /* Attributes - RS-DOS has simple attributes via file type */
    entry->attrib.visibility = UFT_TRSDOS_ATTR_VISIBLE;
    entry->attrib.protection = UFT_TRSDOS_PROT_FULL;
    
    /* First granule - this starts the chain in FAT */
    entry->extents[0].start_granule = de->first_granule;
    entry->extents[0].num_granules = 1; /* Follow chain to count */
    entry->extent_count = 1;
    
    /* Last sector bytes */
    uint16_t last_bytes = de->last_sector_bytes;
    
    /* Size will be calculated when following chain */
    /* For now, estimate based on first granule */
    entry->granules = 1;
    entry->size = 9 * 256; /* One granule = 9 sectors */
    if (last_bytes > 0 && last_bytes <= 256) {
        entry->size = (9 - 1) * 256 + last_bytes;
    }
    
    return true;
}

/*===========================================================================
 * Directory Read
 *===========================================================================*/

/**
 * @brief Read RS-DOS directory
 */
static uft_trsdos_err_t read_dir_rsdos(uft_trsdos_ctx_t *ctx, uft_trsdos_dir_t *dir) {
    uint8_t sector[256];
    
    /* Directory: Track 17, Sectors 3-11 (8 entries per sector, 72 max) */
    int file_count = 0;
    
    for (int sec = 2; sec < 11; sec++) { /* Sectors 3-11 in 0-based = 2-10 */
        uft_trsdos_err_t err = uft_trsdos_read_sector(ctx, 17, 0, sec, sector, sizeof(sector));
        if (err != UFT_TRSDOS_OK) continue;
        
        for (int i = 0; i < 8; i++) {
            const uint8_t *raw = sector + i * 32;
            
            uft_trsdos_entry_t entry;
            if (!parse_rsdos_entry(raw, &entry)) continue;
            
            /* Ensure capacity */
            if (dir->count >= dir->capacity) {
                size_t new_cap = dir->capacity ? dir->capacity * 2 : 32;
                uft_trsdos_entry_t *new_entries = realloc(dir->entries, 
                    new_cap * sizeof(uft_trsdos_entry_t));
                if (!new_entries) return UFT_TRSDOS_ERR_NOMEM;
                dir->entries = new_entries;
                dir->capacity = new_cap;
            }
            
            entry.dir_entry_index = sec * 8 + i;
            
            /* Follow FAT chain to get actual size */
            const uint8_t *fat_sector;
            uint8_t fat[256];
            uft_trsdos_read_sector(ctx, 17, 0, 1, fat, sizeof(fat));
            
            uint8_t granule = entry.extents[0].start_granule;
            uint32_t total_granules = 0;
            uint32_t total_bytes = 0;
            
            while (granule < 68 && total_granules < 68) {
                total_granules++;
                uint8_t next = fat[granule];
                
                if (next >= 0xC0 && next <= 0xC9) {
                    /* Last granule - upper nibble is sector count */
                    uint8_t last_sectors = next - 0xC0;
                    total_bytes += last_sectors * 256;
                    
                    /* Get last sector bytes from directory entry */
                    const uft_rsdos_dir_entry_t *de = (const uft_rsdos_dir_entry_t *)raw;
                    uint16_t last_bytes = de->last_sector_bytes;
                    if (last_bytes > 0) {
                        total_bytes = (total_granules - 1) * 9 * 256 + 
                                      (last_sectors - 1) * 256 + last_bytes;
                    }
                    break;
                } else if (next == 0xFF) {
                    /* Free (shouldn't happen in valid chain) */
                    break;
                } else {
                    /* Continue chain */
                    total_bytes += 9 * 256;
                    granule = next;
                }
            }
            
            entry.granules = (uint8_t)total_granules;
            entry.size = total_bytes;
            entry.sectors = total_granules * 9;
            
            dir->entries[dir->count++] = entry;
            dir->total_files++;
            dir->total_size += entry.size;
            file_count++;
        }
    }
    
    dir->free_granules = ctx->gat.free_granules;
    dir->free_size = uft_trsdos_free_space(ctx);
    
    return UFT_TRSDOS_OK;
}

/**
 * @brief Read TRSDOS 2.3 directory
 */
static uft_trsdos_err_t read_dir_trsdos23(uft_trsdos_ctx_t *ctx, uft_trsdos_dir_t *dir) {
    uint8_t sector[256];
    
    /* Directory: Track 17, Sectors 1-9 (48-byte entries) */
    for (int sec = 1; sec < 10; sec++) {
        uft_trsdos_err_t err = uft_trsdos_read_sector(ctx, 17, 0, sec, sector, sizeof(sector));
        if (err != UFT_TRSDOS_OK) continue;
        
        /* ~5 entries per sector (48 bytes each) */
        for (int i = 0; i < 5; i++) {
            size_t offset = i * 48;
            if (offset + 48 > 256) break;
            
            const uint8_t *raw = sector + offset;
            
            uft_trsdos_entry_t entry;
            if (!parse_trsdos23_entry(raw, &entry)) continue;
            
            /* Ensure capacity */
            if (dir->count >= dir->capacity) {
                size_t new_cap = dir->capacity ? dir->capacity * 2 : 32;
                uft_trsdos_entry_t *new_entries = realloc(dir->entries,
                    new_cap * sizeof(uft_trsdos_entry_t));
                if (!new_entries) return UFT_TRSDOS_ERR_NOMEM;
                dir->entries = new_entries;
                dir->capacity = new_cap;
            }
            
            entry.dir_entry_index = (sec - 1) * 5 + i;
            dir->entries[dir->count++] = entry;
            dir->total_files++;
            dir->total_size += entry.size;
        }
    }
    
    dir->free_granules = ctx->gat.free_granules;
    dir->free_size = uft_trsdos_free_space(ctx);
    
    return UFT_TRSDOS_OK;
}

/**
 * @brief Read TRSDOS 6/LDOS directory
 */
static uft_trsdos_err_t read_dir_trsdos6(uft_trsdos_ctx_t *ctx, uft_trsdos_dir_t *dir) {
    uint8_t sector[256];
    
    /* Directory: Track 17, Sectors 1-8 (32-byte entries) */
    for (int sec = 1; sec < 9; sec++) {
        uft_trsdos_err_t err = uft_trsdos_read_sector(ctx, 17, 0, sec, sector, sizeof(sector));
        if (err != UFT_TRSDOS_OK) continue;
        
        for (int i = 0; i < 8; i++) {
            const uint8_t *raw = sector + i * 32;
            
            uft_trsdos_entry_t entry;
            if (!parse_trsdos6_entry(raw, &entry)) continue;
            
            /* Ensure capacity */
            if (dir->count >= dir->capacity) {
                size_t new_cap = dir->capacity ? dir->capacity * 2 : 32;
                uft_trsdos_entry_t *new_entries = realloc(dir->entries,
                    new_cap * sizeof(uft_trsdos_entry_t));
                if (!new_entries) return UFT_TRSDOS_ERR_NOMEM;
                dir->entries = new_entries;
                dir->capacity = new_cap;
            }
            
            entry.dir_entry_index = (sec - 1) * 8 + i;
            dir->entries[dir->count++] = entry;
            dir->total_files++;
            dir->total_size += entry.size;
        }
    }
    
    dir->free_granules = ctx->gat.free_granules;
    dir->free_size = uft_trsdos_free_space(ctx);
    
    return UFT_TRSDOS_OK;
}

uft_trsdos_err_t uft_trsdos_read_dir(uft_trsdos_ctx_t *ctx, uft_trsdos_dir_t *dir) {
    if (!ctx || !ctx->data || !dir) {
        return UFT_TRSDOS_ERR_NULL;
    }
    
    /* Initialize directory structure */
    memset(dir, 0, sizeof(*dir));
    
    switch (ctx->version) {
        case UFT_TRSDOS_VERSION_RSDOS:
            return read_dir_rsdos(ctx, dir);
        case UFT_TRSDOS_VERSION_23:
            return read_dir_trsdos23(ctx, dir);
        default:
            return read_dir_trsdos6(ctx, dir);
    }
}

void uft_trsdos_free_dir(uft_trsdos_dir_t *dir) {
    if (!dir) return;
    
    if (dir->entries) {
        free(dir->entries);
    }
    memset(dir, 0, sizeof(*dir));
}

uft_trsdos_err_t uft_trsdos_find_file(const uft_trsdos_ctx_t *ctx,
                                       const char *name, const char *ext,
                                       uft_trsdos_entry_t *entry) {
    if (!ctx || !name || !entry) {
        return UFT_TRSDOS_ERR_NULL;
    }
    
    /* Parse and normalize filename */
    char norm_name[UFT_TRSDOS_MAX_NAME + 1];
    char norm_ext[UFT_TRSDOS_MAX_EXT + 1];
    
    memset(norm_name, ' ', UFT_TRSDOS_MAX_NAME);
    memset(norm_ext, ' ', UFT_TRSDOS_MAX_EXT);
    norm_name[UFT_TRSDOS_MAX_NAME] = '\0';
    norm_ext[UFT_TRSDOS_MAX_EXT] = '\0';
    
    size_t name_len = strlen(name);
    if (name_len > UFT_TRSDOS_MAX_NAME) name_len = UFT_TRSDOS_MAX_NAME;
    for (size_t i = 0; i < name_len; i++) {
        norm_name[i] = toupper((unsigned char)name[i]);
    }
    
    if (ext) {
        size_t ext_len = strlen(ext);
        if (ext_len > UFT_TRSDOS_MAX_EXT) ext_len = UFT_TRSDOS_MAX_EXT;
        for (size_t i = 0; i < ext_len; i++) {
            norm_ext[i] = toupper((unsigned char)ext[i]);
        }
    }
    
    /* Read directory and search */
    uft_trsdos_dir_t dir;
    uft_trsdos_err_t err = uft_trsdos_read_dir((uft_trsdos_ctx_t *)ctx, &dir);
    if (err != UFT_TRSDOS_OK) {
        return err;
    }
    
    bool found = false;
    for (size_t i = 0; i < dir.count; i++) {
        if (memcmp(dir.entries[i].name, norm_name, UFT_TRSDOS_MAX_NAME) == 0 &&
            memcmp(dir.entries[i].ext, norm_ext, UFT_TRSDOS_MAX_EXT) == 0) {
            *entry = dir.entries[i];
            found = true;
            break;
        }
    }
    
    uft_trsdos_free_dir(&dir);
    
    return found ? UFT_TRSDOS_OK : UFT_TRSDOS_ERR_NOT_FOUND;
}

int uft_trsdos_foreach(const uft_trsdos_ctx_t *ctx,
                       int (*callback)(const uft_trsdos_entry_t *entry, void *user),
                       void *user_data) {
    if (!ctx || !callback) return 0;
    
    uft_trsdos_dir_t dir;
    if (uft_trsdos_read_dir((uft_trsdos_ctx_t *)ctx, &dir) != UFT_TRSDOS_OK) {
        return 0;
    }
    
    int count = 0;
    for (size_t i = 0; i < dir.count; i++) {
        int result = callback(&dir.entries[i], user_data);
        if (result < 0) break;
        count++;
    }
    
    uft_trsdos_free_dir(&dir);
    return count;
}

/*===========================================================================
 * File Operations
 *===========================================================================*/

/**
 * @brief Read granule data for RS-DOS
 */
static uft_trsdos_err_t read_rsdos_granule(const uft_trsdos_ctx_t *ctx,
                                            uint8_t granule,
                                            uint8_t *buffer, uint8_t sectors) {
    uint8_t track, sector;
    
    /* Calculate track and sector */
    /* RS-DOS: 2 granules per track, 9 sectors per granule */
    track = granule / 2;
    if (track >= 17) track++; /* Skip directory track */
    
    sector = (granule % 2) * 9;
    
    for (int s = 0; s < sectors && s < 9; s++) {
        uft_trsdos_err_t err = uft_trsdos_read_sector(ctx, track, 0, sector + s,
                                                       buffer + s * 256, 256);
        if (err != UFT_TRSDOS_OK) return err;
    }
    
    return UFT_TRSDOS_OK;
}

/**
 * @brief Extract RS-DOS file
 */
static uft_trsdos_err_t extract_rsdos(const uft_trsdos_ctx_t *ctx,
                                       const uft_trsdos_entry_t *entry,
                                       uint8_t *buffer, size_t *size) {
    uint8_t fat[256];
    
    /* Read FAT */
    uft_trsdos_err_t err = uft_trsdos_read_sector(ctx, 17, 0, 1, fat, sizeof(fat));
    if (err != UFT_TRSDOS_OK) return err;
    
    uint8_t granule = entry->extents[0].start_granule;
    size_t offset = 0;
    size_t max_size = *size;
    
    while (granule < 68) {
        uint8_t next = fat[granule];
        uint8_t sectors_to_read = 9;
        bool is_last = false;
        
        if (next >= 0xC0 && next <= 0xC9) {
            /* Last granule */
            sectors_to_read = next - 0xC0;
            is_last = true;
        } else if (next >= 68) {
            break; /* Invalid */
        }
        
        /* Read sectors */
        uint8_t track = granule / 2;
        if (track >= 17) track++;
        uint8_t sector = (granule % 2) * 9;
        
        for (int s = 0; s < sectors_to_read; s++) {
            if (offset + 256 > max_size) {
                *size = entry->size;
                return UFT_TRSDOS_ERR_RANGE;
            }
            
            err = uft_trsdos_read_sector(ctx, track, 0, sector + s,
                                          buffer + offset, 256);
            if (err != UFT_TRSDOS_OK) return err;
            offset += 256;
        }
        
        if (is_last) break;
        granule = next;
    }
    
    /* Adjust for actual file size */
    if (entry->size < offset) {
        offset = entry->size;
    }
    
    *size = offset;
    return UFT_TRSDOS_OK;
}

/**
 * @brief Extract TRSDOS 2.3 file
 */
static uft_trsdos_err_t extract_trsdos23(const uft_trsdos_ctx_t *ctx,
                                          const uft_trsdos_entry_t *entry,
                                          uint8_t *buffer, size_t *size) {
    size_t offset = 0;
    size_t max_size = *size;
    
    for (int e = 0; e < entry->extent_count; e++) {
        uint8_t start = entry->extents[e].start_granule;
        uint8_t count = entry->extents[e].num_granules;
        
        for (int g = 0; g < count; g++) {
            uint8_t granule = start + g;
            
            /* Convert granule to track/sector */
            uint8_t track, first_sector;
            uft_trsdos_granule_to_ts(ctx, granule, &track, &first_sector);
            
            /* Read 5 sectors per granule */
            for (int s = 0; s < 5; s++) {
                if (offset + 256 > max_size) {
                    *size = entry->size;
                    return UFT_TRSDOS_ERR_RANGE;
                }
                
                uft_trsdos_err_t err = uft_trsdos_read_sector(ctx, track, 0, 
                    first_sector + s, buffer + offset, 256);
                if (err != UFT_TRSDOS_OK) return err;
                offset += 256;
            }
        }
    }
    
    /* Truncate to actual file size */
    if (entry->size < offset) {
        offset = entry->size;
    }
    
    *size = offset;
    return UFT_TRSDOS_OK;
}

uft_trsdos_err_t uft_trsdos_extract(const uft_trsdos_ctx_t *ctx,
                                     const char *name, const char *ext,
                                     uint8_t *buffer, size_t *size,
                                     const char *password) {
    if (!ctx || !name || !buffer || !size) {
        return UFT_TRSDOS_ERR_NULL;
    }
    
    /* Find file */
    uft_trsdos_entry_t entry;
    uft_trsdos_err_t err = uft_trsdos_find_file(ctx, name, ext, &entry);
    if (err != UFT_TRSDOS_OK) return err;
    
    /* Check password if required */
    if (entry.attrib.has_password && !password) {
        return UFT_TRSDOS_ERR_PASSWORD;
    }
    
    /* Check buffer size */
    if (*size < entry.size) {
        *size = entry.size;
        return UFT_TRSDOS_ERR_RANGE;
    }
    
    /* Extract based on version */
    switch (ctx->version) {
        case UFT_TRSDOS_VERSION_RSDOS:
            return extract_rsdos(ctx, &entry, buffer, size);
        case UFT_TRSDOS_VERSION_23:
            return extract_trsdos23(ctx, &entry, buffer, size);
        default:
            return extract_trsdos23(ctx, &entry, buffer, size); /* Similar */
    }
}

uft_trsdos_err_t uft_trsdos_extract_to_file(const uft_trsdos_ctx_t *ctx,
                                             const char *name, const char *ext,
                                             const char *output_path,
                                             const char *password) {
    if (!ctx || !name || !output_path) {
        return UFT_TRSDOS_ERR_NULL;
    }
    
    /* Find file to get size */
    uft_trsdos_entry_t entry;
    uft_trsdos_err_t err = uft_trsdos_find_file(ctx, name, ext, &entry);
    if (err != UFT_TRSDOS_OK) return err;
    
    /* Allocate buffer */
    uint8_t *buffer = malloc(entry.size + 256);
    if (!buffer) return UFT_TRSDOS_ERR_NOMEM;
    
    size_t size = entry.size + 256;
    
    /* Extract */
    err = uft_trsdos_extract(ctx, name, ext, buffer, &size, password);
    if (err != UFT_TRSDOS_OK) {
        free(buffer);
        return err;
    }
    
    /* Write to file */
    FILE *fp = fopen(output_path, "wb");
    if (!fp) {
        free(buffer);
        return UFT_TRSDOS_ERR_IO;
    }
    
    size_t written = fwrite(buffer, 1, size, fp);
    fclose(fp);
    free(buffer);
    
    return (written == size) ? UFT_TRSDOS_OK : UFT_TRSDOS_ERR_IO;
}

/*===========================================================================
 * Print/JSON Functions
 *===========================================================================*/

void uft_trsdos_print_dir(const uft_trsdos_ctx_t *ctx) {
    if (!ctx) return;
    
    uft_trsdos_dir_t dir;
    if (uft_trsdos_read_dir((uft_trsdos_ctx_t *)ctx, &dir) != UFT_TRSDOS_OK) {
        printf("Error reading directory\n");
        return;
    }
    
    printf("%-8s %-3s %7s %3s  %s\n", "Name", "Ext", "Size", "Grn", "Attributes");
    printf("-------- --- ------- ---  ----------\n");
    
    for (size_t i = 0; i < dir.count; i++) {
        uft_trsdos_entry_t *e = &dir.entries[i];
        
        char name[16];
        uft_trsdos_format_filename(e->name, e->ext, name);
        
        char attr[16] = "";
        if (e->attrib.visibility == UFT_TRSDOS_ATTR_INVISIBLE) strncat(attr, "I", sizeof(attr) - strlen(attr) - 1);
        if (e->attrib.visibility == UFT_TRSDOS_ATTR_SYSTEM) strncat(attr, "S", sizeof(attr) - strlen(attr) - 1);
        if (e->attrib.has_password) strncat(attr, "P", sizeof(attr) - strlen(attr) - 1);
        if (e->attrib.protection > UFT_TRSDOS_PROT_FULL) strncat(attr, "L", sizeof(attr) - strlen(attr) - 1);
        
        printf("%-8.8s %-3.3s %7u %3u  %s\n",
               e->name, e->ext, e->size, e->granules, attr);
    }
    
    printf("\n%u files, %u bytes used, %u bytes free\n",
           dir.total_files, dir.total_size, dir.free_size);
    
    uft_trsdos_free_dir(&dir);
}

void uft_trsdos_print_info(const uft_trsdos_ctx_t *ctx) {
    if (!ctx) return;
    
    printf("TRSDOS Disk Information\n");
    printf("=======================\n");
    printf("DOS Version:    %s\n", uft_trsdos_version_name(ctx->version));
    printf("Geometry:       %s\n", ctx->geometry.name);
    printf("Tracks:         %u\n", ctx->geometry.tracks);
    printf("Sides:          %u\n", ctx->geometry.sides);
    printf("Sectors/Track:  %u\n", ctx->geometry.sectors_per_track);
    printf("Sector Size:    %u bytes\n", ctx->geometry.sector_size);
    printf("Total Capacity: %u bytes\n", ctx->geometry.total_bytes);
    printf("Free Granules:  %u / %u\n", ctx->gat.free_granules, ctx->gat.total_granules);
    printf("Free Space:     %u bytes\n", uft_trsdos_free_space(ctx));
    
    if (ctx->disk_name[0]) {
        printf("Disk Name:      %s\n", ctx->disk_name);
    }
}

int uft_trsdos_to_json(const uft_trsdos_ctx_t *ctx, char *buffer, size_t size) {
    if (!ctx || !buffer || size == 0) return -1;
    
    uft_trsdos_dir_t dir;
    if (uft_trsdos_read_dir((uft_trsdos_ctx_t *)ctx, &dir) != UFT_TRSDOS_OK) {
        return -1;
    }
    
    int pos = 0;
    pos += snprintf(buffer + pos, size - pos,
        "{\n"
        "  \"version\": \"%s\",\n"
        "  \"geometry\": \"%s\",\n"
        "  \"total_bytes\": %u,\n"
        "  \"free_bytes\": %u,\n"
        "  \"free_granules\": %u,\n"
        "  \"files\": [\n",
        uft_trsdos_version_name(ctx->version),
        ctx->geometry.name,
        ctx->geometry.total_bytes,
        uft_trsdos_free_space(ctx),
        ctx->gat.free_granules);
    
    for (size_t i = 0; i < dir.count; i++) {
        uft_trsdos_entry_t *e = &dir.entries[i];
        
        char name[16];
        uft_trsdos_format_filename(e->name, e->ext, name);
        
        pos += snprintf(buffer + pos, size - pos,
            "    {\"name\": \"%s\", \"size\": %u, \"granules\": %u}%s\n",
            name, e->size, e->granules,
            (i + 1 < dir.count) ? "," : "");
    }
    
    pos += snprintf(buffer + pos, size - pos, "  ]\n}\n");
    
    uft_trsdos_free_dir(&dir);
    return pos;
}

/*===========================================================================
 * RS-DOS Specific
 *===========================================================================*/

uft_rsdos_type_t uft_rsdos_get_type(const uft_trsdos_entry_t *entry) {
    if (!entry || entry->version != UFT_TRSDOS_VERSION_RSDOS) {
        return UFT_RSDOS_TYPE_DATA;
    }
    
    /* Type is stored in entry, but we need to read from raw entry */
    /* For now return based on extension */
    if (strncmp(entry->ext, "BAS", 3) == 0) return UFT_RSDOS_TYPE_BASIC;
    if (strncmp(entry->ext, "BIN", 3) == 0) return UFT_RSDOS_TYPE_ML;
    if (strncmp(entry->ext, "TXT", 3) == 0) return UFT_RSDOS_TYPE_TEXT;
    
    return UFT_RSDOS_TYPE_DATA;
}
