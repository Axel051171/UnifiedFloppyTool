/**
 * @file uft_fat32_mbr.c
 * @brief FAT32 filesystem and MBR partition table implementation
 * 
 * Ported and enhanced from MEGA65 FDISK project (GPL-3.0)
 * Original: Copyright (C) MEGA65 Project
 * 
 * @version 3.8.0
 * @date 2026-01-14
 */

#include "uft/formats/uft_fat32_mbr.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>

/*============================================================================*
 * INTERNAL HELPERS
 *============================================================================*/

/* Sector buffer for operations */
static uint8_t sector_buffer[UFT_SECTOR_SIZE];

/**
 * @brief Read 16-bit little-endian value
 */
static inline uint16_t read_le16(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

/**
 * @brief Read 32-bit little-endian value
 */
static inline uint32_t read_le32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | 
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/**
 * @brief Write 16-bit little-endian value
 */
static inline void write_le16(uint8_t *p, uint16_t v) {
    p[0] = v & 0xFF;
    p[1] = (v >> 8) & 0xFF;
}

/**
 * @brief Write 32-bit little-endian value
 */
static inline void write_le32(uint8_t *p, uint32_t v) {
    p[0] = v & 0xFF;
    p[1] = (v >> 8) & 0xFF;
    p[2] = (v >> 16) & 0xFF;
    p[3] = (v >> 24) & 0xFF;
}

/*============================================================================*
 * PARTITION TYPE NAMES
 *============================================================================*/

static const struct {
    uint8_t type;
    const char *name;
} partition_types[] = {
    { UFT_PART_TYPE_EMPTY,      "Empty" },
    { UFT_PART_TYPE_FAT12,      "FAT12" },
    { UFT_PART_TYPE_FAT16_SM,   "FAT16 (<32MB)" },
    { UFT_PART_TYPE_EXTENDED,   "Extended" },
    { UFT_PART_TYPE_FAT16,      "FAT16" },
    { UFT_PART_TYPE_NTFS,       "NTFS/exFAT" },
    { UFT_PART_TYPE_FAT32_CHS,  "FAT32 (CHS)" },
    { UFT_PART_TYPE_FAT32_LBA,  "FAT32 (LBA)" },
    { UFT_PART_TYPE_FAT16_LBA,  "FAT16 (LBA)" },
    { UFT_PART_TYPE_EXTENDED_LBA, "Extended (LBA)" },
    { UFT_PART_TYPE_MEGA65_SYS, "MEGA65 System" },
    { UFT_PART_TYPE_LINUX,      "Linux" },
    { UFT_PART_TYPE_LINUX_LVM,  "Linux LVM" },
    { 0xA5,                     "FreeBSD" },
    { 0xA6,                     "OpenBSD" },
    { 0xAF,                     "HFS/HFS+" },
    { 0xEE,                     "GPT Protective" },
    { 0xEF,                     "EFI System" },
    { 0, NULL }
};

const char *uft_partition_type_name(uint8_t type)
{
    for (int i = 0; partition_types[i].name != NULL; i++) {
        if (partition_types[i].type == type) {
            return partition_types[i].name;
        }
    }
    return "Unknown";
}

/*============================================================================*
 * CHS/LBA CONVERSION
 *============================================================================*/

void uft_lba_to_chs(uint32_t lba, uint8_t *head, uint8_t *sector, uint8_t *cylinder)
{
    /* Standard CHS geometry: 255 heads, 63 sectors/track */
    const uint32_t heads = 255;
    const uint32_t sectors = 63;
    
    if (lba > (1024 * heads * sectors - 1)) {
        /* LBA exceeds CHS addressable range (8GB), use maximum values */
        *head = 254;
        *sector = 63;
        *cylinder = 255;  /* Will be ORed with high bits */
    } else {
        uint32_t cyl = lba / (heads * sectors);
        uint32_t temp = lba % (heads * sectors);
        *head = temp / sectors;
        *sector = (temp % sectors) + 1;
        *cylinder = cyl & 0xFF;
        
        /* Pack cylinder high bits into sector field */
        *sector |= ((cyl >> 8) & 0x03) << 6;
    }
}

uint32_t uft_chs_to_lba(uint8_t head, uint8_t sector, uint8_t cylinder)
{
    const uint32_t heads = 255;
    const uint32_t sectors = 63;
    
    uint32_t cyl = cylinder | ((sector & 0xC0) << 2);
    uint32_t sec = sector & 0x3F;
    
    return (cyl * heads + head) * sectors + sec - 1;
}

/*============================================================================*
 * MBR FUNCTIONS
 *============================================================================*/

int uft_mbr_is_valid(const uft_disk_io_t *io)
{
    if (!io || !io->read) return 0;
    
    if (io->read(0, sector_buffer, io->user_data) != 0) {
        return 0;
    }
    
    /* Check MBR signature */
    uint16_t sig = read_le16(sector_buffer + 510);
    return (sig == UFT_MBR_SIGNATURE);
}

int uft_mbr_read_partitions(const uft_disk_io_t *io, 
                            uft_partition_info_t *partitions, 
                            int *count)
{
    if (!io || !io->read || !partitions || !count) {
        return UFT_FAT32_ERROR_PARAM;
    }
    
    *count = 0;
    
    /* Read MBR (sector 0) */
    if (io->read(0, sector_buffer, io->user_data) != 0) {
        return UFT_FAT32_ERROR_READ;
    }
    
    /* Check MBR signature */
    uint16_t sig = read_le16(sector_buffer + 510);
    if (sig != UFT_MBR_SIGNATURE) {
        return UFT_FAT32_ERROR_NO_MBR;
    }
    
    /* Parse partition table (offset 446) */
    const uint8_t *ptable = sector_buffer + 446;
    
    for (int i = 0; i < 4; i++) {
        const uint8_t *entry = ptable + (i * 16);
        uint8_t type = entry[4];
        
        if (type != UFT_PART_TYPE_EMPTY) {
            uft_partition_info_t *p = &partitions[*count];
            
            p->index = i;
            p->type = type;
            p->bootable = (entry[0] == 0x80) ? 1 : 0;
            p->start_lba = read_le32(entry + 8);
            p->size_sectors = read_le32(entry + 12);
            p->size_bytes = (uint64_t)p->size_sectors * UFT_SECTOR_SIZE;
            
            strncpy(p->type_name, uft_partition_type_name(type), sizeof(p->type_name) - 1);
            p->type_name[sizeof(p->type_name) - 1] = '\0';
            
            (*count)++;
        }
    }
    
    return UFT_FAT32_OK;
}

int uft_mbr_write_partitions(const uft_disk_io_t *io,
                             const uft_partition_entry_t *partitions,
                             int count)
{
    if (!io || !io->write || !partitions || count < 0 || count > 4) {
        return UFT_FAT32_ERROR_PARAM;
    }
    
    /* Clear buffer and set signature */
    memset(sector_buffer, 0, UFT_SECTOR_SIZE);
    write_le16(sector_buffer + 510, UFT_MBR_SIGNATURE);
    
    /* Copy partition entries */
    uint8_t *ptable = sector_buffer + 446;
    for (int i = 0; i < count && i < 4; i++) {
        memcpy(ptable + (i * 16), &partitions[i], sizeof(uft_partition_entry_t));
    }
    
    /* Write MBR */
    if (io->write(0, sector_buffer, io->user_data) != 0) {
        return UFT_FAT32_ERROR_WRITE;
    }
    
    return UFT_FAT32_OK;
}

int uft_mbr_create_default(const uft_disk_io_t *io, uint32_t sys_partition_size)
{
    if (!io || !io->write) {
        return UFT_FAT32_ERROR_PARAM;
    }
    
    uft_partition_entry_t partitions[4];
    int count = 0;
    uint32_t data_start;
    uint8_t h, s, c;
    
    memset(partitions, 0, sizeof(partitions));
    
    /* Create system partition if requested (e.g., MEGA65) */
    if (sys_partition_size > 0) {
        partitions[count].boot_flag = 0x00;
        partitions[count].type = UFT_PART_TYPE_MEGA65_SYS;
        partitions[count].lba_start = 2048;  /* Aligned to 1MB */
        partitions[count].lba_count = sys_partition_size;
        
        uft_lba_to_chs(partitions[count].lba_start, &h, &s, &c);
        partitions[count].start_head = h;
        partitions[count].start_sector = s;
        partitions[count].start_cylinder = c;
        
        uft_lba_to_chs(partitions[count].lba_start + partitions[count].lba_count - 1, 
                       &h, &s, &c);
        partitions[count].end_head = h;
        partitions[count].end_sector = s;
        partitions[count].end_cylinder = c;
        
        count++;
        data_start = 2048 + sys_partition_size;
    } else {
        data_start = 2048;  /* Standard 1MB alignment */
    }
    
    /* Align data partition to 4KB boundary */
    data_start = (data_start + 7) & ~7;
    
    /* Create main FAT32 partition */
    partitions[count].boot_flag = 0x80;  /* Bootable */
    partitions[count].type = UFT_PART_TYPE_FAT32_LBA;
    partitions[count].lba_start = data_start;
    partitions[count].lba_count = io->total_sectors - data_start;
    
    uft_lba_to_chs(partitions[count].lba_start, &h, &s, &c);
    partitions[count].start_head = h;
    partitions[count].start_sector = s;
    partitions[count].start_cylinder = c;
    
    uft_lba_to_chs(partitions[count].lba_start + partitions[count].lba_count - 1,
                   &h, &s, &c);
    partitions[count].end_head = h;
    partitions[count].end_sector = s;
    partitions[count].end_cylinder = c;
    
    count++;
    
    return uft_mbr_write_partitions(io, partitions, count);
}

/*============================================================================*
 * FAT32 FUNCTIONS
 *============================================================================*/

uint8_t uft_fat32_calc_cluster_size(uint32_t partition_size)
{
    /* Microsoft recommended cluster sizes for FAT32 */
    if (partition_size < 532480)          /* < 260MB */
        return 1;   /* 512 bytes */
    else if (partition_size < 16777216)   /* < 8GB */
        return 8;   /* 4KB */
    else if (partition_size < 33554432)   /* < 16GB */
        return 16;  /* 8KB */
    else if (partition_size < 67108864)   /* < 32GB */
        return 32;  /* 16KB */
    else
        return 64;  /* 32KB */
}

uint32_t uft_fat32_generate_volume_id(void)
{
    /* Use time-based random seed */
    uint32_t id = (uint32_t)time(NULL);
    
    /* XorShift32 to mix bits */
    id ^= (id << 13);
    id ^= (id >> 17);
    id ^= (id << 5);
    
    /* Add some entropy from clock */
#ifdef _POSIX_C_SOURCE
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
        id ^= (uint32_t)(ts.tv_nsec);
    }
#endif
    
    return id;
}

int uft_fat32_read_boot_sector(const uft_disk_io_t *io,
                               uint32_t partition_start,
                               uft_fat32_boot_sector_t *boot_sector)
{
    if (!io || !io->read || !boot_sector) {
        return UFT_FAT32_ERROR_PARAM;
    }
    
    if (io->read(partition_start, (uint8_t *)boot_sector, io->user_data) != 0) {
        return UFT_FAT32_ERROR_READ;
    }
    
    return UFT_FAT32_OK;
}

int uft_fat32_validate(const uft_disk_io_t *io, uint32_t partition_start)
{
    uft_fat32_boot_sector_t boot;
    
    if (uft_fat32_read_boot_sector(io, partition_start, &boot) != UFT_FAT32_OK) {
        return UFT_FAT32_ERROR_READ;
    }
    
    /* Check signature */
    if (boot.signature != UFT_MBR_SIGNATURE) {
        return UFT_FAT32_ERROR_NO_MBR;
    }
    
    /* Check FAT32 markers */
    if (boot.bytes_per_sector != 512 ||
        boot.root_entry_count != 0 ||
        boot.total_sectors_16 != 0 ||
        boot.fat_size_16 != 0) {
        return UFT_FAT32_ERROR_PARAM;
    }
    
    /* Check filesystem type string */
    if (memcmp(boot.fs_type, "FAT32   ", 8) != 0) {
        return UFT_FAT32_ERROR_PARAM;
    }
    
    return UFT_FAT32_OK;
}

int uft_fat32_format(const uft_disk_io_t *io,
                     const uft_fat32_format_params_t *params,
                     void (*progress_cb)(uint32_t current, uint32_t total, void *ctx),
                     void *progress_ctx)
{
    uft_fat32_boot_sector_t *boot;
    uft_fat32_fsinfo_t *fsinfo;
    uint32_t fat_size;
    uint32_t data_sectors;
    uint32_t cluster_count;
    uint32_t fat_sector;
    uint8_t spc;
    
    if (!io || !io->read || !io->write || !params) {
        return UFT_FAT32_ERROR_PARAM;
    }
    
    /* Determine sectors per cluster */
    spc = params->sectors_per_cluster;
    if (spc == 0) {
        spc = uft_fat32_calc_cluster_size(params->partition_size);
    }
    
    /* Calculate FAT size */
    data_sectors = params->partition_size - UFT_FAT32_RESERVED_SECTORS;
    cluster_count = data_sectors / spc;
    fat_size = (cluster_count + 2) * 4;  /* 4 bytes per FAT entry */
    fat_size = (fat_size + UFT_SECTOR_SIZE - 1) / UFT_SECTOR_SIZE;  /* Round up */
    
    /* Verify sufficient space */
    if (fat_size * 2 + UFT_FAT32_RESERVED_SECTORS >= params->partition_size) {
        return UFT_FAT32_ERROR_SIZE;
    }
    
    /* Create boot sector */
    memset(sector_buffer, 0, UFT_SECTOR_SIZE);
    boot = (uft_fat32_boot_sector_t *)sector_buffer;
    
    /* Jump instruction */
    boot->jump_boot[0] = 0xEB;
    boot->jump_boot[1] = 0x58;
    boot->jump_boot[2] = 0x90;
    
    /* OEM name */
    if (params->oem_name[0]) {
        memcpy(boot->oem_name, params->oem_name, 8);
    } else {
        memcpy(boot->oem_name, "UFT     ", 8);
    }
    
    /* BIOS Parameter Block */
    boot->bytes_per_sector = UFT_SECTOR_SIZE;
    boot->sectors_per_cluster = spc;
    boot->reserved_sectors = UFT_FAT32_RESERVED_SECTORS;
    boot->num_fats = UFT_FAT32_NUM_FATS;
    boot->root_entry_count = 0;
    boot->total_sectors_16 = 0;
    boot->media_type = 0xF8;  /* Fixed disk */
    boot->fat_size_16 = 0;
    boot->sectors_per_track = 63;
    boot->num_heads = 255;
    boot->hidden_sectors = params->partition_start;
    boot->total_sectors_32 = params->partition_size;
    
    /* FAT32 specific fields */
    boot->fat_size_32 = fat_size;
    boot->ext_flags = 0;
    boot->fs_version = 0;
    boot->root_cluster = UFT_FAT32_ROOT_CLUSTER;
    boot->fs_info = 1;
    boot->backup_boot_sector = 6;
    boot->drive_number = 0x80;
    boot->boot_signature = 0x29;
    
    /* Volume ID */
    boot->volume_id = params->volume_id ? params->volume_id : uft_fat32_generate_volume_id();
    
    /* Volume label */
    memset(boot->volume_label, ' ', 11);
    size_t label_len = strlen(params->volume_label);
    if (label_len > 11) label_len = 11;
    memcpy(boot->volume_label, params->volume_label, label_len);
    
    memcpy(boot->fs_type, "FAT32   ", 8);
    boot->signature = UFT_MBR_SIGNATURE;
    
    /* Write boot sector */
    if (io->write(params->partition_start, sector_buffer, io->user_data) != 0) {
        return UFT_FAT32_ERROR_WRITE;
    }
    
    /* Write backup boot sector */
    if (io->write(params->partition_start + 6, sector_buffer, io->user_data) != 0) {
        return UFT_FAT32_ERROR_WRITE;
    }
    
    /* Create FSInfo sector */
    memset(sector_buffer, 0, UFT_SECTOR_SIZE);
    fsinfo = (uft_fat32_fsinfo_t *)sector_buffer;
    
    fsinfo->lead_signature = 0x41615252;
    fsinfo->struct_signature = 0x61417272;
    fsinfo->free_count = cluster_count - 1;  /* Minus root directory cluster */
    fsinfo->next_free = 3;
    fsinfo->trail_signature = 0xAA550000;
    
    /* Write FSInfo */
    if (io->write(params->partition_start + 1, sector_buffer, io->user_data) != 0) {
        return UFT_FAT32_ERROR_WRITE;
    }
    
    /* Write backup FSInfo */
    if (io->write(params->partition_start + 7, sector_buffer, io->user_data) != 0) {
        return UFT_FAT32_ERROR_WRITE;
    }
    
    /* Clear remaining reserved sectors */
    memset(sector_buffer, 0, UFT_SECTOR_SIZE);
    for (uint32_t i = 2; i < UFT_FAT32_RESERVED_SECTORS; i++) {
        if (i == 6 || i == 7) continue;  /* Skip backup sectors */
        if (io->write(params->partition_start + i, sector_buffer, io->user_data) != 0) {
            return UFT_FAT32_ERROR_WRITE;
        }
    }
    
    /* Initialize FATs */
    /* First FAT sector has special entries */
    memset(sector_buffer, 0, UFT_SECTOR_SIZE);
    sector_buffer[0] = 0xF8;  /* Media type */
    sector_buffer[1] = 0xFF;
    sector_buffer[2] = 0xFF;
    sector_buffer[3] = 0x0F;  /* End of chain marker for entry 0 */
    sector_buffer[4] = 0xFF;
    sector_buffer[5] = 0xFF;
    sector_buffer[6] = 0xFF;
    sector_buffer[7] = 0x0F;  /* End of chain for entry 1 */
    /* Root directory cluster (cluster 2) - end of chain */
    sector_buffer[8] = 0xFF;
    sector_buffer[9] = 0xFF;
    sector_buffer[10] = 0xFF;
    sector_buffer[11] = 0x0F;
    
    /* Write first sector of both FATs */
    fat_sector = params->partition_start + UFT_FAT32_RESERVED_SECTORS;
    if (io->write(fat_sector, sector_buffer, io->user_data) != 0) {
        return UFT_FAT32_ERROR_WRITE;
    }
    if (io->write(fat_sector + fat_size, sector_buffer, io->user_data) != 0) {
        return UFT_FAT32_ERROR_WRITE;
    }
    
    /* Clear rest of FATs */
    memset(sector_buffer, 0, UFT_SECTOR_SIZE);
    uint32_t total_fat_sectors = fat_size * 2;
    
    for (uint32_t i = 1; i < fat_size; i++) {
        if (io->write(fat_sector + i, sector_buffer, io->user_data) != 0) {
            return UFT_FAT32_ERROR_WRITE;
        }
        if (io->write(fat_sector + fat_size + i, sector_buffer, io->user_data) != 0) {
            return UFT_FAT32_ERROR_WRITE;
        }
        
        /* Progress callback */
        if (progress_cb && (i & 63) == 0) {
            progress_cb(i * 2, total_fat_sectors, progress_ctx);
        }
    }
    
    /* Clear root directory cluster */
    uint32_t root_sector = fat_sector + fat_size * 2;
    for (uint32_t i = 0; i < spc; i++) {
        if (io->write(root_sector + i, sector_buffer, io->user_data) != 0) {
            return UFT_FAT32_ERROR_WRITE;
        }
    }
    
    /* Create volume label entry in root directory */
    uft_fat32_dir_entry_t *entry = (uft_fat32_dir_entry_t *)sector_buffer;
    memset(sector_buffer, 0, UFT_SECTOR_SIZE);
    
    memset(entry->name, ' ', 11);
    memcpy(entry->name, params->volume_label, 
           strlen(params->volume_label) < 11 ? strlen(params->volume_label) : 11);
    entry->attributes = UFT_ATTR_VOLUME_ID;
    
    if (io->write(root_sector, sector_buffer, io->user_data) != 0) {
        return UFT_FAT32_ERROR_WRITE;
    }
    
    /* Final progress */
    if (progress_cb) {
        progress_cb(total_fat_sectors, total_fat_sectors, progress_ctx);
    }
    
    return UFT_FAT32_OK;
}

/*============================================================================*
 * UTILITY FUNCTIONS
 *============================================================================*/

void uft_format_size_string(uint64_t sectors, char *buffer, size_t buffer_size)
{
    uint64_t bytes = sectors * UFT_SECTOR_SIZE;
    
    if (bytes >= (1ULL << 40)) {
        snprintf(buffer, buffer_size, "%.2f TB", (double)bytes / (1ULL << 40));
    } else if (bytes >= (1ULL << 30)) {
        snprintf(buffer, buffer_size, "%.2f GB", (double)bytes / (1ULL << 30));
    } else if (bytes >= (1ULL << 20)) {
        snprintf(buffer, buffer_size, "%.2f MB", (double)bytes / (1ULL << 20));
    } else if (bytes >= (1ULL << 10)) {
        snprintf(buffer, buffer_size, "%.2f KB", (double)bytes / (1ULL << 10));
    } else {
        snprintf(buffer, buffer_size, "%lu B", (unsigned long)bytes);
    }
}
