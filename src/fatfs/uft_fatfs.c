/**
 * @file uft_fatfs.c
 * @brief FatFs Integration Implementation
 * 
 * @copyright UFT Project 2026
 */

#include "uft/uft_fatfs.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ═══════════════════════════════════════════════════════════════════════════════
 * Floppy Geometry Tables
 * ═══════════════════════════════════════════════════════════════════════════════ */

static const uft_floppy_geometry_t floppy_geometries[] = {
    /* UFT_FLOPPY_160K */
    { 40, 1,  8, 512,   320,   163840, "5.25\" SS/SD 160KB" },
    /* UFT_FLOPPY_180K */
    { 40, 1,  9, 512,   360,   184320, "5.25\" SS/SD 180KB" },
    /* UFT_FLOPPY_320K */
    { 40, 2,  8, 512,   640,   327680, "5.25\" DS/SD 320KB" },
    /* UFT_FLOPPY_360K */
    { 40, 2,  9, 512,   720,   368640, "5.25\" DS/DD 360KB" },
    /* UFT_FLOPPY_720K */
    { 80, 2,  9, 512,  1440,   737280, "3.5\" DS/DD 720KB" },
    /* UFT_FLOPPY_1200K */
    { 80, 2, 15, 512,  2400,  1228800, "5.25\" HD 1.2MB" },
    /* UFT_FLOPPY_1440K */
    { 80, 2, 18, 512,  2880,  1474560, "3.5\" HD 1.44MB" },
    /* UFT_FLOPPY_2880K */
    { 80, 2, 36, 512,  5760,  2949120, "3.5\" ED 2.88MB" },
};

const uft_floppy_geometry_t* uft_floppy_get_geometry(uft_floppy_type_t type)
{
    if (type >= UFT_FLOPPY_CUSTOM) return NULL;
    return &floppy_geometries[type];
}

uft_floppy_type_t uft_floppy_detect_type(size_t image_size)
{
    for (int i = 0; i < UFT_FLOPPY_CUSTOM; i++) {
        if (floppy_geometries[i].total_bytes == image_size) {
            return (uft_floppy_type_t)i;
        }
    }
    return UFT_FLOPPY_CUSTOM;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * FAT Image Handle
 * ═══════════════════════════════════════════════════════════════════════════════ */

struct uft_fat_image {
    FILE    *fp;
    char    path[260];
    bool    readonly;
    uint32_t sectors;
    uint16_t sector_size;
    uft_floppy_type_t type;
    uft_fat_boot_sector_t boot;
};

/* ═══════════════════════════════════════════════════════════════════════════════
 * Boot Sector Parsing
 * ═══════════════════════════════════════════════════════════════════════════════ */

int uft_fat_parse_boot_sector(const uint8_t *data, uft_fat_boot_sector_t *info)
{
    if (!data || !info) return -1;
    
    memset(info, 0, sizeof(*info));
    
    /* OEM Name (offset 3, 8 bytes) */
    memcpy(info->oem_name, data + 3, 8);
    info->oem_name[8] = '\0';
    
    /* BPB (BIOS Parameter Block) */
    info->bytes_per_sector = data[11] | (data[12] << 8);
    info->sectors_per_cluster = data[13];
    info->reserved_sectors = data[14] | (data[15] << 8);
    info->fat_count = data[16];
    info->root_entries = data[17] | (data[18] << 8);
    info->total_sectors_16 = data[19] | (data[20] << 8);
    info->media_type = data[21];
    info->sectors_per_fat = data[22] | (data[23] << 8);
    info->sectors_per_track = data[24] | (data[25] << 8);
    info->heads = data[26] | (data[27] << 8);
    info->hidden_sectors = data[28] | (data[29] << 8) | 
                          (data[30] << 16) | (data[31] << 24);
    info->total_sectors_32 = data[32] | (data[33] << 8) |
                            (data[34] << 16) | (data[35] << 24);
    
    /* Extended BPB (FAT12/16) - Volume Label at offset 43 */
    memcpy(info->volume_label, data + 43, 11);
    info->volume_label[11] = '\0';
    
    /* Trim trailing spaces */
    for (int i = 10; i >= 0 && info->volume_label[i] == ' '; i--) {
        info->volume_label[i] = '\0';
    }
    
    /* FS Type at offset 54 */
    memcpy(info->fs_type, data + 54, 8);
    info->fs_type[8] = '\0';
    
    return 0;
}

const char* uft_fat_detect_type(const uft_fat_boot_sector_t *info)
{
    if (!info) return "Unknown";
    
    /* Calculate total sectors */
    uint32_t total = info->total_sectors_16 ? 
                     info->total_sectors_16 : info->total_sectors_32;
    
    /* Calculate data sectors */
    uint32_t root_sectors = ((info->root_entries * 32) + 
                            (info->bytes_per_sector - 1)) / 
                            info->bytes_per_sector;
    uint32_t fat_sectors = info->fat_count * info->sectors_per_fat;
    uint32_t data_sectors = total - info->reserved_sectors - 
                           fat_sectors - root_sectors;
    
    /* Calculate clusters */
    uint32_t clusters = data_sectors / info->sectors_per_cluster;
    
    /* Determine FAT type by cluster count */
    if (clusters < 4085) {
        return "FAT12";
    } else if (clusters < 65525) {
        return "FAT16";
    } else {
        return "FAT32";
    }
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Image Operations
 * ═══════════════════════════════════════════════════════════════════════════════ */

uft_fat_image_t* uft_fat_open(const char *path, bool readonly)
{
    if (!path) return NULL;
    
    FILE *fp = fopen(path, readonly ? "rb" : "r+b");
    if (!fp && !readonly) {
        fp = fopen(path, "rb");
        readonly = true;
    }
    if (!fp) return NULL;
    
    /* Get file size */
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    if (size < 512) {
        fclose(fp);
        return NULL;
    }
    
    /* Read boot sector */
    uint8_t boot[512];
    if (fread(boot, 1, 512, fp) != 512) {
        fclose(fp);
        return NULL;
    }
    fseek(fp, 0, SEEK_SET);
    
    /* Allocate handle */
    uft_fat_image_t *img = calloc(1, sizeof(uft_fat_image_t));
    if (!img) {
        fclose(fp);
        return NULL;
    }
    
    img->fp = fp;
    img->readonly = readonly;
    img->sectors = (uint32_t)(size / 512);
    img->sector_size = 512;
    img->type = uft_floppy_detect_type(size);
    strncpy(img->path, path, sizeof(img->path) - 1);
    
    uft_fat_parse_boot_sector(boot, &img->boot);
    
    return img;
}

void uft_fat_close(uft_fat_image_t *img)
{
    if (img) {
        if (img->fp) fclose(img->fp);
        free(img);
    }
}

int uft_fat_get_info(const uft_fat_image_t *img,
                     uft_floppy_geometry_t *geometry,
                     uint32_t *free_clusters,
                     uint32_t *total_clusters)
{
    if (!img) return -1;
    
    if (geometry) {
        const uft_floppy_geometry_t *geo = uft_floppy_get_geometry(img->type);
        if (geo) {
            *geometry = *geo;
        } else {
            geometry->cylinders = 80;
            geometry->heads = 2;
            geometry->sectors = img->sectors / 160;
            geometry->sector_size = img->sector_size;
            geometry->total_sectors = img->sectors;
            geometry->total_bytes = img->sectors * img->sector_size;
            geometry->name = "Custom";
        }
    }
    
    if (total_clusters) {
        *total_clusters = img->sectors / img->boot.sectors_per_cluster;
    }
    
    /* Free clusters would need FAT scanning */
    if (free_clusters) {
        *free_clusters = 0;  /* Not implemented */
    }
    
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Directory Operations (Simplified - would use full FatFs for production)
 * ═══════════════════════════════════════════════════════════════════════════════ */

int uft_fat_list_dir(uft_fat_image_t *img, const char *path,
                     uft_fat_entry_t *entries, int max_entries)
{
    if (!img || !entries || max_entries <= 0) return -1;
    
    /* This is a simplified implementation */
    /* In production, would use FatFs f_opendir/f_readdir */
    
    /* Calculate root directory location */
    uint32_t root_start = img->boot.reserved_sectors + 
                         (img->boot.fat_count * img->boot.sectors_per_fat);
    uint32_t root_sectors = ((img->boot.root_entries * 32) + 511) / 512;
    
    /* Seek to root directory */
    if (fseek(img->fp, root_start * 512, SEEK_SET) != 0) return -1;
    
    int count = 0;
    for (uint32_t i = 0; i < img->boot.root_entries && count < max_entries; i++) {
        uint8_t entry[32];
        if (fread(entry, 1, 32, img->fp) != 32) break;
        
        /* Check entry validity */
        if (entry[0] == 0x00) break;      /* End of directory */
        if (entry[0] == 0xE5) continue;   /* Deleted entry */
        if (entry[11] == 0x0F) continue;  /* LFN entry */
        
        /* Parse 8.3 filename */
        memset(&entries[count], 0, sizeof(uft_fat_entry_t));
        
        char name[9], ext[4];
        memcpy(name, entry, 8);
        name[8] = '\0';
        memcpy(ext, entry + 8, 3);
        ext[3] = '\0';
        
        /* Trim trailing spaces */
        for (int j = 7; j >= 0 && name[j] == ' '; j--) name[j] = '\0';
        for (int j = 2; j >= 0 && ext[j] == ' '; j--) ext[j] = '\0';
        
        if (ext[0]) {
            snprintf(entries[count].name, sizeof(entries[count].name),
                    "%s.%s", name, ext);
        } else {
            strncpy(entries[count].name, name, sizeof(entries[count].name) - 1);
            entries[count].name[sizeof(entries[count].name) - 1] = '\0';
        }
        
        /* Attributes */
        entries[count].attr = entry[11];
        entries[count].is_dir = (entry[11] & 0x10) != 0;
        entries[count].is_readonly = (entry[11] & 0x01) != 0;
        entries[count].is_hidden = (entry[11] & 0x02) != 0;
        entries[count].is_system = (entry[11] & 0x04) != 0;
        
        /* Size (offset 28, 4 bytes) */
        entries[count].size = entry[28] | (entry[29] << 8) |
                             (entry[30] << 16) | (entry[31] << 24);
        
        /* Date/Time */
        entries[count].time = entry[22] | (entry[23] << 8);
        entries[count].date = entry[24] | (entry[25] << 8);
        
        count++;
    }
    
    return count;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Image Creation
 * ═══════════════════════════════════════════════════════════════════════════════ */

int uft_fat_create_image(const char *path, uft_floppy_type_t type,
                         const char *label)
{
    if (!path || type >= UFT_FLOPPY_CUSTOM) return -1;
    
    const uft_floppy_geometry_t *geo = uft_floppy_get_geometry(type);
    if (!geo) return -1;
    
    FILE *fp = fopen(path, "wb");
    if (!fp) return -1;
    
    /* Write zero-filled image */
    uint8_t zero[512] = {0};
    for (uint32_t i = 0; i < geo->total_sectors; i++) {
        if (fwrite(zero, 1, 512, fp) != 512) {
            fclose(fp);
            return -1;
        }
    }
    
    /* Create boot sector */
    uint8_t boot[512] = {0};
    
    /* Jump instruction */
    boot[0] = 0xEB;
    boot[1] = 0x3C;
    boot[2] = 0x90;
    
    /* OEM Name */
    memcpy(boot + 3, "UFT 3.8 ", 8);
    
    /* BPB */
    boot[11] = 0x00; boot[12] = 0x02;  /* Bytes per sector: 512 */
    boot[13] = 1;                       /* Sectors per cluster */
    boot[14] = 1; boot[15] = 0;         /* Reserved sectors */
    boot[16] = 2;                       /* FAT count */
    boot[17] = 0x70; boot[18] = 0x00;   /* Root entries: 112 */
    boot[19] = geo->total_sectors & 0xFF;
    boot[20] = (geo->total_sectors >> 8) & 0xFF;
    boot[21] = 0xF0;                    /* Media type (floppy) */
    
    /* Sectors per FAT (approximate) */
    uint16_t spf = (geo->total_sectors / 3 + 511) / 512;
    if (spf < 1) spf = 1;
    if (spf > 12) spf = 9;  /* Common values for floppies */
    boot[22] = spf & 0xFF;
    boot[23] = (spf >> 8) & 0xFF;
    
    boot[24] = geo->sectors; boot[25] = 0;  /* Sectors per track */
    boot[26] = geo->heads; boot[27] = 0;    /* Heads */
    
    /* Extended BPB */
    boot[36] = 0x00;        /* Drive number */
    boot[38] = 0x29;        /* Extended boot signature */
    boot[39] = 0x12;        /* Volume serial (random) */
    boot[40] = 0x34;
    boot[41] = 0x56;
    boot[42] = 0x78;
    
    /* Volume label */
    if (label) {
        size_t len = strlen(label);
        if (len > 11) len = 11;
        memcpy(boot + 43, label, len);
        for (size_t i = len; i < 11; i++) boot[43 + i] = ' ';
    } else {
        memcpy(boot + 43, "NO NAME    ", 11);
    }
    
    /* FS Type */
    memcpy(boot + 54, "FAT12   ", 8);
    
    /* Boot signature */
    boot[510] = 0x55;
    boot[511] = 0xAA;
    
    /* Write boot sector */
    if (fseek(fp, 0, SEEK_SET) != 0) return -1;
    fwrite(boot, 1, 512, fp);
    
    /* Initialize FAT (first entries) */
    uint8_t fat[512] = {0};
    fat[0] = 0xF0;  /* Media type */
    fat[1] = 0xFF;
    fat[2] = 0xFF;
    
    if (fseek(fp, 512, SEEK_SET) != 0) return -1;
    fwrite(fat, 1, 512, fp);
    
    /* Second FAT copy */
    if (fseek(fp, 512 + spf * 512, SEEK_SET) != 0) return -1;
    fwrite(fat, 1, 512, fp);
    if (ferror(fp)) {
        fclose(fp);
        return UFT_ERR_IO;
    }
    
        
    fclose(fp);
return 0;
}

int uft_fat_format(uft_fat_image_t *img, const char *label)
{
    if (!img || img->readonly) return -1;
    
    /* Would use FatFs f_mkfs() in production */
    /* For now, just update volume label */
    
    return -1;  /* Not implemented */
}
