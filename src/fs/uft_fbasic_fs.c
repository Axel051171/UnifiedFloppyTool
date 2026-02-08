/**
 * @file uft_fbasic_fs.c
 * @brief Fujitsu F-BASIC Filesystem Implementation
 * 
 * @copyright MIT License
 */

#include "uft/fs/uft_fbasic_fs.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/* ═══════════════════════════════════════════════════════════════════════════════
 * Internal Helpers
 * ═══════════════════════════════════════════════════════════════════════════════ */

static void trim_name(char *dest, const char *src, size_t len) {
    size_t i = len;
    memcpy(dest, src, len);
    dest[len] = '\0';
    while (i > 0 && (dest[i-1] == ' ' || dest[i-1] == '\0')) {
        dest[--i] = '\0';
    }
}

static void pad_name(char *dest, const char *src, size_t len) {
    size_t slen = strlen(src);
    if (slen > len) slen = len;
    memcpy(dest, src, slen);
    memset(dest + slen, ' ', len - slen);
}

static int str_casecmp(const char *a, const char *b) {
    while (*a && *b) {
        int ca = toupper((unsigned char)*a);
        int cb = toupper((unsigned char)*b);
        if (ca != cb) return ca - cb;
        a++; b++;
    }
    return toupper((unsigned char)*a) - toupper((unsigned char)*b);
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Sector I/O
 * ═══════════════════════════════════════════════════════════════════════════════ */

size_t uft_fbasic_ts_to_offset(uint8_t track, uint8_t sector) {
    /* sector is 1-based */
    size_t linear_sector = (size_t)track * UFT_FBASIC_SECTORS_TRACK + (sector - 1);
    return linear_sector * UFT_FBASIC_SECTOR_SIZE;
}

void uft_fbasic_cluster_to_ts(uint8_t cluster, uint8_t *track, uint8_t *sector) {
    /* Cluster 0 starts at track 4 (C=2, H=0 in physical terms) */
    size_t linear_sector = (size_t)UFT_FBASIC_DATA_START_TRACK * UFT_FBASIC_SECTORS_TRACK +
                           (size_t)cluster * UFT_FBASIC_CLUSTER_SECTORS;
    *track = (uint8_t)(linear_sector / UFT_FBASIC_SECTORS_TRACK);
    *sector = (uint8_t)(linear_sector % UFT_FBASIC_SECTORS_TRACK) + 1;
}

int uft_fbasic_read_sector(uft_fbasic_fs_t *fs, uint8_t track, uint8_t sector,
                            uint8_t *buffer) {
    if (!fs || !fs->disk_data || !buffer) return -1;
    if (sector < 1 || sector > UFT_FBASIC_SECTORS_TRACK) return -2;
    
    size_t offset = uft_fbasic_ts_to_offset(track, sector);
    if (offset + UFT_FBASIC_SECTOR_SIZE > fs->disk_size) return -3;
    
    memcpy(buffer, fs->disk_data + offset, UFT_FBASIC_SECTOR_SIZE);
    return 0;
}

int uft_fbasic_write_sector(uft_fbasic_fs_t *fs, uint8_t track, uint8_t sector,
                             const uint8_t *buffer) {
    if (!fs || !fs->disk_data || !buffer) return -1;
    if (sector < 1 || sector > UFT_FBASIC_SECTORS_TRACK) return -2;
    
    size_t offset = uft_fbasic_ts_to_offset(track, sector);
    if (offset + UFT_FBASIC_SECTOR_SIZE > fs->disk_size) return -3;
    
    memcpy(fs->disk_data + offset, buffer, UFT_FBASIC_SECTOR_SIZE);
    fs->modified = true;
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Validation
 * ═══════════════════════════════════════════════════════════════════════════════ */

bool uft_fbasic_is_valid(const uint8_t *data, size_t size) {
    if (!data || size < UFT_FBASIC_SECTOR_SIZE * 3) return false;
    
    /* Check ID sector (Track 0, Sector 3) */
    size_t id_offset = uft_fbasic_ts_to_offset(0, 3);
    if (id_offset + 3 > size) return false;
    
    /* Should start with 'SYS' or similar identifier */
    if (memcmp(data + id_offset, "SYS", 3) == 0) return true;
    
    /* Alternative: Check for valid FAT signature */
    size_t fat_offset = uft_fbasic_ts_to_offset(2, 1);
    if (fat_offset + UFT_FBASIC_SECTOR_SIZE > size) return false;
    
    /* FAT should have reasonable values */
    const uint8_t *fat = data + fat_offset + UFT_FBASIC_FAT_OFFSET;
    int valid_entries = 0;
    for (int i = 0; i < 20; i++) {
        uint8_t v = fat[i];
        if (v == 0xFF || v == 0xFE || v == 0xFD || 
            (v >= 0xC0 && v <= 0xC7) || v < UFT_FBASIC_FAT_SIZE) {
            valid_entries++;
        }
    }
    
    return valid_entries >= 15;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Initialization
 * ═══════════════════════════════════════════════════════════════════════════════ */

int uft_fbasic_open(uft_fbasic_fs_t *fs, uint8_t *data, size_t size) {
    if (!fs || !data) return -1;
    
    memset(fs, 0, sizeof(*fs));
    fs->disk_data = data;
    fs->disk_size = size;
    
    /* Determine disk geometry */
    size_t total_sectors = size / UFT_FBASIC_SECTOR_SIZE;
    if (total_sectors >= 80 * 16) {
        fs->tracks = 80;
        fs->heads = 2;
    } else {
        fs->tracks = 40;
        fs->heads = 2;
    }
    
    /* Validate format */
    if (!uft_fbasic_is_valid(data, size)) {
        return -2;
    }
    
    /* Read FAT and directory */
    if (uft_fbasic_read_fat(fs) != 0) return -3;
    if (uft_fbasic_read_directory(fs) < 0) return -4;
    
    /* Get disk info */
    uft_fbasic_get_info(fs, &fs->info);
    
    return 0;
}

void uft_fbasic_close(uft_fbasic_fs_t *fs) {
    if (!fs) return;
    /* Note: disk_data is owned by caller */
    memset(fs, 0, sizeof(*fs));
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * FAT Operations
 * ═══════════════════════════════════════════════════════════════════════════════ */

int uft_fbasic_read_fat(uft_fbasic_fs_t *fs) {
    if (!fs) return -1;
    
    uint8_t sector[UFT_FBASIC_SECTOR_SIZE];
    if (uft_fbasic_read_sector(fs, UFT_FBASIC_FAT_TRACK, UFT_FBASIC_FAT_SECTOR, sector) != 0) {
        return -1;
    }
    
    memcpy(fs->fat, sector, UFT_FBASIC_SECTOR_SIZE);
    return 0;
}

int uft_fbasic_write_fat(uft_fbasic_fs_t *fs) {
    if (!fs) return -1;
    return uft_fbasic_write_sector(fs, UFT_FBASIC_FAT_TRACK, UFT_FBASIC_FAT_SECTOR, fs->fat);
}

int uft_fbasic_fat_next(uft_fbasic_fs_t *fs, uint8_t cluster) {
    if (!fs || cluster >= UFT_FBASIC_FAT_SIZE) return -1;
    
    uint8_t val = fs->fat[UFT_FBASIC_FAT_OFFSET + cluster];
    
    /* Check for end of chain */
    if ((val & UFT_FBASIC_FAT_LAST_MASK) == UFT_FBASIC_FAT_LAST_BASE) {
        return -1;  /* Last cluster */
    }
    if (val == UFT_FBASIC_FAT_FREE || val == UFT_FBASIC_FAT_RESERVED ||
        val == UFT_FBASIC_FAT_UNUSED) {
        return -1;
    }
    if (val >= UFT_FBASIC_FAT_SIZE) {
        return -1;  /* Invalid */
    }
    
    return val;
}

int uft_fbasic_fat_alloc(uft_fbasic_fs_t *fs) {
    if (!fs) return -1;
    
    for (int i = 0; i < UFT_FBASIC_FAT_SIZE; i++) {
        if (fs->fat[UFT_FBASIC_FAT_OFFSET + i] == UFT_FBASIC_FAT_FREE) {
            return i;
        }
    }
    return -1;  /* Disk full */
}

void uft_fbasic_fat_free_chain(uft_fbasic_fs_t *fs, uint8_t first_cluster) {
    if (!fs) return;
    
    uint8_t cluster = first_cluster;
    while (cluster < UFT_FBASIC_FAT_SIZE) {
        uint8_t val = fs->fat[UFT_FBASIC_FAT_OFFSET + cluster];
        fs->fat[UFT_FBASIC_FAT_OFFSET + cluster] = UFT_FBASIC_FAT_FREE;
        
        if ((val & UFT_FBASIC_FAT_LAST_MASK) == UFT_FBASIC_FAT_LAST_BASE) break;
        if (val >= UFT_FBASIC_FAT_SIZE) break;
        cluster = val;
    }
    fs->modified = true;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Directory Operations
 * ═══════════════════════════════════════════════════════════════════════════════ */

int uft_fbasic_read_directory(uft_fbasic_fs_t *fs) {
    if (!fs) return -1;
    
    uint8_t sector[UFT_FBASIC_SECTOR_SIZE];
    int entry_idx = 0;
    
    /* Read directory sectors (Track 2, Sector 4-16) */
    for (int s = 4; s <= 16 && entry_idx < UFT_FBASIC_MAX_DIR_ENTRIES; s++) {
        if (uft_fbasic_read_sector(fs, 2, s, sector) != 0) break;
        
        for (int i = 0; i < UFT_FBASIC_SECTOR_SIZE / UFT_FBASIC_DIR_ENTRY_SIZE; i++) {
            memcpy(&fs->dir[entry_idx], sector + i * UFT_FBASIC_DIR_ENTRY_SIZE,
                   UFT_FBASIC_DIR_ENTRY_SIZE);
            entry_idx++;
        }
    }
    
    /* Read directory sectors (Track 3, Sector 1-16) */
    for (int s = 1; s <= 16 && entry_idx < UFT_FBASIC_MAX_DIR_ENTRIES; s++) {
        if (uft_fbasic_read_sector(fs, 3, s, sector) != 0) break;
        
        for (int i = 0; i < UFT_FBASIC_SECTOR_SIZE / UFT_FBASIC_DIR_ENTRY_SIZE; i++) {
            memcpy(&fs->dir[entry_idx], sector + i * UFT_FBASIC_DIR_ENTRY_SIZE,
                   UFT_FBASIC_DIR_ENTRY_SIZE);
            entry_idx++;
        }
    }
    
    fs->dir_count = entry_idx;
    return entry_idx;
}

int uft_fbasic_get_file_info(uft_fbasic_fs_t *fs, int index, 
                              uft_fbasic_file_info_t *info) {
    if (!fs || !info || index < 0 || index >= (int)fs->dir_count) return -1;
    
    const uft_fbasic_dir_entry_t *e = &fs->dir[index];
    
    memset(info, 0, sizeof(*info));
    trim_name(info->name, e->name, 8);
    info->file_type = e->file_type;
    info->is_ascii = (e->ascii_flag == UFT_FBASIC_FLAG_ASCII);
    info->is_random = (e->random_flag == UFT_FBASIC_FLAG_RANDOM);
    info->first_cluster = e->first_cluster;
    info->dir_index = index;
    info->deleted = (e->name[0] == 0x00 || (uint8_t)e->name[0] == 0xFF);
    
    /* Calculate file size by following cluster chain */
    if (!info->deleted && info->first_cluster < UFT_FBASIC_FAT_SIZE) {
        uint8_t cluster = info->first_cluster;
        size_t clusters = 0;
        int last_sectors = 8;
        
        while (cluster < UFT_FBASIC_FAT_SIZE && clusters < 200) {
            clusters++;
            uint8_t val = fs->fat[UFT_FBASIC_FAT_OFFSET + cluster];
            
            if ((val & UFT_FBASIC_FAT_LAST_MASK) == UFT_FBASIC_FAT_LAST_BASE) {
                last_sectors = val & 0x07;
                if (last_sectors == 0) last_sectors = 8;
                break;
            }
            if (val >= UFT_FBASIC_FAT_SIZE) break;
            cluster = val;
        }
        
        info->size = (clusters > 0) ? 
            ((clusters - 1) * UFT_FBASIC_CLUSTER_SIZE + last_sectors * UFT_FBASIC_SECTOR_SIZE) : 0;
    }
    
    return 0;
}

int uft_fbasic_find_file(uft_fbasic_fs_t *fs, const char *name,
                          uft_fbasic_file_info_t *info) {
    if (!fs || !name) return -1;
    
    char search_name[9];
    trim_name(search_name, name, 8);
    
    for (int i = 0; i < (int)fs->dir_count; i++) {
        uft_fbasic_file_info_t fi;
        if (uft_fbasic_get_file_info(fs, i, &fi) == 0 && !fi.deleted) {
            if (str_casecmp(fi.name, search_name) == 0) {
                if (info) *info = fi;
                return i;
            }
        }
    }
    return -1;
}

size_t uft_fbasic_format_directory(uft_fbasic_fs_t *fs, char *out, size_t out_cap) {
    if (!fs || !out || out_cap == 0) return 0;
    
    size_t pos = 0;
    const char *type_names[] = {"B", "A", "M"};
    
    pos += snprintf(out + pos, out_cap - pos, 
                    "IDX FILENAME  TYP ASC   CLU  SIZE\n");
    pos += snprintf(out + pos, out_cap - pos,
                    "--- --------  --- ---   ---  ----\n");
    
    int file_num = 0;
    for (int i = 0; i < (int)fs->dir_count && pos < out_cap - 60; i++) {
        uft_fbasic_file_info_t fi;
        if (uft_fbasic_get_file_info(fs, i, &fi) == 0 && !fi.deleted && fi.name[0]) {
            const char *type = (fi.file_type < 3) ? type_names[fi.file_type] : "?";
            pos += snprintf(out + pos, out_cap - pos,
                           "%3d %-8s  %s   %s   %3d  %zu\n",
                           file_num++, fi.name, type,
                           fi.is_ascii ? "Y" : "N",
                           fi.first_cluster, fi.size);
        }
    }
    
    pos += snprintf(out + pos, out_cap - pos,
                    "\n%d files, %d clusters free\n",
                    fs->info.file_count, fs->info.free_clusters);
    
    return pos;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * File Operations
 * ═══════════════════════════════════════════════════════════════════════════════ */

int uft_fbasic_read_file(uft_fbasic_fs_t *fs, int index,
                          uint8_t **data, size_t *size) {
    if (!fs || !data || !size) return -1;
    
    uft_fbasic_file_info_t fi;
    if (uft_fbasic_get_file_info(fs, index, &fi) != 0) return -2;
    if (fi.deleted || fi.size == 0) return -3;
    
    *data = (uint8_t *)malloc(fi.size);
    if (!*data) return -4;
    
    uint8_t *ptr = *data;
    size_t remaining = fi.size;
    uint8_t cluster = fi.first_cluster;
    uint8_t sector_buf[UFT_FBASIC_SECTOR_SIZE];
    
    while (cluster < UFT_FBASIC_FAT_SIZE && remaining > 0) {
        uint8_t track, sector;
        uft_fbasic_cluster_to_ts(cluster, &track, &sector);
        
        /* Read cluster (8 sectors) */
        for (int s = 0; s < UFT_FBASIC_CLUSTER_SECTORS && remaining > 0; s++) {
            if (uft_fbasic_read_sector(fs, track, sector + s, sector_buf) != 0) {
                free(*data);
                *data = NULL;
                return -5;
            }
            
            size_t to_copy = (remaining > UFT_FBASIC_SECTOR_SIZE) ? 
                              UFT_FBASIC_SECTOR_SIZE : remaining;
            memcpy(ptr, sector_buf, to_copy);
            ptr += to_copy;
            remaining -= to_copy;
        }
        
        /* Next cluster */
        int next = uft_fbasic_fat_next(fs, cluster);
        if (next < 0) break;
        cluster = (uint8_t)next;
    }
    
    *size = fi.size;
    return 0;
}

int uft_fbasic_read_file_by_name(uft_fbasic_fs_t *fs, const char *name,
                                  uint8_t **data, size_t *size) {
    int idx = uft_fbasic_find_file(fs, name, NULL);
    if (idx < 0) return -1;
    return uft_fbasic_read_file(fs, idx, data, size);
}

int uft_fbasic_write_file(uft_fbasic_fs_t *fs, const char *name,
                           const uint8_t *data, size_t size,
                           uft_fbasic_file_type_t file_type, bool is_ascii) {
    if (!fs || !name || !data) return -1;
    
    /* Calculate clusters needed */
    size_t clusters_needed = (size + UFT_FBASIC_CLUSTER_SIZE - 1) / UFT_FBASIC_CLUSTER_SIZE;
    if (clusters_needed == 0) clusters_needed = 1;
    
    /* Check free space */
    if (clusters_needed > fs->info.free_clusters) return -2;  /* Disk full */
    
    /* Find or create directory entry */
    int dir_idx = uft_fbasic_find_file(fs, name, NULL);
    if (dir_idx >= 0) {
        /* Delete existing file */
        uft_fbasic_delete_file(fs, dir_idx);
    }
    
    /* Find free directory entry */
    dir_idx = -1;
    for (int i = 0; i < (int)fs->dir_count; i++) {
        if (fs->dir[i].name[0] == 0x00 || (uint8_t)fs->dir[i].name[0] == 0xFF) {
            dir_idx = i;
            break;
        }
    }
    if (dir_idx < 0) return -3;  /* Directory full */
    
    /* Allocate clusters */
    uint8_t first_cluster = 0;
    uint8_t prev_cluster = 0;
    const uint8_t *ptr = data;
    size_t remaining = size;
    
    for (size_t c = 0; c < clusters_needed; c++) {
        int cluster = uft_fbasic_fat_alloc(fs);
        if (cluster < 0) {
            if (c > 0) uft_fbasic_fat_free_chain(fs, first_cluster);
            return -4;
        }
        
        if (c == 0) {
            first_cluster = (uint8_t)cluster;
        } else {
            fs->fat[UFT_FBASIC_FAT_OFFSET + prev_cluster] = (uint8_t)cluster;
        }
        
        /* Write cluster data */
        uint8_t track, sector;
        uft_fbasic_cluster_to_ts((uint8_t)cluster, &track, &sector);
        
        int sectors_used = 0;
        uint8_t sector_buf[UFT_FBASIC_SECTOR_SIZE];
        
        for (int s = 0; s < UFT_FBASIC_CLUSTER_SECTORS && remaining > 0; s++) {
            memset(sector_buf, 0, UFT_FBASIC_SECTOR_SIZE);
            size_t to_copy = (remaining > UFT_FBASIC_SECTOR_SIZE) ?
                              UFT_FBASIC_SECTOR_SIZE : remaining;
            memcpy(sector_buf, ptr, to_copy);
            
            uft_fbasic_write_sector(fs, track, sector + s, sector_buf);
            
            ptr += to_copy;
            remaining -= to_copy;
            sectors_used++;
        }
        
        /* Mark last cluster */
        if (remaining == 0) {
            fs->fat[UFT_FBASIC_FAT_OFFSET + cluster] = 
                UFT_FBASIC_FAT_LAST_BASE | (sectors_used & 0x07);
        }
        
        prev_cluster = (uint8_t)cluster;
    }
    
    /* Write directory entry */
    uft_fbasic_dir_entry_t *e = &fs->dir[dir_idx];
    memset(e, 0, sizeof(*e));
    pad_name(e->name, name, 8);
    e->file_type = file_type;
    e->ascii_flag = is_ascii ? UFT_FBASIC_FLAG_ASCII : UFT_FBASIC_FLAG_BINARY;
    e->random_flag = UFT_FBASIC_FLAG_SEQUENTIAL;
    e->first_cluster = first_cluster;
    
    /* Write back FAT and directory */
    uft_fbasic_write_fat(fs);
    
    /* Write directory sector */
    {
        int entry_idx = 0;
        uint8_t sector_buf[UFT_FBASIC_SECTOR_SIZE];
        
        /* Write directory sectors (Track 2, Sector 4-16) */
        for (int s = 4; s <= 16 && entry_idx < UFT_FBASIC_MAX_DIR_ENTRIES; s++) {
            memset(sector_buf, 0, sizeof(sector_buf));
            for (int i = 0; i < UFT_FBASIC_SECTOR_SIZE / UFT_FBASIC_DIR_ENTRY_SIZE
                 && entry_idx < (int)fs->dir_count + 1; i++) {
                memcpy(sector_buf + i * UFT_FBASIC_DIR_ENTRY_SIZE,
                       &fs->dir[entry_idx], UFT_FBASIC_DIR_ENTRY_SIZE);
                entry_idx++;
            }
            uft_fbasic_write_sector(fs, UFT_FBASIC_DIR_TRACK, s, sector_buf);
        }
        
        /* Write directory sectors (Track 3, Sector 1-16) */
        for (int s = 1; s <= 16 && entry_idx < UFT_FBASIC_MAX_DIR_ENTRIES; s++) {
            memset(sector_buf, 0, sizeof(sector_buf));
            for (int i = 0; i < UFT_FBASIC_SECTOR_SIZE / UFT_FBASIC_DIR_ENTRY_SIZE
                 && entry_idx < (int)fs->dir_count + 1; i++) {
                memcpy(sector_buf + i * UFT_FBASIC_DIR_ENTRY_SIZE,
                       &fs->dir[entry_idx], UFT_FBASIC_DIR_ENTRY_SIZE);
                entry_idx++;
            }
            uft_fbasic_write_sector(fs, UFT_FBASIC_DIR_TRACK + 1, s, sector_buf);
        }
    }
    
    fs->modified = true;
    return 0;
}

int uft_fbasic_delete_file(uft_fbasic_fs_t *fs, int index) {
    if (!fs || index < 0 || index >= (int)fs->dir_count) return -1;
    
    uft_fbasic_dir_entry_t *e = &fs->dir[index];
    if (e->name[0] == 0x00) return 0;  /* Already deleted */
    
    /* Free cluster chain */
    if (e->first_cluster < UFT_FBASIC_FAT_SIZE) {
        uft_fbasic_fat_free_chain(fs, e->first_cluster);
    }
    
    /* Mark entry as deleted */
    e->name[0] = 0x00;
    
    /* Write FAT */
    uft_fbasic_write_fat(fs);
    
    fs->modified = true;
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Disk Info
 * ═══════════════════════════════════════════════════════════════════════════════ */

int uft_fbasic_get_info(uft_fbasic_fs_t *fs, uft_fbasic_disk_info_t *info) {
    if (!fs || !info) return -1;
    
    memset(info, 0, sizeof(*info));
    
    /* Read ID sector */
    uint8_t sector[UFT_FBASIC_SECTOR_SIZE];
    if (uft_fbasic_read_sector(fs, 0, 3, sector) == 0) {
        memcpy(info->id_string, sector, 3);
        info->id_string[3] = '\0';
    }
    
    /* Count clusters */
    info->total_clusters = UFT_FBASIC_FAT_SIZE;
    for (int i = 0; i < UFT_FBASIC_FAT_SIZE; i++) {
        uint8_t val = fs->fat[UFT_FBASIC_FAT_OFFSET + i];
        if (val == UFT_FBASIC_FAT_FREE) {
            info->free_clusters++;
        } else if (val != UFT_FBASIC_FAT_RESERVED && val != UFT_FBASIC_FAT_UNUSED) {
            info->used_clusters++;
        }
    }
    
    /* Count files */
    for (int i = 0; i < (int)fs->dir_count; i++) {
        if (fs->dir[i].name[0] != 0x00 && (uint8_t)fs->dir[i].name[0] != 0xFF) {
            info->file_count++;
        }
    }
    
    info->disk_type = (fs->tracks >= 80) ? 0x10 : 0x00;
    
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Format Disk
 * ═══════════════════════════════════════════════════════════════════════════════ */

int uft_fbasic_format(uft_fbasic_fs_t *fs, const char *disk_name) {
    if (!fs || !fs->disk_data) return -1;
    
    /* Clear disk */
    memset(fs->disk_data, 0x00, fs->disk_size);
    
    /* Write ID sector (Track 0, Sector 3) */
    uint8_t sector[UFT_FBASIC_SECTOR_SIZE];
    memset(sector, 0, sizeof(sector));
    memcpy(sector, "SYS", 3);
    if (disk_name) {
        size_t len = strlen(disk_name);
        if (len > 16) len = 16;
        memcpy(sector + 4, disk_name, len);
    }
    uft_fbasic_write_sector(fs, 0, 3, sector);
    
    /* Initialize FAT */
    memset(fs->fat, 0xFF, sizeof(fs->fat));
    
    /* Reserve system clusters */
    for (int i = 0; i < 5; i++) {
        fs->fat[i] = 0x00;  /* Reserved header bytes */
    }
    
    uft_fbasic_write_fat(fs);
    
    /* Initialize directory */
    fs->dir_count = UFT_FBASIC_MAX_DIR_ENTRIES;
    memset(fs->dir, 0, sizeof(fs->dir));
    
    fs->modified = true;
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * BASIC Decoder (Stub - FM-7 BASIC tokens)
 * ═══════════════════════════════════════════════════════════════════════════════ */

/* FM-7 F-BASIC Token Table (partial) */
static const char *fbasic_tokens[] = {
    /* 0x80 - 0x8F */
    "END", "FOR", "NEXT", "DATA", "INPUT", "DIM", "READ", "LET",
    "GOTO", "RUN", "IF", "RESTORE", "GOSUB", "RETURN", "REM", "STOP",
    /* 0x90 - 0x9F */
    "ON", "LPRINT", "DEF", "POKE", "PRINT", "CONT", "LIST", "LLIST",
    "CLEAR", "NEW", "EXEC", "CLOAD", "CSAVE", "OPEN", "CLOSE", "MOTOR",
    /* ... more tokens */
};

int uft_fbasic_decode_basic(const uint8_t *tokens, size_t token_size,
                             char *text, size_t text_cap) {
    if (!tokens || !text || text_cap == 0) return -1;
    
    size_t pos = 0;
    size_t i = 0;
    
    while (i < token_size && pos < text_cap - 10) {
        /* Line format: 2-byte link, 2-byte line number, tokens..., 0x00 */
        if (tokens[i] == 0x00 && tokens[i+1] == 0x00) {
            break;  /* End of program */
        }
        
        /* Skip link pointer */
        i += 2;
        if (i + 2 > token_size) break;
        
        /* Line number (little-endian) */
        uint16_t line_num = tokens[i] | (tokens[i+1] << 8);
        i += 2;
        
        pos += snprintf(text + pos, text_cap - pos, "%d ", line_num);
        
        /* Decode tokens until 0x00 */
        while (i < token_size && tokens[i] != 0x00 && pos < text_cap - 2) {
            uint8_t c = tokens[i++];
            
            if (c >= 0x80 && c <= 0x9F) {
                /* Token */
                int tok_idx = c - 0x80;
                if (tok_idx < (int)(sizeof(fbasic_tokens)/sizeof(fbasic_tokens[0]))) {
                    pos += snprintf(text + pos, text_cap - pos, "%s", fbasic_tokens[tok_idx]);
                } else {
                    pos += snprintf(text + pos, text_cap - pos, "<%02X>", c);
                }
            } else if (c >= 0x20 && c < 0x7F) {
                /* ASCII */
                text[pos++] = c;
            } else {
                /* Other */
                pos += snprintf(text + pos, text_cap - pos, "<%02X>", c);
            }
        }
        
        /* Skip end-of-line marker */
        if (i < token_size && tokens[i] == 0x00) i++;
        
        text[pos++] = '\n';
    }
    
    text[pos] = '\0';
    return (int)pos;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Motorola S-Record Conversion
 * ═══════════════════════════════════════════════════════════════════════════════ */

int uft_fbasic_to_srec(const uint8_t *data, size_t size, uint16_t load_addr,
                        char *srec, size_t srec_cap) {
    if (!data || !srec || srec_cap == 0) return -1;
    
    size_t pos = 0;
    size_t offset = 0;
    
    /* S0 header record */
    pos += snprintf(srec + pos, srec_cap - pos, "S00600004844521B\n");
    
    /* S1 data records (16 bytes per record) */
    while (offset < size && pos < srec_cap - 50) {
        size_t chunk = (size - offset > 16) ? 16 : (size - offset);
        uint16_t addr = load_addr + (uint16_t)offset;
        uint8_t count = (uint8_t)(chunk + 3);  /* data + addr (2) + checksum (1) */
        
        uint8_t checksum = count + (addr >> 8) + (addr & 0xFF);
        
        pos += snprintf(srec + pos, srec_cap - pos, "S1%02X%04X", count, addr);
        
        for (size_t i = 0; i < chunk; i++) {
            pos += snprintf(srec + pos, srec_cap - pos, "%02X", data[offset + i]);
            checksum += data[offset + i];
        }
        
        checksum = ~checksum;
        pos += snprintf(srec + pos, srec_cap - pos, "%02X\n", checksum);
        
        offset += chunk;
    }
    
    /* S9 end record */
    uint8_t s9_checksum = ~(0x03 + (load_addr >> 8) + (load_addr & 0xFF));
    pos += snprintf(srec + pos, srec_cap - pos, "S903%04X%02X\n", load_addr, s9_checksum);
    
    return (int)pos;
}
