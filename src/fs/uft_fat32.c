/**
 * @file uft_fat32.c
 * @brief FAT32 Filesystem Implementation
 */

#include "uft/fs/uft_fat32.h"
#include "uft/fs/uft_fat_boot.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>

/*===========================================================================
 * Detection
 *===========================================================================*/

bool uft_fat32_detect(const uint8_t *data, size_t size)
{
    if (!data || size < 512) return false;
    
    const uft_fat32_bootsect_t *boot = (const uft_fat32_bootsect_t *)data;
    
    /* Check signature */
    if (boot->signature != UFT_FAT_BOOT_SIG) return false;
    
    /* FAT32 indicators */
    if (boot->fat_size_16 != 0) return false;  /* Must be 0 for FAT32 */
    if (boot->fat_size_32 == 0) return false;  /* Must be non-zero for FAT32 */
    if (boot->root_entry_count != 0) return false; /* Must be 0 for FAT32 */
    
    /* Check fs_type string */
    if (memcmp(boot->fs_type, "FAT32   ", 8) != 0) return false;
    
    return uft_fat32_validate(boot);
}

const uft_fat32_bootsect_t *uft_fat32_get_boot(const uint8_t *data)
{
    if (!data) return NULL;
    return (const uft_fat32_bootsect_t *)data;
}

bool uft_fat32_validate(const uft_fat32_bootsect_t *boot)
{
    if (!boot) return false;
    
    /* Basic sanity checks */
    if (boot->bytes_per_sector < 512 || boot->bytes_per_sector > 4096) return false;
    if (boot->sectors_per_cluster == 0) return false;
    if (boot->num_fats == 0 || boot->num_fats > 2) return false;
    if (boot->reserved_sectors == 0) return false;
    if (boot->root_cluster < 2) return false;
    
    /* Calculate cluster count */
    uint32_t total_sectors = boot->total_sectors_32;
    uint32_t fat_sectors = boot->fat_size_32;
    uint32_t root_sectors = 0;  /* FAT32 has no fixed root */
    uint32_t data_sectors = total_sectors - boot->reserved_sectors - 
                            (boot->num_fats * fat_sectors) - root_sectors;
    uint32_t cluster_count = data_sectors / boot->sectors_per_cluster;
    
    /* FAT32 requires at least 65525 clusters */
    return cluster_count >= UFT_FAT32_MIN_CLUSTERS;
}

/*===========================================================================
 * Parameter Calculation
 *===========================================================================*/

void uft_fat32_format_opts_init(uft_fat32_format_opts_t *opts)
{
    if (!opts) return;
    memset(opts, 0, sizeof(*opts));
    opts->sector_size = 512;
    opts->num_fats = 2;
    opts->backup_boot = 6;
    opts->align_structures = true;
    memcpy(opts->oem_name, UFT_OEM_UFT, 8);
}

uint8_t uft_fat32_recommended_spc(uint64_t size)
{
    /* Microsoft recommendations for FAT32 */
    if (size <= 64ULL * 1024 * 1024) return 1;      /* <= 64MB: 512b clusters */
    if (size <= 128ULL * 1024 * 1024) return 2;     /* <= 128MB: 1KB */
    if (size <= 256ULL * 1024 * 1024) return 4;     /* <= 256MB: 2KB */
    if (size <= 8ULL * 1024 * 1024 * 1024) return 8; /* <= 8GB: 4KB */
    if (size <= 16ULL * 1024 * 1024 * 1024) return 16; /* <= 16GB: 8KB */
    if (size <= 32ULL * 1024 * 1024 * 1024) return 32; /* <= 32GB: 16KB */
    return 64; /* > 32GB: 32KB clusters */
}

int uft_fat32_calc_params(uint64_t size, uft_fat32_format_opts_t *opts)
{
    if (!opts || size < 32 * 1024 * 1024) return -1;  /* Min 32MB for FAT32 */
    
    /* Calculate sectors */
    uint32_t total_sectors = size / opts->sector_size;
    
    /* Set sectors per cluster if auto */
    if (opts->sectors_per_cluster == 0) {
        opts->sectors_per_cluster = uft_fat32_recommended_spc(size);
    }
    
    /* Reserved sectors (typical 32 for FAT32) */
    if (opts->reserved_sectors == 0) {
        opts->reserved_sectors = 32;
    }
    
    /* Calculate FAT size */
    uint32_t cluster_size = opts->sector_size * opts->sectors_per_cluster;
    uint32_t data_sectors = total_sectors - opts->reserved_sectors;
    uint32_t cluster_count = data_sectors / opts->sectors_per_cluster;
    
    /* FAT32 uses 4 bytes per entry */
    uint32_t fat_bytes = (cluster_count + 2) * 4;
    uint32_t fat_sectors = (fat_bytes + opts->sector_size - 1) / opts->sector_size;
    
    /* Recalculate with FAT overhead */
    data_sectors -= fat_sectors * opts->num_fats;
    
    opts->volume_size = size;
    
    return 0;
}

uft_fat_type_t uft_fat_type_for_size(uint64_t size)
{
    uint32_t sectors = size / 512;
    
    /* Approximate cluster counts */
    if (sectors < 8400) return UFT_FAT_TYPE_FAT12;      /* < ~4MB */
    if (sectors < 1048576) return UFT_FAT_TYPE_FAT16;   /* < ~512MB */
    return UFT_FAT_TYPE_FAT32;
}

/*===========================================================================
 * Formatting
 *===========================================================================*/

int uft_fat32_format(uint8_t *data, size_t size, const uft_fat32_format_opts_t *opts)
{
    if (!data || !opts || size < 32 * 1024 * 1024) return -1;
    
    /* Clear entire image */
    memset(data, 0, size);
    
    uft_fat32_bootsect_t *boot = (uft_fat32_bootsect_t *)data;
    
    /* Jump instruction */
    boot->jmp_boot[0] = 0xEB;
    boot->jmp_boot[1] = 0x58;  /* Jump to 0x5A */
    boot->jmp_boot[2] = 0x90;
    
    /* OEM name */
    memcpy(boot->oem_name, opts->oem_name, 8);
    
    /* BPB */
    boot->bytes_per_sector = opts->sector_size;
    boot->sectors_per_cluster = opts->sectors_per_cluster ? 
                                opts->sectors_per_cluster : 
                                uft_fat32_recommended_spc(size);
    boot->reserved_sectors = opts->reserved_sectors ? opts->reserved_sectors : 32;
    boot->num_fats = opts->num_fats ? opts->num_fats : 2;
    boot->root_entry_count = 0;  /* Always 0 for FAT32 */
    boot->total_sectors_16 = 0;  /* Always 0 for FAT32 */
    boot->media_type = 0xF8;     /* Fixed disk */
    boot->fat_size_16 = 0;       /* Always 0 for FAT32 */
    boot->sectors_per_track = 63;
    boot->num_heads = 255;
    boot->hidden_sectors = 0;
    boot->total_sectors_32 = size / opts->sector_size;
    
    /* FAT32 extended BPB */
    uint32_t data_sectors = boot->total_sectors_32 - boot->reserved_sectors;
    uint32_t cluster_count = data_sectors / boot->sectors_per_cluster;
    uint32_t fat_size = ((cluster_count + 2) * 4 + boot->bytes_per_sector - 1) / 
                        boot->bytes_per_sector;
    
    boot->fat_size_32 = fat_size;
    boot->ext_flags = 0;
    boot->fs_version = 0;
    boot->root_cluster = 2;  /* Root starts at cluster 2 */
    boot->fsinfo_sector = 1;
    boot->backup_boot_sector = opts->backup_boot ? opts->backup_boot : 6;
    
    /* Extended boot record */
    boot->drive_number = 0x80;
    boot->boot_signature = UFT_FAT_EXT_BOOT_SIG;
    boot->volume_serial = opts->volume_serial ? opts->volume_serial : 
                          (uint32_t)time(NULL);
    
    /* Volume label */
    if (opts->volume_label[0]) {
        memcpy(boot->volume_label, opts->volume_label, 11);
    } else {
        memcpy(boot->volume_label, "NO NAME    ", 11);
    }
    memcpy(boot->fs_type, "FAT32   ", 8);
    
    /* Boot signature */
    boot->signature = UFT_FAT_BOOT_SIG;
    
    /* Initialize FSInfo sector */
    uft_fat32_fsinfo_t *fsinfo = (uft_fat32_fsinfo_t *)(data + boot->bytes_per_sector);
    fsinfo->lead_sig = UFT_FAT32_FSINFO_SIG1;
    fsinfo->struct_sig = UFT_FAT32_FSINFO_SIG2;
    fsinfo->free_count = 0xFFFFFFFF;  /* Unknown */
    fsinfo->next_free = 3;  /* Start after root cluster */
    fsinfo->trail_sig = UFT_FAT32_FSINFO_SIG3;
    
    /* Copy boot sector to backup location */
    if (boot->backup_boot_sector > 0 && boot->backup_boot_sector < boot->reserved_sectors) {
        memcpy(data + boot->backup_boot_sector * boot->bytes_per_sector, 
               boot, sizeof(*boot));
        /* Backup FSInfo at backup+1 */
        memcpy(data + (boot->backup_boot_sector + 1) * boot->bytes_per_sector,
               fsinfo, sizeof(*fsinfo));
    }
    
    /* Initialize FATs */
    uint32_t fat_offset = boot->reserved_sectors * boot->bytes_per_sector;
    uint32_t *fat = (uint32_t *)(data + fat_offset);
    
    /* FAT[0] = media type | 0x0FFFFF00 */
    fat[0] = 0x0FFFFF00 | boot->media_type;
    /* FAT[1] = EOC marker */
    fat[1] = UFT_FAT32_EOF;
    /* FAT[2] = EOC for root directory (single cluster initially) */
    fat[2] = UFT_FAT32_EOF;
    
    /* Copy to second FAT if present */
    if (boot->num_fats > 1) {
        memcpy(data + fat_offset + fat_size * boot->bytes_per_sector,
               fat, fat_size * boot->bytes_per_sector);
    }
    
    /* Create root directory (just empty for now) */
    /* Volume label entry could be added here */
    
    return 0;
}

/*===========================================================================
 * FSInfo Management
 *===========================================================================*/

int uft_fat32_read_fsinfo(const uint8_t *data, const uft_fat32_bootsect_t *boot,
                          uft_fat32_fsinfo_t *info)
{
    if (!data || !boot || !info) return -1;
    
    uint32_t offset = boot->fsinfo_sector * boot->bytes_per_sector;
    const uft_fat32_fsinfo_t *src = (const uft_fat32_fsinfo_t *)(data + offset);
    
    /* Validate signatures */
    if (src->lead_sig != UFT_FAT32_FSINFO_SIG1) return -2;
    if (src->struct_sig != UFT_FAT32_FSINFO_SIG2) return -3;
    if (src->trail_sig != UFT_FAT32_FSINFO_SIG3) return -4;
    
    memcpy(info, src, sizeof(*info));
    return 0;
}

int uft_fat32_write_fsinfo(uint8_t *data, const uft_fat32_bootsect_t *boot,
                           const uft_fat32_fsinfo_t *info)
{
    if (!data || !boot || !info) return -1;
    
    uint32_t offset = boot->fsinfo_sector * boot->bytes_per_sector;
    memcpy(data + offset, info, sizeof(*info));
    
    /* Update backup if present */
    if (boot->backup_boot_sector > 0) {
        uint32_t backup_offset = (boot->backup_boot_sector + 1) * boot->bytes_per_sector;
        memcpy(data + backup_offset, info, sizeof(*info));
    }
    
    return 0;
}

int uft_fat32_update_fsinfo(uint8_t *data, const uft_fat32_bootsect_t *boot)
{
    if (!data || !boot) return -1;
    
    uft_fat32_fsinfo_t info;
    if (uft_fat32_read_fsinfo(data, boot, &info) != 0) return -2;
    
    /* Count free clusters */
    uint32_t total = uft_fat32_count_clusters(boot);
    uint32_t free_count = 0;
    uint32_t first_free = 0;
    
    for (uint32_t i = 2; i < total + 2; i++) {
        uint32_t entry = uft_fat32_get_entry(data, boot, i);
        if (uft_fat32_is_free(entry)) {
            free_count++;
            if (first_free == 0) first_free = i;
        }
    }
    
    info.free_count = free_count;
    info.next_free = first_free ? first_free : 2;
    
    return uft_fat32_write_fsinfo(data, boot, &info);
}

/*===========================================================================
 * FAT Operations
 *===========================================================================*/

uint32_t uft_fat32_get_entry(const uint8_t *data, const uft_fat32_bootsect_t *boot,
                             uint32_t cluster)
{
    if (!data || !boot || cluster < 2) return 0;
    
    uint32_t fat_offset = boot->reserved_sectors * boot->bytes_per_sector;
    uint32_t entry_offset = fat_offset + cluster * 4;
    
    uint32_t value;
    memcpy(&value, data + entry_offset, 4);
    
    return value & UFT_FAT32_CLUSTER_MASK;
}

int uft_fat32_set_entry(uint8_t *data, const uft_fat32_bootsect_t *boot,
                        uint32_t cluster, uint32_t value)
{
    if (!data || !boot || cluster < 2) return -1;
    
    uint32_t fat_offset = boot->reserved_sectors * boot->bytes_per_sector;
    uint32_t entry_offset = fat_offset + cluster * 4;
    
    /* Preserve upper 4 bits */
    uint32_t old_value;
    memcpy(&old_value, data + entry_offset, 4);
    value = (old_value & 0xF0000000) | (value & UFT_FAT32_CLUSTER_MASK);
    
    memcpy(data + entry_offset, &value, 4);
    
    /* Update second FAT */
    if (boot->num_fats > 1) {
        uint32_t fat2_offset = fat_offset + boot->fat_size_32 * boot->bytes_per_sector;
        memcpy(data + fat2_offset + cluster * 4, &value, 4);
    }
    
    return 0;
}

/*===========================================================================
 * Cluster Operations
 *===========================================================================*/

uint64_t uft_fat32_data_offset(const uft_fat32_bootsect_t *boot)
{
    if (!boot) return 0;
    
    uint32_t fat_sectors = boot->fat_size_32 * boot->num_fats;
    uint32_t first_data_sector = boot->reserved_sectors + fat_sectors;
    
    return (uint64_t)first_data_sector * boot->bytes_per_sector;
}

uint64_t uft_fat32_cluster_offset(const uft_fat32_bootsect_t *boot, uint32_t cluster)
{
    if (!boot || cluster < 2) return 0;
    
    uint64_t data_start = uft_fat32_data_offset(boot);
    uint32_t cluster_size = boot->bytes_per_sector * boot->sectors_per_cluster;
    
    return data_start + (uint64_t)(cluster - 2) * cluster_size;
}

uint32_t uft_fat32_count_clusters(const uft_fat32_bootsect_t *boot)
{
    if (!boot) return 0;
    
    uint32_t fat_sectors = boot->fat_size_32 * boot->num_fats;
    uint32_t data_sectors = boot->total_sectors_32 - boot->reserved_sectors - fat_sectors;
    
    return data_sectors / boot->sectors_per_cluster;
}

int uft_fat32_alloc_chain(uint8_t *data, const uft_fat32_bootsect_t *boot,
                          uint32_t count, uint32_t *first_cluster)
{
    if (!data || !boot || !first_cluster || count == 0) return -1;
    
    uint32_t total = uft_fat32_count_clusters(boot);
    uint32_t allocated = 0;
    uint32_t first = 0;
    uint32_t prev = 0;
    
    for (uint32_t i = 2; i < total + 2 && allocated < count; i++) {
        if (uft_fat32_is_free(uft_fat32_get_entry(data, boot, i))) {
            if (first == 0) {
                first = i;
            } else {
                uft_fat32_set_entry(data, boot, prev, i);
            }
            prev = i;
            allocated++;
        }
    }
    
    if (allocated < count) {
        /* Not enough space - free what we allocated */
        if (first) uft_fat32_free_chain(data, boot, first);
        return -2;  /* Disk full */
    }
    
    /* Mark last cluster as EOF */
    uft_fat32_set_entry(data, boot, prev, UFT_FAT32_EOF);
    
    *first_cluster = first;
    return 0;
}

uint32_t uft_fat32_free_chain(uint8_t *data, const uft_fat32_bootsect_t *boot,
                              uint32_t start)
{
    if (!data || !boot || start < 2) return 0;
    
    uint32_t count = 0;
    uint32_t cluster = start;
    uint32_t total = uft_fat32_count_clusters(boot) + 2;
    
    while (cluster >= 2 && cluster < total) {
        uint32_t next = uft_fat32_get_entry(data, boot, cluster);
        uft_fat32_set_entry(data, boot, cluster, UFT_FAT32_FREE);
        count++;
        
        if (uft_fat32_is_eof(next)) break;
        cluster = next;
    }
    
    return count;
}

/*===========================================================================
 * Backup Boot Sector
 *===========================================================================*/

int uft_fat32_write_backup_boot(uint8_t *data, const uft_fat32_bootsect_t *boot)
{
    if (!data || !boot) return -1;
    if (boot->backup_boot_sector == 0) return 0;  /* No backup configured */
    
    uint32_t backup_offset = boot->backup_boot_sector * boot->bytes_per_sector;
    memcpy(data + backup_offset, boot, boot->bytes_per_sector);
    
    return 0;
}

int uft_fat32_restore_from_backup(uint8_t *data)
{
    if (!data) return -1;
    
    const uft_fat32_bootsect_t *main = (const uft_fat32_bootsect_t *)data;
    if (main->backup_boot_sector == 0) return -2;
    
    uint32_t backup_offset = main->backup_boot_sector * main->bytes_per_sector;
    const uft_fat32_bootsect_t *backup = (const uft_fat32_bootsect_t *)(data + backup_offset);
    
    /* Validate backup */
    if (!uft_fat32_validate(backup)) return -3;
    
    memcpy(data, backup, main->bytes_per_sector);
    return 0;
}

bool uft_fat32_compare_backup(const uint8_t *data)
{
    if (!data) return false;
    
    const uft_fat32_bootsect_t *main = (const uft_fat32_bootsect_t *)data;
    if (main->backup_boot_sector == 0) return true;  /* No backup = matches */
    
    uint32_t backup_offset = main->backup_boot_sector * main->bytes_per_sector;
    
    return memcmp(data, data + backup_offset, main->bytes_per_sector) == 0;
}
