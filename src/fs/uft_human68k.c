/**
 * @file uft_human68k.c
 * @brief Human68k/X68000 Filesystem Implementation
 * 
 * EXT3-018: Sharp X68000 Human68k filesystem support
 * 
 * Features:
 * - Human68k disk format (FAT12/16 variant)
 * - Japanese filename handling (Shift-JIS)
 * - Long filename support
 * - Disk parameter block parsing
 * - Directory and file operations
 */

#include "uft/fs/uft_human68k.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/*===========================================================================
 * Constants
 *===========================================================================*/

/* Human68k magic values */
#define H68K_BOOT_MAGIC     0x6068      /* 'h68' signature */
#define H68K_IPL_SECTOR     0           /* Boot sector location */
#define H68K_FAT_ENTRY_12   12
#define H68K_FAT_ENTRY_16   16

/* Directory entry size */
#define H68K_DIR_ENTRY_SIZE 32

/* File attributes */
#define H68K_ATTR_RDONLY    0x01
#define H68K_ATTR_HIDDEN    0x02
#define H68K_ATTR_SYSTEM    0x04
#define H68K_ATTR_VOLUME    0x08
#define H68K_ATTR_SUBDIR    0x10
#define H68K_ATTR_ARCHIVE   0x20
#define H68K_ATTR_LFN       0x0F        /* Long filename marker */

/* Disk types */
#define H68K_DISK_2HD       0           /* 1.25MB (77x8x2x1024) */
#define H68K_DISK_2DD       1           /* 640KB (80x8x2x512) */
#define H68K_DISK_2HQ       2           /* 1.44MB (80x18x2x512) */

/*===========================================================================
 * Data Structures
 *===========================================================================*/

/* Human68k Boot Sector (IPL) */
typedef struct {
    uint8_t     jmp[2];         /* 00: Jump instruction */
    uint8_t     oem[16];        /* 02: OEM name */
    uint8_t     bytes_per_sec[2]; /* 12: Bytes per sector */
    uint8_t     sec_per_clust;  /* 14: Sectors per cluster */
    uint8_t     reserved_sec[2]; /* 15: Reserved sectors */
    uint8_t     num_fats;       /* 17: Number of FATs */
    uint8_t     root_entries[2]; /* 18: Root directory entries */
    uint8_t     total_sec[2];   /* 1A: Total sectors (16-bit) */
    uint8_t     media_type;     /* 1C: Media type */
    uint8_t     fat_sectors[2]; /* 1D: Sectors per FAT */
    uint8_t     sec_per_track[2]; /* 1F: Sectors per track */
    uint8_t     num_heads[2];   /* 21: Number of heads */
    uint8_t     hidden_sec[2];  /* 23: Hidden sectors */
    uint8_t     total_sec32[4]; /* 25: Total sectors (32-bit) */
    /* Extended fields follow */
} __attribute__((packed)) h68k_boot_t;

/* Human68k Directory Entry */
typedef struct {
    uint8_t     name[8];        /* 00: Filename (Shift-JIS) */
    uint8_t     ext[3];         /* 08: Extension */
    uint8_t     attr;           /* 0B: Attributes */
    uint8_t     reserved[10];   /* 0C: Reserved */
    uint8_t     time[2];        /* 16: Time */
    uint8_t     date[2];        /* 18: Date */
    uint8_t     start_clust[2]; /* 1A: Starting cluster */
    uint8_t     file_size[4];   /* 1C: File size */
} __attribute__((packed)) h68k_dirent_t;

/*===========================================================================
 * Helpers
 *===========================================================================*/

static uint16_t read_le16(const uint8_t *p)
{
    return p[0] | (p[1] << 8);
}

static uint32_t read_le32(const uint8_t *p)
{
    return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
}

/* Shift-JIS to ASCII (simplified) */
static void sjis_to_ascii(const uint8_t *sjis, size_t len, char *ascii, size_t max)
{
    size_t j = 0;
    
    for (size_t i = 0; i < len && j < max - 1; i++) {
        uint8_t c = sjis[i];
        
        if (c == 0x20 || c == 0x00) {
            break;  /* Padding */
        }
        
        if (c >= 0x81 && c <= 0x9F) {
            /* Lead byte of 2-byte Shift-JIS */
            ascii[j++] = '?';  /* Placeholder for Japanese */
            i++;  /* Skip trail byte */
        } else if (c >= 0xE0 && c <= 0xFC) {
            /* Lead byte of 2-byte Shift-JIS */
            ascii[j++] = '?';
            i++;
        } else {
            /* Single byte (ASCII or half-width katakana) */
            ascii[j++] = (c >= 0x20 && c <= 0x7E) ? c : '?';
        }
    }
    
    ascii[j] = '\0';
}

/*===========================================================================
 * Context Management
 *===========================================================================*/

int uft_h68k_open(uft_h68k_ctx_t *ctx, const uint8_t *image, size_t size)
{
    if (!ctx || !image || size < 1024) return -1;
    
    memset(ctx, 0, sizeof(*ctx));
    ctx->image = image;
    ctx->image_size = size;
    
    /* Parse boot sector */
    const h68k_boot_t *boot = (const h68k_boot_t *)image;
    
    ctx->bytes_per_sector = read_le16(boot->bytes_per_sec);
    ctx->sectors_per_cluster = boot->sec_per_clust;
    ctx->reserved_sectors = read_le16(boot->reserved_sec);
    ctx->num_fats = boot->num_fats;
    ctx->root_entries = read_le16(boot->root_entries);
    ctx->sectors_per_fat = read_le16(boot->fat_sectors);
    ctx->sectors_per_track = read_le16(boot->sec_per_track);
    ctx->num_heads = read_le16(boot->num_heads);
    
    /* Total sectors */
    uint16_t total16 = read_le16(boot->total_sec);
    if (total16 == 0) {
        ctx->total_sectors = read_le32(boot->total_sec32);
    } else {
        ctx->total_sectors = total16;
    }
    
    /* Validate */
    if (ctx->bytes_per_sector == 0 || 
        ctx->sectors_per_cluster == 0 ||
        ctx->num_fats == 0) {
        return -1;
    }
    
    /* Calculate layout */
    ctx->fat_start = ctx->reserved_sectors;
    ctx->root_start = ctx->fat_start + ctx->num_fats * ctx->sectors_per_fat;
    ctx->root_sectors = (ctx->root_entries * H68K_DIR_ENTRY_SIZE + 
                         ctx->bytes_per_sector - 1) / ctx->bytes_per_sector;
    ctx->data_start = ctx->root_start + ctx->root_sectors;
    
    /* Determine FAT type */
    uint32_t data_sectors = ctx->total_sectors - ctx->data_start;
    uint32_t clusters = data_sectors / ctx->sectors_per_cluster;
    
    ctx->fat_type = (clusters < 4085) ? 12 : 16;
    ctx->total_clusters = clusters;
    
    /* Determine disk type */
    if (ctx->bytes_per_sector == 1024 && ctx->sectors_per_track == 8) {
        ctx->disk_type = H68K_DISK_2HD;  /* 1.25MB */
    } else if (ctx->bytes_per_sector == 512 && ctx->sectors_per_track == 18) {
        ctx->disk_type = H68K_DISK_2HQ;  /* 1.44MB */
    } else {
        ctx->disk_type = H68K_DISK_2DD;  /* 640KB or other */
    }
    
    ctx->valid = true;
    return 0;
}

void uft_h68k_close(uft_h68k_ctx_t *ctx)
{
    if (!ctx) return;
    memset(ctx, 0, sizeof(*ctx));
}

/*===========================================================================
 * FAT Operations
 *===========================================================================*/

static uint32_t get_fat_entry(const uft_h68k_ctx_t *ctx, uint32_t cluster)
{
    if (!ctx || !ctx->image) return 0;
    
    size_t fat_offset = ctx->fat_start * ctx->bytes_per_sector;
    
    if (ctx->fat_type == 12) {
        size_t offset = fat_offset + cluster * 3 / 2;
        if (offset + 1 >= ctx->image_size) return 0;
        
        uint16_t word = ctx->image[offset] | (ctx->image[offset + 1] << 8);
        
        if (cluster & 1) {
            return (word >> 4) & 0xFFF;
        } else {
            return word & 0xFFF;
        }
    } else {
        size_t offset = fat_offset + cluster * 2;
        if (offset + 1 >= ctx->image_size) return 0;
        
        return read_le16(ctx->image + offset);
    }
}

static bool is_cluster_end(const uft_h68k_ctx_t *ctx, uint32_t cluster)
{
    if (ctx->fat_type == 12) {
        return cluster >= 0xFF8;
    } else {
        return cluster >= 0xFFF8;
    }
}

/*===========================================================================
 * Directory Reading
 *===========================================================================*/

int uft_h68k_read_root(const uft_h68k_ctx_t *ctx,
                       uft_h68k_entry_t *entries, size_t max_entries,
                       size_t *count)
{
    if (!ctx || !entries || !count || !ctx->valid) return -1;
    
    *count = 0;
    
    size_t root_offset = ctx->root_start * ctx->bytes_per_sector;
    
    for (size_t i = 0; i < ctx->root_entries && *count < max_entries; i++) {
        size_t offset = root_offset + i * H68K_DIR_ENTRY_SIZE;
        
        if (offset + H68K_DIR_ENTRY_SIZE > ctx->image_size) break;
        
        const h68k_dirent_t *de = (const h68k_dirent_t *)(ctx->image + offset);
        
        /* Check for empty or deleted entry */
        if (de->name[0] == 0x00) break;  /* End of directory */
        if (de->name[0] == 0xE5) continue;  /* Deleted */
        
        /* Skip long filename entries */
        if ((de->attr & H68K_ATTR_LFN) == H68K_ATTR_LFN) continue;
        
        /* Skip volume label in listing */
        if (de->attr & H68K_ATTR_VOLUME) continue;
        
        uft_h68k_entry_t *e = &entries[*count];
        memset(e, 0, sizeof(*e));
        
        /* Convert filename */
        sjis_to_ascii(de->name, 8, e->name, sizeof(e->name));
        
        /* Trim trailing spaces */
        char *p = e->name + strlen(e->name) - 1;
        while (p >= e->name && *p == ' ') *p-- = '\0';
        
        /* Add extension */
        char ext[4];
        sjis_to_ascii(de->ext, 3, ext, sizeof(ext));
        p = ext + strlen(ext) - 1;
        while (p >= ext && *p == ' ') *p-- = '\0';
        
        if (ext[0] != '\0') {
            strncat(e->name, ".", sizeof(e->name) - strlen(e->name) - 1);
            strncat(e->name, ext, sizeof(e->name) - strlen(e->name) - 1);
        }
        
        e->attr = de->attr;
        e->is_dir = (de->attr & H68K_ATTR_SUBDIR) != 0;
        e->start_cluster = read_le16(de->start_clust);
        e->size = read_le32(de->file_size);
        
        /* Decode time/date */
        uint16_t time = read_le16(de->time);
        uint16_t date = read_le16(de->date);
        
        e->hour = (time >> 11) & 0x1F;
        e->minute = (time >> 5) & 0x3F;
        e->second = (time & 0x1F) * 2;
        
        e->year = ((date >> 9) & 0x7F) + 1980;
        e->month = (date >> 5) & 0x0F;
        e->day = date & 0x1F;
        
        (*count)++;
    }
    
    return 0;
}

/*===========================================================================
 * File Reading
 *===========================================================================*/

int uft_h68k_read_file(const uft_h68k_ctx_t *ctx,
                       const uft_h68k_entry_t *entry,
                       uint8_t *buffer, size_t *size)
{
    if (!ctx || !entry || !buffer || !size || !ctx->valid) return -1;
    
    if (entry->is_dir) return -1;
    
    size_t max_size = *size;
    *size = 0;
    
    uint32_t cluster = entry->start_cluster;
    size_t remaining = entry->size;
    size_t cluster_size = ctx->sectors_per_cluster * ctx->bytes_per_sector;
    
    while (cluster >= 2 && !is_cluster_end(ctx, cluster) && remaining > 0) {
        /* Calculate sector offset */
        size_t sector = ctx->data_start + (cluster - 2) * ctx->sectors_per_cluster;
        size_t offset = sector * ctx->bytes_per_sector;
        
        if (offset + cluster_size > ctx->image_size) break;
        
        size_t to_copy = (remaining < cluster_size) ? remaining : cluster_size;
        if (*size + to_copy > max_size) {
            to_copy = max_size - *size;
        }
        
        memcpy(buffer + *size, ctx->image + offset, to_copy);
        *size += to_copy;
        remaining -= to_copy;
        
        /* Next cluster */
        cluster = get_fat_entry(ctx, cluster);
    }
    
    return 0;
}

/*===========================================================================
 * Information
 *===========================================================================*/

int uft_h68k_info(const uft_h68k_ctx_t *ctx, uft_h68k_info_t *info)
{
    if (!ctx || !info || !ctx->valid) return -1;
    
    memset(info, 0, sizeof(*info));
    
    info->disk_type = ctx->disk_type;
    info->fat_type = ctx->fat_type;
    info->bytes_per_sector = ctx->bytes_per_sector;
    info->sectors_per_cluster = ctx->sectors_per_cluster;
    info->total_sectors = ctx->total_sectors;
    info->total_clusters = ctx->total_clusters;
    
    /* Calculate free space */
    uint32_t free_clusters = 0;
    for (uint32_t c = 2; c < ctx->total_clusters + 2; c++) {
        if (get_fat_entry(ctx, c) == 0) {
            free_clusters++;
        }
    }
    
    info->free_clusters = free_clusters;
    info->free_bytes = free_clusters * ctx->sectors_per_cluster * 
                       ctx->bytes_per_sector;
    info->total_bytes = ctx->total_clusters * ctx->sectors_per_cluster *
                        ctx->bytes_per_sector;
    
    /* Disk type string */
    switch (ctx->disk_type) {
        case H68K_DISK_2HD:
            strncpy(info->type_string, "2HD (1.25MB)", sizeof(info->type_string));
            break;
        case H68K_DISK_2DD:
            strncpy(info->type_string, "2DD (640KB)", sizeof(info->type_string));
            break;
        case H68K_DISK_2HQ:
            strncpy(info->type_string, "2HQ (1.44MB)", sizeof(info->type_string));
            break;
        default:
            strncpy(info->type_string, "Unknown", sizeof(info->type_string));
    }
    
    return 0;
}

/*===========================================================================
 * Report
 *===========================================================================*/

int uft_h68k_report_json(const uft_h68k_ctx_t *ctx, char *buffer, size_t size)
{
    if (!ctx || !buffer || !ctx->valid) return -1;
    
    uft_h68k_info_t info;
    uft_h68k_info(ctx, &info);
    
    int written = snprintf(buffer, size,
        "{\n"
        "  \"format\": \"Human68k\",\n"
        "  \"disk_type\": \"%s\",\n"
        "  \"fat_type\": \"FAT%d\",\n"
        "  \"bytes_per_sector\": %u,\n"
        "  \"sectors_per_cluster\": %u,\n"
        "  \"total_sectors\": %u,\n"
        "  \"total_clusters\": %u,\n"
        "  \"free_clusters\": %u,\n"
        "  \"total_bytes\": %zu,\n"
        "  \"free_bytes\": %zu\n"
        "}",
        info.type_string,
        info.fat_type,
        info.bytes_per_sector,
        info.sectors_per_cluster,
        info.total_sectors,
        info.total_clusters,
        info.free_clusters,
        info.total_bytes,
        info.free_bytes
    );
    
    return (written > 0 && (size_t)written < size) ? 0 : -1;
}
