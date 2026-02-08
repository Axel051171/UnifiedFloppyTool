/**
 * @file uft_amigados_bitmap.c
 * @brief AmigaDOS Bitmap Operations and Validation
 * 
 * Block allocation, bitmap management, filesystem validation.
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

/*===========================================================================
 * Bitmap Access
 *===========================================================================*/

/* Calculate which bitmap block and bit position for a given block number */
static void get_bitmap_position(const uft_amiga_ctx_t *ctx, uint32_t block_num,
                                size_t *bm_index, size_t *word_offset, int *bit_pos)
{
    /* Bitmap covers blocks starting from block 2 (after bootblock) */
    /* Each bitmap block covers 32 * 127 = 4064 blocks (127 longwords of data) */
    /* Bitmap data starts at offset 4 in bitmap block */
    
    uint32_t rel_block = block_num - 2;  /* Relative to start of data area */
    
    /* Which bitmap block? */
    *bm_index = rel_block / (32 * 127);
    uint32_t in_bm = rel_block % (32 * 127);
    
    /* Which longword in bitmap? (offset from start of bitmap data) */
    *word_offset = in_bm / 32;
    *bit_pos = 31 - (in_bm % 32);  /* Bits are numbered 31..0 in each word */
}

bool uft_amiga_is_block_free(const uft_amiga_ctx_t *ctx, uint32_t block_num)
{
    if (!ctx || !ctx->is_valid) return false;
    if (block_num < 2 || block_num >= ctx->total_blocks) return false;
    
    size_t bm_index, word_offset;
    int bit_pos;
    get_bitmap_position(ctx, block_num, &bm_index, &word_offset, &bit_pos);
    
    if (bm_index >= ctx->bitmap_count) return false;
    
    const uint8_t *bm = get_block_ptr(ctx, ctx->bitmap_blocks[bm_index]);
    if (!bm) return false;
    
    /* Bitmap data starts at offset 4 */
    uint32_t word = read_be32(bm + 4 + word_offset * 4);
    return (word & (1U << bit_pos)) != 0;  /* 1 = free, 0 = allocated */
}

static void set_block_bit(uft_amiga_ctx_t *ctx, uint32_t block_num, bool free)
{
    if (!ctx || !ctx->is_valid) return;
    if (block_num < 2 || block_num >= ctx->total_blocks) return;
    
    size_t bm_index, word_offset;
    int bit_pos;
    get_bitmap_position(ctx, block_num, &bm_index, &word_offset, &bit_pos);
    
    if (bm_index >= ctx->bitmap_count) return;
    
    uint8_t *bm = get_block_ptr_rw(ctx, ctx->bitmap_blocks[bm_index]);
    if (!bm) return;
    
    uint32_t word = read_be32(bm + 4 + word_offset * 4);
    
    if (free) {
        word |= (1U << bit_pos);   /* Set bit = free */
    } else {
        word &= ~(1U << bit_pos);  /* Clear bit = allocated */
    }
    
    write_be32(bm + 4 + word_offset * 4, word);
    
    /* Update bitmap block checksum */
    /* Bitmap checksum at offset 0 */
    write_be32(bm, 0);
    uint32_t sum = 0;
    for (int i = 0; i < UFT_AMIGA_BLOCK_SIZE; i += 4) {
        sum -= read_be32(bm + i);
    }
    write_be32(bm, sum);
}

uint32_t uft_amiga_alloc_block(uft_amiga_ctx_t *ctx, uint32_t preferred)
{
    if (!ctx || !ctx->is_valid) return 0;
    
    /* Start search from preferred or middle of disk */
    uint32_t start = preferred;
    if (start < 2 || start >= ctx->total_blocks) {
        start = ctx->root_block;
    }
    
    /* Search forward from start */
    for (uint32_t i = start; i < ctx->total_blocks; i++) {
        if (uft_amiga_is_block_free(ctx, i)) {
            set_block_bit(ctx, i, false);  /* Mark allocated */
            ctx->modified = true;
            return i;
        }
    }
    
    /* Search backward from start */
    for (uint32_t i = start; i >= 2; i--) {
        if (uft_amiga_is_block_free(ctx, i)) {
            set_block_bit(ctx, i, false);
            ctx->modified = true;
            return i;
        }
    }
    
    return 0;  /* No free blocks */
}

int uft_amiga_free_block(uft_amiga_ctx_t *ctx, uint32_t block_num)
{
    if (!ctx || !ctx->is_valid) return -1;
    if (block_num < 2 || block_num >= ctx->total_blocks) return -1;
    
    set_block_bit(ctx, block_num, true);  /* Mark free */
    ctx->modified = true;
    return 0;
}

size_t uft_amiga_alloc_blocks(uft_amiga_ctx_t *ctx, size_t count,
                              uint32_t *blocks)
{
    if (!ctx || !ctx->is_valid || !blocks || count == 0) return 0;
    
    size_t allocated = 0;
    uint32_t last = ctx->root_block;
    
    for (size_t i = 0; i < count; i++) {
        uint32_t blk = uft_amiga_alloc_block(ctx, last + 1);
        if (blk == 0) break;
        blocks[allocated++] = blk;
        last = blk;
    }
    
    return allocated;
}

/*===========================================================================
 * Bitmap Information
 *===========================================================================*/

int uft_amiga_get_bitmap_info(const uft_amiga_ctx_t *ctx,
                              uft_amiga_bitmap_info_t *info)
{
    if (!ctx || !info) return -1;
    
    memset(info, 0, sizeof(*info));
    info->total_blocks = ctx->total_blocks;
    
    /* Copy bitmap block list */
    for (size_t i = 0; i < ctx->bitmap_count && i < UFT_AMIGA_MAX_BITMAP_BLOCKS; i++) {
        info->bitmap_blocks[info->bitmap_count++] = ctx->bitmap_blocks[i];
    }
    
    /* Count free blocks */
    info->free_blocks = 0;
    
    for (size_t bm_idx = 0; bm_idx < ctx->bitmap_count; bm_idx++) {
        const uint8_t *bm = get_block_ptr(ctx, ctx->bitmap_blocks[bm_idx]);
        if (!bm) continue;
        
        /* Bitmap data at offset 4-508 (127 longwords) */
        for (int i = 4; i < 512; i += 4) {
            uint32_t word = read_be32(bm + i);
            /* Count set bits (popcount) */
            while (word) {
                info->free_blocks += (word & 1);
                word >>= 1;
            }
        }
    }
    
    /* Clamp to actual blocks */
    if (info->free_blocks > ctx->total_blocks - 2) {
        info->free_blocks = ctx->total_blocks - 2;
    }
    
    /* Reserved: bootblock (2) + root (1) + bitmap blocks */
    info->reserved_blocks = 2 + 1 + (uint32_t)ctx->bitmap_count;
    info->used_blocks = ctx->total_blocks - info->free_blocks;
    
    if (ctx->total_blocks > 0) {
        info->percent_used = (double)info->used_blocks / ctx->total_blocks * 100.0;
    }
    
    return 0;
}

/*===========================================================================
 * Validation
 *===========================================================================*/

static int add_validation_message(uft_amiga_validation_t *report,
                                  const char *msg, bool is_error)
{
    if (!report || !msg) return -1;
    
    if (report->message_count >= report->message_capacity) {
        size_t new_cap = report->message_capacity ? report->message_capacity * 2 : 64;
        char **new_msgs = realloc(report->messages, new_cap * sizeof(char *));
        if (!new_msgs) return -1;
        report->messages = new_msgs;
        report->message_capacity = new_cap;
    }
    
    report->messages[report->message_count] = strdup(msg);
    if (!report->messages[report->message_count]) return -1;
    report->message_count++;
    
    if (is_error) {
        report->errors++;
    } else {
        report->warnings++;
    }
    
    return 0;
}

/* Recursive validation helper */
typedef struct {
    uft_amiga_validation_t *report;
    uint8_t *block_usage;  /* 0=free, 1=used, 2+=cross-linked */
    const uft_amiga_ctx_t *ctx;
    int depth;
} validate_state_t;

static void validate_entry(validate_state_t *state, uint32_t block_num,
                           const char *path);

static void validate_dir(validate_state_t *state, uint32_t dir_block,
                         const char *path)
{
    if (state->depth > 100) return;  /* Prevent infinite recursion */
    
    const uint8_t *block = get_block_ptr(state->ctx, dir_block);
    if (!block) return;
    
    /* Check hash table entries */
    for (int hash = 0; hash < UFT_AMIGA_HASH_SIZE; hash++) {
        uint32_t entry_block = read_be32(block + 24 + hash * 4);
        
        int chain_len = 0;
        while (entry_block != 0 && entry_block < state->ctx->total_blocks) {
            if (++chain_len > 10000) {
                char msg[256];
                snprintf(msg, sizeof(msg),
                         "Infinite hash chain at %s (hash %d)", path, hash);
                add_validation_message(state->report, msg, true);
                break;
            }
            
            validate_entry(state, entry_block, path);
            
            const uint8_t *ent = get_block_ptr(state->ctx, entry_block);
            if (!ent) break;
            entry_block = read_be32(ent + 496);
        }
    }
}

static void validate_entry(validate_state_t *state, uint32_t block_num,
                           const char *parent_path)
{
    const uft_amiga_ctx_t *ctx = state->ctx;
    uft_amiga_validation_t *report = state->report;
    
    const uint8_t *block = get_block_ptr(ctx, block_num);
    if (!block) {
        char msg[256];
        snprintf(msg, sizeof(msg), "Invalid block %u", block_num);
        add_validation_message(report, msg, true);
        return;
    }
    
    /* Check block usage */
    if (state->block_usage[block_num] > 0) {
        report->cross_linked++;
        char msg[256];
        snprintf(msg, sizeof(msg), "Cross-linked block %u", block_num);
        add_validation_message(report, msg, true);
    }
    state->block_usage[block_num]++;
    
    /* Verify checksum */
    if (ctx->verify_checksums && !uft_amiga_verify_checksum(block)) {
        report->bad_checksums++;
        char msg[256];
        snprintf(msg, sizeof(msg), "Bad checksum at block %u", block_num);
        add_validation_message(report, msg, true);
    }
    
    /* Get entry info */
    uint32_t type = read_be32(block);
    int32_t sec_type = read_be32s(block + 508);
    
    if (type != UFT_AMIGA_T_SHORT) {
        char msg[256];
        snprintf(msg, sizeof(msg), "Invalid block type %u at block %u",
                 type, block_num);
        add_validation_message(report, msg, true);
        return;
    }
    
    /* Read name */
    char name[UFT_AMIGA_MAX_FILENAME_LFS + 1];
    size_t name_len = block[432];
    if (name_len > UFT_AMIGA_MAX_FILENAME_LFS) name_len = UFT_AMIGA_MAX_FILENAME_LFS;
    memcpy(name, block + 433, name_len);
    name[name_len] = '\0';
    
    /* Build full path */
    char full_path[UFT_AMIGA_MAX_PATH];
    snprintf(full_path, sizeof(full_path), "%s/%s",
             parent_path[0] ? parent_path : "", name);
    
    /* Process based on type */
    switch (sec_type) {
        case UFT_AMIGA_ST_USERDIR:
            report->dirs_found++;
            state->depth++;
            validate_dir(state, block_num, full_path);
            state->depth--;
            break;
            
        case UFT_AMIGA_ST_FILE:
            report->files_found++;
            /* Validate data chain */
            {
                uft_amiga_chain_t chain;
                if (uft_amiga_get_chain(ctx, block_num, &chain) == 0) {
                    for (size_t i = 0; i < chain.count; i++) {
                        uint32_t db = chain.blocks[i];
                        if (db >= ctx->total_blocks) {
                            report->broken_chains++;
                            char msg[256];
                            snprintf(msg, sizeof(msg),
                                     "Broken chain at %s (invalid block %u)",
                                     full_path, db);
                            add_validation_message(report, msg, true);
                        } else {
                            if (state->block_usage[db] > 0) {
                                report->cross_linked++;
                            }
                            state->block_usage[db]++;
                        }
                    }
                    uft_amiga_free_chain(&chain);
                } else {
                    report->broken_chains++;
                    char msg[256];
                    snprintf(msg, sizeof(msg), "Cannot read chain for %s", full_path);
                    add_validation_message(report, msg, true);
                }
            }
            break;
            
        case UFT_AMIGA_ST_SOFTLINK:
        case UFT_AMIGA_ST_LINKDIR:
        case UFT_AMIGA_ST_LINKFILE:
            report->links_found++;
            break;
            
        default:
            {
                char msg[256];
                snprintf(msg, sizeof(msg),
                         "Unknown secondary type %d at %s", sec_type, full_path);
                add_validation_message(report, msg, false);
            }
            break;
    }
}

int uft_amiga_validate(const uft_amiga_ctx_t *ctx, uft_amiga_validation_t *report)
{
    if (!ctx || !report) return -1;
    
    memset(report, 0, sizeof(*report));
    report->is_valid = true;
    
    if (!ctx->is_valid) {
        add_validation_message(report, "Filesystem not properly opened", true);
        report->is_valid = false;
        return -1;
    }
    
    /* Allocate block usage map */
    uint8_t *block_usage = calloc(ctx->total_blocks, 1);
    if (!block_usage) {
        add_validation_message(report, "Memory allocation failed", true);
        report->is_valid = false;
        return -1;
    }
    
    /* Mark reserved blocks */
    block_usage[0] = 1;  /* Boot block 0 */
    block_usage[1] = 1;  /* Boot block 1 */
    block_usage[ctx->root_block] = 1;  /* Root */
    
    for (size_t i = 0; i < ctx->bitmap_count; i++) {
        if (ctx->bitmap_blocks[i] < ctx->total_blocks) {
            block_usage[ctx->bitmap_blocks[i]] = 1;  /* Bitmap blocks */
        }
    }
    
    /* Verify bootblock */
    uint32_t boot_checksum = uft_amiga_bootblock_checksum(ctx->data);
    if (boot_checksum != 0) {
        /* Not necessarily an error - many disks have no bootblock */
        add_validation_message(report, "Bootblock checksum invalid (non-bootable)", false);
        report->bootblock_bad = true;
    }
    
    /* Verify root block */
    const uint8_t *root = get_block_ptr(ctx, ctx->root_block);
    if (!root || !uft_amiga_verify_checksum(root)) {
        add_validation_message(report, "Root block checksum invalid", true);
        report->root_bad = true;
        report->is_valid = false;
    }
    
    /* Validate directory tree */
    validate_state_t state = {
        .report = report,
        .block_usage = block_usage,
        .ctx = ctx,
        .depth = 0
    };
    
    validate_dir(&state, ctx->root_block, "");
    
    /* Check for orphan blocks (allocated in bitmap but not used) */
    for (uint32_t b = 2; b < ctx->total_blocks; b++) {
        bool bitmap_allocated = !uft_amiga_is_block_free(ctx, b);
        bool actually_used = (block_usage[b] > 0);
        
        if (bitmap_allocated && !actually_used) {
            report->orphan_blocks++;
        }
    }
    
    if (report->orphan_blocks > 0) {
        char msg[128];
        snprintf(msg, sizeof(msg), "%u orphan blocks found", report->orphan_blocks);
        add_validation_message(report, msg, false);
    }
    
    /* Check bitmap consistency */
    bool bitmap_mismatch = false;
    for (uint32_t b = 2; b < ctx->total_blocks; b++) {
        bool bitmap_allocated = !uft_amiga_is_block_free(ctx, b);
        bool actually_used = (block_usage[b] > 0);
        
        if (bitmap_allocated != actually_used) {
            bitmap_mismatch = true;
            break;
        }
    }
    
    if (bitmap_mismatch) {
        report->bitmap_corrupt = true;
        add_validation_message(report, "Bitmap inconsistent with actual usage", false);
    }
    
    free(block_usage);
    
    /* Overall validity */
    if (report->errors > 0) {
        report->is_valid = false;
    }
    
    return report->is_valid ? 0 : -1;
}

void uft_amiga_free_validation(uft_amiga_validation_t *report)
{
    if (!report) return;
    
    if (report->messages) {
        for (size_t i = 0; i < report->message_count; i++) {
            free(report->messages[i]);
        }
        free(report->messages);
    }
    
    memset(report, 0, sizeof(*report));
}

/*===========================================================================
 * Bitmap Repair
 *===========================================================================*/

int uft_amiga_fix_bitmap(uft_amiga_ctx_t *ctx)
{
    if (!ctx || !ctx->is_valid) return -1;
    
    /* Run validation to find issues */
    uft_amiga_validation_t report;
    uft_amiga_validate(ctx, &report);
    
    int fixes = 0;
    
    /* If bitmap is corrupt, rebuild it */
    if (report.bitmap_corrupt || report.orphan_blocks > 0) {
        fixes = uft_amiga_rebuild_bitmap(ctx);
    }
    
    uft_amiga_free_validation(&report);
    return fixes;
}

int uft_amiga_rebuild_bitmap(uft_amiga_ctx_t *ctx)
{
    if (!ctx || !ctx->is_valid) return -1;
    
    /* Allocate usage map */
    uint8_t *used = calloc(ctx->total_blocks, 1);
    if (!used) return -1;
    
    /* Mark reserved blocks */
    used[0] = 1;
    used[1] = 1;
    used[ctx->root_block] = 1;
    
    for (size_t i = 0; i < ctx->bitmap_count; i++) {
        used[ctx->bitmap_blocks[i]] = 1;
    }
    
    /* Scan directory tree to find all used blocks */
    /* Recursive scan from root block through hash table → file headers → data blocks */
    {
        /* Stack-based traversal to avoid deep recursion */
        uint32_t *stack = (uint32_t *)malloc(ctx->total_blocks * sizeof(uint32_t));
        if (stack) {
            int sp = 0;
            stack[sp++] = ctx->root_block;
            
            while (sp > 0 && sp < (int)ctx->total_blocks) {
                uint32_t blk = stack[--sp];
                if (blk < 2 || blk >= ctx->total_blocks || used[blk]) continue;
                used[blk] = 1;
                
                const uint8_t *block = get_block_ptr(ctx, blk);
                if (!block) continue;
                
                /* Read block type (offset 0) and secondary type (offset blocksize-4) */
                uint32_t primary = (block[0]<<24) | (block[1]<<16) | (block[2]<<8) | block[3];
                uint32_t secondary = (block[508]<<24) | (block[509]<<16) | (block[510]<<8) | block[511];
                
                if (primary == 2) {  /* T_HEADER */
                    if (secondary == 1 || (int32_t)secondary == 2) {
                        /* Root dir or user dir: scan hash table (offset 24, 72 entries) */
                        for (int h = 0; h < 72; h++) {
                            size_t off = 24 + h * 4;
                            uint32_t entry = (block[off]<<24) | (block[off+1]<<16) | 
                                             (block[off+2]<<8) | block[off+3];
                            if (entry >= 2 && entry < ctx->total_blocks && !used[entry]) {
                                stack[sp++] = entry;
                            }
                        }
                    }
                    
                    /* File header: scan 72 data block pointers (offset 308 downward) */
                    if ((int32_t)secondary == -3) {  /* ST_FILE */
                        for (int d = 0; d < 72; d++) {
                            size_t off = 308 - d * 4;
                            uint32_t data_blk = (block[off]<<24) | (block[off+1]<<16) | 
                                                (block[off+2]<<8) | block[off+3];
                            if (data_blk >= 2 && data_blk < ctx->total_blocks) {
                                used[data_blk] = 1;
                            }
                        }
                        /* Extension block */
                        uint32_t ext = (block[496]<<24) | (block[497]<<16) | 
                                       (block[498]<<8) | block[499];
                        if (ext >= 2 && ext < ctx->total_blocks && !used[ext]) {
                            stack[sp++] = ext;
                        }
                    }
                    
                    /* Hash chain: next entry in same hash bucket */
                    uint32_t hash_next = (block[496]<<24) | (block[497]<<16) | 
                                         (block[498]<<8) | block[499];
                    /* Actually hash chain is at offset 432 */
                    hash_next = (block[432]<<24) | (block[433]<<16) |
                                (block[434]<<8) | block[435];
                    if (hash_next >= 2 && hash_next < ctx->total_blocks && !used[hash_next]) {
                        stack[sp++] = hash_next;
                    }
                }
                
                if (primary == 16) {  /* T_LIST (extension block) */
                    /* 72 more data block pointers */
                    for (int d = 0; d < 72; d++) {
                        size_t off = 308 - d * 4;
                        uint32_t data_blk = (block[off]<<24) | (block[off+1]<<16) | 
                                            (block[off+2]<<8) | block[off+3];
                        if (data_blk >= 2 && data_blk < ctx->total_blocks) {
                            used[data_blk] = 1;
                        }
                    }
                    /* Chain to next extension */
                    uint32_t next_ext = (block[496]<<24) | (block[497]<<16) | 
                                        (block[498]<<8) | block[499];
                    if (next_ext >= 2 && next_ext < ctx->total_blocks && !used[next_ext]) {
                        stack[sp++] = next_ext;
                    }
                }
            }
            free(stack);
        }
    }
    
    /* Initialize all bitmap blocks to all-free */
    for (size_t bm_idx = 0; bm_idx < ctx->bitmap_count; bm_idx++) {
        uint8_t *bm = get_block_ptr_rw(ctx, ctx->bitmap_blocks[bm_idx]);
        if (!bm) continue;
        
        /* Set all bits to 1 (free) */
        memset(bm + 4, 0xFF, 508);
    }
    
    /* Mark used blocks as allocated */
    for (uint32_t b = 0; b < ctx->total_blocks; b++) {
        if (used[b]) {
            set_block_bit(ctx, b, false);  /* Allocated */
        }
    }
    
    free(used);
    ctx->modified = true;
    return 0;
}

/*===========================================================================
 * Formatting
 *===========================================================================*/

int uft_amiga_format(uft_amiga_ctx_t *ctx, uft_amiga_fs_type_t fs_type,
                     const char *volume_name)
{
    if (!ctx || !ctx->data || !volume_name) return -1;
    
    /* Clear entire image */
    memset(ctx->data, 0, ctx->size);
    
    /* Set filesystem type */
    ctx->fs_type = fs_type;
    ctx->is_ffs = (fs_type & 0x01) != 0;
    ctx->is_intl = (fs_type >= 2 && fs_type <= 5) || (fs_type >= 6);
    ctx->is_dircache = (fs_type == 4 || fs_type == 5);
    ctx->is_longnames = (fs_type >= 6);
    
    /* Write bootblock */
    ctx->data[0] = 'D';
    ctx->data[1] = 'O';
    ctx->data[2] = 'S';
    ctx->data[3] = (uint8_t)fs_type;
    
    /* Root block in middle */
    ctx->root_block = ctx->total_blocks / 2;
    
    /* Calculate bitmap blocks needed */
    uint32_t blocks_per_bitmap = 32 * 127;  /* Bits per bitmap block */
    ctx->bitmap_count = (ctx->total_blocks + blocks_per_bitmap - 1) / blocks_per_bitmap;
    if (ctx->bitmap_count > UFT_AMIGA_MAX_BITMAP_BLOCKS) {
        ctx->bitmap_count = UFT_AMIGA_MAX_BITMAP_BLOCKS;
    }
    
    /* Place bitmap blocks after root */
    for (size_t i = 0; i < ctx->bitmap_count; i++) {
        ctx->bitmap_blocks[i] = ctx->root_block + 1 + (uint32_t)i;
    }
    
    /* Initialize root block */
    uint8_t *root = get_block_ptr_rw(ctx, ctx->root_block);
    
    write_be32(root + 0, UFT_AMIGA_T_SHORT);  /* Type */
    write_be32(root + 8, UFT_AMIGA_HASH_SIZE);  /* Hash table size */
    /* Hash table at 24-308 is all zeros (empty) */
    
    /* Bitmap flag */
    write_be32(root + 312, 0xFFFFFFFF);  /* bm_flag: bitmap is valid */
    
    /* Bitmap block pointers at 316-392 */
    for (size_t i = 0; i < ctx->bitmap_count; i++) {
        write_be32(root + 316 + i * 4, ctx->bitmap_blocks[i]);
    }
    
    /* Timestamps */
    uint32_t days, mins, ticks;
    uft_amiga_from_unix_time(time(NULL), &days, &mins, &ticks);
    
    /* Last modified */
    write_be32(root + 420, days);
    write_be32(root + 424, mins);
    write_be32(root + 428, ticks);
    
    /* Volume name at 432 */
    size_t name_len = strlen(volume_name);
    if (name_len > UFT_AMIGA_MAX_FILENAME) name_len = UFT_AMIGA_MAX_FILENAME;
    root[432] = (uint8_t)name_len;
    memcpy(root + 433, volume_name, name_len);
    
    /* Creation date at 484-496 */
    write_be32(root + 484, days);
    write_be32(root + 488, mins);
    write_be32(root + 492, ticks);
    
    write_be32(root + 508, UFT_AMIGA_ST_ROOT);  /* Secondary type */
    
    uft_amiga_update_checksum(root);
    
    /* Initialize bitmap blocks - all free except reserved */
    for (size_t bm_idx = 0; bm_idx < ctx->bitmap_count; bm_idx++) {
        uint8_t *bm = get_block_ptr_rw(ctx, ctx->bitmap_blocks[bm_idx]);
        
        /* All 1s = all free */
        memset(bm + 4, 0xFF, 508);
        
        /* Update checksum */
        write_be32(bm, 0);
        uint32_t sum = 0;
        for (int i = 0; i < UFT_AMIGA_BLOCK_SIZE; i += 4) {
            sum -= read_be32(bm + i);
        }
        write_be32(bm, sum);
    }
    
    /* Mark reserved blocks as used */
    set_block_bit(ctx, 0, false);  /* Boot 0 */
    set_block_bit(ctx, 1, false);  /* Boot 1 */
    set_block_bit(ctx, ctx->root_block, false);
    
    for (size_t i = 0; i < ctx->bitmap_count; i++) {
        set_block_bit(ctx, ctx->bitmap_blocks[i], false);
    }
    
    /* Copy volume name */
    strncpy(ctx->volume_name, volume_name, UFT_AMIGA_MAX_FILENAME_LFS);
    ctx->volume_name[UFT_AMIGA_MAX_FILENAME_LFS] = '\0';
    
    ctx->is_valid = true;
    ctx->modified = true;
    
    return 0;
}

int uft_amiga_create_adf(const char *filename, bool is_hd,
                         uft_amiga_fs_type_t fs_type, const char *volume_name)
{
    if (!filename || !volume_name) return -1;
    
    size_t size = is_hd ? UFT_AMIGA_HD_SIZE : UFT_AMIGA_DD_SIZE;
    
    uft_amiga_ctx_t *ctx = uft_amiga_create();
    if (!ctx) return -1;
    
    ctx->data = calloc(1, size);
    if (!ctx->data) {
        uft_amiga_destroy(ctx);
        return -1;
    }
    
    ctx->size = size;
    ctx->owns_data = true;
    ctx->total_blocks = (uint32_t)(size / UFT_AMIGA_BLOCK_SIZE);
    
    int ret = uft_amiga_format(ctx, fs_type, volume_name);
    if (ret != 0) {
        uft_amiga_destroy(ctx);
        return ret;
    }
    
    ret = uft_amiga_save(ctx, filename);
    uft_amiga_destroy(ctx);
    
    return ret;
}
