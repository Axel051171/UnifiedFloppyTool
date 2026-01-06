#define _POSIX_C_SOURCE 200809L
/**
 * @file uft_fat12.c
 * @brief FAT12 filesystem implementation
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <stdio.h>
/* Platform compatibility for ssize_t */
#ifdef _MSC_VER
    #include <BaseTsd.h>
    typedef SSIZE_T ssize_t;
#else
    #include <sys/types.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#include "uft_fat12.h"

/*===========================================================================
 * Internal Structures
 *===========================================================================*/

struct uft_fat12_s {
    uft_disk_t *disk;               /**< Underlying disk handle */
    
    uft_boot_sector_t boot;         /**< Boot sector */
    uint8_t *fat;                   /**< FAT table in memory */
    size_t fat_size;                /**< FAT size in bytes */
    
    uint32_t root_dir_sector;       /**< First root directory sector */
    uint32_t root_dir_sectors;      /**< Number of root directory sectors */
    uint32_t first_data_sector;     /**< First data sector */
    uint32_t total_clusters;        /**< Total data clusters */
    
    bool fat_dirty;                 /**< FAT needs writing */
    bool mounted;                   /**< Volume is mounted */
};

struct uft_fat12_dir_s {
    uft_fat12_t *vol;               /**< Parent volume */
    
    uint32_t start_cluster;         /**< Starting cluster (0 for root) */
    uint32_t current_cluster;       /**< Current cluster */
    uint32_t current_sector;        /**< Current sector in cluster */
    uint8_t  current_entry;         /**< Current entry in sector */
    
    uint8_t sector_buffer[UFT_SECTOR_SIZE]; /**< Sector buffer */
    bool buffer_valid;              /**< Buffer contains valid data */
    bool is_root;                   /**< Is root directory */
};

struct uft_fat12_file_s {
    uft_fat12_t *vol;               /**< Parent volume */
    
    uint16_t start_cluster;         /**< Starting cluster */
    uint16_t current_cluster;       /**< Current cluster */
    uint32_t position;              /**< Current position */
    uint32_t size;                  /**< File size */
    
    uint32_t dir_sector;            /**< Directory entry sector */
    uint8_t  dir_offset;            /**< Directory entry offset */
    
    uft_fat12_mode_t mode;          /**< Open mode */
    bool dirty;                     /**< File modified */
    
    uint8_t buffer[UFT_SECTOR_SIZE]; /**< Sector buffer */
    uint32_t buffer_sector;          /**< Sector in buffer */
    bool buffer_valid;               /**< Buffer contains valid data */
    bool buffer_dirty;               /**< Buffer needs writing */
};

/*===========================================================================
 * FAT12 Helpers
 *===========================================================================*/

static uint16_t fat12_get_entry(const uint8_t *fat, uint16_t cluster) {
    uint32_t offset = cluster + (cluster / 2);  /* cluster * 1.5 */
    uint16_t value = fat[offset] | ((uint16_t)fat[offset + 1] << 8);
    
    if (cluster & 1) {
        /* Odd cluster - take high 12 bits */
        return value >> 4;
    } else {
        /* Even cluster - take low 12 bits */
        return value & 0x0FFF;
    }
}

static void fat12_set_entry(uint8_t *fat, uint16_t cluster, uint16_t value) {
    uint32_t offset = cluster + (cluster / 2);
    value &= 0x0FFF;
    
    if (cluster & 1) {
        /* Odd cluster - set high 12 bits */
        fat[offset] = (fat[offset] & 0x0F) | ((value << 4) & 0xF0);
        fat[offset + 1] = (value >> 4) & 0xFF;
    } else {
        /* Even cluster - set low 12 bits */
        fat[offset] = value & 0xFF;
        fat[offset + 1] = (fat[offset + 1] & 0xF0) | ((value >> 8) & 0x0F);
    }
}

static bool fat12_is_eof(uint16_t cluster) {
    return cluster >= 0x0FF8;
}

static bool fat12_is_free(uint16_t cluster) {
    return cluster == 0x0000;
}

static bool fat12_is_bad(uint16_t cluster) {
    return cluster == 0x0FF7;
}

/*===========================================================================
 * Sector Operations
 *===========================================================================*/

static uft_error_t vol_read_sector(uft_fat12_t *vol, uint32_t sector, void *buffer) {
    return uft_disk_read_sectors(vol->disk, buffer, sector, 1);
}

static uft_error_t vol_write_sector(uft_fat12_t *vol, uint32_t sector, 
                                     const void *buffer) {
    return uft_disk_write_sectors(vol->disk, buffer, sector, 1);
}

static uint32_t cluster_to_sector(uft_fat12_t *vol, uint16_t cluster) {
    return vol->first_data_sector + 
           ((cluster - 2) * vol->boot.bpb.sectors_per_cluster);
}

/*===========================================================================
 * FAT Operations
 *===========================================================================*/

static uft_error_t load_fat(uft_fat12_t *vol) {
    uint16_t sectors_per_fat = uft_le16_to_cpu(vol->boot.bpb.sectors_per_fat);
    vol->fat_size = sectors_per_fat * uft_le16_to_cpu(vol->boot.bpb.bytes_per_sector);
    
    vol->fat = (uint8_t*)malloc(vol->fat_size);
    if (!vol->fat) {
        return UFT_ERR_NO_MEMORY;
    }
    
    uint32_t fat_start = uft_le16_to_cpu(vol->boot.bpb.reserved_sectors);
    
    for (uint16_t i = 0; i < sectors_per_fat; i++) {
        uft_error_t err = vol_read_sector(vol, fat_start + i, 
                                           vol->fat + (i * UFT_SECTOR_SIZE));
        if (err != UFT_OK) {
            free(vol->fat);
            vol->fat = NULL;
            return err;
        }
    }
    
    vol->fat_dirty = false;
    return UFT_OK;
}

static uft_error_t flush_fat(uft_fat12_t *vol) {
    if (!vol->fat_dirty) {
        return UFT_OK;
    }
    
    uint16_t sectors_per_fat = uft_le16_to_cpu(vol->boot.bpb.sectors_per_fat);
    uint32_t fat_start = uft_le16_to_cpu(vol->boot.bpb.reserved_sectors);
    uint8_t num_fats = vol->boot.bpb.num_fats;
    
    /* Write all FAT copies */
    for (uint8_t f = 0; f < num_fats; f++) {
        uint32_t fat_offset = fat_start + (f * sectors_per_fat);
        
        for (uint16_t i = 0; i < sectors_per_fat; i++) {
            uft_error_t err = vol_write_sector(vol, fat_offset + i,
                                                vol->fat + (i * UFT_SECTOR_SIZE));
            if (err != UFT_OK) {
                return err;
            }
        }
    }
    
    vol->fat_dirty = false;
    return UFT_OK;
}

/*===========================================================================
 * Volume Operations
 *===========================================================================*/

uft_error_t uft_fat12_mount(uft_disk_t *disk, uft_fat12_t **vol) {
    if (!disk || !vol) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    uft_fat12_t *v = (uft_fat12_t*)calloc(1, sizeof(uft_fat12_t));
    if (!v) {
        return UFT_ERR_NO_MEMORY;
    }
    
    v->disk = disk;
    
    /* Read boot sector */
    uft_error_t err = vol_read_sector(v, 0, &v->boot);
    if (err != UFT_OK) {
        free(v);
        return err;
    }
    
    /* Verify boot signature */
    if (uft_le16_to_cpu(v->boot.signature) != 0xAA55) {
        free(v);
        return UFT_ERR_INVALID_FORMAT;
    }
    
    /* Verify file system type */
    if (memcmp(v->boot.ebr.fs_type, "FAT12   ", 8) != 0 &&
        memcmp(v->boot.ebr.fs_type, "FAT     ", 8) != 0) {
        /* Not necessarily an error - some disks don't have this */
    }
    
    /* Calculate layout */
    uint16_t bytes_per_sector = uft_le16_to_cpu(v->boot.bpb.bytes_per_sector);
    uint16_t root_entries = uft_le16_to_cpu(v->boot.bpb.root_entries);
    
    v->root_dir_sectors = ((root_entries * 32) + (bytes_per_sector - 1)) / 
                           bytes_per_sector;
    v->root_dir_sector = uft_le16_to_cpu(v->boot.bpb.reserved_sectors) +
                          (v->boot.bpb.num_fats * 
                           uft_le16_to_cpu(v->boot.bpb.sectors_per_fat));
    v->first_data_sector = v->root_dir_sector + v->root_dir_sectors;
    
    uint32_t total_sectors = uft_le16_to_cpu(v->boot.bpb.total_sectors_16);
    if (total_sectors == 0) {
        total_sectors = uft_le32_to_cpu(v->boot.bpb.total_sectors_32);
    }
    
    uint32_t data_sectors = total_sectors - v->first_data_sector;
    v->total_clusters = data_sectors / v->boot.bpb.sectors_per_cluster;
    
    /* Load FAT */
    err = load_fat(v);
    if (err != UFT_OK) {
        free(v);
        return err;
    }
    
    v->mounted = true;
    *vol = v;
    return UFT_OK;
}

void uft_fat12_unmount(uft_fat12_t *vol) {
    if (!vol) return;
    
    if (vol->mounted) {
        flush_fat(vol);
        
        if (vol->fat) {
            free(vol->fat);
            vol->fat = NULL;
        }
        
        vol->mounted = false;
    }
    
    free(vol);
}

uft_error_t uft_fat12_get_info(uft_fat12_t *vol, uft_fat12_info_t *info) {
    if (!vol || !info) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    memset(info, 0, sizeof(*info));
    
    /* Copy OEM name */
    memcpy(info->oem_name, vol->boot.oem_name, 8);
    info->oem_name[8] = '\0';
    
    /* Copy volume label */
    memcpy(info->volume_label, vol->boot.ebr.volume_label, 11);
    info->volume_label[11] = '\0';
    
    /* Trim trailing spaces */
    for (int i = 10; i >= 0 && info->volume_label[i] == ' '; i--) {
        info->volume_label[i] = '\0';
    }
    
    info->volume_serial = uft_le32_to_cpu(vol->boot.ebr.volume_id);
    
    uint32_t total_sectors = uft_le16_to_cpu(vol->boot.bpb.total_sectors_16);
    if (total_sectors == 0) {
        total_sectors = uft_le32_to_cpu(vol->boot.bpb.total_sectors_32);
    }
    
    info->total_sectors = total_sectors;
    info->total_clusters = vol->total_clusters;
    
    /* Count free clusters */
    uint32_t free_clusters = 0;
    for (uint32_t i = 2; i < vol->total_clusters + 2; i++) {
        if (fat12_get_entry(vol->fat, (uint16_t)i) == 0) {
            free_clusters++;
        }
    }
    
    info->free_clusters = free_clusters;
    info->free_sectors = free_clusters * vol->boot.bpb.sectors_per_cluster;
    info->used_sectors = info->total_sectors - info->free_sectors - 
                          vol->first_data_sector;
    
    info->bytes_per_sector = uft_le16_to_cpu(vol->boot.bpb.bytes_per_sector);
    info->sectors_per_cluster = vol->boot.bpb.sectors_per_cluster;
    info->root_entries = uft_le16_to_cpu(vol->boot.bpb.root_entries);
    info->fat_count = vol->boot.bpb.num_fats;
    info->fat_sectors = uft_le16_to_cpu(vol->boot.bpb.sectors_per_fat);
    info->media_type = vol->boot.bpb.media_type;
    info->is_dirty = vol->fat_dirty;
    
    return UFT_OK;
}

uft_error_t uft_fat12_sync(uft_fat12_t *vol) {
    if (!vol) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    uft_error_t err = flush_fat(vol);
    if (err != UFT_OK) {
        return err;
    }
    
    return uft_disk_sync(vol->disk);
}

/*===========================================================================
 * Directory Operations
 *===========================================================================*/

uft_error_t uft_fat12_opendir_root(uft_fat12_t *vol, uft_fat12_dir_t **dir) {
    if (!vol || !dir) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    uft_fat12_dir_t *d = (uft_fat12_dir_t*)calloc(1, sizeof(uft_fat12_dir_t));
    if (!d) {
        return UFT_ERR_NO_MEMORY;
    }
    
    d->vol = vol;
    d->start_cluster = 0;
    d->current_cluster = 0;
    d->current_sector = vol->root_dir_sector;
    d->current_entry = 0;
    d->is_root = true;
    d->buffer_valid = false;
    
    *dir = d;
    return UFT_OK;
}

uft_error_t uft_fat12_readdir(uft_fat12_dir_t *dir, uft_fat12_entry_t *entry) {
    if (!dir || !entry) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    while (1) {
        /* Check if we need to read a new sector */
        if (!dir->buffer_valid || dir->current_entry >= 16) {
            /* Check bounds for root directory */
            if (dir->is_root) {
                if (dir->current_sector >= 
                    dir->vol->root_dir_sector + dir->vol->root_dir_sectors) {
                    return UFT_ERR_END_OF_FILE;
                }
            } else {
                /* Subdirectory - check cluster chain */
                if (dir->current_cluster == 0 || 
                    fat12_is_eof(dir->current_cluster)) {
                    return UFT_ERR_END_OF_FILE;
                }
            }
            
            uft_error_t err = vol_read_sector(dir->vol, dir->current_sector, 
                                               dir->sector_buffer);
            if (err != UFT_OK) {
                return err;
            }
            
            dir->buffer_valid = true;
            dir->current_entry = 0;
        }
        
        /* Get current entry */
        uft_dir_entry_t *de = (uft_dir_entry_t*)(dir->sector_buffer + 
                                                   dir->current_entry * 32);
        
        /* Check for end of directory */
        if (de->name[0] == 0x00) {
            return UFT_ERR_END_OF_FILE;
        }
        
        dir->current_entry++;
        
        /* Check if we need to move to next sector */
        if (dir->current_entry >= 16) {
            dir->current_sector++;
            dir->buffer_valid = false;
            
            if (!dir->is_root) {
                /* Check if we need to move to next cluster */
                uint8_t spc = dir->vol->boot.bpb.sectors_per_cluster;
                if ((dir->current_sector - 
                     cluster_to_sector(dir->vol, dir->current_cluster)) >= spc) {
                    dir->current_cluster = fat12_get_entry(dir->vol->fat, 
                                                            dir->current_cluster);
                    if (!fat12_is_eof(dir->current_cluster)) {
                        dir->current_sector = cluster_to_sector(dir->vol, 
                                                                 dir->current_cluster);
                    }
                }
            }
        }
        
        /* Skip deleted entries */
        if ((uint8_t)de->name[0] == 0xE5) {
            continue;
        }
        
        /* Skip volume labels and LFN entries in basic listing */
        if ((de->attr & UFT_ATTR_VOLUME_ID) || 
            (de->attr & UFT_ATTR_LONG_NAME) == UFT_ATTR_LONG_NAME) {
            continue;
        }
        
        /* Fill entry structure */
        memset(entry, 0, sizeof(*entry));
        
        uft_fat12_format_name(de->name, entry->name);
        memcpy(entry->short_name, de->name, 11);
        entry->short_name[11] = '\0';
        
        entry->attributes = de->attr;
        entry->size = uft_le32_to_cpu(de->file_size);
        entry->cluster = uft_le16_to_cpu(de->cluster_low);
        
        uft_fat12_decode_datetime(uft_le16_to_cpu(de->create_date),
                                   uft_le16_to_cpu(de->create_time),
                                   &entry->created.year, &entry->created.month,
                                   &entry->created.day, &entry->created.hour,
                                   &entry->created.minute, &entry->created.second);
        
        uft_fat12_decode_datetime(uft_le16_to_cpu(de->modify_date),
                                   uft_le16_to_cpu(de->modify_time),
                                   &entry->modified.year, &entry->modified.month,
                                   &entry->modified.day, &entry->modified.hour,
                                   &entry->modified.minute, &entry->modified.second);
        
        uft_fat12_decode_datetime(uft_le16_to_cpu(de->access_date), 0,
                                   &entry->accessed.year, &entry->accessed.month,
                                   &entry->accessed.day, &entry->accessed.hour,
                                   &entry->accessed.minute, &entry->accessed.second);
        
        entry->is_directory = (de->attr & UFT_ATTR_DIRECTORY) != 0;
        entry->is_hidden = (de->attr & UFT_ATTR_HIDDEN) != 0;
        entry->is_system = (de->attr & UFT_ATTR_SYSTEM) != 0;
        entry->is_readonly = (de->attr & UFT_ATTR_READ_ONLY) != 0;
        entry->is_deleted = false;
        
        entry->dir_sector = dir->current_sector - 1;
        if (dir->current_entry == 0) {
            entry->dir_sector++;
        }
        entry->dir_offset = (dir->current_entry == 0) ? 15 : dir->current_entry - 1;
        
        return UFT_OK;
    }
}

void uft_fat12_rewinddir(uft_fat12_dir_t *dir) {
    if (!dir) return;
    
    if (dir->is_root) {
        dir->current_sector = dir->vol->root_dir_sector;
    } else {
        dir->current_cluster = dir->start_cluster;
        dir->current_sector = cluster_to_sector(dir->vol, dir->start_cluster);
    }
    dir->current_entry = 0;
    dir->buffer_valid = false;
}

void uft_fat12_closedir(uft_fat12_dir_t *dir) {
    if (dir) {
        free(dir);
    }
}

/*===========================================================================
 * FAT Entry Operations
 *===========================================================================*/

uft_error_t uft_fat12_get_fat_entry(uft_fat12_t *vol, uint16_t cluster,
                                     uint16_t *value) {
    if (!vol || !value) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    if (cluster < 2 || cluster >= vol->total_clusters + 2) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    *value = fat12_get_entry(vol->fat, cluster);
    return UFT_OK;
}

uft_error_t uft_fat12_set_fat_entry(uft_fat12_t *vol, uint16_t cluster,
                                     uint16_t value) {
    if (!vol) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    if (cluster < 2 || cluster >= vol->total_clusters + 2) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    fat12_set_entry(vol->fat, cluster, value);
    vol->fat_dirty = true;
    
    return UFT_OK;
}

uft_error_t uft_fat12_find_free_cluster(uft_fat12_t *vol, uint16_t *cluster) {
    if (!vol || !cluster) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    for (uint32_t i = 2; i < vol->total_clusters + 2; i++) {
        if (fat12_get_entry(vol->fat, (uint16_t)i) == 0) {
            *cluster = (uint16_t)i;
            return UFT_OK;
        }
    }
    
    return UFT_ERR_DISK_FULL;
}

/*===========================================================================
 * Utility Functions
 *===========================================================================*/

void uft_fat12_format_name(const char *raw, char *buffer) {
    if (!raw || !buffer) return;
    
    int j = 0;
    
    /* Copy filename (first 8 chars, trimming spaces) */
    for (int i = 0; i < 8 && raw[i] != ' '; i++) {
        buffer[j++] = raw[i];
    }
    
    /* Add extension if present */
    if (raw[8] != ' ') {
        buffer[j++] = '.';
        for (int i = 8; i < 11 && raw[i] != ' '; i++) {
            buffer[j++] = raw[i];
        }
    }
    
    buffer[j] = '\0';
}

uft_error_t uft_fat12_parse_name(const char *name, char *buffer) {
    if (!name || !buffer) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    /* Initialize with spaces */
    memset(buffer, ' ', 11);
    
    const char *dot = strchr(name, '.');
    size_t base_len = dot ? (size_t)(dot - name) : strlen(name);
    
    if (base_len > 8) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    /* Copy base name (uppercase) */
    for (size_t i = 0; i < base_len; i++) {
        buffer[i] = (char)toupper((unsigned char)name[i]);
    }
    
    /* Copy extension if present */
    if (dot) {
        const char *ext = dot + 1;
        size_t ext_len = strlen(ext);
        
        if (ext_len > 3) {
            return UFT_ERR_INVALID_PARAM;
        }
        
        for (size_t i = 0; i < ext_len; i++) {
            buffer[8 + i] = (char)toupper((unsigned char)ext[i]);
        }
    }
    
    return UFT_OK;
}

bool uft_fat12_valid_name(const char *name) {
    if (!name || name[0] == '\0') {
        return false;
    }
    
    const char *dot = strchr(name, '.');
    size_t base_len = dot ? (size_t)(dot - name) : strlen(name);
    
    if (base_len == 0 || base_len > 8) {
        return false;
    }
    
    if (dot) {
        size_t ext_len = strlen(dot + 1);
        if (ext_len > 3) {
            return false;
        }
        
        /* Check for invalid characters in extension */
        for (const char *p = dot + 1; *p; p++) {
            if (!isalnum((unsigned char)*p) && *p != '_' && *p != '-') {
                return false;
            }
        }
    }
    
    /* Check for invalid characters in base name */
    for (size_t i = 0; i < base_len; i++) {
        if (!isalnum((unsigned char)name[i]) && name[i] != '_' && name[i] != '-') {
            return false;
        }
    }
    
    return true;
}

void uft_fat12_decode_datetime(uint16_t date, uint16_t time,
                                uint16_t *year, uint8_t *month, uint8_t *day,
                                uint8_t *hour, uint8_t *minute, uint8_t *second) {
    if (year)   *year   = 1980 + ((date >> 9) & 0x7F);
    if (month)  *month  = (date >> 5) & 0x0F;
    if (day)    *day    = date & 0x1F;
    
    if (hour)   *hour   = (time >> 11) & 0x1F;
    if (minute) *minute = (time >> 5) & 0x3F;
    if (second) *second = (time & 0x1F) * 2;
}

void uft_fat12_encode_datetime(uint16_t year, uint8_t month, uint8_t day,
                                uint8_t hour, uint8_t minute, uint8_t second,
                                uint16_t *date, uint16_t *time) {
    if (date) {
        *date = ((year - 1980) << 9) | (month << 5) | day;
    }
    
    if (time) {
        *time = (hour << 11) | (minute << 5) | (second / 2);
    }
}

/*===========================================================================
 * File Operations (Basic Implementation)
 *===========================================================================*/

uft_error_t uft_fat12_open(uft_fat12_t *vol, const char *path,
                            uft_fat12_mode_t mode, uft_fat12_file_t **file) {
    if (!vol || !path || !file) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    /* Find file in directory */
    uft_fat12_entry_t entry;
    uft_error_t err = uft_fat12_find(vol, path, &entry);
    
    if (err == UFT_ERR_NOT_FOUND && (mode & UFT_FAT12_CREATE)) {
        /* TODO: Create file */
        return UFT_ERR_UNSUPPORTED;
    }
    
    if (err != UFT_OK) {
        return err;
    }
    
    if (entry.is_directory) {
        return UFT_ERR_INVALID_PARAM;  /* Can't open directory as file */
    }
    
    /* Create file handle */
    uft_fat12_file_t *f = (uft_fat12_file_t*)calloc(1, sizeof(uft_fat12_file_t));
    if (!f) {
        return UFT_ERR_NO_MEMORY;
    }
    
    f->vol = vol;
    f->start_cluster = entry.cluster;
    f->current_cluster = entry.cluster;
    f->position = 0;
    f->size = entry.size;
    f->dir_sector = entry.dir_sector;
    f->dir_offset = entry.dir_offset;
    f->mode = mode;
    f->dirty = false;
    f->buffer_valid = false;
    f->buffer_dirty = false;
    
    if (mode & UFT_FAT12_APPEND) {
        f->position = f->size;
    }
    
    if (mode & UFT_FAT12_TRUNCATE) {
        /* TODO: Truncate file */
    }
    
    *file = f;
    return UFT_OK;
}

void uft_fat12_close(uft_fat12_file_t *file) {
    if (!file) return;
    
    /* TODO: Flush buffers if dirty */
    
    free(file);
}

uft_error_t uft_fat12_read(uft_fat12_file_t *file, void *buffer,
                            size_t size, size_t *bytes_read) {
    if (!file || !buffer) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    if (!(file->mode & UFT_FAT12_READ)) {
        return UFT_ERR_PERMISSION;
    }
    
    size_t total_read = 0;
    uint8_t *dest = (uint8_t*)buffer;
    uint16_t bytes_per_sector = uft_le16_to_cpu(file->vol->boot.bpb.bytes_per_sector);
    uint8_t sectors_per_cluster = file->vol->boot.bpb.sectors_per_cluster;
    uint32_t bytes_per_cluster = bytes_per_sector * sectors_per_cluster;
    
    while (total_read < size && file->position < file->size) {
        /* Calculate position within current cluster */
        uint32_t cluster_offset = file->position % bytes_per_cluster;
        uint32_t sector_in_cluster = cluster_offset / bytes_per_sector;
        uint32_t byte_in_sector = cluster_offset % bytes_per_sector;
        
        /* Get absolute sector */
        uint32_t sector = cluster_to_sector(file->vol, file->current_cluster) + 
                           sector_in_cluster;
        
        /* Read sector if needed */
        if (!file->buffer_valid || file->buffer_sector != sector) {
            uft_error_t err = vol_read_sector(file->vol, sector, file->buffer);
            if (err != UFT_OK) {
                if (bytes_read) *bytes_read = total_read;
                return err;
            }
            file->buffer_sector = sector;
            file->buffer_valid = true;
        }
        
        /* Calculate bytes to copy from this sector */
        size_t remaining_in_file = file->size - file->position;
        size_t remaining_in_sector = bytes_per_sector - byte_in_sector;
        size_t remaining_to_read = size - total_read;
        
        size_t to_copy = remaining_in_sector;
        if (to_copy > remaining_to_read) to_copy = remaining_to_read;
        if (to_copy > remaining_in_file) to_copy = remaining_in_file;
        
        /* Copy data */
        memcpy(dest + total_read, file->buffer + byte_in_sector, to_copy);
        total_read += to_copy;
        file->position += (uint32_t)to_copy;
        
        /* Move to next cluster if needed */
        if (file->position % bytes_per_cluster == 0 && 
            file->position < file->size) {
            file->current_cluster = fat12_get_entry(file->vol->fat, 
                                                     file->current_cluster);
            if (fat12_is_eof(file->current_cluster)) {
                break;
            }
        }
    }
    
    if (bytes_read) *bytes_read = total_read;
    return UFT_OK;
}

uint32_t uft_fat12_tell(uft_fat12_file_t *file) {
    return file ? file->position : 0;
}

uint32_t uft_fat12_size(uft_fat12_file_t *file) {
    return file ? file->size : 0;
}

bool uft_fat12_eof(uft_fat12_file_t *file) {
    return file ? (file->position >= file->size) : true;
}

/*===========================================================================
 * Find Implementation
 *===========================================================================*/

uft_error_t uft_fat12_find(uft_fat12_t *vol, const char *path,
                            uft_fat12_entry_t *entry) {
    if (!vol || !path || !entry) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    /* Skip leading slashes */
    while (*path == '/' || *path == '\\') path++;
    
    if (*path == '\0') {
        return UFT_ERR_INVALID_PARAM;
    }
    
    /* Parse 8.3 name */
    char search_name[12];
    uft_error_t err = uft_fat12_parse_name(path, search_name);
    if (err != UFT_OK) {
        return err;
    }
    
    /* Open root directory */
    uft_fat12_dir_t *dir;
    err = uft_fat12_opendir_root(vol, &dir);
    if (err != UFT_OK) {
        return err;
    }
    
    /* Search for entry */
    while ((err = uft_fat12_readdir(dir, entry)) == UFT_OK) {
        if (memcmp(entry->short_name, search_name, 11) == 0) {
            uft_fat12_closedir(dir);
            return UFT_OK;
        }
    }
    
    uft_fat12_closedir(dir);
    return UFT_ERR_NOT_FOUND;
}
