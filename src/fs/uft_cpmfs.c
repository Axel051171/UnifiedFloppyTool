/**
 * @file uft_cpmfs.c
 * @brief CP/M Filesystem Implementation
 * 
 * EXT3-010: CP/M 2.2/3.0 filesystem support
 * 
 * Features:
 * - CP/M 2.2 directory structure
 * - CP/M 3.0 (Plus) timestamps
 * - Multiple disk parameter blocks (DPB)
 * - User areas (0-15)
 * - Extent handling
 */

#include "uft/fs/uft_cpmfs.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/*===========================================================================
 * Standard Disk Parameter Blocks
 *===========================================================================*/

/* Standard 8" SSSD (IBM 3740) */
static const uft_cpm_dpb_t DPB_8_SSSD = {
    .spt = 26,          /* Sectors per track */
    .bsh = 3,           /* Block shift (1024 byte blocks) */
    .blm = 7,           /* Block mask */
    .exm = 0,           /* Extent mask */
    .dsm = 242,         /* Max block number */
    .drm = 63,          /* Max directory entry */
    .al0 = 0xC0,        /* Allocation bitmap 0 */
    .al1 = 0x00,        /* Allocation bitmap 1 */
    .cks = 16,          /* Directory check vector size */
    .off = 2,           /* Reserved tracks */
    .sector_size = 128,
    .tracks = 77,
    .sides = 1
};

/* 5.25" DSDD (Kaypro, Osborne) */
static const uft_cpm_dpb_t DPB_525_DSDD = {
    .spt = 40,
    .bsh = 4,           /* 2048 byte blocks */
    .blm = 15,
    .exm = 1,
    .dsm = 194,
    .drm = 63,
    .al0 = 0x80,
    .al1 = 0x00,
    .cks = 16,
    .off = 2,
    .sector_size = 512,
    .tracks = 40,
    .sides = 2
};

/* 3.5" DSDD (Amstrad PCW) */
static const uft_cpm_dpb_t DPB_35_DSDD = {
    .spt = 36,
    .bsh = 4,
    .blm = 15,
    .exm = 1,
    .dsm = 179,
    .drm = 63,
    .al0 = 0xC0,
    .al1 = 0x00,
    .cks = 16,
    .off = 1,
    .sector_size = 512,
    .tracks = 80,
    .sides = 2
};

/*===========================================================================
 * Helpers
 *===========================================================================*/

static uint16_t read_le16(const uint8_t *p)
{
    return p[0] | (p[1] << 8);
}

/* Calculate block size from BSH */
static uint16_t block_size(const uft_cpm_dpb_t *dpb)
{
    return 128 << dpb->bsh;
}

/* Calculate records (128-byte units) per block */
static uint16_t records_per_block(const uft_cpm_dpb_t *dpb)
{
    return 1 << dpb->bsh;
}

/*===========================================================================
 * Filesystem Open/Close
 *===========================================================================*/

int uft_cpmfs_open(uft_cpmfs_ctx_t *ctx, const uint8_t *data, size_t size,
                   const uft_cpm_dpb_t *dpb)
{
    if (!ctx || !data) return -1;
    
    memset(ctx, 0, sizeof(*ctx));
    ctx->data = data;
    ctx->size = size;
    
    /* Use provided DPB or try to detect */
    if (dpb) {
        ctx->dpb = *dpb;
    } else {
        /* Try to detect format by size */
        if (size == 77 * 26 * 128) {
            ctx->dpb = DPB_8_SSSD;
        } else if (size == 40 * 2 * 40 * 512) {
            ctx->dpb = DPB_525_DSDD;
        } else if (size == 80 * 2 * 9 * 512) {
            ctx->dpb = DPB_35_DSDD;
        } else {
            /* Default to common format */
            ctx->dpb = DPB_525_DSDD;
        }
    }
    
    /* Calculate derived values */
    ctx->block_size = block_size(&ctx->dpb);
    ctx->dir_entries = ctx->dpb.drm + 1;
    
    /* Directory starts after reserved tracks */
    ctx->dir_offset = ctx->dpb.off * ctx->dpb.spt * ctx->dpb.sector_size;
    
    /* Directory size in bytes */
    ctx->dir_size = ctx->dir_entries * 32;
    
    /* First data block after directory */
    /* Calculate how many blocks directory uses */
    int dir_blocks = 0;
    uint8_t al = ctx->dpb.al0;
    for (int i = 0; i < 8; i++) {
        if (al & 0x80) dir_blocks++;
        al <<= 1;
    }
    al = ctx->dpb.al1;
    for (int i = 0; i < 8; i++) {
        if (al & 0x80) dir_blocks++;
        al <<= 1;
    }
    
    ctx->data_offset = ctx->dir_offset + dir_blocks * ctx->block_size;
    
    ctx->is_valid = true;
    return 0;
}

void uft_cpmfs_close(uft_cpmfs_ctx_t *ctx)
{
    if (ctx) {
        memset(ctx, 0, sizeof(*ctx));
    }
}

/*===========================================================================
 * Directory Operations
 *===========================================================================*/

static int parse_dir_entry(const uint8_t *entry, uft_cpm_dirent_t *de)
{
    memset(de, 0, sizeof(*de));
    
    de->user = entry[0];
    
    /* Skip deleted/empty entries */
    if (de->user == 0xE5) return -1;
    if (de->user > 31) return -1;  /* Invalid user number */
    
    /* Filename (8 bytes, high bit = attribute) */
    for (int i = 0; i < 8; i++) {
        de->filename[i] = entry[1 + i] & 0x7F;
    }
    de->filename[8] = '\0';
    
    /* Extension (3 bytes, high bits = R/O, SYS, ARC) */
    de->read_only = (entry[9] & 0x80) != 0;
    de->system = (entry[10] & 0x80) != 0;
    de->archived = (entry[11] & 0x80) != 0;
    
    for (int i = 0; i < 3; i++) {
        de->extension[i] = entry[9 + i] & 0x7F;
    }
    de->extension[3] = '\0';
    
    /* Trim trailing spaces */
    for (int i = 7; i >= 0 && de->filename[i] == ' '; i--) {
        de->filename[i] = '\0';
    }
    for (int i = 2; i >= 0 && de->extension[i] == ' '; i--) {
        de->extension[i] = '\0';
    }
    
    /* Extent info */
    de->extent_low = entry[12];
    de->s1 = entry[13];  /* Reserved */
    de->s2 = entry[14];  /* Extent high bits (CP/M 3) */
    de->record_count = entry[15];
    
    /* Allocation blocks (16 bytes) */
    memcpy(de->blocks, entry + 16, 16);
    
    /* Calculate extent number */
    de->extent = de->extent_low | ((de->s2 & 0x3F) << 5);
    
    return 0;
}

int uft_cpmfs_read_directory(const uft_cpmfs_ctx_t *ctx,
                             uft_cpm_dirent_t *entries, size_t max_entries,
                             size_t *count)
{
    if (!ctx || !entries || !count || !ctx->is_valid) return -1;
    
    *count = 0;
    
    if (ctx->dir_offset + ctx->dir_size > ctx->size) return -1;
    
    const uint8_t *dir = ctx->data + ctx->dir_offset;
    
    for (size_t i = 0; i < ctx->dir_entries && *count < max_entries; i++) {
        uft_cpm_dirent_t de;
        
        if (parse_dir_entry(dir + i * 32, &de) == 0) {
            entries[*count] = de;
            (*count)++;
        }
    }
    
    return 0;
}

/*===========================================================================
 * File Operations
 *===========================================================================*/

/* Find all extents for a file */
static int find_file_extents(const uft_cpmfs_ctx_t *ctx, 
                             const char *filename, const char *extension,
                             uint8_t user,
                             uft_cpm_dirent_t *extents, int max_extents,
                             int *extent_count)
{
    *extent_count = 0;
    
    const uint8_t *dir = ctx->data + ctx->dir_offset;
    
    for (size_t i = 0; i < ctx->dir_entries && *extent_count < max_extents; i++) {
        uft_cpm_dirent_t de;
        
        if (parse_dir_entry(dir + i * 32, &de) != 0) continue;
        if (de.user != user) continue;
        
        /* Case-insensitive compare */
        if (strcasecmp(de.filename, filename) != 0) continue;
        if (strcasecmp(de.extension, extension) != 0) continue;
        
        extents[*extent_count] = de;
        (*extent_count)++;
    }
    
    /* Sort by extent number */
    for (int i = 0; i < *extent_count - 1; i++) {
        for (int j = i + 1; j < *extent_count; j++) {
            if (extents[j].extent < extents[i].extent) {
                uft_cpm_dirent_t temp = extents[i];
                extents[i] = extents[j];
                extents[j] = temp;
            }
        }
    }
    
    return 0;
}

int uft_cpmfs_read_file(const uft_cpmfs_ctx_t *ctx,
                        const char *filename, const char *extension,
                        uint8_t user,
                        uint8_t *buffer, size_t max_size, size_t *bytes_read)
{
    if (!ctx || !filename || !buffer || !bytes_read || !ctx->is_valid) {
        return -1;
    }
    
    *bytes_read = 0;
    
    /* Find all extents */
    uft_cpm_dirent_t extents[64];
    int extent_count = 0;
    
    if (find_file_extents(ctx, filename, extension, user,
                          extents, 64, &extent_count) != 0) {
        return -1;
    }
    
    if (extent_count == 0) return -1;  /* File not found */
    
    /* Determine if using 8-bit or 16-bit block numbers */
    bool use_16bit = ctx->dpb.dsm > 255;
    int blocks_per_extent = use_16bit ? 8 : 16;
    
    /* Read each extent */
    for (int e = 0; e < extent_count; e++) {
        const uft_cpm_dirent_t *ext = &extents[e];
        
        /* Number of 128-byte records in this extent */
        int records = ext->record_count;
        if (records == 0 && e < extent_count - 1) {
            records = 128;  /* Full extent */
        }
        
        /* Read blocks */
        int records_read = 0;
        
        for (int b = 0; b < blocks_per_extent && records_read < records; b++) {
            uint16_t block_num;
            
            if (use_16bit) {
                block_num = read_le16(ext->blocks + b * 2);
            } else {
                block_num = ext->blocks[b];
            }
            
            if (block_num == 0) continue;  /* Empty block */
            
            /* Calculate block offset */
            size_t block_offset = ctx->dir_offset + block_num * ctx->block_size;
            
            /* Read records from block */
            int recs_in_block = records_per_block(&ctx->dpb);
            
            for (int r = 0; r < recs_in_block && records_read < records; r++) {
                size_t rec_offset = block_offset + r * 128;
                
                if (rec_offset + 128 > ctx->size) break;
                if (*bytes_read + 128 > max_size) break;
                
                memcpy(buffer + *bytes_read, ctx->data + rec_offset, 128);
                *bytes_read += 128;
                records_read++;
            }
        }
    }
    
    return 0;
}

/*===========================================================================
 * Statistics
 *===========================================================================*/

int uft_cpmfs_get_stats(const uft_cpmfs_ctx_t *ctx, uft_cpm_stats_t *stats)
{
    if (!ctx || !stats || !ctx->is_valid) return -1;
    
    memset(stats, 0, sizeof(*stats));
    
    /* Count files and used blocks */
    bool block_used[4096] = {false};
    bool use_16bit = ctx->dpb.dsm > 255;
    int blocks_per_extent = use_16bit ? 8 : 16;
    
    const uint8_t *dir = ctx->data + ctx->dir_offset;
    
    for (size_t i = 0; i < ctx->dir_entries; i++) {
        uft_cpm_dirent_t de;
        
        if (parse_dir_entry(dir + i * 32, &de) != 0) continue;
        
        /* Count unique files (extent 0) */
        if (de.extent == 0) {
            stats->file_count++;
        }
        
        /* Mark used blocks */
        for (int b = 0; b < blocks_per_extent; b++) {
            uint16_t block_num;
            
            if (use_16bit) {
                block_num = read_le16(de.blocks + b * 2);
            } else {
                block_num = de.blocks[b];
            }
            
            if (block_num > 0 && block_num <= ctx->dpb.dsm) {
                block_used[block_num] = true;
            }
        }
        
        stats->dir_entries_used++;
    }
    
    /* Count used/free blocks */
    for (uint16_t b = 0; b <= ctx->dpb.dsm; b++) {
        if (block_used[b]) {
            stats->blocks_used++;
        } else {
            stats->blocks_free++;
        }
    }
    
    stats->total_blocks = ctx->dpb.dsm + 1;
    stats->block_size = ctx->block_size;
    stats->bytes_free = stats->blocks_free * ctx->block_size;
    
    return 0;
}

/*===========================================================================
 * Format Detection
 *===========================================================================*/

const uft_cpm_dpb_t *uft_cpmfs_detect_format(const uint8_t *data, size_t size)
{
    if (!data || size < 1024) return NULL;
    
    /* Check by size */
    if (size == 77 * 26 * 128) {
        return &DPB_8_SSSD;
    }
    if (size == 40 * 2 * 40 * 512) {
        return &DPB_525_DSDD;
    }
    if (size == 80 * 2 * 9 * 512) {
        return &DPB_35_DSDD;
    }
    
    /* Try to detect by directory structure */
    /* Check various offsets for valid directory entries */
    
    for (int off = 0; off < 4; off++) {
        size_t dir_start = off * 26 * 128;  /* Try different track offsets */
        
        if (dir_start + 32 > size) continue;
        
        int valid_entries = 0;
        
        for (int e = 0; e < 64; e++) {
            const uint8_t *entry = data + dir_start + e * 32;
            
            if (entry[0] == 0xE5) continue;  /* Deleted */
            if (entry[0] > 31) continue;     /* Invalid user */
            
            /* Check for valid filename characters */
            bool valid = true;
            for (int i = 1; i < 12 && valid; i++) {
                uint8_t c = entry[i] & 0x7F;
                if (c < 0x20 || c > 0x7E) valid = false;
            }
            
            if (valid) valid_entries++;
        }
        
        if (valid_entries >= 3) {
            return &DPB_8_SSSD;  /* Return default */
        }
    }
    
    return NULL;
}

/*===========================================================================
 * Report
 *===========================================================================*/

int uft_cpmfs_report_json(const uft_cpmfs_ctx_t *ctx, char *buffer, size_t size)
{
    if (!ctx || !buffer || size == 0) return -1;
    
    uft_cpm_stats_t stats;
    uft_cpmfs_get_stats(ctx, &stats);
    
    int written = snprintf(buffer, size,
        "{\n"
        "  \"filesystem\": \"CP/M\",\n"
        "  \"valid\": %s,\n"
        "  \"block_size\": %u,\n"
        "  \"total_blocks\": %u,\n"
        "  \"blocks_free\": %u,\n"
        "  \"bytes_free\": %u,\n"
        "  \"file_count\": %u,\n"
        "  \"dir_entries\": %u,\n"
        "  \"sector_size\": %u,\n"
        "  \"sectors_per_track\": %u,\n"
        "  \"file_size\": %zu\n"
        "}",
        ctx->is_valid ? "true" : "false",
        ctx->block_size,
        stats.total_blocks,
        stats.blocks_free,
        stats.bytes_free,
        stats.file_count,
        ctx->dir_entries,
        ctx->dpb.sector_size,
        ctx->dpb.spt,
        ctx->size
    );
    
    return (written > 0 && (size_t)written < size) ? 0 : -1;
}
