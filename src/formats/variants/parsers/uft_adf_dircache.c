/**
 * @file uft_adf_dircache.c
 * @brief ADF DirCache (DC) Filesystem Implementation
 * 
 * KRITISCHER FIX: DirCache-Blöcke werden jetzt korrekt geparst!
 * 
 * DirCache Format (AmigaDOS 3.0+):
 * - Speichert Directory-Einträge in separaten Cache-Blöcken
 * - Schnellerer Directory-Zugriff
 * - Root-Block hat Pointer auf ersten Cache-Block
 */

#include "uft/formats/variants/parsers/uft_adf_dircache.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ============================================================================
// Helpers
// ============================================================================

static inline uint32_t read_be32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | (p[2] << 8) | p[3];
}

static inline uint16_t read_be16(const uint8_t* p) {
    return (p[0] << 8) | p[1];
}

static const uint8_t* get_block(const uint8_t* data, size_t size, uint32_t block) {
    size_t offset = block * ADF_BLOCK_SIZE;
    if (offset + ADF_BLOCK_SIZE > size) return NULL;
    return data + offset;
}

// ============================================================================
// Probing
// ============================================================================

int adf_get_fs_type(const uint8_t* data, size_t size) {
    if (!data || size < 4) return -1;
    
    if (memcmp(data, "DOS", 3) != 0) return -1;
    
    return data[3];
}

bool adf_has_dircache(const uint8_t* data, size_t size) {
    int fs_type = adf_get_fs_type(data, size);
    return (fs_type == ADF_FS_OFS_DC || fs_type == ADF_FS_FFS_DC);
}

// ============================================================================
// DirCache Parsing
// ============================================================================

/**
 * @brief Parse a single DirCache entry
 * @return Bytes consumed, 0 on error
 */
static size_t parse_dircache_entry(const uint8_t* data, size_t max_size,
                                    adf_dir_entry_t* entry) {
    if (max_size < 25) return 0;  // Minimum entry size
    
    memset(entry, 0, sizeof(*entry));
    
    // Header (block number)
    entry->block = read_be32(data);
    if (entry->block == 0) return 0;  // End of entries
    
    // Size (for files)
    entry->size = read_be32(data + 4);
    
    // Protection bits
    entry->protect = read_be32(data + 8);
    
    // Days, mins, ticks
    entry->days = read_be16(data + 12);
    entry->mins = read_be16(data + 14);
    entry->ticks = read_be16(data + 16);
    
    // Type
    uint8_t type = data[18];
    entry->is_file = (type == (uint8_t)(-3 & 0xFF));  // ST_FILE = -3
    entry->is_dir = (type == ADF_ST_USERDIR || type == ADF_ST_ROOT);
    entry->is_link = (type == ADF_ST_SOFTLINK || type == ADF_ST_LINKDIR);
    
    // Name
    uint8_t name_len = data[19];
    if (name_len > 30) name_len = 30;
    if (20 + name_len > max_size) return 0;
    
    memcpy(entry->name, data + 20, name_len);
    entry->name[name_len] = '\0';
    
    // Comment (optional)
    size_t offset = 20 + name_len;
    
    // Align to word boundary
    if (offset % 2) offset++;
    
    if (offset < max_size) {
        uint8_t comment_len = data[offset];
        if (comment_len > 0 && comment_len <= 79 && offset + 1 + comment_len <= max_size) {
            memcpy(entry->comment, data + offset + 1, comment_len);
            entry->comment[comment_len] = '\0';
            entry->has_comment = true;
            offset += 1 + comment_len;
        } else {
            offset += 1;  // Skip zero-length comment
        }
    }
    
    // Align to word boundary
    if (offset % 2) offset++;
    
    return offset;
}

adf_dircache_t* adf_dc_read_dir(const uint8_t* data, size_t size,
                                 uint32_t dir_block) {
    // Get directory block
    const uint8_t* dir_data = get_block(data, size, dir_block);
    if (!dir_data) return NULL;
    
    // Check type
    uint32_t type = read_be32(dir_data);
    if (type != ADF_T_HEADER) return NULL;
    
    // Get extension block (first DirCache block for FFS-DC)
    // Offset 432 in directory header
    uint32_t cache_block = read_be32(dir_data + 432);
    if (cache_block == 0) return NULL;  // No cache
    
    adf_dircache_t* cache = calloc(1, sizeof(adf_dircache_t));
    if (!cache) return NULL;
    
    cache->parent_block = dir_block;
    cache->first_cache = cache_block;
    
    // Count entries first
    int total_entries = 0;
    uint32_t current_block = cache_block;
    
    while (current_block != 0) {
        const uint8_t* block = get_block(data, size, current_block);
        if (!block) break;
        
        uint32_t block_type = read_be32(block);
        if (block_type != ADF_T_DIRCACHE) break;
        
        uint32_t record_count = read_be32(block + 12);
        total_entries += record_count;
        cache->cache_blocks_used++;
        
        // Next cache block
        current_block = read_be32(block + 16);
        
        // Safety limit
        if (cache->cache_blocks_used > 100) break;
    }
    
    if (total_entries == 0) {
        free(cache);
        return NULL;
    }
    
    // Allocate entries
    cache->entries = calloc(total_entries, sizeof(adf_dir_entry_t));
    if (!cache->entries) {
        free(cache);
        return NULL;
    }
    
    // Parse entries
    current_block = cache_block;
    int entry_idx = 0;
    
    while (current_block != 0 && entry_idx < total_entries) {
        const uint8_t* block = get_block(data, size, current_block);
        if (!block) break;
        
        uint32_t block_type = read_be32(block);
        if (block_type != ADF_T_DIRCACHE) break;
        
        uint32_t record_count = read_be32(block + 12);
        
        // Parse records in this block
        const uint8_t* records = block + 24;  // Skip header
        size_t records_size = ADF_BLOCK_SIZE - 24;
        size_t offset = 0;
        
        for (uint32_t r = 0; r < record_count && entry_idx < total_entries; r++) {
            if (offset >= records_size) break;
            
            size_t consumed = parse_dircache_entry(records + offset,
                                                    records_size - offset,
                                                    &cache->entries[entry_idx]);
            if (consumed == 0) break;
            
            offset += consumed;
            entry_idx++;
        }
        
        // Next cache block
        current_block = read_be32(block + 16);
    }
    
    cache->entry_count = entry_idx;
    
    // Calculate total size
    for (int i = 0; i < cache->entry_count; i++) {
        if (cache->entries[i].is_file) {
            cache->total_size += cache->entries[i].size;
        }
    }
    
    return cache;
}

adf_dircache_t* adf_dc_read_root(const uint8_t* data, size_t size) {
    if (size != ADF_DD_SIZE && size != ADF_HD_SIZE) return NULL;
    
    // Root block is at block 880 for DD, 1760 for HD
    uint32_t root_block = (size == ADF_DD_SIZE) ? 880 : 1760;
    
    return adf_dc_read_dir(data, size, root_block);
}

bool adf_dc_find_entry(const adf_dircache_t* cache, const char* name,
                       adf_dir_entry_t* entry_out) {
    if (!cache || !name || !entry_out) return false;
    
    for (int i = 0; i < cache->entry_count; i++) {
        if (strcasecmp(cache->entries[i].name, name) == 0) {
            *entry_out = cache->entries[i];
            return true;
        }
    }
    
    return false;
}

// ============================================================================
// Full Image Parsing
// ============================================================================

adf_dc_image_t* adf_dc_open(const uint8_t* data, size_t size) {
    if (size != ADF_DD_SIZE && size != ADF_HD_SIZE) return NULL;
    
    int fs_type = adf_get_fs_type(data, size);
    if (fs_type < 0 || fs_type > 5) return NULL;
    
    adf_dc_image_t* img = calloc(1, sizeof(adf_dc_image_t));
    if (!img) return NULL;
    
    img->fs_type = fs_type;
    img->is_ffs = (fs_type & 1) != 0;
    img->is_intl = (fs_type >= 2);
    img->has_dircache = (fs_type >= 4);
    img->is_hd = (size == ADF_HD_SIZE);
    
    img->num_blocks = size / ADF_BLOCK_SIZE;
    img->root_block = (size == ADF_DD_SIZE) ? 880 : 1760;
    
    // Parse root directory cache if DC filesystem
    if (img->has_dircache) {
        img->root_cache = adf_dc_read_root(data, size);
        
        if (!img->root_cache) {
            snprintf(img->error_msg, sizeof(img->error_msg),
                     "Failed to read root DirCache");
            // Not fatal - can still use standard directory reading
        }
    }
    
    img->is_valid = true;
    return img;
}

void adf_dc_free(adf_dircache_t* cache) {
    if (!cache) return;
    free(cache->entries);
    free(cache);
}

void adf_dc_close(adf_dc_image_t* img) {
    if (!img) return;
    adf_dc_free(img->root_cache);
    free(img);
}
