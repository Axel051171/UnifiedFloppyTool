/**
 * @file uft_msx.c
 * @brief MSX Disk Format Implementation
 * @version 3.6.0
 * 
 * SPDX-License-Identifier: MIT
 */

#include "uft/formats/uft_msx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/*============================================================================
 * Geometry Tables
 *============================================================================*/

static const uft_msx_geometry_t g_msx_geometries[] = {
    [UFT_MSX_GEOM_UNKNOWN] = {
        .type = UFT_MSX_GEOM_UNKNOWN,
        .tracks = 0, .heads = 0, .sectors_per_track = 0, .sector_size = 0,
        .total_bytes = 0, .media_descriptor = 0x00, .name = "Unknown"
    },
    [UFT_MSX_GEOM_1DD_360] = {
        .type = UFT_MSX_GEOM_1DD_360,
        .tracks = 80, .heads = 1, .sectors_per_track = 9, .sector_size = 512,
        .total_bytes = 368640, .media_descriptor = 0xF8, .name = "1DD 360KB SS"
    },
    [UFT_MSX_GEOM_2DD_720] = {
        .type = UFT_MSX_GEOM_2DD_720,
        .tracks = 80, .heads = 2, .sectors_per_track = 9, .sector_size = 512,
        .total_bytes = 737280, .media_descriptor = 0xF9, .name = "2DD 720KB DS"
    },
    [UFT_MSX_GEOM_1DD_180] = {
        .type = UFT_MSX_GEOM_1DD_180,
        .tracks = 40, .heads = 1, .sectors_per_track = 9, .sector_size = 512,
        .total_bytes = 184320, .media_descriptor = 0xFC, .name = "1DD 180KB 5.25\""
    },
    [UFT_MSX_GEOM_2DD_360_5] = {
        .type = UFT_MSX_GEOM_2DD_360_5,
        .tracks = 40, .heads = 2, .sectors_per_track = 9, .sector_size = 512,
        .total_bytes = 368640, .media_descriptor = 0xFD, .name = "2DD 360KB 5.25\""
    },
    [UFT_MSX_GEOM_2HD_1440] = {
        .type = UFT_MSX_GEOM_2HD_1440,
        .tracks = 80, .heads = 2, .sectors_per_track = 18, .sector_size = 512,
        .total_bytes = 1474560, .media_descriptor = 0xF0, .name = "2HD 1.44MB"
    },
    [UFT_MSX_GEOM_CUSTOM] = {
        .type = UFT_MSX_GEOM_CUSTOM,
        .tracks = 0, .heads = 0, .sectors_per_track = 0, .sector_size = 0,
        .total_bytes = 0, .media_descriptor = 0x00, .name = "Custom"
    }
};

/*============================================================================
 * Geometry API
 *============================================================================*/

const uft_msx_geometry_t* uft_msx_get_geometry(uft_msx_geometry_type_t type) {
    if (type < 0 || type >= UFT_MSX_GEOM_COUNT) {
        return &g_msx_geometries[UFT_MSX_GEOM_UNKNOWN];
    }
    return &g_msx_geometries[type];
}

uft_msx_geometry_type_t uft_msx_detect_geometry_by_size(uint64_t file_size, uint8_t* confidence) {
    uint8_t conf = 0;
    uft_msx_geometry_type_t result = UFT_MSX_GEOM_UNKNOWN;
    
    /* Exact matches */
    for (int i = 1; i < UFT_MSX_GEOM_COUNT - 1; i++) {
        if (file_size == g_msx_geometries[i].total_bytes) {
            /* Differentiate between 360KB formats */
            if (file_size == 368640) {
                /* Could be 1DD 80T or 2DD 40T - need BPB analysis */
                result = UFT_MSX_GEOM_1DD_360; /* Default to more common */
                conf = 70;
            } else {
                result = (uft_msx_geometry_type_t)i;
                conf = 95;
            }
            break;
        }
    }
    
    /* Near-matches for truncated/padded images */
    if (result == UFT_MSX_GEOM_UNKNOWN) {
        if (file_size >= 360000 && file_size <= 375000) {
            result = UFT_MSX_GEOM_1DD_360;
            conf = 60;
        } else if (file_size >= 720000 && file_size <= 740000) {
            result = UFT_MSX_GEOM_2DD_720;
            conf = 60;
        } else if (file_size >= 1470000 && file_size <= 1480000) {
            result = UFT_MSX_GEOM_2HD_1440;
            conf = 60;
        } else if (file_size >= 180000 && file_size <= 190000) {
            result = UFT_MSX_GEOM_1DD_180;
            conf = 60;
        }
    }
    
    if (confidence) *confidence = conf;
    return result;
}

uft_msx_rc_t uft_msx_validate_geometry(uint16_t tracks, uint8_t heads,
                                        uint8_t sectors, uint16_t sector_size) {
    if (tracks == 0 || tracks > 85) return UFT_MSX_ERR_GEOMETRY;
    if (heads == 0 || heads > 2) return UFT_MSX_ERR_GEOMETRY;
    if (sectors == 0 || sectors > 36) return UFT_MSX_ERR_GEOMETRY;
    if (sector_size != 128 && sector_size != 256 &&
        sector_size != 512 && sector_size != 1024) {
        return UFT_MSX_ERR_GEOMETRY;
    }
    return UFT_MSX_SUCCESS;
}

/*============================================================================
 * Disk Operations
 *============================================================================*/

uft_msx_rc_t uft_msx_open(uft_msx_ctx_t* ctx, const char* path, bool writable) {
    if (!ctx || !path) return UFT_MSX_ERR_ARG;
    
    memset(ctx, 0, sizeof(*ctx));
    
    FILE* fp = fopen(path, "rb");
    if (!fp) return UFT_MSX_ERR_IO;
    
    /* Get file size */
    if (fseek(fp, 0, SEEK_END) != 0) { /* seek error */ }
    ctx->file_size = (uint64_t)ftell(fp);
    if (fseek(fp, 0, SEEK_SET) != 0) { /* seek error */ }
    /* Read boot sector */
    uint8_t boot_sector[512];
    if (fread(boot_sector, 1, 512, fp) != 512) {
        fclose(fp);
        return UFT_MSX_ERR_IO;
    }
    fclose(fp);
    
    /* Store path */
    ctx->path = strdup(path);
    if (!ctx->path) return UFT_MSX_ERR_NOMEM;
    
    ctx->writable = writable;
    
    /* Parse BPB */
    memcpy(&ctx->bpb, boot_sector, sizeof(ctx->bpb));
    
    /* Validate BPB */
    ctx->has_valid_bpb = false;
    if (ctx->bpb.bytes_per_sector == 512 &&
        ctx->bpb.sectors_per_cluster >= 1 && ctx->bpb.sectors_per_cluster <= 8 &&
        ctx->bpb.num_fats >= 1 && ctx->bpb.num_fats <= 2 &&
        ctx->bpb.root_entries > 0 && ctx->bpb.root_entries <= 512 &&
        ctx->bpb.sectors_per_fat > 0 && ctx->bpb.sectors_per_fat <= 12) {
        ctx->has_valid_bpb = true;
    }
    
    /* Determine geometry */
    if (ctx->has_valid_bpb) {
        ctx->geometry.type = UFT_MSX_GEOM_CUSTOM;
        ctx->geometry.sector_size = ctx->bpb.bytes_per_sector;
        ctx->geometry.sectors_per_track = ctx->bpb.sectors_per_track;
        ctx->geometry.heads = (uint8_t)ctx->bpb.num_heads;
        ctx->geometry.media_descriptor = ctx->bpb.media_descriptor;
        
        uint32_t total_sectors = ctx->bpb.total_sectors_16 ? 
                                 ctx->bpb.total_sectors_16 : ctx->bpb.total_sectors_32;
        if (ctx->geometry.heads > 0 && ctx->geometry.sectors_per_track > 0) {
            ctx->geometry.tracks = total_sectors / 
                (ctx->geometry.heads * ctx->geometry.sectors_per_track);
        }
        ctx->geometry.total_bytes = total_sectors * ctx->geometry.sector_size;
        
        /* Match to known geometry */
        for (int i = 1; i < UFT_MSX_GEOM_COUNT - 1; i++) {
            if (ctx->geometry.total_bytes == g_msx_geometries[i].total_bytes &&
                ctx->geometry.media_descriptor == g_msx_geometries[i].media_descriptor) {
                ctx->geometry.type = (uft_msx_geometry_type_t)i;
                ctx->geometry.name = g_msx_geometries[i].name;
                break;
            }
        }
        
        /* Calculate filesystem locations */
        ctx->fat_start_sector = ctx->bpb.reserved_sectors;
        ctx->fat_sectors = ctx->bpb.sectors_per_fat;
        ctx->root_dir_sector = ctx->fat_start_sector + 
                               (ctx->bpb.num_fats * ctx->fat_sectors);
        ctx->root_dir_sectors = (ctx->bpb.root_entries * 32 + 511) / 512;
        ctx->data_start_sector = ctx->root_dir_sector + ctx->root_dir_sectors;
        
        uint32_t data_sectors = total_sectors - ctx->data_start_sector;
        ctx->total_clusters = data_sectors / ctx->bpb.sectors_per_cluster;
    } else {
        /* No valid BPB - detect by file size */
        uint8_t conf;
        ctx->geometry.type = uft_msx_detect_geometry_by_size(ctx->file_size, &conf);
        if (ctx->geometry.type != UFT_MSX_GEOM_UNKNOWN) {
            ctx->geometry = *uft_msx_get_geometry(ctx->geometry.type);
        }
    }
    
    /* Detect DOS version */
    ctx->dos_version = uft_msx_detect_dos_version(ctx);
    
    return UFT_MSX_SUCCESS;
}

void uft_msx_close(uft_msx_ctx_t* ctx) {
    if (ctx) {
        free(ctx->path);
        memset(ctx, 0, sizeof(*ctx));
    }
}

uft_msx_rc_t uft_msx_read_sector(uft_msx_ctx_t* ctx, uint32_t lba,
                                  uint8_t* buffer, size_t buffer_size) {
    if (!ctx || !ctx->path || !buffer) return UFT_MSX_ERR_ARG;
    
    uint16_t sector_size = ctx->geometry.sector_size ? ctx->geometry.sector_size : 512;
    if (buffer_size < sector_size) return UFT_MSX_ERR_ARG;
    
    uint64_t offset = (uint64_t)lba * sector_size;
    if (offset + sector_size > ctx->file_size) return UFT_MSX_ERR_RANGE;
    
    FILE* fp = fopen(ctx->path, "rb");
    if (!fp) return UFT_MSX_ERR_IO;
    
    if (fseek(fp, (long)offset, SEEK_SET) != 0) {
        fclose(fp);
        return UFT_MSX_ERR_IO;
    }
    
    if (fread(buffer, sector_size, 1, fp) != 1) {
        fclose(fp);
        return UFT_MSX_ERR_IO;
    }
    
    fclose(fp);
    return UFT_MSX_SUCCESS;
}

uft_msx_rc_t uft_msx_write_sector(uft_msx_ctx_t* ctx, uint32_t lba,
                                   const uint8_t* data, size_t data_size) {
    if (!ctx || !ctx->path || !data) return UFT_MSX_ERR_ARG;
    if (!ctx->writable) return UFT_MSX_ERR_READONLY;
    
    uint16_t sector_size = ctx->geometry.sector_size ? ctx->geometry.sector_size : 512;
    if (data_size < sector_size) return UFT_MSX_ERR_ARG;
    
    uint64_t offset = (uint64_t)lba * sector_size;
    if (offset + sector_size > ctx->file_size) return UFT_MSX_ERR_RANGE;
    
    FILE* fp = fopen(ctx->path, "r+b");
    if (!fp) return UFT_MSX_ERR_IO;
    
    if (fseek(fp, (long)offset, SEEK_SET) != 0) {
        fclose(fp);
        return UFT_MSX_ERR_IO;
    }
    
    if (fwrite(data, sector_size, 1, fp) != 1) {
        fclose(fp);
        return UFT_MSX_ERR_IO;
    }
    
    fclose(fp);
    return UFT_MSX_SUCCESS;
}

/*============================================================================
 * DOS Version Detection
 *============================================================================*/

uft_msx_dos_version_t uft_msx_detect_dos_version(const uft_msx_ctx_t* ctx) {
    if (!ctx || !ctx->has_valid_bpb) return UFT_MSX_DOS_UNKNOWN;
    
    /* Check OEM name for hints */
    if (memcmp(ctx->bpb.oem_name, "NEXTOR", 6) == 0) {
        return UFT_MSX_NEXTOR;
    }
    
    /* MSX-DOS 2 uses specific BPB values */
    if (ctx->bpb.hidden_sectors != 0 || ctx->bpb.total_sectors_32 != 0) {
        return UFT_MSX_DOS_2;
    }
    
    /* CP/M-80 check - no standard BPB */
    if (ctx->bpb.bytes_per_sector != 512) {
        return UFT_MSX_CPM;
    }
    
    /* Default to MSX-DOS 1 for standard disks */
    return UFT_MSX_DOS_1;
}

const char* uft_msx_dos_version_name(uft_msx_dos_version_t version) {
    switch (version) {
        case UFT_MSX_DOS_1: return "MSX-DOS 1.x";
        case UFT_MSX_DOS_2: return "MSX-DOS 2.x";
        case UFT_MSX_NEXTOR: return "Nextor";
        case UFT_MSX_BASIC: return "Disk BASIC";
        case UFT_MSX_CPM: return "CP/M-80";
        default: return "Unknown";
    }
}

/*============================================================================
 * Directory Operations
 *============================================================================*/

/* Convert directory entry to file info */
static void dirent_to_info(const uft_msx_dirent_t* de, uft_msx_file_info_t* info) {
    /* Build 8.3 filename */
    int pos = 0;
    for (int i = 0; i < 8 && de->name[i] != ' '; i++) {
        info->filename[pos++] = de->name[i];
    }
    if (de->ext[0] != ' ') {
        info->filename[pos++] = '.';
        for (int i = 0; i < 3 && de->ext[i] != ' '; i++) {
            info->filename[pos++] = de->ext[i];
        }
    }
    info->filename[pos] = '\0';
    
    info->attributes = de->attributes;
    info->size = de->file_size;
    info->start_cluster = de->start_cluster;
    info->date = de->date;
    info->time = de->time;
    
    info->is_directory = (de->attributes & UFT_MSX_ATTR_DIRECTORY) != 0;
    info->is_hidden = (de->attributes & UFT_MSX_ATTR_HIDDEN) != 0;
    info->is_system = (de->attributes & UFT_MSX_ATTR_SYSTEM) != 0;
    info->is_readonly = (de->attributes & UFT_MSX_ATTR_READONLY) != 0;
}

uft_msx_rc_t uft_msx_read_root_dir(uft_msx_ctx_t* ctx,
                                    uft_msx_dir_callback_t callback, void* user_data) {
    if (!ctx || !ctx->has_valid_bpb || !callback) return UFT_MSX_ERR_ARG;
    
    uint8_t sector[512];
    
    for (uint32_t s = 0; s < ctx->root_dir_sectors; s++) {
        uft_msx_rc_t rc = uft_msx_read_sector(ctx, ctx->root_dir_sector + s,
                                               sector, sizeof(sector));
        if (rc != UFT_MSX_SUCCESS) return rc;
        
        /* Process 16 entries per sector */
        for (int e = 0; e < 16; e++) {
            const uft_msx_dirent_t* de = (const uft_msx_dirent_t*)(sector + e * 32);
            
            /* End of directory */
            if (de->name[0] == 0x00) return UFT_MSX_SUCCESS;
            
            /* Skip deleted entries */
            if (de->name[0] == 0xE5) continue;
            
            /* Skip volume label */
            if (de->attributes == UFT_MSX_ATTR_VOLUME) continue;
            
            /* Skip LFN entries (for Nextor compatibility) */
            if ((de->attributes & 0x0F) == 0x0F) continue;
            
            uft_msx_file_info_t info;
            dirent_to_info(de, &info);
            
            if (!callback(&info, user_data)) {
                return UFT_MSX_SUCCESS; /* Callback requested stop */
            }
        }
    }
    
    return UFT_MSX_SUCCESS;
}

/* Find file callback context */
typedef struct {
    const char* target;
    uft_msx_file_info_t* result;
    bool found;
} find_file_ctx_t;

static bool find_file_callback(const uft_msx_file_info_t* file, void* user_data) {
    find_file_ctx_t* ctx = (find_file_ctx_t*)user_data;
    
    /* Case-insensitive compare */
    if (strcasecmp(file->filename, ctx->target) == 0) {
        *ctx->result = *file;
        ctx->found = true;
        return false; /* Stop iteration */
    }
    return true; /* Continue */
}

uft_msx_rc_t uft_msx_find_file(uft_msx_ctx_t* ctx, const char* filename,
                                uft_msx_file_info_t* info) {
    if (!ctx || !filename || !info) return UFT_MSX_ERR_ARG;
    
    find_file_ctx_t find_ctx = {
        .target = filename,
        .result = info,
        .found = false
    };
    
    uft_msx_rc_t rc = uft_msx_read_root_dir(ctx, find_file_callback, &find_ctx);
    if (rc != UFT_MSX_SUCCESS) return rc;
    
    return find_ctx.found ? UFT_MSX_SUCCESS : UFT_MSX_ERR_NOTFOUND;
}

uft_msx_rc_t uft_msx_get_volume_label(uft_msx_ctx_t* ctx, char* label, size_t label_size) {
    if (!ctx || !label || label_size < 1) return UFT_MSX_ERR_ARG;
    
    label[0] = '\0';
    
    if (!ctx->has_valid_bpb) return UFT_MSX_ERR_FORMAT;
    
    uint8_t sector[512];
    
    for (uint32_t s = 0; s < ctx->root_dir_sectors; s++) {
        uft_msx_rc_t rc = uft_msx_read_sector(ctx, ctx->root_dir_sector + s,
                                               sector, sizeof(sector));
        if (rc != UFT_MSX_SUCCESS) return rc;
        
        for (int e = 0; e < 16; e++) {
            const uft_msx_dirent_t* de = (const uft_msx_dirent_t*)(sector + e * 32);
            
            if (de->name[0] == 0x00) break;
            if (de->name[0] == 0xE5) continue;
            
            if (de->attributes == UFT_MSX_ATTR_VOLUME) {
                /* Found volume label */
                int pos = 0;
                for (int i = 0; i < 8 && de->name[i] != ' ' && pos < (int)label_size - 1; i++) {
                    label[pos++] = de->name[i];
                }
                for (int i = 0; i < 3 && de->ext[i] != ' ' && pos < (int)label_size - 1; i++) {
                    label[pos++] = de->ext[i];
                }
                label[pos] = '\0';
                return UFT_MSX_SUCCESS;
            }
        }
    }
    
    return UFT_MSX_ERR_NOTFOUND;
}

/*============================================================================
 * FAT Operations
 *============================================================================*/

uint16_t uft_msx_fat_get_entry(uft_msx_ctx_t* ctx, uint16_t cluster) {
    if (!ctx || !ctx->has_valid_bpb || cluster < 2) return 0xFFF;
    
    /* FAT12: 12 bits per entry */
    uint32_t fat_offset = cluster + (cluster / 2);
    uint32_t fat_sector = ctx->fat_start_sector + (fat_offset / 512);
    uint32_t entry_offset = fat_offset % 512;
    
    uint8_t sector[512];
    if (uft_msx_read_sector(ctx, fat_sector, sector, sizeof(sector)) != UFT_MSX_SUCCESS) {
        return 0xFFF;
    }
    
    uint16_t entry;
    if (entry_offset == 511) {
        /* Entry spans sectors */
        uint8_t sector2[512];
        if (uft_msx_read_sector(ctx, fat_sector + 1, sector2, sizeof(sector2)) != UFT_MSX_SUCCESS) {
            return 0xFFF;
        }
        entry = sector[511] | (sector2[0] << 8);
    } else {
        entry = sector[entry_offset] | (sector[entry_offset + 1] << 8);
    }
    
    /* Extract 12-bit value */
    if (cluster & 1) {
        entry >>= 4;
    } else {
        entry &= 0xFFF;
    }
    
    return entry;
}

uint32_t uft_msx_fat_count_free(uft_msx_ctx_t* ctx) {
    if (!ctx || !ctx->has_valid_bpb) return 0;
    
    uint32_t free_count = 0;
    for (uint16_t c = 2; c < ctx->total_clusters + 2; c++) {
        if (uft_msx_fat_get_entry(ctx, c) == 0) {
            free_count++;
        }
    }
    return free_count;
}

/*============================================================================
 * File Extraction
 *============================================================================*/

uft_msx_rc_t uft_msx_extract_file(uft_msx_ctx_t* ctx, const char* filename,
                                   const char* output_path) {
    if (!ctx || !filename || !output_path) return UFT_MSX_ERR_ARG;
    
    /* Find file */
    uft_msx_file_info_t info;
    uft_msx_rc_t rc = uft_msx_find_file(ctx, filename, &info);
    if (rc != UFT_MSX_SUCCESS) return rc;
    
    if (info.is_directory) return UFT_MSX_ERR_ARG;
    
    FILE* out = fopen(output_path, "wb");
    if (!out) return UFT_MSX_ERR_IO;
    
    uint8_t* cluster_buf = malloc(ctx->bpb.sectors_per_cluster * 512);
    if (!cluster_buf) {
        fclose(out);
        return UFT_MSX_ERR_NOMEM;
    }
    
    uint16_t cluster = info.start_cluster;
    uint32_t remaining = info.size;
    uint32_t cluster_size = ctx->bpb.sectors_per_cluster * 512;
    
    while (cluster >= 2 && cluster < 0xFF0 && remaining > 0) {
        /* Read cluster */
        uint32_t first_sector = ctx->data_start_sector + 
                                (cluster - 2) * ctx->bpb.sectors_per_cluster;
        
        for (int s = 0; s < ctx->bpb.sectors_per_cluster; s++) {
            rc = uft_msx_read_sector(ctx, first_sector + s,
                                      cluster_buf + s * 512, 512);
            if (rc != UFT_MSX_SUCCESS) {
                free(cluster_buf);
                fclose(out);
                return rc;
            }
        }
        
        /* Write data */
        uint32_t to_write = remaining < cluster_size ? remaining : cluster_size;
        if (fwrite(cluster_buf, 1, to_write, out) != to_write) {
            free(cluster_buf);
            fclose(out);
            return UFT_MSX_ERR_IO;
        }
        
        remaining -= to_write;
        cluster = uft_msx_fat_get_entry(ctx, cluster);
    }
    
    free(cluster_buf);
    fclose(out);
    return UFT_MSX_SUCCESS;
}

/*============================================================================
 * Copy Protection Detection
 *============================================================================*/

uft_msx_rc_t uft_msx_detect_protection(const char* path,
                                        uft_msx_protection_result_t* result) {
    if (!path || !result) return UFT_MSX_ERR_ARG;
    
    memset(result, 0, sizeof(*result));
    
    uft_msx_ctx_t ctx;
    uft_msx_rc_t rc = uft_msx_open(&ctx, path, false);
    if (rc != UFT_MSX_SUCCESS) return rc;
    
    int indicators = 0;
    
    /* Check for non-standard media descriptor */
    if (ctx.has_valid_bpb) {
        uint8_t md = ctx.bpb.media_descriptor;
        if (md != 0xF0 && md != 0xF8 && md != 0xF9 && 
            md != 0xFA && md != 0xFB && md != 0xFC && 
            md != 0xFD && md != 0xFE && md != 0xFF) {
            result->flags |= UFT_MSX_PROT_MEDIA_DESC;
            indicators++;
        }
    }
    
    /* Check for extra tracks (file larger than expected) */
    if (ctx.geometry.type != UFT_MSX_GEOM_UNKNOWN) {
        const uft_msx_geometry_t* std_geom = uft_msx_get_geometry(ctx.geometry.type);
        if (ctx.file_size > std_geom->total_bytes) {
            uint64_t extra = ctx.file_size - std_geom->total_bytes;
            result->extra_tracks = (uint8_t)(extra / 
                (std_geom->heads * std_geom->sectors_per_track * std_geom->sector_size));
            if (result->extra_tracks > 0) {
                result->flags |= UFT_MSX_PROT_EXTRA_TRACKS;
                indicators++;
            }
        }
    }
    
    /* Check for extra sectors per track (from BPB) */
    if (ctx.has_valid_bpb && ctx.geometry.type != UFT_MSX_GEOM_UNKNOWN) {
        const uft_msx_geometry_t* std_geom = uft_msx_get_geometry(ctx.geometry.type);
        if (ctx.bpb.sectors_per_track > std_geom->sectors_per_track) {
            result->extra_sectors = ctx.bpb.sectors_per_track - std_geom->sectors_per_track;
            result->flags |= UFT_MSX_PROT_EXTRA_SECTORS;
            indicators++;
        }
    }
    
    /* Calculate confidence */
    result->confidence = indicators * 25;
    if (result->confidence > 100) result->confidence = 100;
    
    /* Build description */
    if (result->flags != 0) {
        char* p = result->description;
        size_t remaining = sizeof(result->description);
        
        if (result->flags & UFT_MSX_PROT_EXTRA_TRACKS) {
            int written = snprintf(p, remaining, "Extra tracks: %d; ", result->extra_tracks);
            p += written;
            remaining -= written;
        }
        if (result->flags & UFT_MSX_PROT_EXTRA_SECTORS) {
            int written = snprintf(p, remaining, "Extra sectors: %d; ", result->extra_sectors);
            p += written;
            remaining -= written;
        }
        if (result->flags & UFT_MSX_PROT_MEDIA_DESC) {
            snprintf(p, remaining, "Non-standard media descriptor: 0x%02X; ",
                     ctx.bpb.media_descriptor);
        }
    } else {
        strncpy(result->description, "No protection detected", 255); result->description[255] = '\0';
    }
    
    uft_msx_close(&ctx);
    return UFT_MSX_SUCCESS;
}

/*============================================================================
 * Format Creation
 *============================================================================*/

uft_msx_rc_t uft_msx_create_blank(const char* path, uft_msx_geometry_type_t geometry,
                                   const char* volume_label) {
    if (!path || geometry <= UFT_MSX_GEOM_UNKNOWN || geometry >= UFT_MSX_GEOM_COUNT) {
        return UFT_MSX_ERR_ARG;
    }
    
    const uft_msx_geometry_t* geom = uft_msx_get_geometry(geometry);
    if (geom->total_bytes == 0) return UFT_MSX_ERR_GEOMETRY;
    
    FILE* fp = fopen(path, "wb");
    if (!fp) return UFT_MSX_ERR_IO;
    
    /* Create boot sector with BPB */
    uint8_t boot[512] = {0};
    
    /* Jump instruction */
    boot[0] = 0xEB;
    boot[1] = 0xFE;
    boot[2] = 0x90;
    
    /* OEM name */
    memcpy(boot + 3, "MSX-UFT ", 8);
    
    /* BPB */
    boot[11] = 0x00; boot[12] = 0x02;  /* Bytes per sector: 512 */
    boot[13] = 0x02;                     /* Sectors per cluster */
    boot[14] = 0x01; boot[15] = 0x00;  /* Reserved sectors */
    boot[16] = 0x02;                     /* Number of FATs */
    boot[17] = 0x70; boot[18] = 0x00;  /* Root entries: 112 */
    
    uint32_t total_sectors = geom->total_bytes / geom->sector_size;
    boot[19] = total_sectors & 0xFF;
    boot[20] = (total_sectors >> 8) & 0xFF;
    
    boot[21] = geom->media_descriptor;   /* Media descriptor */
    boot[22] = 0x03; boot[23] = 0x00;  /* Sectors per FAT */
    boot[24] = geom->sectors_per_track;
    boot[25] = 0x00;
    boot[26] = geom->heads;
    boot[27] = 0x00;
    
    /* Boot signature */
    boot[510] = 0x55;
    boot[511] = 0xAA;
    
    if (fwrite(boot, 1, 512, fp) != 512) {
        fclose(fp);
        return UFT_MSX_ERR_IO;
    }
    
    /* FAT 1 */
    uint8_t fat[512] = {0};
    fat[0] = geom->media_descriptor;
    fat[1] = 0xFF;
    fat[2] = 0xFF;
    
    for (int f = 0; f < 2; f++) {
        if (fwrite(fat, 1, 512, fp) != 512) {
            fclose(fp);
            return UFT_MSX_ERR_IO;
        }
        /* Remaining FAT sectors */
        uint8_t zeros[512] = {0};
        for (int s = 1; s < 3; s++) {
            if (fwrite(zeros, 1, 512, fp) != 512) {
                fclose(fp);
                return UFT_MSX_ERR_IO;
            }
        }
    }
    
    /* Root directory with volume label */
    uint8_t root[512] = {0};
    if (volume_label && volume_label[0]) {
        /* Create volume label entry */
        memset(root, ' ', 11);
        size_t len = strlen(volume_label);
        if (len > 11) len = 11;
        memcpy(root, volume_label, len);
        root[11] = UFT_MSX_ATTR_VOLUME;
    }
    
    /* 7 root directory sectors for 112 entries */
    for (int s = 0; s < 7; s++) {
        if (fwrite(s == 0 ? root : (uint8_t[512]){0}, 1, 512, fp) != 512) {
            fclose(fp);
            return UFT_MSX_ERR_IO;
        }
    }
    
    /* Data area - fill with zeros */
    uint8_t zeros[4096] = {0};
    uint32_t written = 1 + 6 + 7; /* boot + 2 FATs + root dir */
    uint32_t remaining = total_sectors - written;
    
    while (remaining > 0) {
        uint32_t chunk = remaining > 8 ? 8 : remaining;
        if (fwrite(zeros, 512, chunk, fp) != chunk) {
            fclose(fp);
            return UFT_MSX_ERR_IO;
        }
        remaining -= chunk;
    }
    
    fclose(fp);
    return UFT_MSX_SUCCESS;
}

/*============================================================================
 * Conversion
 *============================================================================*/

uft_msx_rc_t uft_msx_to_raw(uft_msx_ctx_t* ctx, const char* output_path) {
    if (!ctx || !ctx->path || !output_path) return UFT_MSX_ERR_ARG;
    
    FILE* fin = fopen(ctx->path, "rb");
    if (!fin) return UFT_MSX_ERR_IO;
    
    FILE* fout = fopen(output_path, "wb");
    if (!fout) {
        fclose(fin);
        return UFT_MSX_ERR_IO;
    }
    
    uint8_t buffer[4096];
    size_t read;
    while ((read = fread(buffer, 1, sizeof(buffer), fin)) > 0) {
        if (fwrite(buffer, 1, read, fout) != read) {
            fclose(fin);
            fclose(fout);
            return UFT_MSX_ERR_IO;
        }
    }
    
    fclose(fin);
    fclose(fout);
    return UFT_MSX_SUCCESS;
}

/*============================================================================
 * Analysis and Reporting
 *============================================================================*/

/* Count callback context */
typedef struct {
    uint32_t file_count;
    uint32_t dir_count;
    bool has_autoexec;
} count_ctx_t;

static bool count_callback(const uft_msx_file_info_t* file, void* user_data) {
    count_ctx_t* ctx = (count_ctx_t*)user_data;
    
    if (file->is_directory) {
        ctx->dir_count++;
    } else {
        ctx->file_count++;
        if (strcasecmp(file->filename, "AUTOEXEC.BAT") == 0 ||
            strcasecmp(file->filename, "AUTOEXEC.BAS") == 0) {
            ctx->has_autoexec = true;
        }
    }
    return true;
}

uft_msx_rc_t uft_msx_analyze(const char* path, uft_msx_report_t* report) {
    if (!path || !report) return UFT_MSX_ERR_ARG;
    
    memset(report, 0, sizeof(*report));
    
    uft_msx_ctx_t ctx;
    uft_msx_rc_t rc = uft_msx_open(&ctx, path, false);
    if (rc != UFT_MSX_SUCCESS) return rc;
    
    /* Copy geometry */
    report->geometry = ctx.geometry;
    report->dos_version = ctx.dos_version;
    
    /* OEM name */
    memcpy(report->oem_name, ctx.bpb.oem_name, 8);
    report->oem_name[8] = '\0';
    
    /* Volume label */
    uft_msx_get_volume_label(&ctx, report->volume_label, sizeof(report->volume_label));
    
    /* Statistics */
    report->total_sectors = ctx.bpb.total_sectors_16 ? 
                           ctx.bpb.total_sectors_16 : ctx.bpb.total_sectors_32;
    report->bytes_per_cluster = ctx.bpb.sectors_per_cluster * 512;
    report->free_clusters = uft_msx_fat_count_free(&ctx);
    report->used_clusters = ctx.total_clusters - report->free_clusters;
    report->total_space = ctx.total_clusters * report->bytes_per_cluster;
    report->free_space = report->free_clusters * report->bytes_per_cluster;
    
    /* Count files */
    count_ctx_t count = {0};
    uft_msx_read_root_dir(&ctx, count_callback, &count);
    report->file_count = count.file_count;
    report->dir_count = count.dir_count;
    report->has_autoexec = count.has_autoexec;
    report->has_subdirs = count.dir_count > 0;
    
    /* Check bootability */
    uint8_t boot[512];
    if (uft_msx_read_sector(&ctx, 0, boot, sizeof(boot)) == UFT_MSX_SUCCESS) {
        report->is_bootable = (boot[0] == 0xEB || boot[0] == 0xE9);
    }
    
    /* Protection detection */
    uft_msx_detect_protection(path, &report->protection);
    
    uft_msx_close(&ctx);
    return UFT_MSX_SUCCESS;
}

int uft_msx_report_to_json(const uft_msx_report_t* report, char* json_out, size_t capacity) {
    if (!report || !json_out || capacity < 512) return -1;
    
    return snprintf(json_out, capacity,
        "{\n"
        "  \"geometry\": {\n"
        "    \"type\": \"%s\",\n"
        "    \"tracks\": %u,\n"
        "    \"heads\": %u,\n"
        "    \"sectors_per_track\": %u,\n"
        "    \"sector_size\": %u,\n"
        "    \"total_bytes\": %u\n"
        "  },\n"
        "  \"dos_version\": \"%s\",\n"
        "  \"oem_name\": \"%s\",\n"
        "  \"volume_label\": \"%s\",\n"
        "  \"statistics\": {\n"
        "    \"total_sectors\": %u,\n"
        "    \"used_clusters\": %u,\n"
        "    \"free_clusters\": %u,\n"
        "    \"total_space\": %u,\n"
        "    \"free_space\": %u,\n"
        "    \"file_count\": %u,\n"
        "    \"dir_count\": %u\n"
        "  },\n"
        "  \"features\": {\n"
        "    \"has_autoexec\": %s,\n"
        "    \"is_bootable\": %s,\n"
        "    \"has_subdirs\": %s\n"
        "  },\n"
        "  \"protection\": {\n"
        "    \"detected\": %s,\n"
        "    \"confidence\": %u,\n"
        "    \"description\": \"%s\"\n"
        "  }\n"
        "}",
        report->geometry.name ? report->geometry.name : "Unknown",
        report->geometry.tracks,
        report->geometry.heads,
        report->geometry.sectors_per_track,
        report->geometry.sector_size,
        report->geometry.total_bytes,
        uft_msx_dos_version_name(report->dos_version),
        report->oem_name,
        report->volume_label,
        report->total_sectors,
        report->used_clusters,
        report->free_clusters,
        report->total_space,
        report->free_space,
        report->file_count,
        report->dir_count,
        report->has_autoexec ? "true" : "false",
        report->is_bootable ? "true" : "false",
        report->has_subdirs ? "true" : "false",
        report->protection.flags ? "true" : "false",
        report->protection.confidence,
        report->protection.description
    );
}

int uft_msx_report_to_markdown(const uft_msx_report_t* report, char* md_out, size_t capacity) {
    if (!report || !md_out || capacity < 1024) return -1;
    
    return snprintf(md_out, capacity,
        "# MSX Disk Analysis Report\n\n"
        "## Geometry\n"
        "- **Type**: %s\n"
        "- **Tracks**: %u\n"
        "- **Heads**: %u\n"
        "- **Sectors/Track**: %u\n"
        "- **Sector Size**: %u bytes\n"
        "- **Total Size**: %u bytes\n\n"
        "## System Information\n"
        "- **DOS Version**: %s\n"
        "- **OEM Name**: %s\n"
        "- **Volume Label**: %s\n\n"
        "## Space Statistics\n"
        "| Metric | Value |\n"
        "|--------|-------|\n"
        "| Total Sectors | %u |\n"
        "| Used Clusters | %u |\n"
        "| Free Clusters | %u |\n"
        "| Total Space | %u bytes |\n"
        "| Free Space | %u bytes |\n\n"
        "## Content\n"
        "- **Files**: %u\n"
        "- **Directories**: %u\n"
        "- **Has AUTOEXEC**: %s\n"
        "- **Bootable**: %s\n\n"
        "## Copy Protection\n"
        "- **Detected**: %s\n"
        "- **Confidence**: %u%%\n"
        "- **Details**: %s\n",
        report->geometry.name ? report->geometry.name : "Unknown",
        report->geometry.tracks,
        report->geometry.heads,
        report->geometry.sectors_per_track,
        report->geometry.sector_size,
        report->geometry.total_bytes,
        uft_msx_dos_version_name(report->dos_version),
        report->oem_name,
        report->volume_label[0] ? report->volume_label : "(none)",
        report->total_sectors,
        report->used_clusters,
        report->free_clusters,
        report->total_space,
        report->free_space,
        report->file_count,
        report->dir_count,
        report->has_autoexec ? "Yes" : "No",
        report->is_bootable ? "Yes" : "No",
        report->protection.flags ? "Yes" : "No",
        report->protection.confidence,
        report->protection.description
    );
}
