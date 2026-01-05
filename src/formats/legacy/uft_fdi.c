/**
 * @file uft_fdi.c
 * @brief FAT Disk Image (FDI) Implementation for UFT
 * 
 * @copyright UFT Project
 */

#include "uft_fdi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/*============================================================================
 * Standard Disk Types
 *============================================================================*/

const uft_fdi_disk_type_t uft_fdi_disk_types[] = {
    /* 160KB 5.25" SS SD */
    { 160, 1, 1, 2, 64, 320, 0xFE, 1, 8, 1 },
    
    /* 180KB 5.25" SS SD */
    { 180, 1, 1, 2, 64, 360, 0xFC, 2, 9, 1 },
    
    /* 320KB 5.25" DS SD */
    { 320, 2, 1, 2, 112, 640, 0xFF, 2, 8, 2 },
    
    /* 360KB 5.25" DS DD */
    { 360, 2, 1, 2, 112, 720, 0xFD, 3, 9, 2 },
    
    /* 720KB 3.5" DS DD */
    { 720, 2, 1, 2, 112, 1440, 0xF9, 5, 9, 2 },
    
    /* 1.2MB 5.25" DS HD */
    { 1200, 1, 1, 2, 224, 2400, 0xF9, 8, 15, 2 },
    
    /* 1.44MB 3.5" DS HD */
    { 1440, 1, 1, 2, 224, 2880, 0xF0, 9, 18, 2 },
    
    /* 2.88MB 3.5" DS ED */
    { 2880, 2, 1, 2, 240, 5760, 0xF0, 9, 36, 2 },
    
    /* End marker */
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }
};

/*============================================================================
 * Initialization and Cleanup
 *============================================================================*/

int uft_fdi_init(uft_fdi_image_t* img)
{
    if (!img) return -1;
    memset(img, 0, sizeof(*img));
    img->bytes_sector = UFT_FDI_SECTOR_SIZE;
    return 0;
}

void uft_fdi_free(uft_fdi_image_t* img)
{
    if (!img) return;
    if (img->data) {
        free(img->data);
        img->data = NULL;
    }
    img->size = 0;
}

/*============================================================================
 * BPB Parsing
 *============================================================================*/

static int parse_bpb(uft_fdi_image_t* img)
{
    if (!img || !img->data || img->size < UFT_FDI_SECTOR_SIZE) {
        return -1;
    }
    
    const uft_fdi_boot_sector_t* bpb = (const uft_fdi_boot_sector_t*)img->data;
    
    /* Validate boot sector */
    if (bpb->bytes_sector != 512) return -1;
    if (bpb->sectors_cluster == 0) return -1;
    if (bpb->num_fats == 0 || bpb->num_fats > 4) return -1;
    
    /* Copy BPB parameters */
    img->bytes_sector = bpb->bytes_sector;
    img->sectors_cluster = bpb->sectors_cluster;
    img->reserved_sectors = bpb->reserved_sectors;
    img->num_fats = bpb->num_fats;
    img->root_entries = bpb->root_entries;
    img->sectors_fat = bpb->sectors_fat;
    img->sectors_track = bpb->sectors_track;
    img->num_heads = bpb->num_heads;
    img->media_id = bpb->media_id;
    
    /* Total sectors */
    if (bpb->total_sectors_16) {
        img->total_sectors = bpb->total_sectors_16;
    } else {
        img->total_sectors = bpb->total_sectors_32;
    }
    
    /* Calculate derived values */
    img->fat_start = img->reserved_sectors;
    img->root_start = img->fat_start + (img->num_fats * img->sectors_fat);
    img->root_sectors = (img->root_entries * 32 + img->bytes_sector - 1) / 
                        img->bytes_sector;
    img->data_start = img->root_start + img->root_sectors;
    img->cluster_size = img->sectors_cluster * img->bytes_sector;
    
    /* Calculate data clusters */
    uint32_t data_sectors = img->total_sectors - img->data_start;
    img->data_clusters = data_sectors / img->sectors_cluster;
    
    /* Determine FAT type */
    img->is_fat16 = (img->data_clusters >= 4085);
    
    return 0;
}

/*============================================================================
 * File I/O
 *============================================================================*/

int uft_fdi_read_mem(const uint8_t* data, size_t size, uft_fdi_image_t* img)
{
    if (!data || !img || size < UFT_FDI_SECTOR_SIZE) {
        return -1;
    }
    
    uft_fdi_init(img);
    
    img->size = size;
    img->data = malloc(size);
    if (!img->data) return -1;
    
    memcpy(img->data, data, size);
    
    if (parse_bpb(img) != 0) {
        free(img->data);
        img->data = NULL;
        return -1;
    }
    
    return 0;
}

int uft_fdi_read(const char* filename, uft_fdi_image_t* img)
{
    if (!filename || !img) return -1;
    
    FILE* fp = fopen(filename, "rb");
    if (!fp) return -1;
    
    if (fseek(fp, 0, SEEK_END) != 0) { /* seek error */ }
    long size = ftell(fp);
    if (fseek(fp, 0, SEEK_SET) != 0) { /* seek error */ }
    if (size <= 0 || size > 64*1024*1024) {
        fclose(fp);
        return -1;
    }
    
    uint8_t* data = malloc(size);
    if (!data) {
        fclose(fp);
        return -1;
    }
    
    if (fread(data, 1, size, fp) != (size_t)size) {
        free(data);
        fclose(fp);
        return -1;
    }
    fclose(fp);
    
    int result = uft_fdi_read_mem(data, size, img);
    free(data);
    
    return result;
}

int uft_fdi_write(const char* filename, const uft_fdi_image_t* img)
{
    if (!filename || !img || !img->data) return -1;
    
    FILE* fp = fopen(filename, "wb");
    if (!fp) return -1;
    
    if (fwrite(img->data, 1, img->size, fp) != img->size) {
        fclose(fp);
        return -1;
    }
    
    fclose(fp);
    return 0;
}

/*============================================================================
 * Sector I/O
 *============================================================================*/

int uft_fdi_read_sector(const uft_fdi_image_t* img, uint32_t sector, uint8_t* buffer)
{
    if (!img || !img->data || !buffer) return -1;
    
    size_t offset = (size_t)sector * img->bytes_sector;
    if (offset + img->bytes_sector > img->size) return -1;
    
    memcpy(buffer, img->data + offset, img->bytes_sector);
    return 0;
}

int uft_fdi_write_sector(uft_fdi_image_t* img, uint32_t sector, const uint8_t* buffer)
{
    if (!img || !img->data || !buffer) return -1;
    
    size_t offset = (size_t)sector * img->bytes_sector;
    if (offset + img->bytes_sector > img->size) return -1;
    
    memcpy(img->data + offset, buffer, img->bytes_sector);
    return 0;
}

/*============================================================================
 * FAT Access
 *============================================================================*/

uint16_t uft_fdi_get_fat(const uft_fdi_image_t* img, uint16_t cluster)
{
    if (!img || !img->data || cluster < 2) return 0xFFFF;
    
    size_t fat_offset = img->fat_start * img->bytes_sector;
    
    if (img->is_fat16) {
        /* FAT16: 2 bytes per entry */
        size_t entry_offset = fat_offset + cluster * 2;
        if (entry_offset + 2 > img->size) return 0xFFFF;
        
        return img->data[entry_offset] | (img->data[entry_offset + 1] << 8);
    } else {
        /* FAT12: 1.5 bytes per entry */
        size_t byte_offset = fat_offset + (cluster * 3) / 2;
        if (byte_offset + 2 > img->size) return 0xFFF;
        
        uint16_t value = img->data[byte_offset] | (img->data[byte_offset + 1] << 8);
        
        if (cluster & 1) {
            return value >> 4;
        } else {
            return value & 0x0FFF;
        }
    }
}

int uft_fdi_set_fat(uft_fdi_image_t* img, uint16_t cluster, uint16_t value)
{
    if (!img || !img->data || cluster < 2) return -1;
    
    /* Update all FAT copies */
    for (uint8_t fat = 0; fat < img->num_fats; fat++) {
        size_t fat_offset = (img->fat_start + fat * img->sectors_fat) * 
                            img->bytes_sector;
        
        if (img->is_fat16) {
            size_t entry_offset = fat_offset + cluster * 2;
            if (entry_offset + 2 > img->size) return -1;
            
            img->data[entry_offset] = value & 0xFF;
            img->data[entry_offset + 1] = (value >> 8) & 0xFF;
        } else {
            size_t byte_offset = fat_offset + (cluster * 3) / 2;
            if (byte_offset + 2 > img->size) return -1;
            
            if (cluster & 1) {
                img->data[byte_offset] = (img->data[byte_offset] & 0x0F) | 
                                         ((value & 0x0F) << 4);
                img->data[byte_offset + 1] = (value >> 4) & 0xFF;
            } else {
                img->data[byte_offset] = value & 0xFF;
                img->data[byte_offset + 1] = (img->data[byte_offset + 1] & 0xF0) | 
                                             ((value >> 8) & 0x0F);
            }
        }
    }
    
    return 0;
}

uint16_t uft_fdi_find_free_cluster(const uft_fdi_image_t* img)
{
    if (!img) return 0;
    
    uint16_t end_marker = img->is_fat16 ? 0xFFF8 : 0xFF8;
    
    for (uint16_t cluster = 2; cluster < img->data_clusters + 2; cluster++) {
        uint16_t value = uft_fdi_get_fat(img, cluster);
        if (value == 0) {
            return cluster;
        }
    }
    
    return 0;  /* Disk full */
}

/*============================================================================
 * Directory Operations
 *============================================================================*/

int uft_fdi_read_dir_entry(const uft_fdi_image_t* img, uint16_t cluster,
                           uint16_t index, uft_fdi_dir_entry_t* entry)
{
    if (!img || !entry) return -1;
    
    uint16_t entries_per_sector = img->bytes_sector / 32;
    uint16_t entries_per_cluster = entries_per_sector * img->sectors_cluster;
    
    uint32_t sector;
    uint16_t entry_in_sector;
    
    if (cluster == 0) {
        /* Root directory */
        if (index >= img->root_entries) return 1;  /* End of directory */
        
        sector = img->root_start + (index / entries_per_sector);
        entry_in_sector = index % entries_per_sector;
    } else {
        /* Subdirectory */
        uint16_t cluster_index = index / entries_per_cluster;
        uint16_t entry_in_cluster = index % entries_per_cluster;
        
        /* Follow cluster chain */
        uint16_t current = cluster;
        for (uint16_t i = 0; i < cluster_index; i++) {
            current = uft_fdi_get_fat(img, current);
            if (current >= 0xFF8 || current < 2) return 1;  /* End */
        }
        
        sector = img->data_start + 
                 (current - 2) * img->sectors_cluster +
                 (entry_in_cluster / entries_per_sector);
        entry_in_sector = entry_in_cluster % entries_per_sector;
    }
    
    /* Read sector and extract entry */
    uint8_t buffer[UFT_FDI_SECTOR_SIZE];
    if (uft_fdi_read_sector(img, sector, buffer) != 0) return -1;
    
    memcpy(entry, buffer + entry_in_sector * 32, 32);
    
    /* Check for end of directory */
    if (entry->name[0] == UFT_FDI_DIR_END) return 1;
    
    return 0;
}

/*============================================================================
 * Name Conversion
 *============================================================================*/

void uft_fdi_name_to_string(const uft_fdi_dir_entry_t* entry, char* buffer)
{
    if (!entry || !buffer) return;
    
    int i, j = 0;
    
    /* Copy name, trim trailing spaces */
    for (i = 0; i < 8 && entry->name[i] != ' '; i++) {
        buffer[j++] = entry->name[i];
    }
    
    /* Add extension if present */
    if (entry->ext[0] != ' ') {
        buffer[j++] = '.';
        for (i = 0; i < 3 && entry->ext[i] != ' '; i++) {
            buffer[j++] = entry->ext[i];
        }
    }
    
    buffer[j] = '\0';
}

int uft_fdi_string_to_name(const char* name, uft_fdi_dir_entry_t* entry)
{
    if (!name || !entry) return -1;
    
    /* Clear name and extension */
    memset(entry->name, ' ', 8);
    memset(entry->ext, ' ', 3);
    
    int i = 0, j = 0;
    
    /* Copy name part */
    while (name[i] && name[i] != '.' && j < 8) {
        entry->name[j++] = toupper((unsigned char)name[i++]);
    }
    
    /* Skip to extension */
    if (name[i] == '.') {
        i++;
        j = 0;
        while (name[i] && j < 3) {
            entry->ext[j++] = toupper((unsigned char)name[i++]);
        }
    }
    
    return 0;
}

/*============================================================================
 * File Extraction
 *============================================================================*/

int uft_fdi_extract_file(const uft_fdi_image_t* img,
                         const uft_fdi_dir_entry_t* entry,
                         uint8_t* buffer, size_t size)
{
    if (!img || !entry || !buffer) return -1;
    if (entry->attr & UFT_FDI_ATTR_DIRECTORY) return -1;
    
    uint32_t file_size = entry->size;
    if (size < file_size) file_size = size;
    
    uint16_t cluster = entry->cluster;
    uint32_t bytes_read = 0;
    uint8_t sector_buffer[UFT_FDI_SECTOR_SIZE];
    
    while (bytes_read < file_size && cluster >= 2) {
        /* Check for end of chain */
        if (img->is_fat16) {
            if (cluster >= 0xFFF8) break;
        } else {
            if (cluster >= 0xFF8) break;
        }
        
        /* Read cluster */
        uint32_t first_sector = img->data_start + 
                                (cluster - 2) * img->sectors_cluster;
        
        for (uint8_t s = 0; s < img->sectors_cluster && bytes_read < file_size; s++) {
            if (uft_fdi_read_sector(img, first_sector + s, sector_buffer) != 0) {
                return -1;
            }
            
            uint32_t copy_size = img->bytes_sector;
            if (bytes_read + copy_size > file_size) {
                copy_size = file_size - bytes_read;
            }
            
            memcpy(buffer + bytes_read, sector_buffer, copy_size);
            bytes_read += copy_size;
        }
        
        /* Next cluster */
        cluster = uft_fdi_get_fat(img, cluster);
    }
    
    return bytes_read;
}

/*============================================================================
 * Information Display
 *============================================================================*/

void uft_fdi_print_info(const uft_fdi_image_t* img, bool verbose)
{
    if (!img) return;
    
    printf("FAT Disk Image Information:\n");
    printf("  Size: %zu bytes (%zu KB)\n", img->size, img->size / 1024);
    printf("  FAT type: FAT%d\n", img->is_fat16 ? 16 : 12);
    printf("  Bytes/sector: %d\n", img->bytes_sector);
    printf("  Sectors/cluster: %d\n", img->sectors_cluster);
    printf("  Reserved sectors: %d\n", img->reserved_sectors);
    printf("  Number of FATs: %d\n", img->num_fats);
    printf("  Root entries: %d\n", img->root_entries);
    printf("  Total sectors: %d\n", img->total_sectors);
    printf("  Sectors/FAT: %d\n", img->sectors_fat);
    printf("  Sectors/track: %d\n", img->sectors_track);
    printf("  Heads: %d\n", img->num_heads);
    printf("  Media ID: 0x%02X\n", img->media_id);
    
    if (verbose) {
        printf("\n  Calculated values:\n");
        printf("    FAT start sector: %d\n", img->fat_start);
        printf("    Root start sector: %d\n", img->root_start);
        printf("    Data start sector: %d\n", img->data_start);
        printf("    Data clusters: %d\n", img->data_clusters);
        printf("    Cluster size: %d bytes\n", img->cluster_size);
        
        /* Count free clusters */
        uint32_t free_clusters = 0;
        for (uint16_t c = 2; c < img->data_clusters + 2; c++) {
            if (uft_fdi_get_fat(img, c) == 0) free_clusters++;
        }
        printf("    Free clusters: %d (%d KB)\n", 
               free_clusters, (free_clusters * img->cluster_size) / 1024);
    }
}
