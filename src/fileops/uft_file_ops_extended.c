/**
 * @file uft_file_ops_extended.c
 * @brief Extended File Operations - SSD/DSD, FAT12, D81, MSA
 * @version 5.31.0
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include "uft_file_ops.h"

/* ============================================================================
 * SSD/DSD BBC Micro File Operations (Acorn DFS)
 * ============================================================================ */

#define DFS_SECTOR_SIZE     256
#define DFS_SECTORS_TRACK   10

/**
 * @brief List files on SSD/DSD image
 */
int ssd_list_files(const uint8_t *image, size_t size, uft_directory_t *dir)
{
    if (!image || !dir) return -1;
    
    memset(dir, 0, sizeof(*dir));
    
    /* Catalog is in sectors 0 and 1 */
    const uint8_t *cat0 = image;
    const uint8_t *cat1 = image + DFS_SECTOR_SIZE;
    
    /* Disk title (first 8 chars in sector 0, next 4 in sector 1) */
    memcpy(dir->disk_name, cat0, 8);
    memcpy(dir->disk_name + 8, cat1, 4);
    dir->disk_name[12] = '\0';
    
    /* Trim trailing spaces */
    for (int i = 11; i >= 0 && dir->disk_name[i] == ' '; i--) {
        dir->disk_name[i] = '\0';
    }
    
    /* Number of files (at cat1[5], stored as count * 8) */
    int file_count = cat1[5] / 8;
    
    /* Total sectors */
    int total_sectors = ((cat1[6] & 0x03) << 8) | cat1[7];
    dir->total_blocks = total_sectors;
    
    /* Read directory entries */
    for (int i = 0; i < file_count && i < 31 && dir->count < UFT_MAX_FILES; i++) {
        uft_file_entry_t *f = &dir->files[dir->count];
        
        /* Filename in sector 0 (at offset 8 + i*8) */
        int name_off = 8 + i * 8;
        memcpy(f->name, cat0 + name_off, 7);
        f->name[7] = '\0';
        
        /* Trim and add directory char */
        for (int j = 6; j >= 0 && f->name[j] == ' '; j--) f->name[j] = '\0';
        
        char dir_char = cat0[name_off + 7] & 0x7F;
        if (dir_char != '$' && dir_char != ' ') {
            /* Prepend directory */
            char tmp[UFT_MAX_FILENAME];
            snprintf(tmp, sizeof(tmp), "%c.%s", dir_char, f->name);
            strncpy(f->name, tmp, sizeof(f->name)-1); f->name[sizeof(f->name)-1] = '\0';
        }
        
        f->locked = (cat0[name_off + 7] & 0x80) != 0;
        
        /* File info in sector 1 (at offset 8 + i*8) */
        int info_off = 8 + i * 8;
        
        f->load_addr = cat1[info_off] | (cat1[info_off + 1] << 8);
        f->exec_addr = cat1[info_off + 2] | (cat1[info_off + 3] << 8);
        f->size = cat1[info_off + 4] | (cat1[info_off + 5] << 8);
        
        uint8_t extra = cat1[info_off + 6];
        f->load_addr |= ((extra & 0x0C) >> 2) << 16;
        f->exec_addr |= ((extra & 0xC0) >> 6) << 16;
        f->size |= ((extra & 0x30) >> 4) << 16;
        
        f->start_sector = cat1[info_off + 7];
        f->start_sector |= ((extra & 0x03)) << 8;
        
        f->blocks = (f->size + 255) / 256;
        f->type = UFT_FTYPE_BINARY;
        
        dir->count++;
    }
    
    /* Calculate free blocks */
    int used = 2;  /* Catalog */
    for (int i = 0; i < dir->count; i++) {
        used += dir->files[i].blocks;
    }
    dir->free_blocks = dir->total_blocks - used;
    
    return 0;
}

/**
 * @brief Extract file from SSD/DSD
 */
int ssd_extract_file(const uint8_t *image, size_t img_size,
                     const char *filename, uint8_t **data, size_t *size)
{
    if (!image || !filename || !data || !size) return -1;
    
    uft_directory_t dir;
    if (ssd_list_files(image, img_size, &dir) != 0) return -1;
    
    /* Find file */
    int found = -1;
    for (int i = 0; i < dir.count; i++) {
        if (strcasecmp(dir.files[i].name, filename) == 0) {
            found = i;
            break;
        }
    }
    
    if (found < 0) return -1;
    
    uft_file_entry_t *f = &dir.files[found];
    
    uint8_t *buf = malloc(f->size);
    if (!buf) return -1;
    
    /* DFS files are stored contiguously */
    size_t offset = f->start_sector * DFS_SECTOR_SIZE;
    if (offset + f->size > img_size) {
        free(buf);
        return -1;
    }
    
    memcpy(buf, image + offset, f->size);
    
    *data = buf;
    *size = f->size;
    
    return 0;
}

/* ============================================================================
 * D81 Commodore 1581 File Operations
 * ============================================================================ */

#define D81_SECTOR_SIZE     256
#define D81_TRACKS          80
#define D81_SECTORS         40
#define D81_HEADER_TRACK    40
#define D81_HEADER_SECTOR   0
#define D81_DIR_TRACK       40
#define D81_DIR_SECTOR      3

/**
 * @brief Get D81 sector offset
 */
static size_t d81_sector_offset(int track, int sector)
{
    return ((track - 1) * D81_SECTORS + sector) * D81_SECTOR_SIZE;
}

/**
 * @brief List files on D81 image
 */
int d81_list_files(const uint8_t *image, size_t size, uft_directory_t *dir)
{
    if (!image || !dir || size < 819200) return -1;
    
    memset(dir, 0, sizeof(*dir));
    
    /* Header at track 40, sector 0 */
    size_t header_off = d81_sector_offset(D81_HEADER_TRACK, D81_HEADER_SECTOR);
    const uint8_t *header = image + header_off;
    
    /* Disk name at offset 4 */
    char name[17];
    memcpy(name, header + 4, 16);
    name[16] = '\0';
    
    /* Convert PETSCII and trim */
    for (int i = 0; i < 16; i++) {
        if (name[i] == 0xA0) name[i] = ' ';
        else if (name[i] >= 0xC1 && name[i] <= 0xDA) name[i] -= 0x80;
    }
    for (int i = 15; i >= 0 && name[i] == ' '; i--) name[i] = '\0';
    strncpy(dir->disk_name, name, 16); dir->disk_name[16] = '\0';
    
    /* Disk ID at offset 22 */
    memcpy(dir->disk_id, header + 22, 2);
    dir->disk_id[2] = '\0';
    
    /* BAM at track 40, sectors 1-2 */
    dir->free_blocks = 0;
    for (int s = 1; s <= 2; s++) {
        size_t bam_off = d81_sector_offset(D81_HEADER_TRACK, s);
        const uint8_t *bam = image + bam_off;
        
        for (int t = 0; t < 40; t++) {
            int offset = 16 + t * 6;
            dir->free_blocks += bam[offset];
        }
    }
    dir->total_blocks = 3160;  /* 80 tracks × 40 sectors - reserved */
    
    /* Directory at track 40, sector 3 onwards */
    int dir_track = D81_DIR_TRACK;
    int dir_sector = D81_DIR_SECTOR;
    
    while (dir_track != 0 && dir->count < UFT_MAX_FILES) {
        size_t offset = d81_sector_offset(dir_track, dir_sector);
        const uint8_t *sect = image + offset;
        
        for (int e = 0; e < 8 && dir->count < UFT_MAX_FILES; e++) {
            const uint8_t *entry = sect + e * 32;
            uint8_t ftype = entry[2];
            
            if (ftype == 0x00) continue;
            
            uft_file_entry_t *f = &dir->files[dir->count];
            
            f->raw_type = ftype;
            f->deleted = (ftype & 0x80) == 0;
            f->locked = (ftype & 0x40) != 0;
            
            switch (ftype & 0x07) {
                case 0x01: f->type = UFT_FTYPE_SEQ; break;
                case 0x02: f->type = UFT_FTYPE_PRG; break;
                case 0x03: f->type = UFT_FTYPE_USR; break;
                case 0x04: f->type = UFT_FTYPE_REL; break;
                case 0x06: f->type = UFT_FTYPE_DIR; break;
                default:   f->type = UFT_FTYPE_UNKNOWN; break;
            }
            
            f->start_track = entry[3];
            f->start_sector = entry[4];
            
            /* Filename */
            char fname[17];
            memcpy(fname, entry + 5, 16);
            fname[16] = '\0';
            for (int i = 0; i < 16; i++) {
                if (fname[i] == 0xA0) fname[i] = ' ';
                else if (fname[i] >= 0xC1 && fname[i] <= 0xDA) fname[i] -= 0x80;
            }
            for (int i = 15; i >= 0 && fname[i] == ' '; i--) fname[i] = '\0';
            strncpy(f->name, fname, sizeof(f->name)-1); f->name[sizeof(f->name)-1] = '\0';
            
            f->blocks = entry[30] | (entry[31] << 8);
            f->size = f->blocks * 254;
            
            if (!f->deleted && f->start_track > 0) {
                dir->count++;
            }
        }
        
        dir_track = sect[0];
        dir_sector = sect[1];
    }
    
    return 0;
}

/* ============================================================================
 * FAT12 File Operations (PC IMG)
 * ============================================================================ */

#define FAT12_SECTOR_SIZE   512

typedef struct {
    uint8_t  jump[3];
    char     oem[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  fat_count;
    uint16_t root_entries;
    uint16_t total_sectors_16;
    uint8_t  media_type;
    uint16_t sectors_per_fat;
    uint16_t sectors_per_track;
    uint16_t heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
} __attribute__((packed)) fat12_bpb_t;

/**
 * @brief Get FAT12 cluster value
 */
static uint16_t fat12_get_cluster(const uint8_t *fat, int cluster)
{
    int offset = cluster + cluster / 2;
    uint16_t val = fat[offset] | (fat[offset + 1] << 8);
    
    if (cluster & 1) {
        return val >> 4;
    } else {
        return val & 0x0FFF;
    }
}

/**
 * @brief List files on FAT12 image
 */
int fat12_list_files(const uint8_t *image, size_t size, uft_directory_t *dir)
{
    if (!image || !dir || size < 163840) return -1;
    
    memset(dir, 0, sizeof(*dir));
    
    /* Read BPB */
    const fat12_bpb_t *bpb = (const fat12_bpb_t*)image;
    
    uint16_t bytes_per_sect = bpb->bytes_per_sector;
    if (bytes_per_sect == 0) bytes_per_sect = 512;
    
    uint8_t sects_per_clust = bpb->sectors_per_cluster;
    if (sects_per_clust == 0) sects_per_clust = 1;
    
    uint16_t reserved = bpb->reserved_sectors;
    if (reserved == 0) reserved = 1;
    
    uint8_t fat_count = bpb->fat_count;
    if (fat_count == 0) fat_count = 2;
    
    uint16_t root_entries = bpb->root_entries;
    if (root_entries == 0) root_entries = 224;
    
    uint16_t sects_per_fat = bpb->sectors_per_fat;
    if (sects_per_fat == 0) sects_per_fat = 9;
    
    /* Calculate offsets */
    size_t fat_start = reserved * bytes_per_sect;
    size_t root_start = fat_start + fat_count * sects_per_fat * bytes_per_sect;
    size_t root_size = root_entries * 32;
    size_t data_start = root_start + root_size;
    
    /* Volume label from root directory or BPB */
    strncpy(dir->disk_name, "", 16); dir->disk_name[16] = '\0';
    
    /* Total/free clusters */
    uint16_t total_sects = bpb->total_sectors_16;
    if (total_sects == 0) total_sects = bpb->total_sectors_32;
    
    int data_sectors = total_sects - (data_start / bytes_per_sect);
    int total_clusters = data_sectors / sects_per_clust;
    
    dir->total_blocks = total_clusters;
    
    /* Count free clusters */
    const uint8_t *fat = image + fat_start;
    int free_clusters = 0;
    for (int c = 2; c < total_clusters + 2; c++) {
        if (fat12_get_cluster(fat, c) == 0) {
            free_clusters++;
        }
    }
    dir->free_blocks = free_clusters;
    
    /* Read root directory */
    const uint8_t *root = image + root_start;
    
    for (int i = 0; i < root_entries && dir->count < UFT_MAX_FILES; i++) {
        const uint8_t *entry = root + i * 32;
        
        if (entry[0] == 0x00) break;      /* End of directory */
        if (entry[0] == 0xE5) continue;   /* Deleted */
        if (entry[11] == 0x0F) continue;  /* Long filename entry */
        if (entry[11] & 0x08) {
            /* Volume label */
            memcpy(dir->disk_name, entry, 11);
            dir->disk_name[11] = '\0';
            for (int j = 10; j >= 0 && dir->disk_name[j] == ' '; j--) {
                dir->disk_name[j] = '\0';
            }
            continue;
        }
        
        uft_file_entry_t *f = &dir->files[dir->count];
        
        /* Filename 8.3 */
        char name[9], ext[4];
        memcpy(name, entry, 8);
        name[8] = '\0';
        memcpy(ext, entry + 8, 3);
        ext[3] = '\0';
        
        for (int j = 7; j >= 0 && name[j] == ' '; j--) name[j] = '\0';
        for (int j = 2; j >= 0 && ext[j] == ' '; j--) ext[j] = '\0';
        
        if (ext[0]) {
            snprintf(f->name, sizeof(f->name), "%s.%s", name, ext);
        } else {
            strncpy(f->name, name, sizeof(f->name)-1); f->name[sizeof(f->name)-1] = '\0';
        }
        
        /* Attributes */
        uint8_t attr = entry[11];
        f->locked = (attr & 0x01) != 0;  /* Read-only */
        
        if (attr & 0x10) {
            f->type = UFT_FTYPE_DIR;
        } else {
            f->type = UFT_FTYPE_BINARY;
        }
        
        /* Size */
        f->size = entry[28] | (entry[29] << 8) | (entry[30] << 16) | (entry[31] << 24);
        f->blocks = (f->size + sects_per_clust * bytes_per_sect - 1) / 
                    (sects_per_clust * bytes_per_sect);
        
        /* Start cluster */
        f->start_sector = entry[26] | (entry[27] << 8);
        
        dir->count++;
    }
    
    return 0;
}

/**
 * @brief Extract file from FAT12 image
 */
int fat12_extract_file(const uint8_t *image, size_t img_size,
                       const char *filename, uint8_t **data, size_t *size)
{
    if (!image || !filename || !data || !size) return -1;
    
    uft_directory_t dir;
    if (fat12_list_files(image, img_size, &dir) != 0) return -1;
    
    /* Find file */
    int found = -1;
    for (int i = 0; i < dir.count; i++) {
        if (strcasecmp(dir.files[i].name, filename) == 0) {
            found = i;
            break;
        }
    }
    
    if (found < 0) return -1;
    
    uft_file_entry_t *f = &dir.files[found];
    
    /* Get geometry from BPB */
    const fat12_bpb_t *bpb = (const fat12_bpb_t*)image;
    
    uint16_t bytes_per_sect = bpb->bytes_per_sector;
    if (bytes_per_sect == 0) bytes_per_sect = 512;
    
    uint8_t sects_per_clust = bpb->sectors_per_cluster;
    if (sects_per_clust == 0) sects_per_clust = 1;
    
    uint16_t reserved = bpb->reserved_sectors;
    if (reserved == 0) reserved = 1;
    
    uint8_t fat_count = bpb->fat_count;
    if (fat_count == 0) fat_count = 2;
    
    uint16_t root_entries = bpb->root_entries;
    if (root_entries == 0) root_entries = 224;
    
    uint16_t sects_per_fat = bpb->sectors_per_fat;
    if (sects_per_fat == 0) sects_per_fat = 9;
    
    size_t fat_start = reserved * bytes_per_sect;
    size_t root_start = fat_start + fat_count * sects_per_fat * bytes_per_sect;
    size_t root_size = root_entries * 32;
    size_t data_start = root_start + root_size;
    
    size_t cluster_size = sects_per_clust * bytes_per_sect;
    
    /* Allocate buffer */
    uint8_t *buf = malloc(f->size);
    if (!buf) return -1;
    
    /* Follow cluster chain */
    const uint8_t *fat = image + fat_start;
    uint16_t cluster = f->start_sector;
    size_t pos = 0;
    
    while (cluster >= 2 && cluster < 0xFF8 && pos < f->size) {
        size_t cluster_offset = data_start + (cluster - 2) * cluster_size;
        
        size_t to_copy = f->size - pos;
        if (to_copy > cluster_size) to_copy = cluster_size;
        
        if (cluster_offset + to_copy > img_size) break;
        
        memcpy(buf + pos, image + cluster_offset, to_copy);
        pos += to_copy;
        
        cluster = fat12_get_cluster(fat, cluster);
    }
    
    *data = buf;
    *size = f->size;
    
    return 0;
}

/* ============================================================================
 * Extended Unified API
 * ============================================================================ */

/**
 * @brief Extended list files (more formats)
 */
int uft_list_files_extended(const char *path, uft_directory_t *dir)
{
    if (!path || !dir) return -1;
    
    FILE *fp = fopen(path, "rb");
    if (!fp) return -1;
    
    if (fseek(fp, 0, SEEK_END) != 0) { /* seek error */ }
    size_t size = ftell(fp);
    if (fseek(fp, 0, SEEK_SET) != 0) { /* seek error */ }
    uint8_t *image = malloc(size);
    if (!image) {
        fclose(fp);
        return -1;
    }
    
    if (fread(image, 1, size, fp) != size) { /* I/O error */ }
    fclose(fp);
    
    int result = -1;
    
    const char *ext = strrchr(path, '.');
    if (ext) ext++;
    
    if (ext && (strcasecmp(ext, "ssd") == 0 || strcasecmp(ext, "dsd") == 0)) {
        result = ssd_list_files(image, size, dir);
    } else if (ext && strcasecmp(ext, "d81") == 0) {
        result = d81_list_files(image, size, dir);
    } else if (ext && (strcasecmp(ext, "img") == 0 || strcasecmp(ext, "ima") == 0)) {
        result = fat12_list_files(image, size, dir);
    } else {
        /* Try by size */
        if (size == 102400 || size == 204800 || size == 409600) {
            result = ssd_list_files(image, size, dir);
        } else if (size == 819200) {
            result = d81_list_files(image, size, dir);
        } else if (size == 737280 || size == 1474560 || size == 368640) {
            result = fat12_list_files(image, size, dir);
        }
    }
    
    free(image);
    return result;
}
