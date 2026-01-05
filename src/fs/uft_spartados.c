/**
 * @file uft_spartados.c
 * @brief SpartaDOS Filesystem Implementation
 * 
 * EXT-008: Atari 8-bit SpartaDOS filesystem support
 * Based on mkatr spartafs.c by Daniel Serpell
 * 
 * @version 1.0.0
 * @date 2026-01-04
 * @author UFT Team
 * 
 * SPDX-License-Identifier: MIT
 */

#include "uft/fs/uft_spartados.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* For mkdir */
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#endif

/*===========================================================================
 * Internal Helpers
 *===========================================================================*/

/** Read 16-bit little-endian value */
static inline uint16_t read16_le(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

/** Read 24-bit little-endian value */
static inline uint32_t read24_le(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16);
}

/** Get sector pointer */
static inline const uint8_t* get_sector(const uft_sparta_ctx_t *ctx, uint16_t sector) {
    if (sector == 0 || sector > ctx->boot.total_sectors) {
        return NULL;
    }
    size_t offset = (size_t)(sector - 1) * ctx->sector_size;
    if (offset + ctx->sector_size > ctx->image_size) {
        return NULL;
    }
    return ctx->image + offset;
}

/** Check if sector is allocated in bitmap */
static bool is_sector_used(const uft_sparta_ctx_t *ctx, uint16_t sector) {
    if (sector == 0 || sector > ctx->boot.total_sectors) {
        return false;
    }
    
    const uint8_t *bitmap = get_sector(ctx, ctx->boot.bitmap_start);
    if (!bitmap) return false;
    
    /* Bitmap bit: 0 = used, 1 = free */
    int byte_idx = sector >> 3;
    int bit_idx = 7 - (sector & 7);
    
    return (bitmap[byte_idx] & (1 << bit_idx)) == 0;
}

/** Parse directory entry */
static void parse_dirent(const uint8_t *data, uft_sparta_fileinfo_t *info) {
    const uft_sparta_dirent_t *de = (const uft_sparta_dirent_t*)data;
    
    /* Status flags */
    info->is_deleted = (de->status & UFT_SPARTA_FLAG_DELETED) != 0;
    info->is_directory = (de->status & UFT_SPARTA_FLAG_SUBDIR) != 0;
    info->is_locked = (de->status & UFT_SPARTA_FLAG_LOCKED) != 0;
    
    /* Filename */
    for (int i = 0; i < 8 && de->filename[i] != ' '; i++) {
        info->filename[i] = de->filename[i];
    }
    info->filename[8] = '\0';
    
    /* Extension */
    for (int i = 0; i < 3 && de->extension[i] != ' '; i++) {
        info->extension[i] = de->extension[i];
    }
    info->extension[3] = '\0';
    
    /* Full path */
    if (info->extension[0]) {
        snprintf(info->full_path, UFT_SPARTA_MAX_PATH, "%s.%s", 
                 info->filename, info->extension);
    } else {
        strncpy(info->full_path, info->filename, UFT_SPARTA_MAX_PATH);
    }
    
    /* File size (24-bit) */
    info->size = (uint32_t)read16_le(&de->file_length_lo) | 
                 ((uint32_t)de->file_length_hi << 16);
    
    /* First sector map */
    info->first_sector = read16_le(&de->sector_map_start);
    
    /* Timestamp */
    info->day = de->date_day;
    info->month = de->date_month;
    info->year = de->date_year;
    info->hour = de->time_hour;
    info->minute = de->time_minute;
    info->second = de->time_second;
}

/*===========================================================================
 * Public API
 *===========================================================================*/

/**
 * @brief Detect SpartaDOS filesystem
 */
bool uft_sparta_detect(const uint8_t *image, size_t size) {
    if (!image || size < 384) {
        return false;  /* Need at least 3 sectors for boot */
    }
    
    /* Check for SpartaDOS signature in boot sector */
    /* Boot sector starts at offset 0 (sector 1) */
    
    /* Flags byte should have valid boot configuration */
    uint8_t flags = image[0];
    if ((flags & 0x3F) != 0) {
        return false;  /* Lower 6 bits should be 0 for SpartaDOS */
    }
    
    /* Boot sectors count (typically 3) */
    uint8_t boot_sectors = image[1];
    if (boot_sectors < 1 || boot_sectors > 9) {
        return false;
    }
    
    /* Check JMP instruction at offset 6 */
    if (image[6] != 0x4C) {  /* JMP opcode */
        return false;
    }
    
    /* Total sectors should be reasonable */
    uint16_t total_sectors = read16_le(&image[11]);
    if (total_sectors < 720 || total_sectors > 65535) {
        return false;
    }
    
    /* Free sectors should be <= total */
    uint16_t free_sectors = read16_le(&image[13]);
    if (free_sectors > total_sectors) {
        return false;
    }
    
    /* Bitmap and directory start should be valid */
    uint16_t bitmap_start = read16_le(&image[16]);
    uint16_t dir_start = read16_le(&image[20]);
    
    if (bitmap_start < 4 || bitmap_start > total_sectors) {
        return false;
    }
    if (dir_start < 4 || dir_start > total_sectors) {
        return false;
    }
    
    return true;
}

/**
 * @brief Initialize filesystem context
 */
int uft_sparta_init(uft_sparta_ctx_t *ctx, const uint8_t *image, size_t size) {
    if (!ctx || !image) {
        return -1;
    }
    
    if (!uft_sparta_detect(image, size)) {
        return -2;  /* Not SpartaDOS */
    }
    
    memset(ctx, 0, sizeof(*ctx));
    ctx->image = image;
    ctx->image_size = size;
    
    /* Parse boot sector */
    const uint8_t *boot = image;
    ctx->boot.flags = boot[0];
    ctx->boot.boot_sectors = boot[1];
    ctx->boot.boot_addr = read16_le(&boot[2]);
    ctx->boot.init_addr = read16_le(&boot[4]);
    ctx->boot.jmp_opcode = boot[6];
    ctx->boot.jmp_addr = read16_le(&boot[7]);
    ctx->boot.volume_seq = boot[9];
    ctx->boot.volume_random = boot[10];
    ctx->boot.total_sectors = read16_le(&boot[11]);
    ctx->boot.free_sectors = read16_le(&boot[13]);
    ctx->boot.bitmap_sectors = boot[15];
    ctx->boot.bitmap_start = read16_le(&boot[16]);
    ctx->boot.data_start = read16_le(&boot[18]);
    ctx->boot.dir_start = read16_le(&boot[20]);
    memcpy(ctx->boot.volume_name, &boot[22], 8);
    ctx->boot.tracks = boot[30];
    ctx->boot.sector_size = boot[31];
    ctx->boot.version = boot[32];
    
    /* Determine sector size */
    switch (ctx->boot.sector_size) {
        case 0:
        case 128:
            ctx->sector_size = 128;
            ctx->density = UFT_SPARTA_DENSITY_SD;
            break;
        case 1:
        case 256:
            ctx->sector_size = 256;
            ctx->density = UFT_SPARTA_DENSITY_DD;
            break;
        case 2:
        case 512:
            ctx->sector_size = 512;
            ctx->density = UFT_SPARTA_DENSITY_QD;
            break;
        default:
            ctx->sector_size = 256;  /* Default */
            ctx->density = UFT_SPARTA_DENSITY_DD;
            break;
    }
    
    /* Determine version */
    if (ctx->boot.version >= 0x30) {
        ctx->version = UFT_SPARTA_X;
    } else if (ctx->boot.version >= 0x20) {
        ctx->version = UFT_SPARTA_V3;
    } else if (ctx->boot.version >= 0x10) {
        ctx->version = UFT_SPARTA_V2;
    } else {
        ctx->version = UFT_SPARTA_V1;
    }
    
    /* Calculate sizes */
    ctx->total_size = (uint32_t)ctx->boot.total_sectors * ctx->sector_size;
    ctx->free_size = (uint32_t)ctx->boot.free_sectors * ctx->sector_size;
    
    return 0;
}

/**
 * @brief Get filesystem info
 */
int uft_sparta_get_info(const uft_sparta_ctx_t *ctx, char *buffer, size_t size) {
    if (!ctx || !buffer || size == 0) {
        return -1;
    }
    
    char vol_name[9];
    memcpy(vol_name, ctx->boot.volume_name, 8);
    vol_name[8] = '\0';
    
    /* Trim trailing spaces */
    for (int i = 7; i >= 0 && vol_name[i] == ' '; i--) {
        vol_name[i] = '\0';
    }
    
    snprintf(buffer, size,
             "SpartaDOS %s Filesystem\n"
             "Volume:       %s\n"
             "Total:        %u sectors (%u KB)\n"
             "Free:         %u sectors (%u KB)\n"
             "Sector Size:  %u bytes\n"
             "Tracks:       %u\n"
             "Bitmap at:    sector %u (%u sectors)\n"
             "Root Dir at:  sector %u\n",
             uft_sparta_version_name(ctx->version),
             vol_name[0] ? vol_name : "(unnamed)",
             ctx->boot.total_sectors, ctx->total_size / 1024,
             ctx->boot.free_sectors, ctx->free_size / 1024,
             ctx->sector_size,
             ctx->boot.tracks,
             ctx->boot.bitmap_start, ctx->boot.bitmap_sectors,
             ctx->boot.dir_start);
    
    return 0;
}

/**
 * @brief List directory
 */
int uft_sparta_list_dir(uft_sparta_ctx_t *ctx, const char *path,
                        uft_sparta_fileinfo_t *files, size_t max_files,
                        size_t *count)
{
    if (!ctx || !files || !count || max_files == 0) {
        return -1;
    }
    
    *count = 0;
    
    /* For now, only support root directory */
    uint16_t dir_sector = ctx->boot.dir_start;
    
    /* Read directory sector map */
    const uint8_t *map = get_sector(ctx, dir_sector);
    if (!map) {
        return -2;
    }
    
    /* Directory entries start after sector map header (4 bytes) */
    uint16_t next_map = read16_le(&map[0]);
    uint8_t sequence = map[2];
    uint8_t entry_count = map[3];
    
    /* First entry sector from map */
    uint16_t entry_sector = read16_le(&map[4]);
    
    const uint8_t *dir_data = get_sector(ctx, entry_sector);
    if (!dir_data) {
        return -3;
    }
    
    /* Parse directory entries */
    int entries_per_sector = ctx->sector_size / UFT_SPARTA_DIR_ENTRY_SIZE;
    
    for (int i = 0; i < entries_per_sector && *count < max_files; i++) {
        const uint8_t *entry = dir_data + (i * UFT_SPARTA_DIR_ENTRY_SIZE);
        
        /* Check if entry is in use */
        if ((entry[0] & UFT_SPARTA_FLAG_INUSE) == 0) {
            continue;  /* Empty entry */
        }
        
        parse_dirent(entry, &files[*count]);
        (*count)++;
    }
    
    return 0;
}

/**
 * @brief Get file info
 */
int uft_sparta_stat(uft_sparta_ctx_t *ctx, const char *path,
                    uft_sparta_fileinfo_t *info)
{
    if (!ctx || !path || !info) {
        return -1;
    }
    
    /* List root directory and find file */
    uft_sparta_fileinfo_t files[64];
    size_t count;
    
    if (uft_sparta_list_dir(ctx, "/", files, 64, &count) != 0) {
        return -2;
    }
    
    /* Search for file */
    for (size_t i = 0; i < count; i++) {
        if (strcasecmp(files[i].full_path, path) == 0 ||
            strcasecmp(files[i].filename, path) == 0) {
            *info = files[i];
            return 0;
        }
    }
    
    return -3;  /* Not found */
}

/**
 * @brief Read file
 */
int uft_sparta_read_file(uft_sparta_ctx_t *ctx, const char *path,
                         uint8_t *buffer, size_t max_size, size_t *bytes_read)
{
    if (!ctx || !path || !buffer || !bytes_read) {
        return -1;
    }
    
    *bytes_read = 0;
    
    /* Get file info */
    uft_sparta_fileinfo_t info;
    if (uft_sparta_stat(ctx, path, &info) != 0) {
        return -2;  /* File not found */
    }
    
    if (info.is_directory) {
        return -3;  /* Is a directory */
    }
    
    /* Read sectors via sector map */
    uint16_t map_sector = info.first_sector;
    size_t remaining = info.size;
    
    while (map_sector != 0 && remaining > 0 && *bytes_read < max_size) {
        const uint8_t *map = get_sector(ctx, map_sector);
        if (!map) break;
        
        uint16_t next_map = read16_le(&map[0]);
        
        /* Read data sectors from this map */
        for (int i = 4; i < ctx->sector_size && remaining > 0; i += 2) {
            uint16_t data_sector = read16_le(&map[i]);
            if (data_sector == 0) break;
            
            const uint8_t *data = get_sector(ctx, data_sector);
            if (!data) break;
            
            size_t copy_size = (remaining < ctx->sector_size) ? remaining : ctx->sector_size;
            if (*bytes_read + copy_size > max_size) {
                copy_size = max_size - *bytes_read;
            }
            
            memcpy(buffer + *bytes_read, data, copy_size);
            *bytes_read += copy_size;
            remaining -= copy_size;
        }
        
        map_sector = next_map;
    }
    
    return 0;
}

/**
 * @brief Extract single file to disk
 */
static int extract_file(uft_sparta_ctx_t *ctx, const uft_sparta_fileinfo_t *info,
                        const char *output_dir, const char *prefix)
{
    if (!ctx || !info || !output_dir) return -1;
    
    /* Build output path */
    char out_path[512];
    if (prefix && prefix[0]) {
        snprintf(out_path, sizeof(out_path), "%s/%s/%s.%s",
                 output_dir, prefix, info->filename, info->extension);
    } else {
        snprintf(out_path, sizeof(out_path), "%s/%s.%s",
                 output_dir, info->filename, info->extension);
    }
    
    /* Build internal path for reading */
    char int_path[UFT_SPARTA_MAX_PATH];
    if (prefix && prefix[0]) {
        snprintf(int_path, sizeof(int_path), "%s/%s.%s",
                 prefix, info->filename, info->extension);
    } else {
        snprintf(int_path, sizeof(int_path), "%s.%s",
                 info->filename, info->extension);
    }
    
    /* Read file data */
    uint8_t *buffer = malloc(info->size + 1);
    if (!buffer) return -2;
    
    size_t bytes_read = 0;
    int rc = uft_sparta_read_file(ctx, int_path, buffer, info->size, &bytes_read);
    if (rc != 0) {
        free(buffer);
        return -3;
    }
    
    /* Write to output file */
    FILE *fp = fopen(out_path, "wb");
    if (!fp) {
        free(buffer);
        return -4;
    }
    
    size_t written = fwrite(buffer, 1, bytes_read, fp);
    fclose(fp);
    free(buffer);
    
    return (written == bytes_read) ? 0 : -5;
}

/**
 * @brief Recursively extract directory
 */
static int extract_directory(uft_sparta_ctx_t *ctx, const char *dir_path,
                             const char *output_dir, int depth)
{
    if (!ctx || !output_dir) return -1;
    if (depth > 10) return 0;  /* Prevent infinite recursion */
    
    /* List directory contents */
    uft_sparta_fileinfo_t files[256];
    size_t count = 0;
    
    int rc = uft_sparta_list_dir(ctx, dir_path, files, 256, &count);
    if (rc != 0) return rc;
    
    int extracted = 0;
    int errors = 0;
    
    for (size_t i = 0; i < count; i++) {
        if (files[i].is_deleted) continue;
        
        if (files[i].is_directory) {
            /* Create subdirectory and recurse */
            char subdir[512];
            if (dir_path && dir_path[0] && strcmp(dir_path, "/") != 0) {
                snprintf(subdir, sizeof(subdir), "%s/%s/%s",
                         output_dir, dir_path, files[i].filename);
            } else {
                snprintf(subdir, sizeof(subdir), "%s/%s",
                         output_dir, files[i].filename);
            }
            
            /* Create directory (platform-specific) */
#ifdef _WIN32
            _mkdir(subdir);
#else
            mkdir(subdir, 0755);
#endif
            
            /* Build new path and recurse */
            char new_path[UFT_SPARTA_MAX_PATH];
            if (dir_path && dir_path[0] && strcmp(dir_path, "/") != 0) {
                snprintf(new_path, sizeof(new_path), "%s/%s",
                         dir_path, files[i].filename);
            } else {
                snprintf(new_path, sizeof(new_path), "%s", files[i].filename);
            }
            
            int sub_rc = extract_directory(ctx, new_path, output_dir, depth + 1);
            if (sub_rc > 0) extracted += sub_rc;
        } else {
            /* Extract file */
            rc = extract_file(ctx, &files[i], output_dir, dir_path);
            if (rc == 0) {
                extracted++;
            } else {
                errors++;
            }
        }
    }
    
    return extracted;
}

/**
 * @brief Extract all files
 */
int uft_sparta_extract_all(uft_sparta_ctx_t *ctx, const char *output_dir) {
    if (!ctx || !output_dir) return -1;
    
    /* Create output directory if needed */
#ifdef _WIN32
    _mkdir(output_dir);
#else
    mkdir(output_dir, 0755);
#endif
    
    return extract_directory(ctx, "/", output_dir, 0);
}

/**
 * @brief Get version name
 */
const char *uft_sparta_version_name(uft_sparta_version_t ver) {
    switch (ver) {
        case UFT_SPARTA_V1: return "1.x";
        case UFT_SPARTA_V2: return "2.x";
        case UFT_SPARTA_V3: return "3.x (SDX)";
        case UFT_SPARTA_X:  return "X";
        default:            return "Unknown";
    }
}
