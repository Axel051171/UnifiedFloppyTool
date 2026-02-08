/**
 * @file uft_amigados_file.c
 * @brief AmigaDOS File Operations
 * 
 * File extraction, injection, deletion, renaming, block chains.
 */

#include "uft/fs/uft_amigados.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/*===========================================================================
 * Internal Helpers
 *===========================================================================*/

static inline uint32_t read_be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static inline int32_t read_be32s(const uint8_t *p)
{
    return (int32_t)read_be32(p);
}

static inline void write_be32(uint8_t *p, uint32_t v)
{
    p[0] = (v >> 24) & 0xFF;
    p[1] = (v >> 16) & 0xFF;
    p[2] = (v >> 8) & 0xFF;
    p[3] = v & 0xFF;
}

static const uint8_t *get_block_ptr(const uft_amiga_ctx_t *ctx, uint32_t block_num)
{
    if (!ctx || !ctx->data || block_num >= ctx->total_blocks) return NULL;
    return ctx->data + (size_t)block_num * UFT_AMIGA_BLOCK_SIZE;
}

static uint8_t *get_block_ptr_rw(uft_amiga_ctx_t *ctx, uint32_t block_num)
{
    if (!ctx || !ctx->data || block_num >= ctx->total_blocks) return NULL;
    return ctx->data + (size_t)block_num * UFT_AMIGA_BLOCK_SIZE;
}

static void write_bcpl_string(uint8_t *dst, const char *src, size_t max_len)
{
    size_t len = strlen(src);
    if (len > max_len - 1) len = max_len - 1;
    dst[0] = (uint8_t)len;
    memcpy(dst + 1, src, len);
    for (size_t i = len + 1; i < max_len; i++) {
        dst[i] = 0;
    }
}

/*===========================================================================
 * Block Chain Operations
 *===========================================================================*/

static int ensure_chain_capacity(uft_amiga_chain_t *chain, size_t needed)
{
    if (chain->capacity >= needed) return 0;
    
    size_t new_cap = chain->capacity ? chain->capacity * 2 : 128;
    while (new_cap < needed) new_cap *= 2;
    
    uint32_t *new_blocks = realloc(chain->blocks, new_cap * sizeof(uint32_t));
    if (!new_blocks) return -1;
    
    chain->blocks = new_blocks;
    chain->capacity = new_cap;
    return 0;
}

int uft_amiga_get_chain(const uft_amiga_ctx_t *ctx, uint32_t file_block,
                        uft_amiga_chain_t *chain)
{
    if (!ctx || !ctx->is_valid || !chain) return -1;
    
    memset(chain, 0, sizeof(*chain));
    chain->header_block = file_block;
    
    const uint8_t *header = get_block_ptr(ctx, file_block);
    if (!header) return -1;
    
    /* Verify it's a file header */
    uint32_t type = read_be32(header);
    int32_t sec_type = read_be32s(header + 508);
    
    if (type != UFT_AMIGA_T_SHORT || sec_type != UFT_AMIGA_ST_FILE) {
        return -1;
    }
    
    /* Get file size */
    chain->total_size = read_be32(header + 324);
    
    /* High_seq: number of data blocks (in FFS) */
    uint32_t high_seq = read_be32(header + 8);
    
    if (ctx->is_ffs) {
        /* FFS: Data blocks directly contain data (no header)
         * Data block pointers in header block at offset 308 going backwards
         * Max 72 data block pointers per header block */
        
        uint32_t ext_block = file_block;
        bool first = true;
        int iterations = 0;
        
        while (ext_block != 0 && iterations++ < 10000) {
            const uint8_t *blk = get_block_ptr(ctx, ext_block);
            if (!blk) break;
            
            /* Data blocks at offset 308-24 (72 entries, stored backwards) */
            /* First valid index depends on high_seq for file header,
             * or stored count for extension blocks */
            uint32_t count;
            if (first) {
                count = high_seq;
                if (count > UFT_AMIGA_MAX_DATA_BLOCKS) count = UFT_AMIGA_MAX_DATA_BLOCKS;
            } else {
                /* Extension block: high_seq at offset 8 */
                count = read_be32(blk + 8);
                if (count > UFT_AMIGA_MAX_EXT_BLOCKS) count = UFT_AMIGA_MAX_EXT_BLOCKS;
            }
            
            /* Data block pointers stored backwards from offset 308 */
            for (uint32_t i = 0; i < count && i < 72; i++) {
                uint32_t data_block = read_be32(blk + 308 - i * 4);
                if (data_block == 0 || data_block >= ctx->total_blocks) continue;
                
                if (ensure_chain_capacity(chain, chain->count + 1) != 0) {
                    uft_amiga_free_chain(chain);
                    return -1;
                }
                chain->blocks[chain->count++] = data_block;
            }
            
            /* Extension block pointer at offset 492 */
            ext_block = read_be32(blk + 492);
            chain->has_extension = (ext_block != 0);
            first = false;
        }
    } else {
        /* OFS: Data blocks have header (24 bytes overhead)
         * Follow linked list of data blocks */
        uint32_t data_block = read_be32(header + 16);  /* first_data */
        int iterations = 0;
        
        while (data_block != 0 && data_block < ctx->total_blocks &&
               iterations++ < 100000) {
            const uint8_t *blk = get_block_ptr(ctx, data_block);
            if (!blk) break;
            
            /* Verify it's a data block */
            if (read_be32(blk) != UFT_AMIGA_T_DATA) break;
            
            if (ensure_chain_capacity(chain, chain->count + 1) != 0) {
                uft_amiga_free_chain(chain);
                return -1;
            }
            chain->blocks[chain->count++] = data_block;
            
            /* Next data block at offset 16 */
            data_block = read_be32(blk + 16);
        }
    }
    
    return 0;
}

void uft_amiga_free_chain(uft_amiga_chain_t *chain)
{
    if (!chain) return;
    if (chain->blocks) {
        free(chain->blocks);
    }
    memset(chain, 0, sizeof(*chain));
}

int uft_amiga_check_chain(const uft_amiga_ctx_t *ctx, uint32_t header_block)
{
    uft_amiga_chain_t chain;
    int ret = uft_amiga_get_chain(ctx, header_block, &chain);
    if (ret != 0) return ret;
    
    /* Verify all blocks are valid */
    for (size_t i = 0; i < chain.count; i++) {
        if (chain.blocks[i] >= ctx->total_blocks) {
            uft_amiga_free_chain(&chain);
            return -1;
        }
    }
    
    uft_amiga_free_chain(&chain);
    return 0;
}

/*===========================================================================
 * File Extraction
 *===========================================================================*/

int uft_amiga_extract_file(const uft_amiga_ctx_t *ctx, const char *path,
                           uint8_t *data, size_t *size)
{
    if (!ctx || !ctx->is_valid || !path || !data || !size) return -1;
    
    /* Find file */
    uft_amiga_entry_t entry;
    if (uft_amiga_find_path(ctx, path, &entry) != 0) {
        return -1;
    }
    
    if (!entry.is_file) {
        return -1;  /* Not a file */
    }
    
    if (*size < entry.size) {
        *size = entry.size;  /* Tell caller needed size */
        return -2;  /* Buffer too small */
    }
    
    /* Get block chain */
    uft_amiga_chain_t chain;
    if (uft_amiga_get_chain(ctx, entry.header_block, &chain) != 0) {
        return -1;
    }
    
    /* Extract data */
    size_t offset = 0;
    size_t remaining = entry.size;
    
    if (ctx->is_ffs) {
        /* FFS: Full 512 bytes of data per block */
        for (size_t i = 0; i < chain.count && remaining > 0; i++) {
            const uint8_t *blk = get_block_ptr(ctx, chain.blocks[i]);
            if (!blk) {
                uft_amiga_free_chain(&chain);
                return -1;
            }
            
            size_t bytes = (remaining > UFT_AMIGA_BLOCK_SIZE) ?
                           UFT_AMIGA_BLOCK_SIZE : remaining;
            memcpy(data + offset, blk, bytes);
            offset += bytes;
            remaining -= bytes;
        }
    } else {
        /* OFS: 488 bytes of data per block (24 byte header) */
        for (size_t i = 0; i < chain.count && remaining > 0; i++) {
            const uint8_t *blk = get_block_ptr(ctx, chain.blocks[i]);
            if (!blk) {
                uft_amiga_free_chain(&chain);
                return -1;
            }
            
            /* Data size in this block at offset 12 */
            uint32_t data_size = read_be32(blk + 12);
            if (data_size > 488) data_size = 488;
            if (data_size > remaining) data_size = (uint32_t)remaining;
            
            /* Data at offset 24 */
            memcpy(data + offset, blk + 24, data_size);
            offset += data_size;
            remaining -= data_size;
        }
    }
    
    *size = entry.size;
    uft_amiga_free_chain(&chain);
    return 0;
}

int uft_amiga_extract_file_alloc(const uft_amiga_ctx_t *ctx, const char *path,
                                 uint8_t **data, size_t *size)
{
    if (!ctx || !ctx->is_valid || !path || !data || !size) return -1;
    
    /* Find file first */
    uft_amiga_entry_t entry;
    if (uft_amiga_find_path(ctx, path, &entry) != 0) {
        return -1;
    }
    
    if (!entry.is_file) {
        return -1;
    }
    
    /* Allocate buffer */
    *data = malloc(entry.size > 0 ? entry.size : 1);
    if (!*data) return -1;
    
    *size = entry.size;
    
    int ret = uft_amiga_extract_file(ctx, path, *data, size);
    if (ret != 0) {
        free(*data);
        *data = NULL;
        return ret;
    }
    
    return 0;
}

int uft_amiga_extract_to_file(const uft_amiga_ctx_t *ctx, const char *path,
                              const char *dest_path)
{
    if (!ctx || !path || !dest_path) return -1;
    
    uint8_t *data;
    size_t size;
    
    if (uft_amiga_extract_file_alloc(ctx, path, &data, &size) != 0) {
        return -1;
    }
    
    FILE *f = fopen(dest_path, "wb");
    if (!f) {
        free(data);
        return -1;
    }
    
    size_t written = fwrite(data, 1, size, f);
    fclose(f);
    free(data);
    
    return (written == size) ? 0 : -1;
}

/*===========================================================================
 * File Injection
 *===========================================================================*/

int uft_amiga_inject_file(uft_amiga_ctx_t *ctx, const char *dest_dir,
                          const char *name, const uint8_t *data, size_t size)
{
    if (!ctx || !ctx->is_valid || !name || (!data && size > 0)) return -1;
    
    /* Validate filename length */
    size_t name_len = strlen(name);
    size_t max_name = ctx->is_longnames ? UFT_AMIGA_MAX_FILENAME_LFS :
                                           UFT_AMIGA_MAX_FILENAME;
    if (name_len > max_name || name_len == 0) {
        return -1;
    }
    
    /* Find destination directory */
    uint32_t dir_block;
    if (!dest_dir || dest_dir[0] == '\0' || strcmp(dest_dir, "/") == 0) {
        dir_block = ctx->root_block;
    } else {
        uft_amiga_entry_t dir_entry;
        if (uft_amiga_find_path(ctx, dest_dir, &dir_entry) != 0) {
            return -1;
        }
        if (!dir_entry.is_dir) {
            return -1;
        }
        dir_block = dir_entry.header_block;
    }
    
    /* Check if file already exists */
    uft_amiga_entry_t existing;
    if (uft_amiga_find_entry(ctx, dir_block, name, &existing) == 0) {
        return -2;  /* File exists */
    }
    
    /* Calculate blocks needed */
    size_t data_per_block = ctx->is_ffs ? UFT_AMIGA_BLOCK_SIZE : 488;
    size_t data_blocks_needed = (size + data_per_block - 1) / data_per_block;
    if (data_blocks_needed == 0 && size == 0) data_blocks_needed = 0;
    
    /* For FFS, we need extension blocks if > 72 data blocks */
    size_t ext_blocks_needed = 0;
    if (data_blocks_needed > UFT_AMIGA_MAX_DATA_BLOCKS) {
        ext_blocks_needed = (data_blocks_needed - UFT_AMIGA_MAX_DATA_BLOCKS +
                            UFT_AMIGA_MAX_EXT_BLOCKS - 1) / UFT_AMIGA_MAX_EXT_BLOCKS;
    }
    
    size_t total_blocks = 1 + data_blocks_needed + ext_blocks_needed;  /* header + data + ext */
    
    /* Allocate blocks */
    uint32_t *blocks = malloc(total_blocks * sizeof(uint32_t));
    if (!blocks) return -1;
    
    size_t allocated = uft_amiga_alloc_blocks(ctx, total_blocks, blocks);
    if (allocated < total_blocks) {
        /* Not enough space - free what we allocated */
        for (size_t i = 0; i < allocated; i++) {
            uft_amiga_free_block(ctx, blocks[i]);
        }
        free(blocks);
        return -3;  /* Disk full */
    }
    
    uint32_t header_block = blocks[0];
    
    /* Initialize file header block */
    uint8_t *header = get_block_ptr_rw(ctx, header_block);
    memset(header, 0, UFT_AMIGA_BLOCK_SIZE);
    
    write_be32(header + 0, UFT_AMIGA_T_SHORT);  /* Type */
    write_be32(header + 4, header_block);       /* Own key */
    
    uint32_t blocks_in_header = data_blocks_needed;
    if (blocks_in_header > UFT_AMIGA_MAX_DATA_BLOCKS) {
        blocks_in_header = UFT_AMIGA_MAX_DATA_BLOCKS;
    }
    write_be32(header + 8, (uint32_t)blocks_in_header);  /* High_seq */
    
    /* Write data block pointers (backwards from offset 308) */
    for (uint32_t i = 0; i < blocks_in_header; i++) {
        write_be32(header + 308 - i * 4, blocks[1 + i]);  /* data blocks start at index 1 */
    }
    
    write_be32(header + 324, (uint32_t)size);  /* File size */
    
    /* Extension block if needed */
    if (ext_blocks_needed > 0) {
        write_be32(header + 492, blocks[1 + data_blocks_needed]);  /* First extension */
    }
    
    write_be32(header + 504, dir_block);  /* Parent */
    write_bcpl_string(header + 432, name, max_name + 1);  /* Name */
    
    /* Timestamp */
    uint32_t days, mins, ticks;
    uft_amiga_from_unix_time(time(NULL), &days, &mins, &ticks);
    write_be32(header + 420, days);
    write_be32(header + 424, mins);
    write_be32(header + 428, ticks);
    
    write_be32(header + 508, (uint32_t)UFT_AMIGA_ST_FILE);  /* Secondary type */
    
    /* Update checksum */
    uft_amiga_update_checksum(header);
    
    /* Write data blocks */
    size_t data_offset = 0;
    
    if (ctx->is_ffs) {
        /* FFS: Direct data in blocks */
        for (size_t i = 0; i < data_blocks_needed; i++) {
            uint8_t *blk = get_block_ptr_rw(ctx, blocks[1 + i]);
            size_t bytes = (size - data_offset > UFT_AMIGA_BLOCK_SIZE) ?
                           UFT_AMIGA_BLOCK_SIZE : (size - data_offset);
            
            memset(blk, 0, UFT_AMIGA_BLOCK_SIZE);
            if (bytes > 0) {
                memcpy(blk, data + data_offset, bytes);
            }
            data_offset += bytes;
        }
    } else {
        /* OFS: Data blocks with header */
        uint32_t seq = 1;
        for (size_t i = 0; i < data_blocks_needed; i++) {
            uint8_t *blk = get_block_ptr_rw(ctx, blocks[1 + i]);
            memset(blk, 0, UFT_AMIGA_BLOCK_SIZE);
            
            write_be32(blk + 0, UFT_AMIGA_T_DATA);  /* Type */
            write_be32(blk + 4, header_block);     /* Header key */
            write_be32(blk + 8, seq++);            /* Sequence number */
            
            size_t bytes = (size - data_offset > 488) ? 488 : (size - data_offset);
            write_be32(blk + 12, (uint32_t)bytes);  /* Data size */
            
            /* Next data block */
            if (i + 1 < data_blocks_needed) {
                write_be32(blk + 16, blocks[2 + i]);
            }
            
            /* Data at offset 24 */
            if (bytes > 0) {
                memcpy(blk + 24, data + data_offset, bytes);
            }
            data_offset += bytes;
            
            /* OFS data block checksum at offset 20 */
            uft_amiga_update_checksum(blk);
        }
    }
    
    /* Write extension blocks if needed (FFS only practical) */
    if (ext_blocks_needed > 0) {
        /*
         * Amiga Extension Block (T_LIST) Structure:
         * Offset 0:   Type = 16 (T_LIST)
         * Offset 4:   Own key (this block number)
         * Offset 8:   High_seq (number of data blocks in this extension)
         * Offset 16-308: Data block pointers (72 entries, backwards from 308)
         * Offset 492: Extension (next extension block, 0 if last)
         * Offset 496: Parent (file header block)
         * Offset 508: Secondary type (-3 for file)
         * Checksum recalculated at offset 20
         */
        
        size_t remaining_data_blocks = data_blocks_needed - UFT_AMIGA_MAX_DATA_BLOCKS;
        size_t data_block_index = 1 + UFT_AMIGA_MAX_DATA_BLOCKS;  /* Start after header's data blocks */
        
        for (size_t ext_idx = 0; ext_idx < ext_blocks_needed; ext_idx++) {
            uint32_t ext_block = blocks[1 + data_blocks_needed + ext_idx];
            uint8_t *ext = get_block_ptr_rw(ctx, ext_block);
            if (!ext) continue;
            
            memset(ext, 0, UFT_AMIGA_BLOCK_SIZE);
            
            /* How many data blocks in this extension */
            size_t blocks_in_ext = remaining_data_blocks;
            if (blocks_in_ext > UFT_AMIGA_MAX_EXT_BLOCKS) {
                blocks_in_ext = UFT_AMIGA_MAX_EXT_BLOCKS;
            }
            
            write_be32(ext + 0, UFT_AMIGA_T_LIST);   /* Type = extension list */
            write_be32(ext + 4, ext_block);          /* Own key */
            write_be32(ext + 8, (uint32_t)blocks_in_ext);  /* High_seq */
            
            /* Write data block pointers (backwards from offset 308) */
            for (size_t i = 0; i < blocks_in_ext; i++) {
                write_be32(ext + 308 - i * 4, blocks[data_block_index + i]);
            }
            
            /* Next extension block (or 0 if last) */
            if (ext_idx + 1 < ext_blocks_needed) {
                write_be32(ext + 492, blocks[1 + data_blocks_needed + ext_idx + 1]);
            } else {
                write_be32(ext + 492, 0);  /* Last extension */
            }
            
            write_be32(ext + 496, header_block);  /* Parent = file header */
            write_be32(ext + 508, (uint32_t)UFT_AMIGA_ST_FILE);  /* Secondary type */
            
            /* Update checksum */
            uft_amiga_update_checksum(ext);
            
            data_block_index += blocks_in_ext;
            remaining_data_blocks -= blocks_in_ext;
        }
    }
    
    /* Add to directory hash table */
    uint8_t *dir = get_block_ptr_rw(ctx, dir_block);
    uint32_t hash = uft_amiga_hash_name(name, ctx->is_intl);
    uint32_t old_chain = read_be32(dir + 24 + hash * 4);
    
    /* Insert at head of hash chain */
    write_be32(dir + 24 + hash * 4, header_block);
    write_be32(header + 496, old_chain);  /* Old chain becomes our next */
    
    /* Re-update header checksum after chain update */
    uft_amiga_update_checksum(header);
    
    /* Update directory checksum */
    uft_amiga_update_checksum(dir);
    
    free(blocks);
    ctx->modified = true;
    return 0;
}

int uft_amiga_inject_from_file(uft_amiga_ctx_t *ctx, const char *dest_dir,
                               const char *src_path)
{
    if (!ctx || !src_path) return -1;
    
    FILE *f = fopen(src_path, "rb");
    if (!f) return -1;
    
    if (fseek(f, 0, SEEK_END) != 0) { /* seek error */ }
    long fsize = ftell(f);
    if (fseek(f, 0, SEEK_SET) != 0) { /* seek error */ }
    if (fsize < 0 || fsize > 100 * 1024 * 1024) {
        fclose(f);
        return -1;
    }
    
    uint8_t *data = malloc((size_t)fsize > 0 ? (size_t)fsize : 1);
    if (!data) {
        fclose(f);
        return -1;
    }
    
    if (fsize > 0) {
        size_t n = fread(data, 1, (size_t)fsize, f);
        if (n != (size_t)fsize) {
            free(data);
            fclose(f);
            return -1;
        }
    }
    fclose(f);
    
    /* Extract filename from path */
    const char *name = strrchr(src_path, '/');
    if (!name) name = strrchr(src_path, '\\');
    name = name ? name + 1 : src_path;
    
    int ret = uft_amiga_inject_file(ctx, dest_dir, name, data, (size_t)fsize);
    free(data);
    return ret;
}

/*===========================================================================
 * File Deletion
 *===========================================================================*/

int uft_amiga_delete(uft_amiga_ctx_t *ctx, const char *path)
{
    if (!ctx || !ctx->is_valid || !path) return -1;
    
    /* Find the entry */
    uft_amiga_entry_t entry;
    if (uft_amiga_find_path(ctx, path, &entry) != 0) {
        return -1;
    }
    
    /* Check protection bits */
    if (entry.protection & UFT_AMIGA_PROT_DELETE) {
        return -2;  /* Delete-protected */
    }
    
    /* If directory, must be empty */
    if (entry.is_dir) {
        uft_amiga_dir_t dir;
        if (uft_amiga_load_dir(ctx, entry.header_block, &dir) != 0) {
            return -1;
        }
        bool empty = (dir.count == 0);
        uft_amiga_free_dir(&dir);
        if (!empty) {
            return -3;  /* Directory not empty */
        }
    }
    
    /* Remove from parent's hash chain */
    uint8_t *parent = get_block_ptr_rw(ctx, entry.parent_block);
    if (!parent) return -1;
    
    uint32_t hash = uft_amiga_hash_name(entry.name, ctx->is_intl);
    uint32_t prev_block = 0;
    uint32_t curr_block = read_be32(parent + 24 + hash * 4);
    
    while (curr_block != 0 && curr_block != entry.header_block) {
        const uint8_t *curr = get_block_ptr(ctx, curr_block);
        if (!curr) break;
        prev_block = curr_block;
        curr_block = read_be32(curr + 496);  /* Hash chain */
    }
    
    if (curr_block == entry.header_block) {
        if (prev_block == 0) {
            /* First in chain */
            write_be32(parent + 24 + hash * 4, entry.hash_chain);
        } else {
            /* Middle/end of chain */
            uint8_t *prev = get_block_ptr_rw(ctx, prev_block);
            write_be32(prev + 496, entry.hash_chain);
            uft_amiga_update_checksum(prev);
        }
        uft_amiga_update_checksum(parent);
    }
    
    /* Free data blocks */
    if (entry.is_file) {
        uft_amiga_chain_t chain;
        if (uft_amiga_get_chain(ctx, entry.header_block, &chain) == 0) {
            for (size_t i = 0; i < chain.count; i++) {
                uft_amiga_free_block(ctx, chain.blocks[i]);
            }
            uft_amiga_free_chain(&chain);
        }
    }
    
    /* Free header block */
    uft_amiga_free_block(ctx, entry.header_block);
    
    ctx->modified = true;
    return 0;
}

/*===========================================================================
 * File Renaming
 *===========================================================================*/

int uft_amiga_rename(uft_amiga_ctx_t *ctx, const char *old_path,
                     const char *new_name)
{
    if (!ctx || !ctx->is_valid || !old_path || !new_name) return -1;
    
    /* Validate new name */
    size_t name_len = strlen(new_name);
    size_t max_name = ctx->is_longnames ? UFT_AMIGA_MAX_FILENAME_LFS :
                                           UFT_AMIGA_MAX_FILENAME;
    if (name_len > max_name || name_len == 0) {
        return -1;
    }
    
    /* Find entry */
    uft_amiga_entry_t entry;
    if (uft_amiga_find_path(ctx, old_path, &entry) != 0) {
        return -1;
    }
    
    /* Check if new name exists in same directory */
    uft_amiga_entry_t existing;
    if (uft_amiga_find_entry(ctx, entry.parent_block, new_name, &existing) == 0) {
        return -2;  /* Name exists */
    }
    
    /* Remove from old hash chain */
    uint8_t *parent = get_block_ptr_rw(ctx, entry.parent_block);
    uint32_t old_hash = uft_amiga_hash_name(entry.name, ctx->is_intl);
    uint32_t prev_block = 0;
    uint32_t curr_block = read_be32(parent + 24 + old_hash * 4);
    
    while (curr_block != 0 && curr_block != entry.header_block) {
        const uint8_t *curr = get_block_ptr(ctx, curr_block);
        if (!curr) break;
        prev_block = curr_block;
        curr_block = read_be32(curr + 496);
    }
    
    if (curr_block == entry.header_block) {
        if (prev_block == 0) {
            write_be32(parent + 24 + old_hash * 4, entry.hash_chain);
        } else {
            uint8_t *prev = get_block_ptr_rw(ctx, prev_block);
            write_be32(prev + 496, entry.hash_chain);
            uft_amiga_update_checksum(prev);
        }
    }
    
    /* Update name in header block */
    uint8_t *header = get_block_ptr_rw(ctx, entry.header_block);
    write_bcpl_string(header + 432, new_name, max_name + 1);
    
    /* Add to new hash chain */
    uint32_t new_hash = uft_amiga_hash_name(new_name, ctx->is_intl);
    uint32_t old_chain = read_be32(parent + 24 + new_hash * 4);
    write_be32(parent + 24 + new_hash * 4, entry.header_block);
    write_be32(header + 496, old_chain);
    
    uft_amiga_update_checksum(header);
    uft_amiga_update_checksum(parent);
    
    ctx->modified = true;
    return 0;
}

/*===========================================================================
 * Directory Creation
 *===========================================================================*/

int uft_amiga_mkdir(uft_amiga_ctx_t *ctx, const char *parent_dir,
                    const char *name)
{
    if (!ctx || !ctx->is_valid || !name) return -1;
    
    /* Validate name */
    size_t name_len = strlen(name);
    size_t max_name = ctx->is_longnames ? UFT_AMIGA_MAX_FILENAME_LFS :
                                           UFT_AMIGA_MAX_FILENAME;
    if (name_len > max_name || name_len == 0) {
        return -1;
    }
    
    /* Find parent directory */
    uint32_t parent_block;
    if (!parent_dir || parent_dir[0] == '\0' || strcmp(parent_dir, "/") == 0) {
        parent_block = ctx->root_block;
    } else {
        uft_amiga_entry_t parent_entry;
        if (uft_amiga_find_path(ctx, parent_dir, &parent_entry) != 0) {
            return -1;
        }
        if (!parent_entry.is_dir) {
            return -1;
        }
        parent_block = parent_entry.header_block;
    }
    
    /* Check if name exists */
    uft_amiga_entry_t existing;
    if (uft_amiga_find_entry(ctx, parent_block, name, &existing) == 0) {
        return -2;  /* Exists */
    }
    
    /* Allocate block for directory header */
    uint32_t dir_block = uft_amiga_alloc_block(ctx, 0);
    if (dir_block == 0) {
        return -3;  /* Disk full */
    }
    
    /* Initialize directory header */
    uint8_t *header = get_block_ptr_rw(ctx, dir_block);
    memset(header, 0, UFT_AMIGA_BLOCK_SIZE);
    
    write_be32(header + 0, UFT_AMIGA_T_SHORT);  /* Type */
    write_be32(header + 4, dir_block);          /* Own key */
    /* Hash table at 24-308 is all zeros (empty) */
    
    write_be32(header + 504, parent_block);  /* Parent */
    write_bcpl_string(header + 432, name, max_name + 1);
    
    uint32_t days, mins, ticks;
    uft_amiga_from_unix_time(time(NULL), &days, &mins, &ticks);
    write_be32(header + 420, days);
    write_be32(header + 424, mins);
    write_be32(header + 428, ticks);
    
    write_be32(header + 508, UFT_AMIGA_ST_USERDIR);  /* Secondary type */
    
    uft_amiga_update_checksum(header);
    
    /* Add to parent hash table */
    uint8_t *parent = get_block_ptr_rw(ctx, parent_block);
    uint32_t hash = uft_amiga_hash_name(name, ctx->is_intl);
    uint32_t old_chain = read_be32(parent + 24 + hash * 4);
    write_be32(parent + 24 + hash * 4, dir_block);
    write_be32(header + 496, old_chain);
    
    uft_amiga_update_checksum(header);
    uft_amiga_update_checksum(parent);
    
    ctx->modified = true;
    return 0;
}

/*===========================================================================
 * Protection and Comment
 *===========================================================================*/

int uft_amiga_set_protection(uft_amiga_ctx_t *ctx, const char *path,
                             uint32_t protection)
{
    if (!ctx || !ctx->is_valid || !path) return -1;
    
    uft_amiga_entry_t entry;
    if (uft_amiga_find_path(ctx, path, &entry) != 0) {
        return -1;
    }
    
    uint8_t *header = get_block_ptr_rw(ctx, entry.header_block);
    write_be32(header + 500, protection);
    uft_amiga_update_checksum(header);
    
    ctx->modified = true;
    return 0;
}

int uft_amiga_set_comment(uft_amiga_ctx_t *ctx, const char *path,
                          const char *comment)
{
    if (!ctx || !ctx->is_valid || !path) return -1;
    
    uft_amiga_entry_t entry;
    if (uft_amiga_find_path(ctx, path, &entry) != 0) {
        return -1;
    }
    
    uint8_t *header = get_block_ptr_rw(ctx, entry.header_block);
    
    if (comment) {
        write_bcpl_string(header + 396, comment, UFT_AMIGA_MAX_COMMENT + 1);
    } else {
        header[396] = 0;  /* Empty BCPL string */
    }
    
    uft_amiga_update_checksum(header);
    ctx->modified = true;
    return 0;
}
