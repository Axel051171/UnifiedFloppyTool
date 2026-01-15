/**
 * @file uft_fat_editor.c
 * @brief FAT Filesystem Editor Implementation
 * 
 * @version 1.0.0
 * @date 2026-01-15
 */

#include "uft/uft_fat_editor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/*===========================================================================
 * INTERNAL STRUCTURES
 *===========================================================================*/

struct uft_fat_context {
    uint8_t *image;             /**< Image data */
    size_t image_size;          /**< Image size */
    bool owns_image;            /**< True if we allocated image */
    
    uft_fat_type_t type;
    
    /* Cached BPB values */
    uint32_t bytes_per_sector;
    uint32_t sectors_per_cluster;
    uint32_t reserved_sectors;
    uint32_t num_fats;
    uint32_t root_entry_count;
    uint32_t total_sectors;
    uint32_t fat_size;          /**< Sectors per FAT */
    uint32_t root_cluster;      /**< FAT32 root cluster */
    
    /* Calculated values */
    uint32_t fat_start;         /**< First FAT sector */
    uint32_t root_start;        /**< Root dir sector (FAT12/16) */
    uint32_t data_start;        /**< First data sector */
    uint32_t total_clusters;
    uint32_t bytes_per_cluster;
    
    char volume_label[12];
    bool modified;
};

/*===========================================================================
 * FAT12/16/32 CLUSTER ACCESS
 *===========================================================================*/

static uint32_t fat12_get_entry(const uint8_t *fat, uint32_t cluster) {
    uint32_t offset = cluster + (cluster / 2);
    uint16_t val = fat[offset] | (fat[offset + 1] << 8);
    
    if (cluster & 1) {
        return val >> 4;
    } else {
        return val & 0x0FFF;
    }
}

static void fat12_set_entry(uint8_t *fat, uint32_t cluster, uint32_t value) {
    uint32_t offset = cluster + (cluster / 2);
    uint16_t val = fat[offset] | (fat[offset + 1] << 8);
    
    if (cluster & 1) {
        val = (val & 0x000F) | ((value & 0x0FFF) << 4);
    } else {
        val = (val & 0xF000) | (value & 0x0FFF);
    }
    
    fat[offset] = val & 0xFF;
    fat[offset + 1] = (val >> 8) & 0xFF;
}

static uint32_t fat16_get_entry(const uint8_t *fat, uint32_t cluster) {
    uint32_t offset = cluster * 2;
    return fat[offset] | (fat[offset + 1] << 8);
}

static void fat16_set_entry(uint8_t *fat, uint32_t cluster, uint32_t value) {
    uint32_t offset = cluster * 2;
    fat[offset] = value & 0xFF;
    fat[offset + 1] = (value >> 8) & 0xFF;
}

static uint32_t fat32_get_entry(const uint8_t *fat, uint32_t cluster) {
    uint32_t offset = cluster * 4;
    return (fat[offset] | (fat[offset + 1] << 8) |
            (fat[offset + 2] << 16) | (fat[offset + 3] << 24)) & 0x0FFFFFFF;
}

static void fat32_set_entry(uint8_t *fat, uint32_t cluster, uint32_t value) {
    uint32_t offset = cluster * 4;
    uint32_t existing = fat[offset + 3] & 0xF0;  /* Preserve high 4 bits */
    fat[offset] = value & 0xFF;
    fat[offset + 1] = (value >> 8) & 0xFF;
    fat[offset + 2] = (value >> 16) & 0xFF;
    fat[offset + 3] = ((value >> 24) & 0x0F) | existing;
}

/*===========================================================================
 * INTERNAL HELPERS
 *===========================================================================*/

static uint8_t* get_fat_ptr(uft_fat_t *fat, int fat_num) {
    if (!fat || fat_num >= (int)fat->num_fats) return NULL;
    uint32_t offset = (fat->fat_start + fat_num * fat->fat_size) * 
                      fat->bytes_per_sector;
    return fat->image + offset;
}

static uint32_t cluster_to_sector(uft_fat_t *fat, uint32_t cluster) {
    return fat->data_start + (cluster - 2) * fat->sectors_per_cluster;
}

static uft_fat_type_t detect_fat_type(uint32_t total_clusters) {
    if (total_clusters < 4085) return UFT_FAT_12;
    if (total_clusters < 65525) return UFT_FAT_16;
    return UFT_FAT_32;
}

/*===========================================================================
 * LIFECYCLE
 *===========================================================================*/

bool uft_fat_probe(const uint8_t *image, size_t size) {
    if (!image || size < 512) return false;
    
    /* Check boot signature */
    if (image[510] != 0x55 || image[511] != 0xAA) return false;
    
    /* Check jump instruction */
    if (image[0] != 0xEB && image[0] != 0xE9) return false;
    
    /* Check bytes per sector */
    uint16_t bps = image[11] | (image[12] << 8);
    if (bps != 512 && bps != 1024 && bps != 2048 && bps != 4096) return false;
    
    /* Check sectors per cluster (power of 2) */
    uint8_t spc = image[13];
    if (spc == 0 || (spc & (spc - 1)) != 0) return false;
    
    return true;
}

uft_fat_t* uft_fat_open(const uint8_t *image, size_t size) {
    if (!uft_fat_probe(image, size)) return NULL;
    
    uft_fat_t *fat = (uft_fat_t *)calloc(1, sizeof(*fat));
    if (!fat) return NULL;
    
    /* Copy image */
    fat->image = (uint8_t *)malloc(size);
    if (!fat->image) {
        free(fat);
        return NULL;
    }
    memcpy(fat->image, image, size);
    fat->image_size = size;
    fat->owns_image = true;
    
    /* Parse BPB */
    fat->bytes_per_sector = image[11] | (image[12] << 8);
    fat->sectors_per_cluster = image[13];
    fat->reserved_sectors = image[14] | (image[15] << 8);
    fat->num_fats = image[16];
    fat->root_entry_count = image[17] | (image[18] << 8);
    
    uint16_t total16 = image[19] | (image[20] << 8);
    uint32_t total32 = image[32] | (image[33] << 8) | 
                       (image[34] << 16) | (image[35] << 24);
    fat->total_sectors = total16 ? total16 : total32;
    
    uint16_t fat_size16 = image[22] | (image[23] << 8);
    uint32_t fat_size32 = image[36] | (image[37] << 8) |
                          (image[38] << 16) | (image[39] << 24);
    fat->fat_size = fat_size16 ? fat_size16 : fat_size32;
    
    /* Calculate layout */
    fat->fat_start = fat->reserved_sectors;
    
    uint32_t root_sectors = ((fat->root_entry_count * 32) + 
                             fat->bytes_per_sector - 1) / fat->bytes_per_sector;
    
    fat->root_start = fat->fat_start + fat->num_fats * fat->fat_size;
    fat->data_start = fat->root_start + root_sectors;
    
    uint32_t data_sectors = fat->total_sectors - fat->data_start;
    fat->total_clusters = data_sectors / fat->sectors_per_cluster;
    fat->bytes_per_cluster = fat->bytes_per_sector * fat->sectors_per_cluster;
    
    /* Determine FAT type */
    fat->type = detect_fat_type(fat->total_clusters);
    
    if (fat->type == UFT_FAT_32) {
        fat->root_cluster = image[44] | (image[45] << 8) |
                            (image[46] << 16) | (image[47] << 24);
        /* Copy volume label from FAT32 boot sector */
        memcpy(fat->volume_label, image + 71, 11);
    } else {
        fat->root_cluster = 0;
        /* Copy volume label from FAT12/16 boot sector */
        memcpy(fat->volume_label, image + 43, 11);
    }
    fat->volume_label[11] = '\0';
    
    return fat;
}

uft_fat_t* uft_fat_open_file(const char *path) {
    if (!path) return NULL;
    
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    if (size <= 0 || size > 2LL * 1024 * 1024 * 1024) {
        fclose(f);
        return NULL;
    }
    
    uint8_t *data = (uint8_t *)malloc(size);
    if (!data) {
        fclose(f);
        return NULL;
    }
    
    fread(data, 1, size, f);
    fclose(f);
    
    uft_fat_t *fat = uft_fat_open(data, size);
    free(data);
    
    return fat;
}

void uft_fat_close(uft_fat_t *fat) {
    if (!fat) return;
    
    if (fat->owns_image && fat->image) {
        free(fat->image);
    }
    free(fat);
}

/*===========================================================================
 * INFORMATION
 *===========================================================================*/

uft_fat_type_t uft_fat_get_type(uft_fat_t *fat) {
    return fat ? fat->type : UFT_FAT_UNKNOWN;
}

const char* uft_fat_type_name(uft_fat_type_t type) {
    switch (type) {
        case UFT_FAT_12: return "FAT12";
        case UFT_FAT_16: return "FAT16";
        case UFT_FAT_32: return "FAT32";
        default: return "Unknown";
    }
}

int uft_fat_get_stats(uft_fat_t *fat, uft_fat_stats_t *stats) {
    if (!fat || !stats) return -1;
    
    memset(stats, 0, sizeof(*stats));
    stats->type = fat->type;
    stats->total_clusters = fat->total_clusters;
    stats->bytes_per_cluster = fat->bytes_per_cluster;
    stats->total_size = (uint64_t)fat->total_clusters * fat->bytes_per_cluster;
    
    /* Count cluster usage */
    for (uint32_t c = 2; c < fat->total_clusters + 2; c++) {
        uint32_t val = uft_fat_get_cluster(fat, c);
        
        if (val == 0) {
            stats->free_clusters++;
        } else if ((fat->type == UFT_FAT_12 && val == 0xFF7) ||
                   (fat->type == UFT_FAT_16 && val == 0xFFF7) ||
                   (fat->type == UFT_FAT_32 && val == 0x0FFFFFF7)) {
            stats->bad_clusters++;
        } else if (val == 1) {
            stats->reserved_clusters++;
        } else {
            stats->used_clusters++;
        }
    }
    
    stats->free_size = (uint64_t)stats->free_clusters * fat->bytes_per_cluster;
    
    return 0;
}

const char* uft_fat_get_volume_label(uft_fat_t *fat) {
    return fat ? fat->volume_label : NULL;
}

/*===========================================================================
 * CLUSTER OPERATIONS
 *===========================================================================*/

uint32_t uft_fat_get_cluster(uft_fat_t *fat, uint32_t cluster) {
    if (!fat || cluster < 2 || cluster >= fat->total_clusters + 2) return 0;
    
    uint8_t *fat_ptr = get_fat_ptr(fat, 0);
    if (!fat_ptr) return 0;
    
    switch (fat->type) {
        case UFT_FAT_12: return fat12_get_entry(fat_ptr, cluster);
        case UFT_FAT_16: return fat16_get_entry(fat_ptr, cluster);
        case UFT_FAT_32: return fat32_get_entry(fat_ptr, cluster);
        default: return 0;
    }
}

int uft_fat_set_cluster(uft_fat_t *fat, uint32_t cluster, uint32_t value) {
    if (!fat || cluster < 2 || cluster >= fat->total_clusters + 2) return -1;
    
    /* Update all FAT copies */
    for (uint32_t i = 0; i < fat->num_fats; i++) {
        uint8_t *fat_ptr = get_fat_ptr(fat, i);
        if (!fat_ptr) continue;
        
        switch (fat->type) {
            case UFT_FAT_12: fat12_set_entry(fat_ptr, cluster, value); break;
            case UFT_FAT_16: fat16_set_entry(fat_ptr, cluster, value); break;
            case UFT_FAT_32: fat32_set_entry(fat_ptr, cluster, value); break;
            default: return -1;
        }
    }
    
    fat->modified = true;
    return 0;
}

uft_cluster_status_t uft_fat_get_cluster_status(uft_fat_t *fat, uint32_t cluster) {
    if (!fat) return UFT_CLUSTER_FREE;
    
    uint32_t val = uft_fat_get_cluster(fat, cluster);
    
    if (val == 0) return UFT_CLUSTER_FREE;
    
    uint32_t bad_marker, end_marker;
    switch (fat->type) {
        case UFT_FAT_12: bad_marker = 0xFF7; end_marker = 0xFF8; break;
        case UFT_FAT_16: bad_marker = 0xFFF7; end_marker = 0xFFF8; break;
        case UFT_FAT_32: bad_marker = 0x0FFFFFF7; end_marker = 0x0FFFFFF8; break;
        default: return UFT_CLUSTER_FREE;
    }
    
    if (val == bad_marker) return UFT_CLUSTER_BAD;
    if (val >= end_marker) return UFT_CLUSTER_END;
    if (val == 1) return UFT_CLUSTER_RESERVED;
    
    return UFT_CLUSTER_USED;
}

int uft_fat_mark_bad(uft_fat_t *fat, uint32_t cluster) {
    uint32_t bad_marker;
    switch (fat->type) {
        case UFT_FAT_12: bad_marker = 0xFF7; break;
        case UFT_FAT_16: bad_marker = 0xFFF7; break;
        case UFT_FAT_32: bad_marker = 0x0FFFFFF7; break;
        default: return -1;
    }
    return uft_fat_set_cluster(fat, cluster, bad_marker);
}

int uft_fat_mark_free(uft_fat_t *fat, uint32_t cluster) {
    return uft_fat_set_cluster(fat, cluster, 0);
}

int uft_fat_get_chain(uft_fat_t *fat, uint32_t start_cluster,
                       uft_cluster_chain_t *chain) {
    if (!fat || !chain || start_cluster < 2) return -1;
    
    memset(chain, 0, sizeof(*chain));
    chain->start_cluster = start_cluster;
    
    /* First pass: count clusters */
    int count = 0;
    uint32_t c = start_cluster;
    while (c >= 2 && c < fat->total_clusters + 2 && count < 100000) {
        count++;
        uint32_t next = uft_fat_get_cluster(fat, c);
        if (uft_fat_get_cluster_status(fat, c) == UFT_CLUSTER_END) break;
        c = next;
    }
    
    /* Allocate */
    chain->clusters = (uint32_t *)malloc(count * sizeof(uint32_t));
    if (!chain->clusters) return -1;
    
    /* Second pass: fill array */
    c = start_cluster;
    chain->cluster_count = 0;
    while (c >= 2 && c < fat->total_clusters + 2 && 
           chain->cluster_count < count) {
        chain->clusters[chain->cluster_count++] = c;
        if (uft_fat_get_cluster_status(fat, c) == UFT_CLUSTER_END) break;
        c = uft_fat_get_cluster(fat, c);
    }
    
    return 0;
}

void uft_fat_free_chain(uft_cluster_chain_t *chain) {
    if (chain) {
        free(chain->clusters);
        memset(chain, 0, sizeof(*chain));
    }
}

int uft_fat_read_cluster(uft_fat_t *fat, uint32_t cluster,
                          uint8_t *buffer, size_t size) {
    if (!fat || !buffer || cluster < 2) return -1;
    
    uint32_t sector = cluster_to_sector(fat, cluster);
    uint32_t offset = sector * fat->bytes_per_sector;
    
    size_t to_read = fat->bytes_per_cluster;
    if (to_read > size) to_read = size;
    if (offset + to_read > fat->image_size) return -1;
    
    memcpy(buffer, fat->image + offset, to_read);
    return (int)to_read;
}

/*===========================================================================
 * DIRECTORY OPERATIONS
 *===========================================================================*/

void uft_fat_name_to_string(const uft_fat_dirent_t *entry, 
                             char *name, size_t size) {
    if (!entry || !name || size == 0) return;
    
    /* Check for deleted or empty */
    if (entry->name[0] == 0x00 || entry->name[0] == (char)0xE5) {
        name[0] = '\0';
        return;
    }
    
    /* Copy name, trim trailing spaces */
    int i = 7;
    while (i >= 0 && entry->name[i] == ' ') i--;
    int name_len = i + 1;
    
    /* Copy extension, trim trailing spaces */
    i = 2;
    while (i >= 0 && entry->ext[i] == ' ') i--;
    int ext_len = i + 1;
    
    int pos = 0;
    for (i = 0; i < name_len && pos < (int)size - 1; i++) {
        name[pos++] = entry->name[i];
    }
    
    if (ext_len > 0 && pos < (int)size - 1) {
        name[pos++] = '.';
        for (i = 0; i < ext_len && pos < (int)size - 1; i++) {
            name[pos++] = entry->ext[i];
        }
    }
    
    name[pos] = '\0';
}

int uft_fat_read_root(uft_fat_t *fat, uft_fat_file_info_t *entries,
                       int max_entries) {
    if (!fat || !entries || max_entries <= 0) return -1;
    
    int count = 0;
    
    if (fat->type == UFT_FAT_32) {
        /* FAT32: root is a cluster chain - read from root_cluster */
        /* Simplified: just read first cluster */
        uint8_t *buffer = (uint8_t *)malloc(fat->bytes_per_cluster);
        if (!buffer) return -1;
        
        if (uft_fat_read_cluster(fat, fat->root_cluster, buffer, 
                                  fat->bytes_per_cluster) > 0) {
            int entries_per_cluster = fat->bytes_per_cluster / 32;
            for (int i = 0; i < entries_per_cluster && count < max_entries; i++) {
                uft_fat_dirent_t *de = (uft_fat_dirent_t *)(buffer + i * 32);
                
                if (de->name[0] == 0x00) break;
                if (de->name[0] == (char)0xE5) continue;
                if (de->attributes == UFT_ATTR_LFN) continue;
                
                uft_fat_file_info_t *info = &entries[count];
                memset(info, 0, sizeof(*info));
                
                uft_fat_name_to_string(de, info->short_name, sizeof(info->short_name));
                info->attributes = de->attributes;
                info->first_cluster = de->first_cluster_lo | 
                                      ((uint32_t)de->first_cluster_hi << 16);
                info->file_size = de->file_size;
                count++;
            }
        }
        free(buffer);
        return count;
    }
    
    /* FAT12/16: root is fixed location */
    uint32_t offset = fat->root_start * fat->bytes_per_sector;
    
    for (uint32_t i = 0; i < fat->root_entry_count && count < max_entries; i++) {
        uft_fat_dirent_t *de = (uft_fat_dirent_t *)(fat->image + offset + i * 32);
        
        if (de->name[0] == 0x00) break;  /* End of directory */
        if (de->name[0] == (char)0xE5) continue;  /* Deleted */
        if (de->attributes == UFT_ATTR_LFN) continue;  /* LFN entry */
        
        uft_fat_file_info_t *info = &entries[count];
        memset(info, 0, sizeof(*info));
        
        uft_fat_name_to_string(de, info->short_name, sizeof(info->short_name));
        info->attributes = de->attributes;
        info->first_cluster = de->first_cluster_lo | 
                              ((uint32_t)de->first_cluster_hi << 16);
        info->file_size = de->file_size;
        info->create_date = de->create_date;
        info->create_time = de->create_time;
        info->modify_date = de->modify_date;
        info->modify_time = de->modify_time;
        
        count++;
    }
    
    return count;
}

/*===========================================================================
 * UTILITIES
 *===========================================================================*/

void uft_fat_decode_date(uint16_t date, int *year, int *month, int *day) {
    if (year) *year = ((date >> 9) & 0x7F) + 1980;
    if (month) *month = (date >> 5) & 0x0F;
    if (day) *day = date & 0x1F;
}

void uft_fat_decode_time(uint16_t time, int *hour, int *minute, int *second) {
    if (hour) *hour = (time >> 11) & 0x1F;
    if (minute) *minute = (time >> 5) & 0x3F;
    if (second) *second = (time & 0x1F) * 2;
}

uint16_t uft_fat_encode_date(int year, int month, int day) {
    return ((year - 1980) << 9) | (month << 5) | day;
}

uint16_t uft_fat_encode_time(int hour, int minute, int second) {
    return (hour << 11) | (minute << 5) | (second / 2);
}

void uft_fat_print_summary(uft_fat_t *fat) {
    if (!fat) return;
    
    uft_fat_stats_t stats;
    uft_fat_get_stats(fat, &stats);
    
    printf("FAT Filesystem Summary:\n");
    printf("  Type: %s\n", uft_fat_type_name(fat->type));
    printf("  Volume Label: %.11s\n", fat->volume_label);
    printf("  Bytes/Sector: %u\n", fat->bytes_per_sector);
    printf("  Sectors/Cluster: %u\n", fat->sectors_per_cluster);
    printf("  Total Clusters: %u\n", stats.total_clusters);
    printf("  Free Clusters: %u\n", stats.free_clusters);
    printf("  Bad Clusters: %u\n", stats.bad_clusters);
    printf("  Total Size: %llu bytes\n", (unsigned long long)stats.total_size);
    printf("  Free Size: %llu bytes\n", (unsigned long long)stats.free_size);
}

void uft_fat_print_chain(uft_fat_t *fat, uint32_t start_cluster) {
    if (!fat) return;
    
    printf("Cluster chain starting at %u:\n  ", start_cluster);
    
    uft_cluster_chain_t chain;
    if (uft_fat_get_chain(fat, start_cluster, &chain) == 0) {
        for (int i = 0; i < chain.cluster_count; i++) {
            printf("%u", chain.clusters[i]);
            if (i < chain.cluster_count - 1) printf(" -> ");
            if ((i + 1) % 10 == 0) printf("\n  ");
        }
        printf(" [END]\n");
        uft_fat_free_chain(&chain);
    }
}
