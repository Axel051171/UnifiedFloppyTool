/**
 * @file uft_apple_file.c
 * @brief Apple II File Operations
 * @version 3.6.0
 * 
 * Unified file operations for DOS 3.3 and ProDOS.
 */

#include "uft/fs/uft_apple_dos.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/*===========================================================================
 * Forward Declarations (from other modules)
 *===========================================================================*/

/* DOS 3.3 functions */
extern int uft_dos33_find_entry(const uft_apple_ctx_t *ctx, const char *name,
                                uft_apple_entry_t *entry,
                                uint8_t *cat_track, uint8_t *cat_sector, int *cat_index);
extern int uft_dos33_read_file_data(const uft_apple_ctx_t *ctx, 
                                    uint8_t ts_track, uint8_t ts_sector,
                                    uint8_t **data_out, size_t *size_out);
extern int uft_dos33_create_ts_list(uft_apple_ctx_t *ctx, const uint8_t *data, size_t size,
                                    uint8_t *ts_track, uint8_t *ts_sector,
                                    uint16_t *sector_count);
extern int uft_dos33_free_file_sectors(uft_apple_ctx_t *ctx, 
                                       uint8_t ts_track, uint8_t ts_sector);
extern int uft_dos33_add_catalog_entry(uft_apple_ctx_t *ctx, const char *name,
                                       uint8_t file_type,
                                       uint8_t ts_track, uint8_t ts_sector,
                                       uint16_t sector_count);
extern int uft_dos33_delete_catalog_entry(uft_apple_ctx_t *ctx,
                                          uint8_t cat_track, uint8_t cat_sector,
                                          int cat_index);

/* ProDOS functions */
extern int uft_prodos_resolve_path(const uft_apple_ctx_t *ctx, const char *path,
                                   uint16_t *dir_block, char *filename);
extern int uft_prodos_find_entry(const uft_apple_ctx_t *ctx, uint16_t dir_block,
                                 const char *name, uft_apple_entry_t *entry,
                                 uint16_t *entry_block, int *entry_index);
extern int uft_prodos_read_file(const uft_apple_ctx_t *ctx, 
                                const uft_apple_entry_t *entry,
                                uint8_t **data_out, size_t *size_out);
extern int uft_prodos_write_file(uft_apple_ctx_t *ctx, const uint8_t *data, size_t size,
                                 uint16_t *key_block, uint8_t *storage_type,
                                 uint16_t *blocks_used);
extern int uft_prodos_create_entry(uft_apple_ctx_t *ctx, uint16_t dir_block,
                                   const char *name, uint8_t file_type, uint16_t aux_type,
                                   uint16_t key_block, uint8_t storage_type,
                                   uint16_t blocks_used, uint32_t eof);
extern int uft_prodos_read_dir(const uft_apple_ctx_t *ctx, uint16_t key_block,
                               uft_apple_dir_t *dir);

/*===========================================================================
 * File Extraction
 *===========================================================================*/

uint8_t *uft_apple_extract(const uft_apple_ctx_t *ctx, const char *path,
                           uint8_t *buffer, size_t *size) {
    if (!ctx || !path || !size) return NULL;
    
    *size = 0;
    
    if (ctx->fs_type == UFT_APPLE_FS_DOS33 || ctx->fs_type == UFT_APPLE_FS_DOS32) {
        /* DOS 3.3 extraction */
        uft_apple_entry_t entry;
        int ret = uft_dos33_find_entry(ctx, path, &entry, NULL, NULL, NULL);
        if (ret < 0) return NULL;
        
        uint8_t *data = NULL;
        size_t data_size = 0;
        
        ret = uft_dos33_read_file_data(ctx, entry.ts_track, entry.ts_sector,
                                       &data, &data_size);
        if (ret < 0) return NULL;
        
        /* For binary files, first 4 bytes are address and length */
        size_t actual_size = data_size;
        size_t start_offset = 0;
        
        if (entry.file_type == UFT_DOS33_TYPE_BINARY && data_size >= 4) {
            uint16_t file_len = data[2] | (data[3] << 8);
            if (file_len > 0 && file_len <= data_size - 4) {
                actual_size = file_len;
                start_offset = 4;
            }
        }
        
        if (buffer) {
            if (*size >= actual_size) {
                memcpy(buffer, data + start_offset, actual_size);
                free(data);
                *size = actual_size;
                return buffer;
            }
            free(data);
            return NULL;
        }
        
        /* Caller wants us to allocate */
        if (start_offset > 0) {
            memmove(data, data + start_offset, actual_size);
        }
        
        *size = actual_size;
        return data;
        
    } else if (ctx->fs_type == UFT_APPLE_FS_PRODOS) {
        /* ProDOS extraction */
        uint16_t dir_block;
        char filename[16];
        
        int ret = uft_prodos_resolve_path(ctx, path, &dir_block, filename);
        if (ret < 0) return NULL;
        
        uft_apple_entry_t entry;
        ret = uft_prodos_find_entry(ctx, dir_block, filename, &entry, NULL, NULL);
        if (ret < 0) return NULL;
        
        uint8_t *data = NULL;
        size_t data_size = 0;
        
        ret = uft_prodos_read_file(ctx, &entry, &data, &data_size);
        if (ret < 0) return NULL;
        
        if (buffer) {
            if (*size >= data_size) {
                memcpy(buffer, data, data_size);
                free(data);
                *size = data_size;
                return buffer;
            }
            free(data);
            return NULL;
        }
        
        *size = data_size;
        return data;
    }
    
    return NULL;
}

int uft_apple_extract_to_file(const uft_apple_ctx_t *ctx, const char *path,
                              const char *dest_path) {
    if (!ctx || !path || !dest_path) return UFT_APPLE_ERR_INVALID;
    
    size_t size = 0;
    uint8_t *data = uft_apple_extract(ctx, path, NULL, &size);
    if (!data && size > 0) return UFT_APPLE_ERR_NOMEM;
    
    FILE *fp = fopen(dest_path, "wb");
    if (!fp) {
        free(data);
        return UFT_APPLE_ERR_IO;
    }
    
    if (size > 0) {
        if (fwrite(data, 1, size, fp) != size) {
            fclose(fp);
            free(data);
            return UFT_APPLE_ERR_IO;
        }
    }
    
    fclose(fp);
    free(data);
    return 0;
}

/*===========================================================================
 * File Injection
 *===========================================================================*/

int uft_apple_inject(uft_apple_ctx_t *ctx, const char *path, uint8_t file_type,
                     uint16_t aux_type, const uint8_t *data, size_t size) {
    if (!ctx || !path) return UFT_APPLE_ERR_INVALID;
    
    if (ctx->fs_type == UFT_APPLE_FS_DOS33 || ctx->fs_type == UFT_APPLE_FS_DOS32) {
        /* DOS 3.3 injection */
        
        /* For binary files, prepend address and length */
        uint8_t *file_data = NULL;
        size_t file_size = size;
        
        if (file_type == UFT_DOS33_TYPE_BINARY) {
            file_size = size + 4;
            file_data = malloc(file_size);
            if (!file_data) return UFT_APPLE_ERR_NOMEM;
            
            /* Default load address 0x2000, or use aux_type */
            uint16_t load_addr = aux_type ? aux_type : 0x2000;
            file_data[0] = load_addr & 0xFF;
            file_data[1] = (load_addr >> 8) & 0xFF;
            file_data[2] = size & 0xFF;
            file_data[3] = (size >> 8) & 0xFF;
            
            if (data && size > 0) {
                memcpy(file_data + 4, data, size);
            }
        } else if (data && size > 0) {
            file_data = malloc(size);
            if (!file_data) return UFT_APPLE_ERR_NOMEM;
            memcpy(file_data, data, size);
        }
        
        /* Create T/S list */
        uint8_t ts_track, ts_sector;
        uint16_t sector_count;
        
        int ret = uft_dos33_create_ts_list(ctx, file_data, file_size,
                                           &ts_track, &ts_sector, &sector_count);
        free(file_data);
        if (ret < 0) return ret;
        
        /* Add catalog entry */
        ret = uft_dos33_add_catalog_entry(ctx, path, file_type,
                                          ts_track, ts_sector, sector_count);
        if (ret < 0) {
            uft_dos33_free_file_sectors(ctx, ts_track, ts_sector);
            return ret;
        }
        
        return 0;
        
    } else if (ctx->fs_type == UFT_APPLE_FS_PRODOS) {
        /* ProDOS injection */
        uint16_t dir_block;
        char filename[16];
        
        int ret = uft_prodos_resolve_path(ctx, path, &dir_block, filename);
        if (ret < 0) return ret;
        
        /* Write file data */
        uint16_t key_block;
        uint8_t storage_type;
        uint16_t blocks_used;
        
        ret = uft_prodos_write_file(ctx, data, size, &key_block, 
                                    &storage_type, &blocks_used);
        if (ret < 0) return ret;
        
        /* Create directory entry */
        ret = uft_prodos_create_entry(ctx, dir_block, filename, file_type, aux_type,
                                      key_block, storage_type, blocks_used, size);
        if (ret < 0) {
            /* Free allocated blocks: key block and any index/data blocks.
             * ProDOS bitmap is stored at ctx->bitmap_block.
             * Each bit represents one block; 0=used, 1=free. */
            if (key_block > 0 && key_block < ctx->total_blocks && ctx->data) {
                size_t bm_offset = (size_t)ctx->bitmap_block * 512;
                if (bm_offset + (ctx->total_blocks + 7) / 8 <= ctx->data_size) {
                    uint8_t *bitmap = ctx->data + bm_offset;
                    
                    /* Mark key block as free */
                    bitmap[key_block / 8] |= (0x80 >> (key_block % 8));
                    
                    /* For sapling/tree: read index block and free data blocks */
                    if (storage_type >= 0x20) {
                        const uint8_t *idx = ctx->data + (size_t)key_block * 512;
                        for (int i = 0; i < 256; i++) {
                            uint16_t dblk = idx[i] | ((uint16_t)idx[i + 256] << 8);
                            if (dblk > 0 && dblk < ctx->total_blocks) {
                                bitmap[dblk / 8] |= (0x80 >> (dblk % 8));
                            }
                        }
                    }
                    ctx->modified = true;
                }
            }
            return ret;
        }
        
        return 0;
    }
    
    return UFT_APPLE_ERR_BADTYPE;
}

/*===========================================================================
 * File Deletion
 *===========================================================================*/

/**
 * @brief Free ProDOS file blocks based on storage type
 */
static int free_prodos_file_blocks(uft_apple_ctx_t *ctx, const uft_apple_entry_t *entry) {
    uint8_t block[512];
    
    switch (entry->storage_type) {
    case UFT_PRODOS_STORAGE_SEEDLING:
        return uft_apple_free_block(ctx, entry->key_block);
        
    case UFT_PRODOS_STORAGE_SAPLING:
        /* Free index block and data blocks */
        if (uft_apple_read_block(ctx, entry->key_block, block) < 0) {
            return UFT_APPLE_ERR_IO;
        }
        for (int i = 0; i < 256; i++) {
            uint16_t data_block = block[i] | (block[i + 256] << 8);
            if (data_block != 0) {
                uft_apple_free_block(ctx, data_block);
            }
        }
        return uft_apple_free_block(ctx, entry->key_block);
        
    case UFT_PRODOS_STORAGE_TREE:
        /* Free master index, index blocks, and data blocks */
        if (uft_apple_read_block(ctx, entry->key_block, block) < 0) {
            return UFT_APPLE_ERR_IO;
        }
        for (int m = 0; m < 256; m++) {
            uint16_t index_block = block[m] | (block[m + 256] << 8);
            if (index_block == 0) continue;
            
            uint8_t index[512];
            if (uft_apple_read_block(ctx, index_block, index) < 0) continue;
            
            for (int i = 0; i < 256; i++) {
                uint16_t data_block = index[i] | (index[i + 256] << 8);
                if (data_block != 0) {
                    uft_apple_free_block(ctx, data_block);
                }
            }
            uft_apple_free_block(ctx, index_block);
        }
        return uft_apple_free_block(ctx, entry->key_block);
        
    default:
        return UFT_APPLE_ERR_BADTYPE;
    }
}

int uft_apple_delete(uft_apple_ctx_t *ctx, const char *path) {
    if (!ctx || !path) return UFT_APPLE_ERR_INVALID;
    
    if (ctx->fs_type == UFT_APPLE_FS_DOS33 || ctx->fs_type == UFT_APPLE_FS_DOS32) {
        /* DOS 3.3 deletion */
        uft_apple_entry_t entry;
        uint8_t cat_track, cat_sector;
        int cat_index;
        
        int ret = uft_dos33_find_entry(ctx, path, &entry, 
                                       &cat_track, &cat_sector, &cat_index);
        if (ret < 0) return ret;
        
        /* Check if locked */
        if (entry.is_locked) {
            return UFT_APPLE_ERR_READONLY;
        }
        
        /* Free file sectors */
        ret = uft_dos33_free_file_sectors(ctx, entry.ts_track, entry.ts_sector);
        if (ret < 0) return ret;
        
        /* Delete catalog entry */
        return uft_dos33_delete_catalog_entry(ctx, cat_track, cat_sector, cat_index);
        
    } else if (ctx->fs_type == UFT_APPLE_FS_PRODOS) {
        /* ProDOS deletion */
        uint16_t dir_block;
        char filename[16];
        
        int ret = uft_prodos_resolve_path(ctx, path, &dir_block, filename);
        if (ret < 0) return ret;
        
        uft_apple_entry_t entry;
        uint16_t entry_block;
        int entry_index;
        
        ret = uft_prodos_find_entry(ctx, dir_block, filename, &entry,
                                    &entry_block, &entry_index);
        if (ret < 0) return ret;
        
        /* Check if locked */
        if (entry.is_locked) {
            return UFT_APPLE_ERR_READONLY;
        }
        
        /* Free file blocks */
        ret = free_prodos_file_blocks(ctx, &entry);
        if (ret < 0) return ret;
        
        /* Delete directory entry */
        uint8_t block_data[512];
        ret = uft_apple_read_block(ctx, entry_block, block_data);
        if (ret < 0) return ret;
        
        /* Clear entry */
        uint8_t *entry_data = &block_data[4 + entry_index * 39];
        memset(entry_data, 0, 39);
        
        ret = uft_apple_write_block(ctx, entry_block, block_data);
        if (ret < 0) return ret;
        
        /* Update file count in header */
        ret = uft_apple_read_block(ctx, dir_block, block_data);
        if (ret < 0) return ret;
        
        uint16_t file_count = block_data[0x25] | (block_data[0x26] << 8);
        if (file_count > 0) file_count--;
        block_data[0x25] = file_count & 0xFF;
        block_data[0x26] = (file_count >> 8) & 0xFF;
        
        return uft_apple_write_block(ctx, dir_block, block_data);
    }
    
    return UFT_APPLE_ERR_BADTYPE;
}

/*===========================================================================
 * File Rename
 *===========================================================================*/

int uft_apple_rename(uft_apple_ctx_t *ctx, const char *old_path, const char *new_path) {
    if (!ctx || !old_path || !new_path) return UFT_APPLE_ERR_INVALID;
    
    if (ctx->fs_type == UFT_APPLE_FS_DOS33 || ctx->fs_type == UFT_APPLE_FS_DOS32) {
        /* DOS 3.3 rename */
        uint8_t cat_track, cat_sector;
        int cat_index;
        
        int ret = uft_dos33_find_entry(ctx, old_path, NULL,
                                       &cat_track, &cat_sector, &cat_index);
        if (ret < 0) return ret;
        
        /* Check if new name exists */
        ret = uft_dos33_find_entry(ctx, new_path, NULL, NULL, NULL, NULL);
        if (ret == 0) return UFT_APPLE_ERR_EXISTS;
        
        /* Update entry */
        uint8_t sector_data[UFT_APPLE_SECTOR_SIZE];
        ret = uft_apple_read_sector(ctx, cat_track, cat_sector, sector_data);
        if (ret < 0) return ret;
        
        /* Entry offset: 0x0B + index * 35 */
        uint8_t *entry = &sector_data[0x0B + cat_index * 35];
        
        /* Encode new filename (with high bit set) */
        memset(&entry[3], 0xA0, 30);  /* Space with high bit */
        int len = strlen(new_path);
        if (len > 30) len = 30;
        for (int i = 0; i < len; i++) {
            entry[3 + i] = toupper(new_path[i]) | 0x80;
        }
        
        return uft_apple_write_sector(ctx, cat_track, cat_sector, sector_data);
        
    } else if (ctx->fs_type == UFT_APPLE_FS_PRODOS) {
        /* ProDOS rename */
        uint16_t dir_block;
        char old_filename[16], new_filename[16];
        
        int ret = uft_prodos_resolve_path(ctx, old_path, &dir_block, old_filename);
        if (ret < 0) return ret;
        
        /* For simplicity, require same directory */
        uint16_t new_dir_block;
        ret = uft_prodos_resolve_path(ctx, new_path, &new_dir_block, new_filename);
        if (ret < 0) return ret;
        
        if (dir_block != new_dir_block) {
            /* Cross-directory rename not implemented */
            return UFT_APPLE_ERR_INVALID;
        }
        
        /* Check new name doesn't exist */
        ret = uft_prodos_find_entry(ctx, dir_block, new_filename, NULL, NULL, NULL);
        if (ret == 0) return UFT_APPLE_ERR_EXISTS;
        
        /* Find old entry */
        uint16_t entry_block;
        int entry_index;
        ret = uft_prodos_find_entry(ctx, dir_block, old_filename, NULL,
                                    &entry_block, &entry_index);
        if (ret < 0) return ret;
        
        /* Update entry */
        uint8_t block_data[512];
        ret = uft_apple_read_block(ctx, entry_block, block_data);
        if (ret < 0) return ret;
        
        uint8_t *entry_data = &block_data[4 + entry_index * 39];
        
        /* Update name */
        int new_len = strlen(new_filename);
        if (new_len > 15) new_len = 15;
        
        uint8_t storage = (entry_data[0] >> 4) & 0x0F;
        entry_data[0] = (storage << 4) | new_len;
        
        memset(&entry_data[1], 0, 15);
        for (int i = 0; i < new_len; i++) {
            entry_data[1 + i] = toupper(new_filename[i]);
        }
        
        return uft_apple_write_block(ctx, entry_block, block_data);
    }
    
    return UFT_APPLE_ERR_BADTYPE;
}

/*===========================================================================
 * File Lock/Unlock
 *===========================================================================*/

int uft_apple_set_locked(uft_apple_ctx_t *ctx, const char *path, bool locked) {
    if (!ctx || !path) return UFT_APPLE_ERR_INVALID;
    
    if (ctx->fs_type == UFT_APPLE_FS_DOS33 || ctx->fs_type == UFT_APPLE_FS_DOS32) {
        /* DOS 3.3: Lock bit is high bit of file type */
        uint8_t cat_track, cat_sector;
        int cat_index;
        
        int ret = uft_dos33_find_entry(ctx, path, NULL,
                                       &cat_track, &cat_sector, &cat_index);
        if (ret < 0) return ret;
        
        uint8_t sector_data[UFT_APPLE_SECTOR_SIZE];
        ret = uft_apple_read_sector(ctx, cat_track, cat_sector, sector_data);
        if (ret < 0) return ret;
        
        uint8_t *entry = &sector_data[0x0B + cat_index * 35];
        
        if (locked) {
            entry[2] |= 0x80;
        } else {
            entry[2] &= 0x7F;
        }
        
        return uft_apple_write_sector(ctx, cat_track, cat_sector, sector_data);
        
    } else if (ctx->fs_type == UFT_APPLE_FS_PRODOS) {
        /* ProDOS: Access byte bit 1 = write enabled */
        uint16_t dir_block;
        char filename[16];
        
        int ret = uft_prodos_resolve_path(ctx, path, &dir_block, filename);
        if (ret < 0) return ret;
        
        uint16_t entry_block;
        int entry_index;
        ret = uft_prodos_find_entry(ctx, dir_block, filename, NULL,
                                    &entry_block, &entry_index);
        if (ret < 0) return ret;
        
        uint8_t block_data[512];
        ret = uft_apple_read_block(ctx, entry_block, block_data);
        if (ret < 0) return ret;
        
        uint8_t *entry_data = &block_data[4 + entry_index * 39];
        
        if (locked) {
            entry_data[0x1E] &= ~0x02;  /* Clear write enable */
        } else {
            entry_data[0x1E] |= 0x02;   /* Set write enable */
        }
        
        return uft_apple_write_block(ctx, entry_block, block_data);
    }
    
    return UFT_APPLE_ERR_BADTYPE;
}

/*===========================================================================
 * Directory Creation (ProDOS only)
 *===========================================================================*/

int uft_apple_mkdir(uft_apple_ctx_t *ctx, const char *path) {
    if (!ctx || !path) return UFT_APPLE_ERR_INVALID;
    
    if (ctx->fs_type != UFT_APPLE_FS_PRODOS) {
        return UFT_APPLE_ERR_BADTYPE;  /* DOS 3.3 has no subdirectories */
    }
    
    uint16_t parent_block;
    char dirname[16];
    
    int ret = uft_prodos_resolve_path(ctx, path, &parent_block, dirname);
    if (ret < 0) return ret;
    
    /* Check if name exists */
    ret = uft_prodos_find_entry(ctx, parent_block, dirname, NULL, NULL, NULL);
    if (ret == 0) return UFT_APPLE_ERR_EXISTS;
    
    /* Allocate block for subdirectory */
    uint16_t dir_key;
    ret = uft_apple_alloc_block(ctx, &dir_key);
    if (ret < 0) return ret;
    
    /* Initialize directory block */
    uint8_t dir_data[512] = {0};
    
    /* Previous/Next pointers = 0 for single-block dir */
    dir_data[0] = 0;
    dir_data[1] = 0;
    dir_data[2] = 0;
    dir_data[3] = 0;
    
    /* Directory header (entry 0) */
    int name_len = strlen(dirname);
    if (name_len > 15) name_len = 15;
    
    dir_data[4] = (UFT_PRODOS_STORAGE_SUBDIR << 4) | name_len;
    for (int i = 0; i < name_len; i++) {
        dir_data[5 + i] = toupper(dirname[i]);
    }
    
    /* Reserved at 0x14-0x1B */
    
    /* Creation date/time */
    uft_prodos_datetime_t now = uft_prodos_from_unix_time(time(NULL));
    dir_data[0x1C] = now.date & 0xFF;
    dir_data[0x1D] = (now.date >> 8) & 0xFF;
    dir_data[0x1E] = now.time & 0xFF;
    dir_data[0x1F] = (now.time >> 8) & 0xFF;
    
    /* Version */
    dir_data[0x20] = 0;
    dir_data[0x21] = 0;
    
    /* Access */
    dir_data[0x22] = 0xC3;
    
    /* Entry length = 39 */
    dir_data[0x23] = 0x27;
    
    /* Entries per block = 13 */
    dir_data[0x24] = 0x0D;
    
    /* File count = 0 */
    dir_data[0x25] = 0;
    dir_data[0x26] = 0;
    
    /* Parent pointer */
    dir_data[0x27] = parent_block & 0xFF;
    dir_data[0x28] = (parent_block >> 8) & 0xFF;
    
    /* Parent entry number (filled later) */
    dir_data[0x29] = 0;
    
    /* Parent entry length */
    dir_data[0x2A] = 0x27;
    
    ret = uft_apple_write_block(ctx, dir_key, dir_data);
    if (ret < 0) {
        uft_apple_free_block(ctx, dir_key);
        return ret;
    }
    
    /* Create entry in parent directory */
    ret = uft_prodos_create_entry(ctx, parent_block, dirname, UFT_PRODOS_TYPE_DIR, 0,
                                  dir_key, UFT_PRODOS_STORAGE_SUBDIR, 1, 512);
    if (ret < 0) {
        uft_apple_free_block(ctx, dir_key);
        return ret;
    }
    
    return 0;
}

/*===========================================================================
 * JSON Export
 *===========================================================================*/

size_t uft_apple_to_json(const uft_apple_ctx_t *ctx, char *buffer, size_t size) {
    if (!ctx || !buffer || size == 0) return 0;
    
    char *p = buffer;
    char *end = buffer + size;
    
    #define APPEND(...) do { \
        int n = snprintf(p, end - p, __VA_ARGS__); \
        if (n > 0 && p + n < end) p += n; \
    } while(0)
    
    APPEND("{\n");
    
    /* Filesystem info */
    const char *fs_name = "unknown";
    if (ctx->fs_type == UFT_APPLE_FS_DOS33) fs_name = "DOS 3.3";
    else if (ctx->fs_type == UFT_APPLE_FS_DOS32) fs_name = "DOS 3.2";
    else if (ctx->fs_type == UFT_APPLE_FS_PRODOS) fs_name = "ProDOS";
    else if (ctx->fs_type == UFT_APPLE_FS_PASCAL) fs_name = "Pascal";
    
    APPEND("  \"filesystem\": \"%s\",\n", fs_name);
    APPEND("  \"tracks\": %d,\n", ctx->tracks);
    APPEND("  \"sectors_per_track\": %d,\n", ctx->sectors_per_track);
    
    if (ctx->fs_type == UFT_APPLE_FS_DOS33 || ctx->fs_type == UFT_APPLE_FS_DOS32) {
        APPEND("  \"volume_number\": %d,\n", ctx->vtoc.volume_number);
    } else if (ctx->fs_type == UFT_APPLE_FS_PRODOS) {
        APPEND("  \"volume_name\": \"%s\",\n", ctx->volume_name);
        APPEND("  \"total_blocks\": %d,\n", ctx->total_blocks);
    }
    
    /* Free space */
    uint16_t free_count = 0;
    if (ctx->fs_type == UFT_APPLE_FS_DOS33 || ctx->fs_type == UFT_APPLE_FS_DOS32) {
        uft_apple_get_free(ctx, &free_count);
        APPEND("  \"free_sectors\": %d,\n", free_count);
    }
    
    /* File listing */
    APPEND("  \"files\": [\n");
    
    uft_apple_dir_t dir;
    uft_apple_dir_init(&dir);
    
    if (ctx->fs_type == UFT_APPLE_FS_DOS33 || ctx->fs_type == UFT_APPLE_FS_DOS32) {
        uft_apple_read_dir(ctx, NULL, &dir);
    } else if (ctx->fs_type == UFT_APPLE_FS_PRODOS) {
        uft_prodos_read_dir(ctx, UFT_PRODOS_KEY_BLOCK, &dir);
    }
    
    for (size_t i = 0; i < dir.count; i++) {
        const uft_apple_entry_t *e = &dir.entries[i];
        
        APPEND("    {\n");
        APPEND("      \"name\": \"%s\",\n", e->name);
        
        if (ctx->fs_type == UFT_APPLE_FS_DOS33 || ctx->fs_type == UFT_APPLE_FS_DOS32) {
            APPEND("      \"type\": \"%c\",\n", uft_dos33_type_char(e->file_type));
            APPEND("      \"sectors\": %d,\n", e->sector_count);
        } else {
            APPEND("      \"type\": \"%s\",\n", uft_prodos_type_string(e->file_type));
            APPEND("      \"blocks\": %d,\n", e->blocks_used);
            APPEND("      \"size\": %u,\n", (unsigned)e->size);
        }
        
        APPEND("      \"locked\": %s\n", e->is_locked ? "true" : "false");
        APPEND("    }%s\n", (i < dir.count - 1) ? "," : "");
    }
    
    uft_apple_dir_free(&dir);
    
    APPEND("  ]\n");
    APPEND("}\n");
    
    #undef APPEND
    
    return p - buffer;
}
