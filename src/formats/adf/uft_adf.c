/**
 * @file uft_adf.c
 * @brief ADF Full Support with DirCache Implementation
 * 
 * ROADMAP F1.2 - Priority P1
 */

#include "uft/core/uft_error_compat.h"
#include "uft/formats/uft_adf.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

// ============================================================================
// Helpers
// ============================================================================

static inline uint32_t read_be32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | (p[2] << 8) | p[3];
}

static inline void write_be32(uint8_t* p, uint32_t v) {
    p[0] = (v >> 24) & 0xFF;
    p[1] = (v >> 16) & 0xFF;
    p[2] = (v >> 8) & 0xFF;
    p[3] = v & 0xFF;
}

static inline int32_t read_be32s(const uint8_t* p) {
    return (int32_t)read_be32(p);
}

static const uint8_t* get_block(const adf_image_t* img, uint32_t block) {
    if (!img || !img->data || block >= (uint32_t)img->blocks) return NULL;
    return img->data + block * ADF_BLOCK_SIZE;
}

// ============================================================================
// Checksum
// ============================================================================

uint32_t adf_checksum(const uint8_t* block) {
    uint32_t sum = 0;
    for (int i = 0; i < ADF_BLOCK_SIZE; i += 4) {
        if (i != 20) {  // Skip checksum field
            sum += read_be32(block + i);
        }
    }
    return (uint32_t)(-(int32_t)sum);
}

bool adf_verify_checksum(const uint8_t* block) {
    uint32_t expected = read_be32(block + 20);
    uint32_t calculated = adf_checksum(block);
    return expected == calculated;
}

// ============================================================================
// Detection
// ============================================================================

int adf_detect_variant(const uint8_t* data, size_t size,
                       adf_detect_result_t* result) {
    if (!data || !result) return -1;
    
    memset(result, 0, sizeof(*result));
    
    // Size check
    if (size == ADF_DD_SIZE) {
        result->is_hd = false;
        result->confidence = 80;
    } else if (size == ADF_HD_SIZE) {
        result->is_hd = true;
        result->variant |= ADF_VAR_HD;
        result->confidence = 80;
    } else {
        return -1;  // Not a standard ADF
    }
    
    // Check bootblock for DOS signature
    if (memcmp(data, "DOS", 3) == 0) {
        uint8_t fs_byte = data[3];
        
        switch (fs_byte) {
            case 0:
                result->fs_type = ADF_FS_OFS;
                result->variant |= ADF_VAR_OFS;
                break;
            case 1:
                result->fs_type = ADF_FS_FFS;
                result->variant |= ADF_VAR_FFS;
                break;
            case 2:
                result->fs_type = ADF_FS_OFS_INTL;
                result->variant |= ADF_VAR_OFS | ADF_VAR_INTL;
                break;
            case 3:
                result->fs_type = ADF_FS_FFS_INTL;
                result->variant |= ADF_VAR_FFS | ADF_VAR_INTL;
                break;
            case 4:
                result->fs_type = ADF_FS_OFS_DC;
                result->variant |= ADF_VAR_OFS | ADF_VAR_DIRCACHE;
                result->has_dircache = true;
                break;
            case 5:
                result->fs_type = ADF_FS_FFS_DC;
                result->variant |= ADF_VAR_FFS | ADF_VAR_DIRCACHE;
                result->has_dircache = true;
                break;
            default:
                result->fs_type = ADF_FS_UNKNOWN;
                result->variant |= ADF_VAR_NDOS;
        }
        
        result->confidence += 15;
        snprintf(result->explanation, sizeof(result->explanation),
                 "%s %s%s%s",
                 result->is_hd ? "HD" : "DD",
                 adf_fs_type_str(result->fs_type),
                 result->has_dircache ? " with DirCache" : "",
                 result->is_bootable ? " [Bootable]" : "");
    } else {
        // Check for PC-FAT
        if (size >= 512 && data[510] == 0x55 && data[511] == 0xAA) {
            result->variant = ADF_VAR_PC_FAT;
            result->fs_type = ADF_FS_UNKNOWN;
            result->confidence = 90;
            snprintf(result->explanation, sizeof(result->explanation),
                     "PC-FAT formatted ADF");
        } else {
            result->variant = ADF_VAR_NDOS;
            result->fs_type = ADF_FS_UNKNOWN;
            result->confidence = 50;
            snprintf(result->explanation, sizeof(result->explanation),
                     "Non-DOS ADF");
        }
    }
    
    // Check for bootable
    if (data[0] == 'D' && data[1] == 'O' && data[2] == 'S') {
        uint32_t checksum = read_be32(data + 4);
        if (checksum != 0) {
            result->is_bootable = true;
            result->variant |= ADF_VAR_BOOTABLE;
        }
    }
    
    // Verify root block
    uint32_t root = result->is_hd ? 1760 : 880;
    if (size > (root + 1) * ADF_BLOCK_SIZE) {
        const uint8_t* root_block = data + root * ADF_BLOCK_SIZE;
        uint32_t type = read_be32(root_block);
        
        if (type == ADF_T_HEADER && adf_verify_checksum(root_block)) {
            result->confidence += 5;
        }
    }
    
    if (result->confidence > 100) result->confidence = 100;
    
    return 0;
}

// ============================================================================
// Open/Create/Close
// ============================================================================

adf_image_t* adf_open_memory(const uint8_t* data, size_t size) {
    adf_detect_result_t detect;
    if (adf_detect_variant(data, size, &detect) != 0) {
        return NULL;
    }
    
    adf_image_t* img = calloc(1, sizeof(adf_image_t));
    if (!img) return NULL;
    
    img->data = malloc(size);
    if (!img->data) {
        free(img);
        return NULL;
    }
    memcpy(img->data, data, size);
    img->data_size = size;
    
    // Set variant info
    img->variant = detect.variant;
    img->fs_type = detect.fs_type;
    img->confidence = detect.confidence;
    img->is_hd = detect.is_hd;
    img->has_dircache = detect.has_dircache;
    img->is_bootable = detect.is_bootable;
    
    // Geometry
    img->blocks = size / ADF_BLOCK_SIZE;
    img->tracks = 80;
    img->heads = 2;
    img->sectors = img->is_hd ? 22 : 11;
    
    // Copy boot code
    if (img->is_bootable) {
        memcpy(img->boot_code, data, 1024);
    }
    
    // Read volume info from root block
    uint32_t root = img->is_hd ? 1760 : 880;
    const uint8_t* root_block = get_block(img, root);
    
    if (root_block) {
        // Name at offset 432 (1 byte len + 30 bytes name)
        int name_len = root_block[432];
        if (name_len > 30) name_len = 30;
        memcpy(img->volume.name, root_block + 433, name_len);
        img->volume.name[name_len] = '\0';
        
        img->volume.root_block = root;
        img->volume.hash_table_size = read_be32(root_block + 12);
        
        // Creation date (offset 420)
        img->volume.creation.days = read_be32s(root_block + 420);
        img->volume.creation.mins = read_be32s(root_block + 424);
        img->volume.creation.ticks = read_be32s(root_block + 428);
    }
    
    // Read DirCache if available
    if (img->has_dircache) {
        img->root_cache = adf_read_dircache(img, root);
    }
    
    // Count free blocks
    img->free_blocks = adf_count_free(img);
    img->used_blocks = img->blocks - img->free_blocks;
    
    img->is_valid = true;
    return img;
}

adf_image_t* adf_open(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    
    if (fseek(f, 0, SEEK_END) != 0) { /* seek error */ }
    long size = ftell(f);
    if (fseek(f, 0, SEEK_SET) != 0) { /* seek error */ }
    if (size <= 0) {
        fclose(f);
        return NULL;
    }
    
    uint8_t* data = malloc(size);
    if (!data) {
        fclose(f);
        return NULL;
    }
    
    if (fread(data, 1, size, f) != (size_t)size) {
        free(data);
        fclose(f);
        return NULL;
    }
    fclose(f);
    
    adf_image_t* img = adf_open_memory(data, size);
    free(data);
    
    return img;
}

adf_image_t* adf_create(bool is_hd, adf_fs_type_t fs_type) {
    size_t size = is_hd ? ADF_HD_SIZE : ADF_DD_SIZE;
    
    adf_image_t* img = calloc(1, sizeof(adf_image_t));
    if (!img) return NULL;
    
    img->data = calloc(1, size);
    if (!img->data) {
        free(img);
        return NULL;
    }
    
    img->data_size = size;
    img->fs_type = fs_type;
    img->is_hd = is_hd;
    img->blocks = size / ADF_BLOCK_SIZE;
    img->tracks = 80;
    img->heads = 2;
    img->sectors = is_hd ? 22 : 11;
    
    // Set variant
    img->variant = is_hd ? ADF_VAR_HD : ADF_VAR_NONE;
    if (fs_type & 1) img->variant |= ADF_VAR_FFS;
    else img->variant |= ADF_VAR_OFS;
    if (fs_type >= 4) img->variant |= ADF_VAR_DIRCACHE;
    
    // Write bootblock
    img->data[0] = 'D';
    img->data[1] = 'O';
    img->data[2] = 'S';
    img->data[3] = fs_type;
    
    // Initialize root block
    uint32_t root = is_hd ? 1760 : 880;
    uint8_t* root_block = img->data + root * ADF_BLOCK_SIZE;
    
    write_be32(root_block, ADF_T_HEADER);
    write_be32(root_block + 12, 72);  // Hash table size
    write_be32(root_block + 508, ADF_ST_ROOT);
    
    // Disk name
    root_block[432] = 5;
    memcpy(root_block + 433, "EMPTY", 5);
    
    // Update checksum
    uint32_t checksum = adf_checksum(root_block);
    write_be32(root_block + 20, checksum);
    
    img->volume.root_block = root;
    strncpy(img->volume.name, "EMPTY", sizeof(img->volume.name)-1);
    
    img->is_valid = true;
    img->confidence = 100;
    
    return img;
}

int adf_save(const adf_image_t* img, const char* path) {
    if (!img || !img->data || !path) return -1;
    
    FILE* f = fopen(path, "wb");
    if (!f) return -1;
    
    if (fwrite(img->data, 1, img->data_size, f) != img->data_size) {
        fclose(f);
        return -1;
    }
    if (ferror(f)) {
        fclose(f);
        return UFT_ERR_IO;
    }
    
        
    fclose(f);
return 0;
}

void adf_close(adf_image_t* img) {
    if (!img) return;
    
    adf_free_dircache(img->root_cache);
    free(img->data);
    free(img);
}

// ============================================================================
// Block API
// ============================================================================

int adf_read_block(const adf_image_t* img, uint32_t block, uint8_t* buffer) {
    const uint8_t* data = get_block(img, block);
    if (!data || !buffer) return -1;
    
    memcpy(buffer, data, ADF_BLOCK_SIZE);
    return 0;
}

int adf_write_block(adf_image_t* img, uint32_t block, const uint8_t* buffer) {
    if (!img || !img->data || !buffer) return -1;
    if (block >= (uint32_t)img->blocks) return -1;
    
    memcpy(img->data + block * ADF_BLOCK_SIZE, buffer, ADF_BLOCK_SIZE);
    img->is_modified = true;
    return 0;
}

// ============================================================================
// Directory API
// ============================================================================

static int read_dir_from_block(const adf_image_t* img, uint32_t block,
                                adf_entry_t* entries, int max) {
    const uint8_t* dir_block = get_block(img, block);
    if (!dir_block) return -1;
    
    int count = 0;
    int hash_size = read_be32(dir_block + 12);
    if (hash_size <= 0 || hash_size > 72) hash_size = 72;
    
    // Hash table starts at offset 24
    for (int i = 0; i < hash_size && count < max; i++) {
        uint32_t entry_block = read_be32(dir_block + 24 + i * 4);
        
        while (entry_block != 0 && count < max) {
            const uint8_t* entry_data = get_block(img, entry_block);
            if (!entry_data) break;
            
            adf_entry_t* entry = &entries[count];
            memset(entry, 0, sizeof(*entry));
            
            entry->header_block = entry_block;
            entry->secondary_type = read_be32s(entry_data + 508);
            
            // Name
            int name_len = entry_data[432];
            if (name_len > 30) name_len = 30;
            memcpy(entry->name, entry_data + 433, name_len);
            entry->name[name_len] = '\0';
            
            // Size (for files)
            entry->size = read_be32(entry_data + 324);
            
            // Protection
            entry->protection = read_be32(entry_data + 484);
            
            // Date
            entry->date.days = read_be32s(entry_data + 420);
            entry->date.mins = read_be32s(entry_data + 424);
            entry->date.ticks = read_be32s(entry_data + 428);
            
            // Type flags
            entry->is_file = (entry->secondary_type == ADF_ST_FILE);
            entry->is_dir = (entry->secondary_type == ADF_ST_USERDIR);
            entry->is_link = (entry->secondary_type == ADF_ST_SOFTLINK ||
                             entry->secondary_type == ADF_ST_HARDLINK);
            
            // Comment (offset 496, len at 495)
            int comment_len = entry_data[495];
            if (comment_len > 0 && comment_len <= 79) {
                memcpy(entry->comment, entry_data + 496, comment_len);
                entry->has_comment = true;
            }
            
            count++;
            
            // Next in hash chain
            entry_block = read_be32(entry_data + 496 - 4);  // hash_chain at 492
        }
    }
    
    return count;
}

int adf_read_root(const adf_image_t* img, adf_entry_t* entries, int max) {
    if (!img) return -1;
    return adf_read_dir(img, img->volume.root_block, entries, max);
}

int adf_read_dir(const adf_image_t* img, uint32_t block,
                 adf_entry_t* entries, int max) {
    if (!img || !entries || max <= 0) return -1;
    
    // Use DirCache if available
    if (img->has_dircache) {
        return adf_read_dir_cached(img, block, entries, max);
    }
    
    return read_dir_from_block(img, block, entries, max);
}

int adf_read_dir_cached(const adf_image_t* img, uint32_t block,
                        adf_entry_t* entries, int max) {
    if (!img || !entries || max <= 0) return -1;
    
    adf_dircache_t* cache = adf_read_dircache(img, block);
    if (!cache) {
        // Fall back to standard read
        return read_dir_from_block(img, block, entries, max);
    }
    
    int count = cache->entry_count < max ? cache->entry_count : max;
    memcpy(entries, cache->entries, count * sizeof(adf_entry_t));
    
    adf_free_dircache(cache);
    return count;
}

// ============================================================================
// DirCache API
// ============================================================================

bool adf_has_dircache(const adf_image_t* img) {
    return img && img->has_dircache;
}

adf_dircache_t* adf_read_dircache(const adf_image_t* img, uint32_t dir_block) {
    if (!img || !img->has_dircache) return NULL;
    
    const uint8_t* dir_data = get_block(img, dir_block);
    if (!dir_data) return NULL;
    
    // DirCache pointer is at offset 488 (extension)
    uint32_t cache_block = read_be32(dir_data + 488);
    if (cache_block == 0) return NULL;
    
    adf_dircache_t* cache = calloc(1, sizeof(adf_dircache_t));
    if (!cache) return NULL;
    
    cache->parent_block = dir_block;
    cache->first_cache = cache_block;
    
    // Count entries first
    uint32_t current = cache_block;
    int total = 0;
    
    while (current != 0 && cache->cache_blocks < 100) {
        const uint8_t* block = get_block(img, current);
        if (!block) break;
        
        if (read_be32(block) != ADF_T_DIRCACHE) break;
        
        total += read_be32(block + 12);  // records count
        cache->cache_blocks++;
        
        current = read_be32(block + 16);  // next cache
    }
    
    if (total == 0) {
        free(cache);
        return NULL;
    }
    
    // Allocate entries
    cache->entries = calloc(total, sizeof(adf_entry_t));
    if (!cache->entries) {
        free(cache);
        return NULL;
    }
    
    // Parse entries
    current = cache_block;
    int idx = 0;
    
    while (current != 0 && idx < total) {
        const uint8_t* block = get_block(img, current);
        if (!block) break;
        
        int records = read_be32(block + 12);
        const uint8_t* ptr = block + 24;  // Skip header
        
        for (int r = 0; r < records && idx < total; r++) {
            adf_entry_t* entry = &cache->entries[idx];
            
            entry->header_block = read_be32(ptr);
            entry->size = read_be32(ptr + 4);
            entry->protection = read_be32(ptr + 8);
            
            entry->date.days = (int16_t)((ptr[12] << 8) | ptr[13]);
            entry->date.mins = (int16_t)((ptr[14] << 8) | ptr[15]);
            entry->date.ticks = (int16_t)((ptr[16] << 8) | ptr[17]);
            
            entry->secondary_type = (int8_t)ptr[18];
            entry->is_file = (entry->secondary_type == -3);
            entry->is_dir = (entry->secondary_type == 2);
            
            int name_len = ptr[19];
            if (name_len > 30) name_len = 30;
            memcpy(entry->name, ptr + 20, name_len);
            
            // Skip to next record (align to 2)
            int rec_size = 20 + name_len;
            if (rec_size & 1) rec_size++;
            
            ptr += rec_size;
            idx++;
        }
        
        current = read_be32(block + 16);
    }
    
    cache->entry_count = idx;
    return cache;
}

void adf_free_dircache(adf_dircache_t* cache) {
    if (!cache) return;
    free(cache->entries);
    free(cache);
}

// ============================================================================
// Bitmap API
// ============================================================================

static uint32_t get_bitmap_block(const adf_image_t* img, int index) {
    if (!img) return 0;
    
    uint32_t root = img->volume.root_block;
    const uint8_t* root_block = get_block(img, root);
    if (!root_block) return 0;
    
    // Bitmap pointers at offset 316 (25 pointers)
    if (index < 25) {
        return read_be32(root_block + 316 + index * 4);
    }
    
    return 0;
}

bool adf_is_block_free(const adf_image_t* img, uint32_t block) {
    if (!img || block >= (uint32_t)img->blocks) return false;
    
    // Calculate which bitmap block and bit
    int bm_index = block / (ADF_BLOCK_SIZE * 8 - 32);
    uint32_t bm_block = get_bitmap_block(img, bm_index);
    if (bm_block == 0) return false;
    
    const uint8_t* bitmap = get_block(img, bm_block);
    if (!bitmap) return false;
    
    int bit_index = block % (ADF_BLOCK_SIZE * 8 - 32);
    int byte_offset = 4 + (bit_index / 8);
    int bit = bit_index % 8;
    
    return (bitmap[byte_offset] & (1 << bit)) != 0;
}

int adf_count_free(const adf_image_t* img) {
    if (!img) return 0;
    
    int count = 0;
    for (uint32_t b = 2; b < (uint32_t)img->blocks; b++) {
        if (adf_is_block_free(img, b)) count++;
    }
    
    return count;
}

// ============================================================================
// Utility
// ============================================================================

time_t adf_date_to_time(const adf_date_t* date) {
    if (!date) return 0;
    
    // Amiga epoch: 1978-01-01
    struct tm base = {0};
    base.tm_year = 78;
    base.tm_mon = 0;
    base.tm_mday = 1;
    
    time_t epoch = mktime(&base);
    return epoch + date->days * 86400 + date->mins * 60 + date->ticks / 50;
}

const char* adf_fs_type_str(adf_fs_type_t type) {
    switch (type) {
        case ADF_FS_OFS:      return "OFS";
        case ADF_FS_FFS:      return "FFS";
        case ADF_FS_OFS_INTL: return "OFS-INTL";
        case ADF_FS_FFS_INTL: return "FFS-INTL";
        case ADF_FS_OFS_DC:   return "OFS-DC";
        case ADF_FS_FFS_DC:   return "FFS-DC";
        default:              return "Unknown";
    }
}

const char* adf_variant_name(adf_variant_t variant) {
    if (variant & ADF_VAR_PC_FAT) return "PC-FAT";
    if (variant & ADF_VAR_NDOS) return "NDOS";
    if (variant & ADF_VAR_DIRCACHE) {
        return (variant & ADF_VAR_FFS) ? "FFS-DC" : "OFS-DC";
    }
    if (variant & ADF_VAR_INTL) {
        return (variant & ADF_VAR_FFS) ? "FFS-INTL" : "OFS-INTL";
    }
    return (variant & ADF_VAR_FFS) ? "FFS" : "OFS";
}

uint32_t adf_hash_name(const char* name, int table_size) {
    uint32_t hash = strlen(name);
    
    for (int i = 0; name[i]; i++) {
        char c = name[i];
        if (c >= 'a' && c <= 'z') c -= 32;  // toupper
        hash = (hash * 13 + c) & 0x7FF;
    }
    
    return hash % table_size;
}
