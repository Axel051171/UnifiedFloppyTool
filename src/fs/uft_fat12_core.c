/**
 * @file uft_fat12_core.c
 * @brief FAT12/FAT16 Core Implementation
 * @version 3.6.0
 * 
 * Lifecycle, detection, volume info, FAT table operations
 */

#include "uft/fs/uft_fat12.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/*===========================================================================
 * Internal Helpers
 *===========================================================================*/

/** Read little-endian 16-bit */
static inline uint16_t read_le16(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

/** Read little-endian 32-bit */
static inline uint32_t read_le32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/** Write little-endian 16-bit */
static inline void write_le16(uint8_t *p, uint16_t v) {
    p[0] = v & 0xFF;
    p[1] = (v >> 8) & 0xFF;
}

/** Write little-endian 32-bit */
static inline void write_le32(uint8_t *p, uint32_t v) {
    p[0] = v & 0xFF;
    p[1] = (v >> 8) & 0xFF;
    p[2] = (v >> 16) & 0xFF;
    p[3] = (v >> 24) & 0xFF;
}

/*===========================================================================
 * Error Messages
 *===========================================================================*/

static const char *error_messages[] = {
    "Success",                          /* 0 */
    "Invalid parameter or data",        /* -1 */
    "Out of memory",                    /* -2 */
    "I/O error",                        /* -3 */
    "File or directory not found",      /* -4 */
    "File or directory already exists", /* -5 */
    "Disk full",                        /* -6 */
    "Directory not empty",              /* -7 */
    "Read-only filesystem",             /* -8 */
    "Bad cluster chain",                /* -9 */
    "Filename too long",                /* -10 */
    "Invalid filename",                 /* -11 */
};

const char *uft_fat_strerror(int error) {
    if (error >= 0) return error_messages[0];
    error = -error;
    if (error < (int)(sizeof(error_messages)/sizeof(error_messages[0]))) {
        return error_messages[error];
    }
    return "Unknown error";
}

/*===========================================================================
 * Lifecycle
 *===========================================================================*/

uft_fat_ctx_t *uft_fat_create(void) {
    uft_fat_ctx_t *ctx = calloc(1, sizeof(uft_fat_ctx_t));
    return ctx;
}

void uft_fat_destroy(uft_fat_ctx_t *ctx) {
    if (!ctx) return;
    
    if (ctx->owns_data && ctx->data) {
        free(ctx->data);
    }
    if (ctx->fat_cache) {
        free(ctx->fat_cache);
    }
    free(ctx);
}

/*===========================================================================
 * Detection
 *===========================================================================*/

/**
 * @brief Check if BPB values are valid
 */
static bool validate_bpb(const uft_fat_bootsect_t *boot, size_t image_size) {
    /* Check basic sanity */
    uint16_t bps = read_le16((const uint8_t*)&boot->bytes_per_sector);
    if (bps != 512 && bps != 1024 && bps != 2048 && bps != 4096) {
        return false;
    }
    
    uint8_t spc = boot->sectors_per_cluster;
    if (spc == 0 || (spc & (spc - 1)) != 0) {
        return false;  /* Must be power of 2 */
    }
    
    uint16_t reserved = read_le16((const uint8_t*)&boot->reserved_sectors);
    if (reserved == 0) {
        return false;
    }
    
    uint8_t nfats = boot->num_fats;
    if (nfats == 0 || nfats > 4) {
        return false;
    }
    
    uint16_t root_entries = read_le16((const uint8_t*)&boot->root_entry_count);
    if (root_entries == 0) {
        /* FAT32 has root_entries = 0, but we don't support that for floppies */
        return false;
    }
    
    /* Check total sectors */
    uint16_t total16 = read_le16((const uint8_t*)&boot->total_sectors_16);
    uint32_t total32 = read_le32((const uint8_t*)&boot->total_sectors_32);
    uint32_t total = (total16 != 0) ? total16 : total32;
    
    if (total == 0) {
        return false;
    }
    
    /* Check if size matches (with some tolerance) */
    size_t expected_size = (size_t)total * bps;
    if (expected_size > image_size * 2 || expected_size < image_size / 2) {
        return false;
    }
    
    return true;
}

/**
 * @brief Determine FAT type from cluster count
 */
static uft_fat_type_t determine_fat_type(uint32_t cluster_count) {
    if (cluster_count < 4085) {
        return UFT_FAT_TYPE_FAT12;
    } else if (cluster_count < 65525) {
        return UFT_FAT_TYPE_FAT16;
    } else {
        return UFT_FAT_TYPE_FAT32;
    }
}

const uft_fat_geometry_t *uft_fat_geometry_from_size(size_t size) {
    uint32_t sectors = (uint32_t)(size / UFT_FAT_SECTOR_SIZE);
    
    for (const uft_fat_geometry_t *g = uft_fat_std_geometries; g->name; g++) {
        if (g->total_sectors == sectors) {
            return g;
        }
    }
    return NULL;
}

uft_fat_platform_t uft_fat_detect_platform(const uft_fat_bootsect_t *boot) {
    /* Check OEM name for platform hints */
    if (memcmp(boot->oem_name, "MSX", 3) == 0) {
        return UFT_FAT_PLATFORM_MSX;
    }
    if (memcmp(boot->oem_name, "Human", 5) == 0) {
        return UFT_FAT_PLATFORM_H68K;
    }
    if (memcmp(boot->oem_name, "X68", 3) == 0) {
        return UFT_FAT_PLATFORM_H68K;
    }
    if (memcmp(boot->oem_name, "NECPC-98", 8) == 0 ||
        memcmp(boot->oem_name, "NEC", 3) == 0) {
        return UFT_FAT_PLATFORM_PC98;
    }
    
    /* Check for Atari ST specific values */
    uint16_t sectors = read_le16((const uint8_t*)&boot->total_sectors_16);
    uint8_t media = boot->media_type;
    
    /* Atari ST often uses 0xF8 for single-sided */
    if (media == 0xF8 && (sectors == 720 || sectors == 1440)) {
        return UFT_FAT_PLATFORM_ATARI;
    }
    
    return UFT_FAT_PLATFORM_PC;
}

int uft_fat_detect(const uint8_t *data, size_t size, uft_fat_detect_t *result) {
    if (!data || !result || size < 512) {
        return UFT_FAT_ERR_INVALID;
    }
    
    memset(result, 0, sizeof(*result));
    
    const uft_fat_bootsect_t *boot = (const uft_fat_bootsect_t *)data;
    
    /* Check boot signature */
    uint16_t sig = read_le16((const uint8_t*)&boot->signature);
    if (sig != UFT_FAT_BOOT_SIG) {
        result->boot_sig_missing = true;
        /* Don't fail yet - some old disks don't have signature */
    }
    
    /* Validate BPB */
    if (!validate_bpb(boot, size)) {
        /* Try heuristics for unusual formats */
        result->geometry = uft_fat_geometry_from_size(size);
        if (result->geometry) {
            /* Might be bootable floppy without valid BPB */
            result->valid = false;
            result->confidence = 20;
            snprintf(result->description, sizeof(result->description),
                     "Possible %s (no valid BPB)", result->geometry->name);
            return 0;
        }
        result->valid = false;
        result->confidence = 0;
        snprintf(result->description, sizeof(result->description),
                 "Not a FAT filesystem");
        return 0;
    }
    
    /* Extract BPB values */
    uint16_t bps = read_le16((const uint8_t*)&boot->bytes_per_sector);
    uint8_t spc = boot->sectors_per_cluster;
    uint16_t reserved = read_le16((const uint8_t*)&boot->reserved_sectors);
    uint8_t nfats = boot->num_fats;
    uint16_t root_entries = read_le16((const uint8_t*)&boot->root_entry_count);
    uint16_t fat_size = read_le16((const uint8_t*)&boot->fat_size_16);
    
    uint16_t total16 = read_le16((const uint8_t*)&boot->total_sectors_16);
    uint32_t total32 = read_le32((const uint8_t*)&boot->total_sectors_32);
    uint32_t total = (total16 != 0) ? total16 : total32;
    
    /* Calculate layout */
    uint32_t root_dir_sectors = ((root_entries * 32) + (bps - 1)) / bps;
    uint32_t data_start = reserved + (nfats * fat_size) + root_dir_sectors;
    uint32_t data_sectors = total - data_start;
    uint32_t data_clusters = data_sectors / spc;
    
    /* Determine FAT type */
    result->type = determine_fat_type(data_clusters);
    if (result->type == UFT_FAT_TYPE_FAT32) {
        result->valid = false;
        result->confidence = 30;
        snprintf(result->description, sizeof(result->description),
                 "FAT32 detected (not supported for floppy images)");
        return 0;
    }
    
    /* Match geometry */
    result->geometry = uft_fat_geometry_from_size(size);
    
    /* Detect platform */
    result->platform = uft_fat_detect_platform(boot);
    
    /* Check FAT consistency */
    if (nfats >= 2 && fat_size > 0) {
        const uint8_t *fat1 = data + reserved * bps;
        const uint8_t *fat2 = fat1 + fat_size * bps;
        
        if ((size_t)((fat2 - data) + fat_size * bps) <= size) {
            if (memcmp(fat1, fat2, fat_size * bps) != 0) {
                result->fat_mismatch = true;
            }
        }
    }
    
    /* Calculate confidence */
    result->confidence = 50;
    if (!result->boot_sig_missing) result->confidence += 20;
    if (result->geometry) result->confidence += 20;
    if (!result->fat_mismatch) result->confidence += 10;
    
    result->valid = true;
    
    /* Build description */
    const char *type_str = (result->type == UFT_FAT_TYPE_FAT12) ? "FAT12" : "FAT16";
    const char *platform_str = "";
    switch (result->platform) {
        case UFT_FAT_PLATFORM_MSX:   platform_str = " (MSX-DOS)"; break;
        case UFT_FAT_PLATFORM_ATARI: platform_str = " (Atari ST)"; break;
        case UFT_FAT_PLATFORM_PC98:  platform_str = " (PC-98)"; break;
        case UFT_FAT_PLATFORM_H68K:  platform_str = " (Human68K)"; break;
        default: break;
    }
    
    if (result->geometry) {
        snprintf(result->description, sizeof(result->description),
                 "%s %s%s", type_str, result->geometry->name, platform_str);
    } else {
        snprintf(result->description, sizeof(result->description),
                 "%s %u sectors%s", type_str, total, platform_str);
    }
    
    return 0;
}

/*===========================================================================
 * Volume Initialization
 *===========================================================================*/

static int init_volume_info(uft_fat_ctx_t *ctx) {
    if (!ctx || !ctx->data || ctx->data_size < 512) {
        return UFT_FAT_ERR_INVALID;
    }
    
    const uft_fat_bootsect_t *boot = (const uft_fat_bootsect_t *)ctx->data;
    uft_fat_volume_t *vol = &ctx->vol;
    
    /* Extract BPB values */
    vol->bytes_per_sector = read_le16((const uint8_t*)&boot->bytes_per_sector);
    vol->sectors_per_cluster = boot->sectors_per_cluster;
    vol->reserved_sectors = read_le16((const uint8_t*)&boot->reserved_sectors);
    vol->num_fats = boot->num_fats;
    vol->root_entry_count = read_le16((const uint8_t*)&boot->root_entry_count);
    vol->fat_size = read_le16((const uint8_t*)&boot->fat_size_16);
    vol->media_type = boot->media_type;
    
    /* Get total sectors */
    uint16_t total16 = read_le16((const uint8_t*)&boot->total_sectors_16);
    uint32_t total32 = read_le32((const uint8_t*)&boot->total_sectors_32);
    vol->total_sectors = (total16 != 0) ? total16 : total32;
    
    /* Calculate layout */
    vol->fat_start_sector = vol->reserved_sectors;
    vol->root_dir_sectors = ((vol->root_entry_count * 32) + (vol->bytes_per_sector - 1)) 
                            / vol->bytes_per_sector;
    vol->root_dir_sector = vol->fat_start_sector + (vol->num_fats * vol->fat_size);
    vol->data_start_sector = vol->root_dir_sector + vol->root_dir_sectors;
    
    uint32_t data_sectors = vol->total_sectors - vol->data_start_sector;
    vol->data_clusters = data_sectors / vol->sectors_per_cluster;
    vol->last_cluster = vol->data_clusters + UFT_FAT_FIRST_CLUSTER - 1;
    
    /* Determine FAT type */
    vol->fat_type = determine_fat_type(vol->data_clusters);
    
    /* Detect platform */
    vol->platform = uft_fat_detect_platform(boot);
    
    /* Extract volume info */
    if (boot->boot_signature == UFT_FAT_EXT_BOOT_SIG) {
        vol->serial = read_le32((const uint8_t*)&boot->volume_serial);
        memcpy(vol->label, boot->volume_label, 11);
        vol->label[11] = '\0';
        /* Trim trailing spaces */
        for (int i = 10; i >= 0 && vol->label[i] == ' '; i--) {
            vol->label[i] = '\0';
        }
    } else {
        vol->serial = 0;
        vol->label[0] = '\0';
    }
    
    /* Copy OEM name */
    memcpy(vol->oem_name, boot->oem_name, 8);
    vol->oem_name[8] = '\0';
    for (int i = 7; i >= 0 && vol->oem_name[i] == ' '; i--) {
        vol->oem_name[i] = '\0';
    }
    
    return 0;
}

/**
 * @brief Cache FAT table
 */
static int cache_fat(uft_fat_ctx_t *ctx) {
    if (!ctx) return UFT_FAT_ERR_INVALID;
    
    const uft_fat_volume_t *vol = &ctx->vol;
    size_t fat_bytes = (size_t)vol->fat_size * vol->bytes_per_sector;
    
    /* Free existing cache */
    if (ctx->fat_cache) {
        free(ctx->fat_cache);
    }
    
    ctx->fat_cache = malloc(fat_bytes);
    if (!ctx->fat_cache) {
        return UFT_FAT_ERR_NOMEM;
    }
    
    /* Copy FAT1 */
    size_t fat_offset = (size_t)vol->fat_start_sector * vol->bytes_per_sector;
    memcpy(ctx->fat_cache, ctx->data + fat_offset, fat_bytes);
    ctx->fat_dirty = false;
    
    return 0;
}

/**
 * @brief Flush FAT cache to image
 */
static int flush_fat(uft_fat_ctx_t *ctx) {
    if (!ctx || !ctx->fat_dirty) return 0;
    
    const uft_fat_volume_t *vol = &ctx->vol;
    size_t fat_bytes = (size_t)vol->fat_size * vol->bytes_per_sector;
    
    /* Write to all FAT copies */
    for (uint8_t i = 0; i < vol->num_fats; i++) {
        size_t offset = (size_t)(vol->fat_start_sector + i * vol->fat_size) 
                        * vol->bytes_per_sector;
        memcpy(ctx->data + offset, ctx->fat_cache, fat_bytes);
    }
    
    ctx->fat_dirty = false;
    ctx->modified = true;
    
    return 0;
}

/*===========================================================================
 * Open/Save
 *===========================================================================*/

int uft_fat_open(uft_fat_ctx_t *ctx, const uint8_t *data, size_t size, bool copy) {
    if (!ctx || !data || size < 512) {
        return UFT_FAT_ERR_INVALID;
    }
    
    /* Detect filesystem first */
    uft_fat_detect_t detect;
    int rc = uft_fat_detect(data, size, &detect);
    if (rc != 0) return rc;
    
    if (!detect.valid) {
        return UFT_FAT_ERR_INVALID;
    }
    
    /* Clean up any existing data */
    if (ctx->owns_data && ctx->data) {
        free(ctx->data);
        ctx->data = NULL;
    }
    if (ctx->fat_cache) {
        free(ctx->fat_cache);
        ctx->fat_cache = NULL;
    }
    
    /* Store data */
    if (copy) {
        ctx->data = malloc(size);
        if (!ctx->data) return UFT_FAT_ERR_NOMEM;
        memcpy(ctx->data, data, size);
        ctx->owns_data = true;
    } else {
        ctx->data = (uint8_t *)data;
        ctx->owns_data = false;
    }
    ctx->data_size = size;
    ctx->modified = false;
    
    /* Initialize volume info */
    rc = init_volume_info(ctx);
    if (rc != 0) return rc;
    
    /* Cache FAT */
    rc = cache_fat(ctx);
    if (rc != 0) return rc;
    
    return 0;
}

int uft_fat_open_file(uft_fat_ctx_t *ctx, const char *filename) {
    if (!ctx || !filename) {
        return UFT_FAT_ERR_INVALID;
    }
    
    FILE *f = fopen(filename, "rb");
    if (!f) return UFT_FAT_ERR_IO;
    
    /* Get file size */
    if (fseek(f, 0, SEEK_END) != 0) { /* seek error */ }
    long size = ftell(f);
    if (fseek(f, 0, SEEK_SET) != 0) { /* seek error */ }
    if (size <= 0 || size > 10 * 1024 * 1024) {
        fclose(f);
        return UFT_FAT_ERR_INVALID;
    }
    
    /* Read file */
    uint8_t *data = malloc((size_t)size);
    if (!data) {
        fclose(f);
        return UFT_FAT_ERR_NOMEM;
    }
    
    size_t read = fread(data, 1, (size_t)size, f);
    fclose(f);
    
    if (read != (size_t)size) {
        free(data);
        return UFT_FAT_ERR_IO;
    }
    
    /* Open with ownership */
    int rc = uft_fat_open(ctx, data, (size_t)size, false);
    if (rc == 0) {
        ctx->owns_data = true;
    } else {
        free(data);
    }
    
    return rc;
}

int uft_fat_save(uft_fat_ctx_t *ctx, const char *filename) {
    if (!ctx || !ctx->data) {
        return UFT_FAT_ERR_INVALID;
    }
    
    /* Flush FAT cache */
    flush_fat(ctx);
    
    if (!filename && !ctx->modified) {
        return 0;  /* Nothing to do */
    }
    
    if (!filename) {
        return UFT_FAT_ERR_INVALID;  /* Need filename for new save */
    }
    
    FILE *f = fopen(filename, "wb");
    if (!f) return UFT_FAT_ERR_IO;
    
    size_t written = fwrite(ctx->data, 1, ctx->data_size, f);
    fclose(f);
    
    if (written != ctx->data_size) {
        return UFT_FAT_ERR_IO;
    }
    
    ctx->modified = false;
    return 0;
}

const uint8_t *uft_fat_get_data(const uft_fat_ctx_t *ctx, size_t *size) {
    if (!ctx) return NULL;
    if (size) *size = ctx->data_size;
    return ctx->data;
}

/*===========================================================================
 * Volume Info
 *===========================================================================*/

const uft_fat_volume_t *uft_fat_get_volume(const uft_fat_ctx_t *ctx) {
    if (!ctx) return NULL;
    return &ctx->vol;
}

int uft_fat_get_label(const uft_fat_ctx_t *ctx, char *label) {
    if (!ctx || !label) return UFT_FAT_ERR_INVALID;
    
    /* Try root directory first for volume label entry */
    const uft_fat_volume_t *vol = &ctx->vol;
    size_t root_offset = (size_t)vol->root_dir_sector * vol->bytes_per_sector;
    
    for (uint32_t i = 0; i < vol->root_entry_count; i++) {
        const uft_fat_sfn_t *entry = (const uft_fat_sfn_t *)
            (ctx->data + root_offset + i * 32);
        
        if ((uint8_t)entry->name[0] == UFT_FAT_DIRENT_END) break;
        if ((uint8_t)entry->name[0] == UFT_FAT_DIRENT_FREE) continue;
        
        if (entry->attributes == UFT_FAT_ATTR_VOLUME_ID) {
            /* Found volume label entry */
            memcpy(label, entry->name, 8);
            memcpy(label + 8, entry->ext, 3);
            label[11] = '\0';
            /* Trim trailing spaces */
            for (int j = 10; j >= 0 && label[j] == ' '; j--) {
                label[j] = '\0';
            }
            return 0;
        }
    }
    
    /* Fall back to boot sector label */
    strncpy(label, ctx->vol.label, 12); label[11] = '\0';
    return 0;
}

int uft_fat_set_label(uft_fat_ctx_t *ctx, const char *label) {
    if (!ctx || ctx->read_only) return UFT_FAT_ERR_READONLY;
    if (!label) return UFT_FAT_ERR_INVALID;
    
    /* Validate and pad label */
    char new_label[12];
    memset(new_label, ' ', 11);
    new_label[11] = '\0';
    
    size_t len = strlen(label);
    if (len > 11) len = 11;
    
    for (size_t i = 0; i < len; i++) {
        char c = label[i];
        if (c >= 'a' && c <= 'z') c = c - 'a' + 'A';
        new_label[i] = c;
    }
    
    /* Update boot sector */
    uft_fat_bootsect_t *boot = (uft_fat_bootsect_t *)ctx->data;
    memcpy(boot->volume_label, new_label, 11);
    boot->boot_signature = UFT_FAT_EXT_BOOT_SIG;
    
    /* Update root directory volume label entry */
    const uft_fat_volume_t *vol = &ctx->vol;
    size_t root_offset = (size_t)vol->root_dir_sector * vol->bytes_per_sector;
    bool found = false;
    
    for (uint32_t i = 0; i < vol->root_entry_count && !found; i++) {
        uft_fat_sfn_t *entry = (uft_fat_sfn_t *)
            (ctx->data + root_offset + i * 32);
        
        if ((uint8_t)entry->name[0] == UFT_FAT_DIRENT_END) break;
        if ((uint8_t)entry->name[0] == UFT_FAT_DIRENT_FREE) continue;
        
        if (entry->attributes == UFT_FAT_ATTR_VOLUME_ID) {
            memcpy(entry->name, new_label, 8);
            memcpy(entry->ext, new_label + 8, 3);
            found = true;
        }
    }
    
    /* Update cached label */
    strncpy(ctx->vol.label, new_label, 12); ctx->vol.label[11] = '\0';
    for (int i = 10; i >= 0 && ctx->vol.label[i] == ' '; i--) {
        ctx->vol.label[i] = '\0';
    }
    
    ctx->modified = true;
    return 0;
}

uint64_t uft_fat_get_free_space(const uft_fat_ctx_t *ctx) {
    if (!ctx) return 0;
    
    const uft_fat_volume_t *vol = &ctx->vol;
    uint32_t free_clusters = 0;
    
    for (uint32_t i = UFT_FAT_FIRST_CLUSTER; i <= vol->last_cluster; i++) {
        if (uft_fat_cluster_is_free(ctx, i)) {
            free_clusters++;
        }
    }
    
    return (uint64_t)free_clusters * vol->sectors_per_cluster * vol->bytes_per_sector;
}

uint64_t uft_fat_get_used_space(const uft_fat_ctx_t *ctx) {
    if (!ctx) return 0;
    
    const uft_fat_volume_t *vol = &ctx->vol;
    uint64_t total = (uint64_t)vol->data_clusters * vol->sectors_per_cluster 
                     * vol->bytes_per_sector;
    
    return total - uft_fat_get_free_space(ctx);
}

/*===========================================================================
 * FAT Table Operations
 *===========================================================================*/

int32_t uft_fat_get_entry(const uft_fat_ctx_t *ctx, uint32_t cluster) {
    if (!ctx || !ctx->fat_cache) return UFT_FAT_ERR_INVALID;
    
    const uft_fat_volume_t *vol = &ctx->vol;
    
    if (cluster < UFT_FAT_FIRST_CLUSTER || cluster > vol->last_cluster) {
        return UFT_FAT_ERR_INVALID;
    }
    
    if (vol->fat_type == UFT_FAT_TYPE_FAT12) {
        /* FAT12: 12-bit entries packed */
        size_t offset = cluster + (cluster / 2);  /* cluster * 1.5 */
        uint16_t value = ctx->fat_cache[offset] | 
                         ((uint16_t)ctx->fat_cache[offset + 1] << 8);
        
        if (cluster & 1) {
            return (value >> 4) & 0xFFF;
        } else {
            return value & 0xFFF;
        }
    } else {
        /* FAT16: 16-bit entries */
        size_t offset = cluster * 2;
        return read_le16(ctx->fat_cache + offset);
    }
}

int uft_fat_set_entry(uft_fat_ctx_t *ctx, uint32_t cluster, uint32_t value) {
    if (!ctx || !ctx->fat_cache || ctx->read_only) {
        return UFT_FAT_ERR_INVALID;
    }
    
    const uft_fat_volume_t *vol = &ctx->vol;
    
    if (cluster < UFT_FAT_FIRST_CLUSTER || cluster > vol->last_cluster) {
        return UFT_FAT_ERR_INVALID;
    }
    
    if (vol->fat_type == UFT_FAT_TYPE_FAT12) {
        size_t offset = cluster + (cluster / 2);
        
        if (cluster & 1) {
            /* Odd cluster: high 12 bits */
            ctx->fat_cache[offset] = (ctx->fat_cache[offset] & 0x0F) | 
                                     ((value & 0x0F) << 4);
            ctx->fat_cache[offset + 1] = (value >> 4) & 0xFF;
        } else {
            /* Even cluster: low 12 bits */
            ctx->fat_cache[offset] = value & 0xFF;
            ctx->fat_cache[offset + 1] = (ctx->fat_cache[offset + 1] & 0xF0) |
                                         ((value >> 8) & 0x0F);
        }
    } else {
        size_t offset = cluster * 2;
        write_le16(ctx->fat_cache + offset, (uint16_t)value);
    }
    
    ctx->fat_dirty = true;
    return 0;
}

bool uft_fat_cluster_is_free(const uft_fat_ctx_t *ctx, uint32_t cluster) {
    int32_t entry = uft_fat_get_entry(ctx, cluster);
    return entry == 0;
}

bool uft_fat_cluster_is_eof(const uft_fat_ctx_t *ctx, uint32_t cluster) {
    int32_t entry = uft_fat_get_entry(ctx, cluster);
    if (entry < 0) return false;
    
    if (ctx->vol.fat_type == UFT_FAT_TYPE_FAT12) {
        return entry >= (int32_t)UFT_FAT12_EOF_MIN;
    } else {
        return entry >= (int32_t)UFT_FAT16_EOF_MIN;
    }
}

bool uft_fat_cluster_is_bad(const uft_fat_ctx_t *ctx, uint32_t cluster) {
    int32_t entry = uft_fat_get_entry(ctx, cluster);
    if (entry < 0) return false;
    
    if (ctx->vol.fat_type == UFT_FAT_TYPE_FAT12) {
        return entry == UFT_FAT12_BAD;
    } else {
        return entry == UFT_FAT16_BAD;
    }
}

int32_t uft_fat_alloc_cluster(uft_fat_ctx_t *ctx, uint32_t hint) {
    if (!ctx || ctx->read_only) return UFT_FAT_ERR_READONLY;
    
    const uft_fat_volume_t *vol = &ctx->vol;
    uint32_t start = (hint >= UFT_FAT_FIRST_CLUSTER && hint <= vol->last_cluster) 
                     ? hint : UFT_FAT_FIRST_CLUSTER;
    
    /* Search forward from hint */
    for (uint32_t i = start; i <= vol->last_cluster; i++) {
        if (uft_fat_cluster_is_free(ctx, i)) {
            uint32_t eof = (vol->fat_type == UFT_FAT_TYPE_FAT12) ? 
                           UFT_FAT12_EOF : UFT_FAT16_EOF;
            uft_fat_set_entry(ctx, i, eof);
            return (int32_t)i;
        }
    }
    
    /* Wrap around */
    for (uint32_t i = UFT_FAT_FIRST_CLUSTER; i < start; i++) {
        if (uft_fat_cluster_is_free(ctx, i)) {
            uint32_t eof = (vol->fat_type == UFT_FAT_TYPE_FAT12) ?
                           UFT_FAT12_EOF : UFT_FAT16_EOF;
            uft_fat_set_entry(ctx, i, eof);
            return (int32_t)i;
        }
    }
    
    return UFT_FAT_ERR_FULL;
}

int uft_fat_free_chain(uft_fat_ctx_t *ctx, uint32_t start) {
    if (!ctx || ctx->read_only) return UFT_FAT_ERR_READONLY;
    
    const uft_fat_volume_t *vol = &ctx->vol;
    uint32_t current = start;
    uint32_t count = 0;
    const uint32_t max_clusters = vol->data_clusters + 10;  /* Safety limit */
    
    while (current >= UFT_FAT_FIRST_CLUSTER && 
           current <= vol->last_cluster &&
           count < max_clusters) {
        
        int32_t next = uft_fat_get_entry(ctx, current);
        uft_fat_set_entry(ctx, current, 0);
        
        if (next < (int32_t)UFT_FAT_FIRST_CLUSTER) break;
        if (uft_fat_cluster_is_eof(ctx, current)) break;
        
        current = (uint32_t)next;
        count++;
    }
    
    return 0;
}

/*===========================================================================
 * Cluster Chain
 *===========================================================================*/

void uft_fat_chain_init(uft_fat_chain_t *chain) {
    if (!chain) return;
    memset(chain, 0, sizeof(*chain));
}

void uft_fat_chain_free(uft_fat_chain_t *chain) {
    if (!chain) return;
    if (chain->clusters) {
        free(chain->clusters);
    }
    memset(chain, 0, sizeof(*chain));
}

int uft_fat_get_chain(const uft_fat_ctx_t *ctx, uint32_t start, uft_fat_chain_t *chain) {
    if (!ctx || !chain) return UFT_FAT_ERR_INVALID;
    
    uft_fat_chain_init(chain);
    
    if (start < UFT_FAT_FIRST_CLUSTER) {
        return 0;  /* Empty chain (e.g., empty file) */
    }
    
    const uft_fat_volume_t *vol = &ctx->vol;
    const uint32_t max_clusters = vol->data_clusters + 10;
    
    /* Initial allocation */
    chain->capacity = 64;
    chain->clusters = malloc(chain->capacity * sizeof(uint32_t));
    if (!chain->clusters) return UFT_FAT_ERR_NOMEM;
    
    /* Track visited clusters for loop detection */
    uint8_t *visited = calloc((vol->last_cluster + 8) / 8, 1);
    if (!visited) {
        free(chain->clusters);
        chain->clusters = NULL;
        return UFT_FAT_ERR_NOMEM;
    }
    
    uint32_t current = start;
    
    while (current >= UFT_FAT_FIRST_CLUSTER &&
           current <= vol->last_cluster &&
           chain->count < max_clusters) {
        
        /* Check for loop */
        size_t byte_idx = current / 8;
        uint8_t bit_mask = 1 << (current % 8);
        if (visited[byte_idx] & bit_mask) {
            chain->has_loops = true;
            break;
        }
        visited[byte_idx] |= bit_mask;
        
        /* Grow array if needed */
        if (chain->count >= chain->capacity) {
            chain->capacity *= 2;
            uint32_t *new_clusters = realloc(chain->clusters, 
                                             chain->capacity * sizeof(uint32_t));
            if (!new_clusters) {
                free(visited);
                return UFT_FAT_ERR_NOMEM;
            }
            chain->clusters = new_clusters;
        }
        
        chain->clusters[chain->count++] = current;
        
        int32_t next = uft_fat_get_entry(ctx, current);
        
        if (next < 0) {
            break;  /* Error */
        }
        
        if (uft_fat_cluster_is_eof(ctx, current)) {
            chain->complete = true;
            break;
        }
        
        if (uft_fat_cluster_is_bad(ctx, current)) {
            chain->has_bad = true;
        }
        
        if (next < (int32_t)UFT_FAT_FIRST_CLUSTER) {
            break;  /* Invalid next */
        }
        
        current = (uint32_t)next;
    }
    
    free(visited);
    return 0;
}

int uft_fat_alloc_chain(uft_fat_ctx_t *ctx, size_t count, uft_fat_chain_t *chain) {
    if (!ctx || !chain || count == 0) return UFT_FAT_ERR_INVALID;
    if (ctx->read_only) return UFT_FAT_ERR_READONLY;
    
    uft_fat_chain_init(chain);
    
    chain->clusters = malloc(count * sizeof(uint32_t));
    if (!chain->clusters) return UFT_FAT_ERR_NOMEM;
    chain->capacity = count;
    
    uint32_t hint = UFT_FAT_FIRST_CLUSTER;
    
    for (size_t i = 0; i < count; i++) {
        int32_t cluster = uft_fat_alloc_cluster(ctx, hint);
        if (cluster < 0) {
            /* Allocation failed - free what we allocated */
            for (size_t j = 0; j < chain->count; j++) {
                uft_fat_set_entry(ctx, chain->clusters[j], 0);
            }
            uft_fat_chain_free(chain);
            return UFT_FAT_ERR_FULL;
        }
        
        chain->clusters[chain->count++] = (uint32_t)cluster;
        hint = (uint32_t)cluster + 1;
        
        /* Link to previous */
        if (i > 0) {
            uft_fat_set_entry(ctx, chain->clusters[i-1], (uint32_t)cluster);
        }
    }
    
    chain->complete = true;
    return 0;
}

/*===========================================================================
 * Cluster I/O
 *===========================================================================*/

int64_t uft_fat_cluster_offset(const uft_fat_ctx_t *ctx, uint32_t cluster) {
    if (!ctx) return -1;
    
    const uft_fat_volume_t *vol = &ctx->vol;
    
    if (cluster < UFT_FAT_FIRST_CLUSTER || cluster > vol->last_cluster) {
        return -1;
    }
    
    uint32_t sector = vol->data_start_sector + 
                      (cluster - UFT_FAT_FIRST_CLUSTER) * vol->sectors_per_cluster;
    
    return (int64_t)sector * vol->bytes_per_sector;
}

size_t uft_fat_cluster_size(const uft_fat_ctx_t *ctx) {
    if (!ctx) return 0;
    return (size_t)ctx->vol.sectors_per_cluster * ctx->vol.bytes_per_sector;
}

int uft_fat_read_cluster(const uft_fat_ctx_t *ctx, uint32_t cluster, uint8_t *buffer) {
    if (!ctx || !buffer) return UFT_FAT_ERR_INVALID;
    
    int64_t offset = uft_fat_cluster_offset(ctx, cluster);
    if (offset < 0) return UFT_FAT_ERR_INVALID;
    
    size_t size = uft_fat_cluster_size(ctx);
    
    if ((size_t)offset + size > ctx->data_size) {
        return UFT_FAT_ERR_IO;
    }
    
    memcpy(buffer, ctx->data + offset, size);
    return 0;
}

int uft_fat_write_cluster(uft_fat_ctx_t *ctx, uint32_t cluster, const uint8_t *buffer) {
    if (!ctx || !buffer) return UFT_FAT_ERR_INVALID;
    if (ctx->read_only) return UFT_FAT_ERR_READONLY;
    
    int64_t offset = uft_fat_cluster_offset(ctx, cluster);
    if (offset < 0) return UFT_FAT_ERR_INVALID;
    
    size_t size = uft_fat_cluster_size(ctx);
    
    if ((size_t)offset + size > ctx->data_size) {
        return UFT_FAT_ERR_IO;
    }
    
    memcpy(ctx->data + offset, buffer, size);
    ctx->modified = true;
    return 0;
}

/*===========================================================================
 * Root Directory I/O
 *===========================================================================*/

int uft_fat_read_root_sector(const uft_fat_ctx_t *ctx, uint32_t index, uint8_t *buffer) {
    if (!ctx || !buffer) return UFT_FAT_ERR_INVALID;
    
    const uft_fat_volume_t *vol = &ctx->vol;
    
    if (index >= vol->root_dir_sectors) {
        return UFT_FAT_ERR_INVALID;
    }
    
    size_t offset = (size_t)(vol->root_dir_sector + index) * vol->bytes_per_sector;
    
    if (offset + vol->bytes_per_sector > ctx->data_size) {
        return UFT_FAT_ERR_IO;
    }
    
    memcpy(buffer, ctx->data + offset, vol->bytes_per_sector);
    return 0;
}

int uft_fat_write_root_sector(uft_fat_ctx_t *ctx, uint32_t index, const uint8_t *buffer) {
    if (!ctx || !buffer) return UFT_FAT_ERR_INVALID;
    if (ctx->read_only) return UFT_FAT_ERR_READONLY;
    
    const uft_fat_volume_t *vol = &ctx->vol;
    
    if (index >= vol->root_dir_sectors) {
        return UFT_FAT_ERR_INVALID;
    }
    
    size_t offset = (size_t)(vol->root_dir_sector + index) * vol->bytes_per_sector;
    
    if (offset + vol->bytes_per_sector > ctx->data_size) {
        return UFT_FAT_ERR_IO;
    }
    
    memcpy(ctx->data + offset, buffer, vol->bytes_per_sector);
    ctx->modified = true;
    return 0;
}

/*===========================================================================
 * Time Conversion
 *===========================================================================*/

time_t uft_fat_to_unix_time(uint16_t fat_time, uint16_t fat_date) {
    struct tm tm = {0};
    
    tm.tm_sec  = (fat_time & 0x1F) * 2;
    tm.tm_min  = (fat_time >> 5) & 0x3F;
    tm.tm_hour = (fat_time >> 11) & 0x1F;
    tm.tm_mday = fat_date & 0x1F;
    tm.tm_mon  = ((fat_date >> 5) & 0x0F) - 1;
    tm.tm_year = ((fat_date >> 9) & 0x7F) + 80;  /* 1980 base */
    tm.tm_isdst = -1;
    
    return mktime(&tm);
}

void uft_fat_from_unix_time(time_t unix_time, uint16_t *fat_time, uint16_t *fat_date) {
    struct tm *tm = localtime(&unix_time);
    if (!tm) {
        *fat_time = 0;
        *fat_date = 0x21;  /* 1980-01-01 */
        return;
    }
    
    *fat_time = ((tm->tm_hour & 0x1F) << 11) |
                ((tm->tm_min & 0x3F) << 5) |
                ((tm->tm_sec / 2) & 0x1F);
    
    int year = tm->tm_year - 80;
    if (year < 0) year = 0;
    if (year > 127) year = 127;
    
    *fat_date = ((year & 0x7F) << 9) |
                (((tm->tm_mon + 1) & 0x0F) << 5) |
                (tm->tm_mday & 0x1F);
}

/*===========================================================================
 * Attribute String
 *===========================================================================*/

char *uft_fat_attr_to_string(uint8_t attr, char *buffer) {
    if (!buffer) return NULL;
    
    buffer[0] = (attr & UFT_FAT_ATTR_READONLY)  ? 'R' : '-';
    buffer[1] = (attr & UFT_FAT_ATTR_HIDDEN)    ? 'H' : '-';
    buffer[2] = (attr & UFT_FAT_ATTR_SYSTEM)    ? 'S' : '-';
    buffer[3] = (attr & UFT_FAT_ATTR_VOLUME_ID) ? 'V' : '-';
    buffer[4] = (attr & UFT_FAT_ATTR_DIRECTORY) ? 'D' : '-';
    buffer[5] = (attr & UFT_FAT_ATTR_ARCHIVE)   ? 'A' : '-';
    buffer[6] = '\0';
    
    return buffer;
}
