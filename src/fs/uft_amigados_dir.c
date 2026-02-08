/**
 * @file uft_amigados_dir.c
 * @brief AmigaDOS Directory Operations
 * 
 * Directory parsing, hash table traversal, entry lookup.
 */

#include "uft/fs/uft_amigados.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

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

static void read_bcpl_string(const uint8_t *src, char *dst, size_t max_len)
{
    size_t len = src[0];
    if (len > max_len - 1) len = max_len - 1;
    memcpy(dst, src + 1, len);
    dst[len] = '\0';
}

/* Get direct pointer to block in image */
static const uint8_t *get_block_ptr(const uft_amiga_ctx_t *ctx, uint32_t block_num)
{
    if (!ctx || !ctx->data || block_num >= ctx->total_blocks) return NULL;
    return ctx->data + (size_t)block_num * UFT_AMIGA_BLOCK_SIZE;
}

/* Case-insensitive compare for Amiga filenames */
static int amiga_strcasecmp(const char *a, const char *b, bool intl)
{
    while (*a && *b) {
        unsigned char ca = (unsigned char)*a;
        unsigned char cb = (unsigned char)*b;
        
        /* Fold to uppercase */
        if (ca >= 'a' && ca <= 'z') ca -= 32;
        if (cb >= 'a' && cb <= 'z') cb -= 32;
        
        if (intl) {
            /* International: also fold accented chars */
            if (ca >= 0xE0 && ca <= 0xFE && ca != 0xF7) ca -= 32;
            if (cb >= 0xE0 && cb <= 0xFE && cb != 0xF7) cb -= 32;
        }
        
        if (ca != cb) return (int)ca - (int)cb;
        a++;
        b++;
    }
    
    return (unsigned char)*a - (unsigned char)*b;
}

/*===========================================================================
 * Entry Parsing
 *===========================================================================*/

static int parse_entry_block(const uft_amiga_ctx_t *ctx, uint32_t block_num,
                             uft_amiga_entry_t *entry)
{
    if (!ctx || !entry) return -1;
    
    const uint8_t *block = get_block_ptr(ctx, block_num);
    if (!block) return -1;
    
    memset(entry, 0, sizeof(*entry));
    
    /* Verify block type */
    uint32_t type = read_be32(block);
    if (type != UFT_AMIGA_T_SHORT) return -1;
    
    /* Verify checksum */
    if (ctx->verify_checksums && !uft_amiga_verify_checksum(block)) {
        return -1;
    }
    
    entry->header_block = block_num;
    
    /* Secondary type at offset 508 */
    entry->secondary_type = read_be32s(block + 508);
    
    switch (entry->secondary_type) {
        case UFT_AMIGA_ST_ROOT:
            entry->is_dir = true;
            break;
        case UFT_AMIGA_ST_USERDIR:
            entry->is_dir = true;
            break;
        case UFT_AMIGA_ST_FILE:
            entry->is_file = true;
            break;
        case UFT_AMIGA_ST_SOFTLINK:
            entry->is_softlink = true;
            break;
        case UFT_AMIGA_ST_LINKDIR:
        case UFT_AMIGA_ST_LINKFILE:
            entry->is_hardlink = true;
            entry->real_entry = read_be32(block + 444);  /* Real header */
            break;
        default:
            return -1;  /* Unknown type */
    }
    
    /* Name at offset 432 (BCPL string, 30 bytes max standard) */
    if (ctx->is_longnames) {
        read_bcpl_string(block + 432, entry->name, UFT_AMIGA_MAX_FILENAME_LFS + 1);
    } else {
        read_bcpl_string(block + 432, entry->name, UFT_AMIGA_MAX_FILENAME + 1);
    }
    
    /* Comment at offset 396 (BCPL string, 79 bytes max) */
    read_bcpl_string(block + 396, entry->comment, UFT_AMIGA_MAX_COMMENT + 1);
    
    /* Parent block at offset 504 */
    entry->parent_block = read_be32(block + 504);
    
    /* Hash chain (next entry with same hash) at offset 496 */
    entry->hash_chain = read_be32(block + 496);
    
    /* Protection bits at offset 500 */
    entry->protection = read_be32(block + 500);
    
    /* File-specific fields */
    if (entry->is_file || entry->secondary_type == UFT_AMIGA_ST_LINKFILE) {
        entry->size = read_be32(block + 324);
        entry->blocks = read_be32(block + 8);  /* High_seq / num data blocks */
        
        if (ctx->is_ffs) {
            /* FFS: first data block in data_blocks[] at offset 308 */
            entry->first_data = read_be32(block + 308);
        } else {
            /* OFS: first_data at offset 16 */
            entry->first_data = read_be32(block + 16);
        }
        
        /* Extension block at offset 492 */
        entry->extension = read_be32(block + 492);
    }
    
    /* Soft link target */
    if (entry->is_softlink) {
        /* Target path is stored as BCPL string at offset 432 onwards
         * (actually in comment field area for softlinks) */
        read_bcpl_string(block + 432, entry->link_target, UFT_AMIGA_MAX_PATH);
    }
    
    /* Timestamp at offset 420-428 (last modified) */
    uint32_t days = read_be32(block + 420);
    uint32_t mins = read_be32(block + 424);
    uint32_t ticks = read_be32(block + 428);
    entry->mtime = uft_amiga_to_unix_time(days, mins, ticks);
    
    return 0;
}

/*===========================================================================
 * Directory Listing
 *===========================================================================*/

static int ensure_dir_capacity(uft_amiga_dir_t *dir, size_t needed)
{
    if (dir->capacity >= needed) return 0;
    
    size_t new_cap = dir->capacity ? dir->capacity * 2 : 64;
    while (new_cap < needed) new_cap *= 2;
    
    uft_amiga_entry_t *new_entries = realloc(dir->entries,
                                              new_cap * sizeof(uft_amiga_entry_t));
    if (!new_entries) return -1;
    
    dir->entries = new_entries;
    dir->capacity = new_cap;
    return 0;
}

static int load_dir_internal(const uft_amiga_ctx_t *ctx, uint32_t dir_block,
                             uft_amiga_dir_t *dir, bool is_root)
{
    if (!ctx || !dir) return -1;
    
    const uint8_t *block = get_block_ptr(ctx, dir_block);
    if (!block) return -1;
    
    /* Verify block type */
    uint32_t type = read_be32(block);
    int32_t sec_type = read_be32s(block + 508);
    
    if (type != UFT_AMIGA_T_SHORT) return -1;
    if (is_root && sec_type != UFT_AMIGA_ST_ROOT) return -1;
    if (!is_root && sec_type != UFT_AMIGA_ST_USERDIR) return -1;
    
    /* Verify checksum */
    if (ctx->verify_checksums && !uft_amiga_verify_checksum(block)) {
        return -1;
    }
    
    /* Initialize directory */
    memset(dir, 0, sizeof(*dir));
    dir->dir_block = dir_block;
    
    /* Get directory name */
    read_bcpl_string(block + 432, dir->dir_name,
                     ctx->is_longnames ? UFT_AMIGA_MAX_FILENAME_LFS + 1 :
                                         UFT_AMIGA_MAX_FILENAME + 1);
    
    /* Hash table at offset 24 (72 entries × 4 bytes) */
    for (int hash = 0; hash < UFT_AMIGA_HASH_SIZE; hash++) {
        uint32_t entry_block = read_be32(block + 24 + hash * 4);
        
        while (entry_block != 0 && entry_block < ctx->total_blocks) {
            /* Prevent infinite loops */
            bool found_cycle = false;
            for (size_t i = 0; i < dir->count; i++) {
                if (dir->entries[i].header_block == entry_block) {
                    found_cycle = true;
                    break;
                }
            }
            if (found_cycle) break;
            
            /* Parse entry */
            uft_amiga_entry_t entry;
            if (parse_entry_block(ctx, entry_block, &entry) != 0) {
                break;  /* Invalid entry, stop chain */
            }
            
            /* Add to list */
            if (ensure_dir_capacity(dir, dir->count + 1) != 0) {
                return -1;
            }
            dir->entries[dir->count++] = entry;
            
            /* Follow hash chain */
            entry_block = entry.hash_chain;
        }
    }
    
    return 0;
}

int uft_amiga_load_root(const uft_amiga_ctx_t *ctx, uft_amiga_dir_t *dir)
{
    if (!ctx || !ctx->is_valid || !dir) return -1;
    return load_dir_internal(ctx, ctx->root_block, dir, true);
}

int uft_amiga_load_dir(const uft_amiga_ctx_t *ctx, uint32_t block_num,
                       uft_amiga_dir_t *dir)
{
    if (!ctx || !ctx->is_valid || !dir) return -1;
    
    if (block_num == ctx->root_block) {
        return load_dir_internal(ctx, block_num, dir, true);
    }
    return load_dir_internal(ctx, block_num, dir, false);
}

int uft_amiga_load_dir_path(const uft_amiga_ctx_t *ctx, const char *path,
                            uft_amiga_dir_t *dir)
{
    if (!ctx || !ctx->is_valid || !dir) return -1;
    
    /* Empty or root path */
    if (!path || path[0] == '\0' || (path[0] == '/' && path[1] == '\0')) {
        return uft_amiga_load_root(ctx, dir);
    }
    
    /* Find directory by path */
    uft_amiga_entry_t entry;
    if (uft_amiga_find_path(ctx, path, &entry) != 0) {
        return -1;
    }
    
    if (!entry.is_dir) {
        return -1;  /* Not a directory */
    }
    
    return uft_amiga_load_dir(ctx, entry.header_block, dir);
}

void uft_amiga_free_dir(uft_amiga_dir_t *dir)
{
    if (!dir) return;
    
    if (dir->entries) {
        free(dir->entries);
    }
    memset(dir, 0, sizeof(*dir));
}

/*===========================================================================
 * Entry Finding
 *===========================================================================*/

int uft_amiga_find_entry(const uft_amiga_ctx_t *ctx, uint32_t dir_block,
                         const char *name, uft_amiga_entry_t *entry)
{
    if (!ctx || !ctx->is_valid || !name || !entry) return -1;
    
    if (dir_block == 0) {
        dir_block = ctx->root_block;
    }
    
    const uint8_t *block = get_block_ptr(ctx, dir_block);
    if (!block) return -1;
    
    /* Calculate hash */
    uint32_t hash = uft_amiga_hash_name(name, ctx->is_intl);
    
    /* Get first entry in hash chain */
    uint32_t entry_block = read_be32(block + 24 + hash * 4);
    
    /* Follow chain */
    int iterations = 0;
    while (entry_block != 0 && entry_block < ctx->total_blocks) {
        if (++iterations > 10000) break;  /* Safety limit */
        
        uft_amiga_entry_t temp;
        if (parse_entry_block(ctx, entry_block, &temp) != 0) {
            break;
        }
        
        /* Case-insensitive compare */
        if (amiga_strcasecmp(temp.name, name, ctx->is_intl) == 0) {
            *entry = temp;
            return 0;
        }
        
        entry_block = temp.hash_chain;
    }
    
    return -1;  /* Not found */
}

int uft_amiga_find_path(const uft_amiga_ctx_t *ctx, const char *path,
                        uft_amiga_entry_t *entry)
{
    if (!ctx || !ctx->is_valid || !path || !entry) return -1;
    
    /* Skip leading slash */
    while (*path == '/') path++;
    
    /* Empty path = root */
    if (*path == '\0') {
        return parse_entry_block(ctx, ctx->root_block, entry);
    }
    
    /* Copy path for tokenization */
    char *path_copy = strdup(path);
    if (!path_copy) return -1;
    
    uint32_t current_dir = ctx->root_block;
    char *saveptr = NULL;
    char *token = strtok_r(path_copy, "/", &saveptr);
    
    while (token) {
        /* Skip empty tokens (double slashes) */
        if (token[0] == '\0') {
            token = strtok_r(NULL, "/", &saveptr);
            continue;
        }
        
        /* Find entry in current directory */
        if (uft_amiga_find_entry(ctx, current_dir, token, entry) != 0) {
            free(path_copy);
            return -1;
        }
        
        token = strtok_r(NULL, "/", &saveptr);
        
        /* If more path components, must be directory */
        if (token && !entry->is_dir) {
            free(path_copy);
            return -1;
        }
        
        if (entry->is_dir) {
            current_dir = entry->header_block;
        }
    }
    
    free(path_copy);
    return 0;
}

/*===========================================================================
 * Directory Iteration
 *===========================================================================*/

int uft_amiga_foreach_entry(const uft_amiga_ctx_t *ctx, uint32_t dir_block,
                            uft_amiga_dir_callback_t callback, void *user_data)
{
    if (!ctx || !ctx->is_valid || !callback) return -1;
    
    uft_amiga_dir_t dir;
    int ret;
    
    if (dir_block == 0) {
        ret = uft_amiga_load_root(ctx, &dir);
    } else {
        ret = uft_amiga_load_dir(ctx, dir_block, &dir);
    }
    
    if (ret != 0) return ret;
    
    for (size_t i = 0; i < dir.count; i++) {
        ret = callback(&dir.entries[i], user_data);
        if (ret != 0) {
            uft_amiga_free_dir(&dir);
            return ret;
        }
    }
    
    uft_amiga_free_dir(&dir);
    return 0;
}

/* Recursive iteration helper */
typedef struct {
    const uft_amiga_ctx_t *ctx;
    uft_amiga_dir_callback_t callback;
    void *user_data;
    int depth;
} foreach_file_state_t;

static int foreach_file_recurse(const uft_amiga_entry_t *entry, void *user_data)
{
    foreach_file_state_t *state = (foreach_file_state_t *)user_data;
    
    /* Prevent infinite recursion */
    if (state->depth > 100) return 0;
    
    /* Call callback for this entry */
    int ret = state->callback(entry, state->user_data);
    if (ret != 0) return ret;
    
    /* Recurse into directories */
    if (entry->is_dir && !entry->is_hardlink) {
        foreach_file_state_t child_state = *state;
        child_state.depth++;
        
        uft_amiga_foreach_entry(state->ctx, entry->header_block,
                                foreach_file_recurse, &child_state);
    }
    
    return 0;
}

int uft_amiga_foreach_file(const uft_amiga_ctx_t *ctx,
                           uft_amiga_dir_callback_t callback, void *user_data)
{
    if (!ctx || !ctx->is_valid || !callback) return -1;
    
    foreach_file_state_t state = {
        .ctx = ctx,
        .callback = callback,
        .user_data = user_data,
        .depth = 0
    };
    
    return uft_amiga_foreach_entry(ctx, 0, foreach_file_recurse, &state);
}

/*===========================================================================
 * Directory Printing
 *===========================================================================*/

void uft_amiga_print_dir(const uft_amiga_dir_t *dir)
{
    if (!dir) return;
    
    printf("Directory: %s (block %u)\n", dir->dir_name, dir->dir_block);
    printf("%-32s %8s  %-8s  %s\n", "Name", "Size", "Prot", "Comment");
    printf("%-32s %8s  %-8s  %s\n", "----", "----", "----", "-------");
    
    for (size_t i = 0; i < dir->count; i++) {
        const uft_amiga_entry_t *e = &dir->entries[i];
        
        char prot_str[16];
        uft_amiga_protection_str(e->protection, prot_str);
        
        char type_char = '-';
        if (e->is_dir) type_char = 'd';
        else if (e->is_softlink) type_char = 'l';
        else if (e->is_hardlink) type_char = 'h';
        
        if (e->is_dir) {
            printf("%c %-30s    <DIR>  %-8s  %s\n",
                   type_char, e->name, prot_str, e->comment);
        } else {
            printf("%c %-30s %8u  %-8s  %s\n",
                   type_char, e->name, e->size, prot_str, e->comment);
        }
    }
    
    printf("\n%zu entries\n", dir->count);
}
