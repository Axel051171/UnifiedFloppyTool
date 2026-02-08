/**
 * @file uft_amigados.c
 * @brief AmigaDOS Filesystem Implementation
 * 
 * EXT3-008: AmigaDOS OFS/FFS filesystem support
 * 
 * Features:
 * - OFS (Original File System)
 * - FFS (Fast File System)
 * - Directory/File parsing
 * - File extraction
 * - Bitmap handling
 */

#include "uft/fs/uft_amigados.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/*===========================================================================
 * Constants
 *===========================================================================*/

#define AMIGA_BLOCK_SIZE    512
#define AMIGA_DD_BLOCKS     1760    /* 80 tracks * 2 sides * 11 sectors */
#define AMIGA_HD_BLOCKS     3520    /* 80 tracks * 2 sides * 22 sectors */

/* Block types */
#define T_HEADER            2
#define T_DATA              8
#define T_LIST              16
#define T_DIRCACHE          33

/* Secondary types */
#define ST_ROOT             1
#define ST_USERDIR          2
#define ST_SOFTLINK         3
#define ST_LINKDIR          4
#define ST_FILE             -3
#define ST_LINKFILE         -4

/* Filesystem flags */
#define FS_OFS              0
#define FS_FFS              1
#define FS_INTL             2
#define FS_DCACHE           4

/* Hash table size */
#define HT_SIZE             72

/*===========================================================================
 * Helpers
 *===========================================================================*/

static uint32_t read_be32(const uint8_t *p)
{
    return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
}

static int32_t read_be32s(const uint8_t *p)
{
    return (int32_t)read_be32(p);
}

/* Block checksum */
static uint32_t block_checksum(const uint8_t *block, size_t size)
{
    uint32_t sum = 0;
    for (size_t i = 0; i < size; i += 4) {
        sum += read_be32(block + i);
    }
    return -sum;  /* Should be 0 when correct */
}

/* BCPL string: first byte is length */
static void read_bcpl_string(const uint8_t *src, char *dst, size_t max_len)
{
    size_t len = src[0];
    if (len > max_len - 1) len = max_len - 1;
    memcpy(dst, src + 1, len);
    dst[len] = '\0';
}

/* Hash function for filenames */
static uint32_t hash_name(const char *name, bool intl)
{
    uint32_t hash = strlen(name);
    
    for (const char *p = name; *p; p++) {
        char c = *p;
        if (intl) {
            /* International mode: case insensitive + accents */
            if (c >= 'a' && c <= 'z') c -= 32;
            if (c >= 0xE0 && c <= 0xFE && c != 0xF7) c -= 32;
        } else {
            /* Standard: just ASCII case insensitive */
            if (c >= 'a' && c <= 'z') c -= 32;
        }
        hash = (hash * 13 + c) & 0x7FF;
    }
    
    return hash % HT_SIZE;
}

/*===========================================================================
 * Block Access
 *===========================================================================*/

static const uint8_t *get_block(const uft_amigados_ctx_t *ctx, uint32_t block)
{
    if (block >= ctx->total_blocks) return NULL;
    return ctx->data + block * AMIGA_BLOCK_SIZE;
}

/*===========================================================================
 * Filesystem Open/Close
 *===========================================================================*/

int uft_amigados_open(uft_amigados_ctx_t *ctx, const uint8_t *data, size_t size)
{
    if (!ctx || !data) return -1;
    
    memset(ctx, 0, sizeof(*ctx));
    ctx->data = data;
    ctx->size = size;
    
    /* Determine block count */
    if (size == AMIGA_DD_BLOCKS * AMIGA_BLOCK_SIZE) {
        ctx->total_blocks = AMIGA_DD_BLOCKS;
    } else if (size == AMIGA_HD_BLOCKS * AMIGA_BLOCK_SIZE) {
        ctx->total_blocks = AMIGA_HD_BLOCKS;
    } else {
        ctx->total_blocks = size / AMIGA_BLOCK_SIZE;
    }
    
    /* Root block is at middle of disk */
    ctx->root_block = ctx->total_blocks / 2;
    
    /* Read bootblock to determine filesystem type */
    const uint8_t *bb = get_block(ctx, 0);
    if (!bb) return -1;
    
    if (memcmp(bb, "DOS", 3) == 0) {
        ctx->fs_flags = bb[3];
        ctx->is_ffs = (ctx->fs_flags & FS_FFS) != 0;
        ctx->is_intl = (ctx->fs_flags & FS_INTL) != 0;
        ctx->is_dircache = (ctx->fs_flags & FS_DCACHE) != 0;
    } else {
        /* Not a DOS disk - might be NDOS or bootblock only */
        return -1;
    }
    
    /* Verify root block */
    const uint8_t *root = get_block(ctx, ctx->root_block);
    if (!root) return -1;
    
    uint32_t type = read_be32(root);
    int32_t sec_type = read_be32s(root + 508);
    
    if (type != T_HEADER || sec_type != ST_ROOT) {
        return -1;
    }
    
    /* Read root block info */
    read_bcpl_string(root + 432, ctx->volume_name, sizeof(ctx->volume_name));
    
    /* Bitmap blocks */
    ctx->bitmap_flag = read_be32(root + 312);
    for (int i = 0; i < 25; i++) {
        ctx->bitmap_blocks[i] = read_be32(root + 316 + i * 4);
    }
    
    /* Dates */
    ctx->days = read_be32(root + 420);
    ctx->mins = read_be32(root + 424);
    ctx->ticks = read_be32(root + 428);
    
    ctx->is_valid = true;
    return 0;
}

void uft_amigados_close(uft_amigados_ctx_t *ctx)
{
    if (ctx) {
        memset(ctx, 0, sizeof(*ctx));
    }
}

/*===========================================================================
 * Directory Parsing
 *===========================================================================*/

static int parse_entry(const uft_amigados_ctx_t *ctx, uint32_t block,
                       uft_amigados_entry_t *entry)
{
    const uint8_t *blk = get_block(ctx, block);
    if (!blk) return -1;
    
    memset(entry, 0, sizeof(*entry));
    entry->header_block = block;
    
    uint32_t type = read_be32(blk);
    int32_t sec_type = read_be32s(blk + 508);
    
    if (type != T_HEADER) return -1;
    
    entry->type = sec_type;
    entry->header_key = read_be32(blk + 4);
    entry->parent = read_be32(blk + 500);
    entry->hash_chain = read_be32(blk + 496);
    
    /* Name at offset 432 (BCPL string) */
    read_bcpl_string(blk + 432, entry->name, sizeof(entry->name));
    
    /* Protection bits */
    entry->protection = read_be32(blk + 484);
    
    /* Date */
    entry->days = read_be32(blk + 420);
    entry->mins = read_be32(blk + 424);
    entry->ticks = read_be32(blk + 428);
    
    if (sec_type == ST_FILE) {
        entry->is_file = true;
        entry->size = read_be32(blk + 324);
        entry->first_data = read_be32(blk + 16);
        
        /* Data blocks in OFS are at 24-308 (72 pointers) */
        /* Extension block at 8 */
        entry->extension = read_be32(blk + 8);
    } else if (sec_type == ST_USERDIR || sec_type == ST_ROOT) {
        entry->is_dir = true;
    } else if (sec_type == ST_SOFTLINK) {
        entry->is_link = true;
    }
    
    return 0;
}

int uft_amigados_read_dir(const uft_amigados_ctx_t *ctx, uint32_t dir_block,
                          uft_amigados_entry_t *entries, size_t max_entries,
                          size_t *count)
{
    if (!ctx || !entries || !count || !ctx->is_valid) return -1;
    
    *count = 0;
    
    const uint8_t *dir = get_block(ctx, dir_block);
    if (!dir) return -1;
    
    /* Hash table at offset 24-312 (72 entries) */
    for (int h = 0; h < HT_SIZE && *count < max_entries; h++) {
        uint32_t block = read_be32(dir + 24 + h * 4);
        
        /* Follow hash chain */
        while (block != 0 && *count < max_entries) {
            uft_amigados_entry_t *entry = &entries[*count];
            
            if (parse_entry(ctx, block, entry) == 0) {
                (*count)++;
                block = entry->hash_chain;
            } else {
                break;
            }
        }
    }
    
    return 0;
}

int uft_amigados_list_root(const uft_amigados_ctx_t *ctx,
                           uft_amigados_entry_t *entries, size_t max_entries,
                           size_t *count)
{
    return uft_amigados_read_dir(ctx, ctx->root_block, entries, max_entries, count);
}

/*===========================================================================
 * File Reading
 *===========================================================================*/

static int read_ofs_data(const uft_amigados_ctx_t *ctx, uint32_t block,
                         uint8_t *buffer, size_t offset, size_t size)
{
    /* OFS: data blocks have header */
    const uint8_t *blk = get_block(ctx, block);
    if (!blk) return -1;
    
    uint32_t type = read_be32(blk);
    if (type != T_DATA) return -1;
    
    uint32_t data_size = read_be32(blk + 12);
    if (data_size > 488) data_size = 488;
    
    /* Data at offset 24 */
    size_t copy = (size < data_size - offset) ? size : (data_size - offset);
    memcpy(buffer, blk + 24 + offset, copy);
    
    return copy;
}

static int read_ffs_data(const uft_amigados_ctx_t *ctx, uint32_t block,
                         uint8_t *buffer, size_t offset, size_t size)
{
    /* FFS: pure data blocks */
    const uint8_t *blk = get_block(ctx, block);
    if (!blk) return -1;
    
    size_t copy = (size < AMIGA_BLOCK_SIZE - offset) ? size : (AMIGA_BLOCK_SIZE - offset);
    memcpy(buffer, blk + offset, copy);
    
    return copy;
}

int uft_amigados_read_file(const uft_amigados_ctx_t *ctx,
                           const uft_amigados_entry_t *entry,
                           uint8_t *buffer, size_t max_size, size_t *bytes_read)
{
    if (!ctx || !entry || !buffer || !bytes_read || !ctx->is_valid) return -1;
    if (!entry->is_file) return -1;
    
    *bytes_read = 0;
    
    size_t file_size = entry->size;
    if (file_size > max_size) file_size = max_size;
    
    /* Read header block */
    const uint8_t *hdr = get_block(ctx, entry->header_block);
    if (!hdr) return -1;
    
    size_t block_data_size = ctx->is_ffs ? AMIGA_BLOCK_SIZE : 488;
    uint32_t current_ext = entry->header_block;
    
    while (*bytes_read < file_size && current_ext != 0) {
        const uint8_t *ext = get_block(ctx, current_ext);
        if (!ext) break;
        
        /* Data block pointers at 24-308 (72 entries), stored backwards */
        int num_ptrs = (current_ext == entry->header_block) ? 72 : 72;
        
        for (int i = num_ptrs - 1; i >= 0 && *bytes_read < file_size; i--) {
            uint32_t data_block = read_be32(ext + 24 + i * 4);
            if (data_block == 0) continue;
            
            size_t remain = file_size - *bytes_read;
            size_t to_read = (remain < block_data_size) ? remain : block_data_size;
            
            int read;
            if (ctx->is_ffs) {
                read = read_ffs_data(ctx, data_block, buffer + *bytes_read, 0, to_read);
            } else {
                read = read_ofs_data(ctx, data_block, buffer + *bytes_read, 0, to_read);
            }
            
            if (read > 0) {
                *bytes_read += read;
            } else {
                break;
            }
        }
        
        /* Next extension block */
        current_ext = read_be32(ext + 8);
    }
    
    return 0;
}

/*===========================================================================
 * Path Navigation
 *===========================================================================*/

int uft_amigados_find_entry(const uft_amigados_ctx_t *ctx, const char *path,
                            uft_amigados_entry_t *entry)
{
    if (!ctx || !path || !entry || !ctx->is_valid) return -1;
    
    /* Start at root */
    uint32_t current_dir = ctx->root_block;
    
    /* Copy path for tokenizing */
    char path_copy[256];
    strncpy(path_copy, path, sizeof(path_copy) - 1);
    path_copy[sizeof(path_copy) - 1] = '\0';
    
    /* Skip leading slashes */
    char *p = path_copy;
    while (*p == '/' || *p == ':') p++;
    
    if (*p == '\0') {
        /* Root directory */
        return parse_entry(ctx, ctx->root_block, entry);
    }
    
    /* Parse path components */
    char *token = strtok(p, "/");
    
    while (token != NULL) {
        const uint8_t *dir = get_block(ctx, current_dir);
        if (!dir) return -1;
        
        /* Calculate hash */
        uint32_t hash = hash_name(token, ctx->is_intl);
        uint32_t block = read_be32(dir + 24 + hash * 4);
        
        bool found = false;
        
        while (block != 0) {
            uft_amigados_entry_t temp;
            if (parse_entry(ctx, block, &temp) != 0) break;
            
            /* Case-insensitive compare */
            if (strcasecmp(temp.name, token) == 0) {
                *entry = temp;
                found = true;
                
                if (temp.is_dir) {
                    current_dir = block;
                }
                break;
            }
            
            block = temp.hash_chain;
        }
        
        if (!found) return -1;
        
        token = strtok(NULL, "/");
    }
    
    return 0;
}

/*===========================================================================
 * Bitmap Operations
 *===========================================================================*/

int uft_amigados_count_free_blocks(const uft_amigados_ctx_t *ctx, uint32_t *free_blocks)
{
    if (!ctx || !free_blocks || !ctx->is_valid) return -1;
    
    *free_blocks = 0;
    
    /* Read bitmap blocks */
    for (int i = 0; i < 25 && ctx->bitmap_blocks[i] != 0; i++) {
        const uint8_t *bm = get_block(ctx, ctx->bitmap_blocks[i]);
        if (!bm) continue;
        
        /* Bitmap data at offset 4-508 */
        for (int j = 4; j < 508; j++) {
            uint8_t byte = bm[j];
            /* Count set bits (1 = free) */
            for (int b = 0; b < 8; b++) {
                if (byte & (1 << b)) {
                    (*free_blocks)++;
                }
            }
        }
    }
    
    return 0;
}

/*===========================================================================
 * Report
 *===========================================================================*/

int uft_amigados_report_json(const uft_amigados_ctx_t *ctx, char *buffer, size_t size)
{
    if (!ctx || !buffer || size == 0) return -1;
    
    uint32_t free_blocks = 0;
    uft_amigados_count_free_blocks(ctx, &free_blocks);
    
    int written = snprintf(buffer, size,
        "{\n"
        "  \"filesystem\": \"%s\",\n"
        "  \"valid\": %s,\n"
        "  \"volume_name\": \"%s\",\n"
        "  \"total_blocks\": %u,\n"
        "  \"free_blocks\": %u,\n"
        "  \"root_block\": %u,\n"
        "  \"international\": %s,\n"
        "  \"dircache\": %s,\n"
        "  \"file_size\": %zu\n"
        "}",
        ctx->is_ffs ? "FFS" : "OFS",
        ctx->is_valid ? "true" : "false",
        ctx->volume_name,
        ctx->total_blocks,
        free_blocks,
        ctx->root_block,
        ctx->is_intl ? "true" : "false",
        ctx->is_dircache ? "true" : "false",
        ctx->size
    );
    
    return (written > 0 && (size_t)written < size) ? 0 : -1;
}
