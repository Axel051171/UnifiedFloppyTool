/**
 * @file uft_apple_prodos.c
 * @brief ProDOS Filesystem Implementation
 * @version 3.6.0
 * 
 * Volume directory, subdirectories, block bitmap, file storage types.
 */

#include "uft/fs/uft_apple_dos.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/*===========================================================================
 * ProDOS Constants
 *===========================================================================*/

#define PRODOS_BLOCK_SIZE     512
#define PRODOS_ENTRY_SIZE     39
#define PRODOS_ENTRIES_PER_BLOCK  13
#define PRODOS_INDEX_ENTRIES  256

/*===========================================================================
 * Block Bitmap Operations
 *===========================================================================*/

/**
 * @brief Check if block is free in bitmap
 */
static bool prodos_is_block_free(const uft_apple_ctx_t *ctx, uint16_t block) {
    if (!ctx || !ctx->data) return false;
    
    /* Bitmap starts at bitmap_block */
    uint16_t bitmap_start = ctx->bitmap_block;
    uint16_t byte_offset = block / 8;
    uint8_t bit = 7 - (block & 7);
    
    /* Calculate which bitmap block and offset */
    uint16_t bitmap_block = bitmap_start + (byte_offset / PRODOS_BLOCK_SIZE);
    uint16_t block_offset = byte_offset % PRODOS_BLOCK_SIZE;
    
    /* Read bitmap block (direct access to avoid recursion) */
    size_t offset = bitmap_block * PRODOS_BLOCK_SIZE;
    if (offset + PRODOS_BLOCK_SIZE > ctx->size) return false;
    
    /* Bit = 1 means free */
    return (ctx->data[offset + block_offset] >> bit) & 1;
}

/**
 * @brief Mark block as used/free in bitmap
 */
static void prodos_set_block_status(uft_apple_ctx_t *ctx, uint16_t block, bool free) {
    if (!ctx || !ctx->data) return;
    
    uint16_t bitmap_start = ctx->bitmap_block;
    uint16_t byte_offset = block / 8;
    uint8_t bit = 7 - (block & 7);
    
    uint16_t bitmap_block = bitmap_start + (byte_offset / PRODOS_BLOCK_SIZE);
    uint16_t block_offset = byte_offset % PRODOS_BLOCK_SIZE;
    
    size_t offset = bitmap_block * PRODOS_BLOCK_SIZE;
    if (offset + PRODOS_BLOCK_SIZE > ctx->size) return;
    
    if (free) {
        ctx->data[offset + block_offset] |= (1 << bit);
    } else {
        ctx->data[offset + block_offset] &= ~(1 << bit);
    }
    
    ctx->is_modified = true;
}

/*===========================================================================
 * Block Allocation
 *===========================================================================*/

int uft_apple_alloc_block(uft_apple_ctx_t *ctx, uint16_t *block) {
    if (!ctx || !block) return UFT_APPLE_ERR_INVALID;
    
    if (ctx->fs_type != UFT_APPLE_FS_PRODOS) {
        return UFT_APPLE_ERR_BADTYPE;
    }
    
    /* Search for free block, starting after boot/bitmap area */
    for (uint16_t b = ctx->bitmap_block + 1; b < ctx->total_blocks; b++) {
        if (prodos_is_block_free(ctx, b)) {
            prodos_set_block_status(ctx, b, false);
            *block = b;
            return 0;
        }
    }
    
    return UFT_APPLE_ERR_DISKFULL;
}

int uft_apple_free_block(uft_apple_ctx_t *ctx, uint16_t block) {
    if (!ctx) return UFT_APPLE_ERR_INVALID;
    
    if (ctx->fs_type != UFT_APPLE_FS_PRODOS) {
        return UFT_APPLE_ERR_BADTYPE;
    }
    
    prodos_set_block_status(ctx, block, true);
    return 0;
}

/**
 * @brief Count free blocks (ProDOS version)
 */
int uft_prodos_get_free(const uft_apple_ctx_t *ctx, uint16_t *free_count) {
    if (!ctx || !free_count) return UFT_APPLE_ERR_INVALID;
    
    *free_count = 0;
    
    for (uint16_t b = 0; b < ctx->total_blocks; b++) {
        if (prodos_is_block_free(ctx, b)) {
            (*free_count)++;
        }
    }
    
    return 0;
}

/*===========================================================================
 * Directory Parsing
 *===========================================================================*/

/**
 * @brief Parse ProDOS directory entry
 */
static void parse_prodos_entry(const uint8_t *entry_data, uft_apple_entry_t *entry) {
    memset(entry, 0, sizeof(*entry));
    
    /* Storage type and name length */
    uint8_t storage_len = entry_data[0];
    uint8_t storage = (storage_len >> 4) & 0x0F;
    uint8_t name_len = storage_len & 0x0F;
    
    if (storage == 0 || name_len == 0) {
        entry->is_deleted = true;
        return;
    }
    
    /* Filename */
    memcpy(entry->name, &entry_data[1], name_len);
    entry->name[name_len] = '\0';
    
    /* File type */
    entry->file_type = entry_data[0x10];
    
    /* Key block */
    entry->key_block = entry_data[0x11] | (entry_data[0x12] << 8);
    
    /* Blocks used */
    entry->blocks_used = entry_data[0x13] | (entry_data[0x14] << 8);
    
    /* EOF (24-bit) */
    entry->size = entry_data[0x15] | 
                  (entry_data[0x16] << 8) | 
                  ((uint32_t)entry_data[0x17] << 16);
    
    /* Creation date/time */
    uft_prodos_datetime_t created;
    created.date = entry_data[0x18] | (entry_data[0x19] << 8);
    created.time = entry_data[0x1A] | (entry_data[0x1B] << 8);
    entry->created = uft_prodos_to_unix_time(created);
    
    /* Version info at 0x1C-0x1D */
    
    /* Access bits */
    entry->access = entry_data[0x1E];
    entry->is_locked = (entry->access & 0x02) == 0;  /* Write-disabled */
    
    /* Aux type */
    entry->aux_type = entry_data[0x1F] | (entry_data[0x20] << 8);
    
    /* Modification date/time */
    uft_prodos_datetime_t modified;
    modified.date = entry_data[0x21] | (entry_data[0x22] << 8);
    modified.time = entry_data[0x23] | (entry_data[0x24] << 8);
    entry->modified = uft_prodos_to_unix_time(modified);
    
    /* Storage type specific */
    entry->storage_type = storage;
    entry->is_directory = (storage == UFT_PRODOS_STORAGE_SUBDIR);
}

/**
 * @brief Read ProDOS directory
 */
int uft_prodos_read_dir(const uft_apple_ctx_t *ctx, uint16_t key_block, 
                        uft_apple_dir_t *dir) {
    if (!ctx || !dir) return UFT_APPLE_ERR_INVALID;
    
    uft_apple_dir_init(dir);
    
    uint8_t block_data[PRODOS_BLOCK_SIZE];
    uint16_t current_block = key_block;
    int safety = 100;
    bool first_block = true;
    
    while (current_block != 0 && safety-- > 0) {
        int ret = uft_apple_read_block(ctx, current_block, block_data);
        if (ret < 0) return ret;
        
        /* Previous/Next block pointers */
        uint16_t next_block = block_data[2] | (block_data[3] << 8);
        
        /* First entry in first block is volume/directory header */
        int start_entry = first_block ? 1 : 0;
        first_block = false;
        
        /* Parse entries */
        for (int i = start_entry; i < PRODOS_ENTRIES_PER_BLOCK; i++) {
            const uint8_t *entry_data = &block_data[4 + i * PRODOS_ENTRY_SIZE];
            
            /* Skip empty entries */
            if (entry_data[0] == 0) continue;
            
            /* Skip deleted entries */
            uint8_t storage = (entry_data[0] >> 4) & 0x0F;
            if (storage == 0) continue;
            
            /* Skip header entries */
            if (storage >= 0x0E) continue;
            
            /* Expand directory if needed */
            if (dir->count >= dir->capacity) {
                size_t new_cap = dir->capacity ? dir->capacity * 2 : 16;
                uft_apple_entry_t *new_entries = realloc(dir->entries,
                                                         new_cap * sizeof(uft_apple_entry_t));
                if (!new_entries) return UFT_APPLE_ERR_NOMEM;
                dir->entries = new_entries;
                dir->capacity = new_cap;
            }
            
            parse_prodos_entry(entry_data, &dir->entries[dir->count++]);
        }
        
        current_block = next_block;
    }
    
    return 0;
}

/**
 * @brief Find entry in ProDOS directory
 */
int uft_prodos_find_entry(const uft_apple_ctx_t *ctx, uint16_t dir_block,
                          const char *name, uft_apple_entry_t *entry,
                          uint16_t *entry_block, int *entry_index) {
    if (!ctx || !name) return UFT_APPLE_ERR_INVALID;
    
    /* Convert search name to uppercase */
    char search_name[16];
    int len = 0;
    for (int i = 0; name[i] && len < 15; i++) {
        search_name[len++] = toupper(name[i]);
    }
    search_name[len] = '\0';
    
    uint8_t block_data[PRODOS_BLOCK_SIZE];
    uint16_t current_block = dir_block;
    int safety = 100;
    bool first_block = true;
    
    while (current_block != 0 && safety-- > 0) {
        int ret = uft_apple_read_block(ctx, current_block, block_data);
        if (ret < 0) return ret;
        
        uint16_t next_block = block_data[2] | (block_data[3] << 8);
        int start_entry = first_block ? 1 : 0;
        first_block = false;
        
        for (int i = start_entry; i < PRODOS_ENTRIES_PER_BLOCK; i++) {
            const uint8_t *entry_data = &block_data[4 + i * PRODOS_ENTRY_SIZE];
            
            uint8_t storage = (entry_data[0] >> 4) & 0x0F;
            uint8_t name_len = entry_data[0] & 0x0F;
            
            if (storage == 0 || name_len == 0) continue;
            if (storage >= 0x0E) continue;
            
            /* Compare name */
            if (name_len == len) {
                char entry_name[16];
                memcpy(entry_name, &entry_data[1], name_len);
                entry_name[name_len] = '\0';
                
                if (strcmp(entry_name, search_name) == 0) {
                    if (entry) {
                        parse_prodos_entry(entry_data, entry);
                    }
                    if (entry_block) *entry_block = current_block;
                    if (entry_index) *entry_index = i;
                    return 0;
                }
            }
        }
        
        current_block = next_block;
    }
    
    return UFT_APPLE_ERR_NOTFOUND;
}

/*===========================================================================
 * Path Resolution
 *===========================================================================*/

/**
 * @brief Resolve ProDOS path to directory block and filename
 */
int uft_prodos_resolve_path(const uft_apple_ctx_t *ctx, const char *path,
                            uint16_t *dir_block, char *filename) {
    if (!ctx || !path || !dir_block) return UFT_APPLE_ERR_INVALID;
    
    /* Start at root directory */
    *dir_block = UFT_PRODOS_KEY_BLOCK;
    
    /* Skip leading / or volume name */
    const char *p = path;
    if (*p == '/') p++;
    
    /* Skip volume name if present */
    const char *slash = strchr(p, '/');
    if (slash) {
        /* Check if first component is volume name */
        char vol_name[16];
        int len = slash - p;
        if (len > 15) len = 15;
        memcpy(vol_name, p, len);
        vol_name[len] = '\0';
        
        /* Convert to uppercase for comparison */
        for (int i = 0; vol_name[i]; i++) {
            vol_name[i] = toupper(vol_name[i]);
        }
        
        if (strcmp(vol_name, ctx->volume_name) == 0) {
            p = slash + 1;
        }
    }
    
    /* Parse path components */
    while (*p) {
        /* Skip slashes */
        while (*p == '/') p++;
        if (!*p) break;
        
        /* Find end of component */
        const char *end = strchr(p, '/');
        if (!end) {
            /* Last component = filename */
            if (filename) {
                int len = strlen(p);
                if (len > 15) len = 15;
                memcpy(filename, p, len);
                filename[len] = '\0';
            }
            break;
        }
        
        /* Intermediate directory */
        char dirname[16];
        int len = end - p;
        if (len > 15) len = 15;
        memcpy(dirname, p, len);
        dirname[len] = '\0';
        
        /* Find directory */
        uft_apple_entry_t entry;
        int ret = uft_prodos_find_entry(ctx, *dir_block, dirname, &entry, NULL, NULL);
        if (ret < 0) return ret;
        
        if (!entry.is_directory) {
            return UFT_APPLE_ERR_NOTFOUND;
        }
        
        *dir_block = entry.key_block;
        p = end + 1;
    }
    
    return 0;
}

/*===========================================================================
 * File Data Access
 *===========================================================================*/

/**
 * @brief Read seedling file (single block)
 */
static int read_seedling(const uft_apple_ctx_t *ctx, uint16_t key_block,
                         uint8_t *data, size_t size) {
    uint8_t block[PRODOS_BLOCK_SIZE];
    int ret = uft_apple_read_block(ctx, key_block, block);
    if (ret < 0) return ret;
    
    size_t to_copy = size < PRODOS_BLOCK_SIZE ? size : PRODOS_BLOCK_SIZE;
    memcpy(data, block, to_copy);
    
    return 0;
}

/**
 * @brief Read sapling file (index block + data blocks)
 */
static int read_sapling(const uft_apple_ctx_t *ctx, uint16_t key_block,
                        uint8_t *data, size_t size) {
    uint8_t index[PRODOS_BLOCK_SIZE];
    int ret = uft_apple_read_block(ctx, key_block, index);
    if (ret < 0) return ret;
    
    size_t offset = 0;
    
    for (int i = 0; i < 256 && offset < size; i++) {
        uint16_t data_block = index[i] | (index[i + 256] << 8);
        
        if (data_block == 0) {
            /* Sparse block - zero fill */
            size_t to_zero = size - offset;
            if (to_zero > PRODOS_BLOCK_SIZE) to_zero = PRODOS_BLOCK_SIZE;
            memset(data + offset, 0, to_zero);
        } else {
            uint8_t block[PRODOS_BLOCK_SIZE];
            ret = uft_apple_read_block(ctx, data_block, block);
            if (ret < 0) return ret;
            
            size_t to_copy = size - offset;
            if (to_copy > PRODOS_BLOCK_SIZE) to_copy = PRODOS_BLOCK_SIZE;
            memcpy(data + offset, block, to_copy);
        }
        
        offset += PRODOS_BLOCK_SIZE;
    }
    
    return 0;
}

/**
 * @brief Read tree file (master index + index blocks + data blocks)
 */
static int read_tree(const uft_apple_ctx_t *ctx, uint16_t key_block,
                     uint8_t *data, size_t size) {
    uint8_t master[PRODOS_BLOCK_SIZE];
    int ret = uft_apple_read_block(ctx, key_block, master);
    if (ret < 0) return ret;
    
    size_t offset = 0;
    
    for (int m = 0; m < 256 && offset < size; m++) {
        uint16_t index_block = master[m] | (master[m + 256] << 8);
        
        if (index_block == 0) {
            /* Sparse index - zero fill 128K */
            size_t to_zero = 256 * PRODOS_BLOCK_SIZE;
            if (offset + to_zero > size) to_zero = size - offset;
            memset(data + offset, 0, to_zero);
            offset += 256 * PRODOS_BLOCK_SIZE;
            continue;
        }
        
        uint8_t index[PRODOS_BLOCK_SIZE];
        ret = uft_apple_read_block(ctx, index_block, index);
        if (ret < 0) return ret;
        
        for (int i = 0; i < 256 && offset < size; i++) {
            uint16_t data_block = index[i] | (index[i + 256] << 8);
            
            if (data_block == 0) {
                size_t to_zero = size - offset;
                if (to_zero > PRODOS_BLOCK_SIZE) to_zero = PRODOS_BLOCK_SIZE;
                memset(data + offset, 0, to_zero);
            } else {
                uint8_t block[PRODOS_BLOCK_SIZE];
                ret = uft_apple_read_block(ctx, data_block, block);
                if (ret < 0) return ret;
                
                size_t to_copy = size - offset;
                if (to_copy > PRODOS_BLOCK_SIZE) to_copy = PRODOS_BLOCK_SIZE;
                memcpy(data + offset, block, to_copy);
            }
            
            offset += PRODOS_BLOCK_SIZE;
        }
    }
    
    return 0;
}

/**
 * @brief Read ProDOS file data
 */
int uft_prodos_read_file(const uft_apple_ctx_t *ctx, const uft_apple_entry_t *entry,
                         uint8_t **data_out, size_t *size_out) {
    if (!ctx || !entry || !data_out || !size_out) {
        return UFT_APPLE_ERR_INVALID;
    }
    
    *data_out = NULL;
    *size_out = 0;
    
    if (entry->size == 0) {
        return 0;
    }
    
    uint8_t *data = malloc(entry->size);
    if (!data) return UFT_APPLE_ERR_NOMEM;
    
    int ret;
    
    switch (entry->storage_type) {
    case UFT_PRODOS_STORAGE_SEEDLING:
        ret = read_seedling(ctx, entry->key_block, data, entry->size);
        break;
    case UFT_PRODOS_STORAGE_SAPLING:
        ret = read_sapling(ctx, entry->key_block, data, entry->size);
        break;
    case UFT_PRODOS_STORAGE_TREE:
        ret = read_tree(ctx, entry->key_block, data, entry->size);
        break;
    default:
        free(data);
        return UFT_APPLE_ERR_BADTYPE;
    }
    
    if (ret < 0) {
        free(data);
        return ret;
    }
    
    *data_out = data;
    *size_out = entry->size;
    
    return 0;
}

/*===========================================================================
 * File Writing
 *===========================================================================*/

/**
 * @brief Write seedling file
 */
static int write_seedling(uft_apple_ctx_t *ctx, const uint8_t *data, size_t size,
                          uint16_t *key_block, uint16_t *blocks_used) {
    int ret = uft_apple_alloc_block(ctx, key_block);
    if (ret < 0) return ret;
    
    uint8_t block[PRODOS_BLOCK_SIZE] = {0};
    if (size > PRODOS_BLOCK_SIZE) size = PRODOS_BLOCK_SIZE;
    memcpy(block, data, size);
    
    ret = uft_apple_write_block(ctx, *key_block, block);
    if (ret < 0) return ret;
    
    *blocks_used = 1;
    return 0;
}

/**
 * @brief Write sapling file
 */
static int write_sapling(uft_apple_ctx_t *ctx, const uint8_t *data, size_t size,
                         uint16_t *key_block, uint16_t *blocks_used) {
    /* Allocate index block */
    int ret = uft_apple_alloc_block(ctx, key_block);
    if (ret < 0) return ret;
    
    uint8_t index[PRODOS_BLOCK_SIZE] = {0};
    *blocks_used = 1;
    
    /* Allocate and write data blocks */
    size_t offset = 0;
    int block_num = 0;
    
    while (offset < size && block_num < 256) {
        uint16_t data_block;
        ret = uft_apple_alloc_block(ctx, &data_block);
        if (ret < 0) return ret;
        
        uint8_t block[PRODOS_BLOCK_SIZE] = {0};
        size_t to_copy = size - offset;
        if (to_copy > PRODOS_BLOCK_SIZE) to_copy = PRODOS_BLOCK_SIZE;
        memcpy(block, data + offset, to_copy);
        
        ret = uft_apple_write_block(ctx, data_block, block);
        if (ret < 0) return ret;
        
        /* Add to index */
        index[block_num] = data_block & 0xFF;
        index[block_num + 256] = (data_block >> 8) & 0xFF;
        
        (*blocks_used)++;
        offset += PRODOS_BLOCK_SIZE;
        block_num++;
    }
    
    /* Write index block */
    ret = uft_apple_write_block(ctx, *key_block, index);
    return ret;
}

/**
 * @brief Write tree file
 */
static int write_tree(uft_apple_ctx_t *ctx, const uint8_t *data, size_t size,
                      uint16_t *key_block, uint16_t *blocks_used) {
    /* Allocate master index */
    int ret = uft_apple_alloc_block(ctx, key_block);
    if (ret < 0) return ret;
    
    uint8_t master[PRODOS_BLOCK_SIZE] = {0};
    *blocks_used = 1;
    
    size_t offset = 0;
    int master_idx = 0;
    
    while (offset < size && master_idx < 256) {
        /* Allocate index block */
        uint16_t index_block;
        ret = uft_apple_alloc_block(ctx, &index_block);
        if (ret < 0) return ret;
        
        uint8_t index[PRODOS_BLOCK_SIZE] = {0};
        (*blocks_used)++;
        
        /* Fill index block */
        int idx = 0;
        while (offset < size && idx < 256) {
            uint16_t data_block;
            ret = uft_apple_alloc_block(ctx, &data_block);
            if (ret < 0) return ret;
            
            uint8_t block[PRODOS_BLOCK_SIZE] = {0};
            size_t to_copy = size - offset;
            if (to_copy > PRODOS_BLOCK_SIZE) to_copy = PRODOS_BLOCK_SIZE;
            memcpy(block, data + offset, to_copy);
            
            ret = uft_apple_write_block(ctx, data_block, block);
            if (ret < 0) return ret;
            
            index[idx] = data_block & 0xFF;
            index[idx + 256] = (data_block >> 8) & 0xFF;
            
            (*blocks_used)++;
            offset += PRODOS_BLOCK_SIZE;
            idx++;
        }
        
        /* Write index block */
        ret = uft_apple_write_block(ctx, index_block, index);
        if (ret < 0) return ret;
        
        /* Add to master index */
        master[master_idx] = index_block & 0xFF;
        master[master_idx + 256] = (index_block >> 8) & 0xFF;
        
        master_idx++;
    }
    
    /* Write master index */
    ret = uft_apple_write_block(ctx, *key_block, master);
    return ret;
}

/**
 * @brief Write ProDOS file
 */
int uft_prodos_write_file(uft_apple_ctx_t *ctx, const uint8_t *data, size_t size,
                          uint16_t *key_block, uint8_t *storage_type,
                          uint16_t *blocks_used) {
    if (!ctx || !key_block || !storage_type || !blocks_used) {
        return UFT_APPLE_ERR_INVALID;
    }
    
    /* Determine storage type based on size */
    if (size <= PRODOS_BLOCK_SIZE) {
        *storage_type = UFT_PRODOS_STORAGE_SEEDLING;
        return write_seedling(ctx, data, size, key_block, blocks_used);
    } else if (size <= 256 * PRODOS_BLOCK_SIZE) {
        *storage_type = UFT_PRODOS_STORAGE_SAPLING;
        return write_sapling(ctx, data, size, key_block, blocks_used);
    } else {
        *storage_type = UFT_PRODOS_STORAGE_TREE;
        return write_tree(ctx, data, size, key_block, blocks_used);
    }
}

/*===========================================================================
 * Directory Entry Creation
 *===========================================================================*/

/**
 * @brief Create directory entry in ProDOS directory
 */
int uft_prodos_create_entry(uft_apple_ctx_t *ctx, uint16_t dir_block,
                            const char *name, uint8_t file_type, uint16_t aux_type,
                            uint16_t key_block, uint8_t storage_type,
                            uint16_t blocks_used, uint32_t eof) {
    if (!ctx || !name) return UFT_APPLE_ERR_INVALID;
    
    /* Check if name exists */
    int ret = uft_prodos_find_entry(ctx, dir_block, name, NULL, NULL, NULL);
    if (ret == 0) return UFT_APPLE_ERR_EXISTS;
    
    /* Find free slot */
    uint8_t block_data[PRODOS_BLOCK_SIZE];
    uint16_t current_block = dir_block;
    int safety = 100;
    bool first_block = true;
    
    while (current_block != 0 && safety-- > 0) {
        ret = uft_apple_read_block(ctx, current_block, block_data);
        if (ret < 0) return ret;
        
        int start_entry = first_block ? 1 : 0;
        first_block = false;
        
        for (int i = start_entry; i < PRODOS_ENTRIES_PER_BLOCK; i++) {
            uint8_t *entry_data = &block_data[4 + i * PRODOS_ENTRY_SIZE];
            
            uint8_t storage = (entry_data[0] >> 4) & 0x0F;
            
            if (storage == 0) {
                /* Found free slot */
                memset(entry_data, 0, PRODOS_ENTRY_SIZE);
                
                /* Name */
                int name_len = strlen(name);
                if (name_len > 15) name_len = 15;
                entry_data[0] = (storage_type << 4) | name_len;
                for (int j = 0; j < name_len; j++) {
                    entry_data[1 + j] = toupper(name[j]);
                }
                
                /* File type */
                entry_data[0x10] = file_type;
                
                /* Key block */
                entry_data[0x11] = key_block & 0xFF;
                entry_data[0x12] = (key_block >> 8) & 0xFF;
                
                /* Blocks used */
                entry_data[0x13] = blocks_used & 0xFF;
                entry_data[0x14] = (blocks_used >> 8) & 0xFF;
                
                /* EOF */
                entry_data[0x15] = eof & 0xFF;
                entry_data[0x16] = (eof >> 8) & 0xFF;
                entry_data[0x17] = (eof >> 16) & 0xFF;
                
                /* Creation date/time */
                uft_prodos_datetime_t now = uft_prodos_from_unix_time(time(NULL));
                entry_data[0x18] = now.date & 0xFF;
                entry_data[0x19] = (now.date >> 8) & 0xFF;
                entry_data[0x1A] = now.time & 0xFF;
                entry_data[0x1B] = (now.time >> 8) & 0xFF;
                
                /* Version */
                entry_data[0x1C] = 0;
                entry_data[0x1D] = 0;
                
                /* Access */
                entry_data[0x1E] = 0xC3;  /* Read/Write enabled */
                
                /* Aux type */
                entry_data[0x1F] = aux_type & 0xFF;
                entry_data[0x20] = (aux_type >> 8) & 0xFF;
                
                /* Modification date/time */
                entry_data[0x21] = now.date & 0xFF;
                entry_data[0x22] = (now.date >> 8) & 0xFF;
                entry_data[0x23] = now.time & 0xFF;
                entry_data[0x24] = (now.time >> 8) & 0xFF;
                
                /* Write block */
                ret = uft_apple_write_block(ctx, current_block, block_data);
                if (ret < 0) return ret;
                
                /* Update file count in header */
                ret = uft_apple_read_block(ctx, dir_block, block_data);
                if (ret < 0) return ret;
                
                uint16_t file_count = block_data[0x25] | (block_data[0x26] << 8);
                file_count++;
                block_data[0x25] = file_count & 0xFF;
                block_data[0x26] = (file_count >> 8) & 0xFF;
                
                return uft_apple_write_block(ctx, dir_block, block_data);
            }
        }
        
        current_block = block_data[2] | (block_data[3] << 8);
    }
    
    return UFT_APPLE_ERR_DISKFULL;
}

/*===========================================================================
 * Print ProDOS Directory
 *===========================================================================*/

void uft_prodos_print_dir(const uft_apple_ctx_t *ctx, const char *path, FILE *fp) {
    if (!ctx || !fp) return;
    
    uint16_t dir_block = UFT_PRODOS_KEY_BLOCK;
    char filename[16] = "";
    
    if (path && path[0]) {
        uft_prodos_resolve_path(ctx, path, &dir_block, filename);
        if (filename[0]) {
            /* Path points to a file, not directory */
            return;
        }
    }
    
    uft_apple_dir_t dir;
    uft_apple_dir_init(&dir);
    
    if (uft_prodos_read_dir(ctx, dir_block, &dir) < 0) {
        fprintf(fp, "Error reading directory\n");
        return;
    }
    
    fprintf(fp, "\n/%s\n\n", ctx->volume_name);
    fprintf(fp, " NAME           TYPE  BLOCKS  MODIFIED          SIZE\n\n");
    
    for (size_t i = 0; i < dir.count; i++) {
        const uft_apple_entry_t *e = &dir.entries[i];
        
        char date_str[20] = "";
        if (e->modified) {
            struct tm *tm = localtime(&e->modified);
            if (tm) {
                strftime(date_str, sizeof(date_str), "%d-%b-%y %H:%M", tm);
            }
        }
        
        fprintf(fp, "%c%-15s %s  %5d  %-17s %6u\n",
                e->is_directory ? '/' : (e->is_locked ? '*' : ' '),
                e->name,
                uft_prodos_type_string(e->file_type),
                e->blocks_used,
                date_str,
                (unsigned)e->size);
    }
    
    uint16_t free_blocks;
    uft_prodos_get_free(ctx, &free_blocks);
    fprintf(fp, "\nBLOCKS FREE: %5d     BLOCKS USED: %5d\n",
            free_blocks, ctx->total_blocks - free_blocks);
    
    uft_apple_dir_free(&dir);
}
