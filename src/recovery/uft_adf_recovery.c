/**
 * @file uft_adf_recovery.c
 * @brief ADF Recovery Implementation
 */

#include "uft_adf_recovery.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ═══════════════════════════════════════════════════════════════════════════════
 * Amiga Filesystem Constants
 * ═══════════════════════════════════════════════════════════════════════════════ */

#define ADF_BLOCK_SIZE      512
#define ADF_ROOT_BLOCK      880     /* For DD disks */
#define ADF_BLOCKS_DD       1760
#define ADF_BLOCKS_HD       3520

/* Block types */
#define T_HEADER    2
#define T_DATA      8
#define T_LIST      16
#define T_DIRCACHE  33

/* Secondary types */
#define ST_ROOT     1
#define ST_DIR      2
#define ST_FILE     -3
#define ST_LFILE    -4  /* Hard link to file */
#define ST_LDIR     4   /* Hard link to dir */
#define ST_LSOFT    3   /* Soft link */

/* ═══════════════════════════════════════════════════════════════════════════════
 * Helper Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

static uint32_t read_be32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static int32_t read_be32_signed(const uint8_t *p) {
    return (int32_t)read_be32(p);
}

/* Calculate Amiga checksum */
static uint32_t calc_checksum(const uint8_t *block) {
    uint32_t sum = 0;
    for (int i = 0; i < ADF_BLOCK_SIZE; i += 4) {
        sum += read_be32(block + i);
    }
    return -sum;  /* Checksum makes sum equal 0 */
}

/* Check if block has valid header */
static bool is_valid_header(const uint8_t *block) {
    int32_t type = read_be32_signed(block);
    int32_t secondary = read_be32_signed(block + ADF_BLOCK_SIZE - 4);
    
    return (type == T_HEADER) && 
           (secondary == ST_FILE || secondary == ST_DIR ||
            secondary == ST_LFILE || secondary == ST_LDIR ||
            secondary == ST_LSOFT);
}

/* Read BCPL string */
static void read_bcpl_name(const uint8_t *block, int offset, char *name, int max) {
    int len = block[offset];
    if (len >= max) len = max - 1;
    memcpy(name, block + offset + 1, len);
    name[len] = '\0';
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Scanning Implementation
 * ═══════════════════════════════════════════════════════════════════════════════ */

int uft_adf_scan_deleted(const char *path, 
                         uft_deleted_entry_t *entries, 
                         int max_entries) {
    if (!path || !entries || max_entries <= 0) return -1;
    
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    
    /* Get file size to determine disk type */
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    int total_blocks = file_size / ADF_BLOCK_SIZE;
    int root_block = (file_size == ADF_BLOCKS_HD * ADF_BLOCK_SIZE) ? 
                     ADF_ROOT_BLOCK * 2 : ADF_ROOT_BLOCK;
    
    /* Read bitmap to know which blocks are free */
    uint8_t *bitmap = calloc(total_blocks / 8 + 1, 1);
    if (!bitmap) {
        fclose(f);
        return -1;
    }
    
    /* Read bitmap blocks from root */
    uint8_t root_buf[ADF_BLOCK_SIZE];
    if (fseek(f, root_block * ADF_BLOCK_SIZE, SEEK_SET) != 0) return -1;
    if (fread(root_buf, 1, ADF_BLOCK_SIZE, f) != ADF_BLOCK_SIZE) {
        free(bitmap);
        fclose(f);
        return -1;
    }
    
    /* Bitmap pointers are at offset 316 (bm_pages) */
    int bm_ext_count = 0;
    for (int i = 0; i < 25; i++) {
        uint32_t bm_block = read_be32(root_buf + 316 + i * 4);
        if (bm_block == 0) break;
        bm_ext_count++;
        
        uint8_t bm_buf[ADF_BLOCK_SIZE];
        if (fseek(f, bm_block * ADF_BLOCK_SIZE, SEEK_SET) != 0) continue;
        if (fread(bm_buf, 1, ADF_BLOCK_SIZE, f) == ADF_BLOCK_SIZE) {
            /* Copy bitmap data (skip first 4 bytes = checksum) */
            int bit_offset = i * (ADF_BLOCK_SIZE - 4) * 8;
            for (int j = 4; j < ADF_BLOCK_SIZE && bit_offset < total_blocks; j++) {
                for (int b = 0; b < 8 && bit_offset < total_blocks; b++, bit_offset++) {
                    if (bm_buf[j] & (1 << b)) {
                        /* Block is free */
                        bitmap[bit_offset / 8] |= (1 << (bit_offset % 8));
                    }
                }
            }
        }
    }
    
    int found = 0;
    uint8_t block[ADF_BLOCK_SIZE];
    
    /* Scan all blocks for deleted file headers */
    for (int blk = 2; blk < total_blocks && found < max_entries; blk++) {
        /* Skip if block is in use */
        if (!(bitmap[blk / 8] & (1 << (blk % 8)))) continue;
        
        if (fseek(f, blk * ADF_BLOCK_SIZE, SEEK_SET) != 0) continue;
        if (fread(block, 1, ADF_BLOCK_SIZE, f) != ADF_BLOCK_SIZE) continue;
        
        /* Check if this looks like a deleted file/dir header */
        if (!is_valid_header(block)) continue;
        
        int32_t secondary = read_be32_signed(block + ADF_BLOCK_SIZE - 4);
        
        uft_deleted_entry_t *entry = &entries[found];
        memset(entry, 0, sizeof(*entry));
        
        entry->header_block = blk;
        entry->is_directory = (secondary == ST_DIR);
        entry->parent_block = read_be32(block + ADF_BLOCK_SIZE - 12);
        
        if (!entry->is_directory) {
            entry->size = read_be32(block + ADF_BLOCK_SIZE - 188);
        }
        
        /* Read name from header */
        read_bcpl_name(block, ADF_BLOCK_SIZE - 80, entry->name, 256);
        
        /* Skip empty names */
        if (entry->name[0] == '\0') continue;
        
        /* Read date */
        uint32_t days = read_be32(block + ADF_BLOCK_SIZE - 92);
        entry->year = 1978 + days / 365;  /* Simplified */
        entry->month = 1;
        entry->day = 1;
        
        /* Analyze recoverability */
        entry->blocks_total = (entry->size + ADF_BLOCK_SIZE - 1) / ADF_BLOCK_SIZE;
        entry->blocks_recoverable = 0;
        entry->blocks_overwritten = 0;
        
        /* Quick check of data blocks */
        for (int i = 0; i < 72 && entry->blocks_recoverable < entry->blocks_total; i++) {
            uint32_t data_blk = read_be32(block + 308 - i * 4);
            if (data_blk == 0) break;
            
            if (bitmap[data_blk / 8] & (1 << (data_blk % 8))) {
                entry->blocks_recoverable++;
            } else {
                entry->blocks_overwritten++;
            }
        }
        
        /* Determine recoverability */
        if (entry->blocks_overwritten == 0) {
            entry->recoverability = UFT_ENTRY_RECOVERABLE;
            entry->recovery_confidence = 1.0;
        } else if (entry->blocks_recoverable > 0) {
            entry->recoverability = UFT_ENTRY_PARTIAL;
            entry->recovery_confidence = 
                (double)entry->blocks_recoverable / entry->blocks_total;
        } else {
            entry->recoverability = UFT_ENTRY_LOST;
            entry->recovery_confidence = 0.0;
        }
        
        found++;
    }
    
    free(bitmap);
    fclose(f);
    return found;
}

uft_recovery_status_t uft_adf_recover_file(const char *path,
                                           const uft_deleted_entry_t *entry,
                                           const char *output_path) {
    if (!path || !entry || !output_path) return UFT_RECOVERY_FAILED;
    if (entry->is_directory) return UFT_RECOVERY_FAILED;
    
    FILE *adf = fopen(path, "rb");
    if (!adf) return UFT_RECOVERY_FAILED;
    
    FILE *out = fopen(output_path, "wb");
    if (!out) {
        fclose(adf);
        return UFT_RECOVERY_FAILED;
    }
    
    /* Read header block */
    uint8_t header[ADF_BLOCK_SIZE];
    if (fseek(adf, entry->header_block * ADF_BLOCK_SIZE, SEEK_SET) != 0) return UFT_RECOVERY_FAILED;
    if (fread(header, 1, ADF_BLOCK_SIZE, adf) != ADF_BLOCK_SIZE) {
        fclose(adf);
        fclose(out);
        return UFT_RECOVERY_FAILED;
    }
    
    uint32_t bytes_left = entry->size;
    bool had_errors = false;
    
    /* Read data blocks from header (72 direct pointers) */
    for (int i = 0; i < 72 && bytes_left > 0; i++) {
        uint32_t data_blk = read_be32(header + 308 - i * 4);
        if (data_blk == 0) break;
        
        uint8_t data[ADF_BLOCK_SIZE];
        if (fseek(adf, data_blk * ADF_BLOCK_SIZE, SEEK_SET) != 0) continue;
        
        if (fread(data, 1, ADF_BLOCK_SIZE, adf) == ADF_BLOCK_SIZE) {
            /* OFS: data starts at offset 24, FFS: data starts at 0 */
            int data_offset = 24;  /* Assume OFS for deleted files */
            int data_size = ADF_BLOCK_SIZE - data_offset;
            
            if (bytes_left < (uint32_t)data_size) {
                data_size = bytes_left;
            }
            
            fwrite(data + data_offset, 1, data_size, out);
            bytes_left -= data_size;
        } else {
            /* Write zeros for missing blocks */
            uint8_t zeros[ADF_BLOCK_SIZE] = {0};
            int to_write = (bytes_left < ADF_BLOCK_SIZE) ? bytes_left : ADF_BLOCK_SIZE;
            fwrite(zeros, 1, to_write, out);
            bytes_left -= to_write;
            had_errors = true;
        }
    }
    
    /* Handle extension blocks for files > 72 blocks.
     * Extension block pointer is at offset 496 in file header/extension blocks.
     * Each extension block has 72 more data block pointers (same layout). */
    uint32_t ext_block = read_be32(header + 496);
    while (ext_block != 0 && bytes_left > 0) {
        uint8_t ext_buf[ADF_BLOCK_SIZE];
        if (fseek(adf, ext_block * ADF_BLOCK_SIZE, SEEK_SET) != 0) break;
        if (fread(ext_buf, 1, ADF_BLOCK_SIZE, adf) != ADF_BLOCK_SIZE) break;
        
        /* Verify it's an extension block (type = 16 at offset 0) */
        uint32_t block_type = read_be32(ext_buf);
        if (block_type != 16) break;  /* Not a valid extension */
        
        /* Read 72 data block pointers from extension (same layout as header) */
        for (int i = 0; i < 72 && bytes_left > 0; i++) {
            uint32_t data_blk = read_be32(ext_buf + 308 - i * 4);
            if (data_blk == 0) break;
            
            uint8_t data[ADF_BLOCK_SIZE];
            if (fseek(adf, data_blk * ADF_BLOCK_SIZE, SEEK_SET) != 0) continue;
            
            if (fread(data, 1, ADF_BLOCK_SIZE, adf) == ADF_BLOCK_SIZE) {
                int data_offset = 24;  /* OFS */
                int data_size = ADF_BLOCK_SIZE - data_offset;
                if (bytes_left < (uint32_t)data_size) data_size = bytes_left;
                fwrite(data + data_offset, 1, data_size, out);
                bytes_left -= data_size;
            } else {
                uint8_t zeros[ADF_BLOCK_SIZE] = {0};
                int to_write = (bytes_left < ADF_BLOCK_SIZE) ? bytes_left : ADF_BLOCK_SIZE;
                fwrite(zeros, 1, to_write, out);
                bytes_left -= to_write;
                had_errors = true;
            }
        }
        
        /* Chain to next extension block */
        ext_block = read_be32(ext_buf + 496);
    }
    
    fclose(adf);
    fclose(out);
    
    if (bytes_left > 0) {
        return UFT_RECOVERY_CHAIN_BROKEN;
    }
    
    return had_errors ? UFT_RECOVERY_PARTIAL : UFT_RECOVERY_OK;
}

int uft_adf_get_recovery_stats(const char *path, uft_recovery_stats_t *stats) {
    if (!path || !stats) return -1;
    
    memset(stats, 0, sizeof(*stats));
    
    uft_deleted_entry_t entries[256];
    int count = uft_adf_scan_deleted(path, entries, 256);
    
    if (count < 0) return -1;
    
    stats->total_entries = count;
    
    for (int i = 0; i < count; i++) {
        switch (entries[i].recoverability) {
            case UFT_ENTRY_RECOVERABLE:
                stats->recoverable_entries++;
                stats->total_bytes_recoverable += entries[i].size;
                break;
            case UFT_ENTRY_PARTIAL:
                stats->partial_entries++;
                stats->total_bytes_recoverable += 
                    (entries[i].size * entries[i].blocks_recoverable) / 
                    entries[i].blocks_total;
                break;
            case UFT_ENTRY_LOST:
                stats->lost_entries++;
                break;
        }
    }
    
    return 0;
}

const char* uft_recoverability_name(uft_entry_recoverability_t r) {
    switch (r) {
        case UFT_ENTRY_RECOVERABLE: return "Recoverable";
        case UFT_ENTRY_PARTIAL: return "Partial";
        case UFT_ENTRY_LOST: return "Lost";
        default: return "Unknown";
    }
}

const char* uft_recovery_status_name(uft_recovery_status_t s) {
    switch (s) {
        case UFT_RECOVERY_OK: return "OK";
        case UFT_RECOVERY_PARTIAL: return "Partial";
        case UFT_RECOVERY_OVERWRITTEN: return "Overwritten";
        case UFT_RECOVERY_CHAIN_BROKEN: return "Chain Broken";
        case UFT_RECOVERY_FAILED: return "Failed";
        default: return "Unknown";
    }
}
