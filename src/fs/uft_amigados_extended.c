/**
 * @file uft_amigados_extended.c
 * @brief Extended AmigaDOS Operations.
 *
 * Partial implementation: uft_amiga_validate_ext performs REAL
 * validation (bootblock via T2 scanner, root-block checksum,
 * bitmap-block checksum). uft_amiga_repair_bitmap / _salvage /
 * _lha_* / _batch_* / _scan_directory remain unimplemented — they
 * return UFT_ERR_NOT_IMPLEMENTED instead of silently pretending to
 * work (Prinzip 1).
 *
 * Part of MASTER_PLAN.md M2 T3. See docs/XCOPY_INTEGRATION_TODO.md.
 */

#include "uft/fs/uft_amigados_extended.h"
#include "uft/fs/uft_amigados.h"
#include "uft/fs/uft_bootblock_scanner.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifndef UFT_AMIGA_BLOCK_SIZE
#define UFT_AMIGA_BLOCK_SIZE  512u
#endif

/* Read 32 bits big-endian from any 4 bytes. */
static inline uint32_t amiga_rd_be32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] <<  8) |  (uint32_t)p[3];
}

/*
 * Standard AmigaDOS block checksum: sum all 128 big-endian u32 longs.
 * For a valid block the sum (mod 2^32) is 0. The stored checksum is
 * at offset 20 (word index 5) in root/header blocks.
 * The bootblock uses a different algorithm (add-with-carry-round) —
 * see T2's uft_bb_compute_checksum.
 */
static bool amiga_block_checksum_ok(const uint8_t *block) {
    uint32_t sum = 0;
    for (size_t i = 0; i < UFT_AMIGA_BLOCK_SIZE; i += 4) {
        sum += amiga_rd_be32(block + i);
    }
    return sum == 0;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Directory Walker (M2 T3 — partial DiskSalv-style structural validator)
 *
 * The walker traverses the AmigaDOS hash-table tree starting at the root
 * block. For every header block it touches it verifies the checksum and
 * follows hash chains (per-bucket linked list of files with the same hash).
 * Files: header-block checksum verified, data-block chain NOT walked (that
 * needs extension-block traversal — left for the DiskSalv follow-up T7).
 *
 * AmigaDOS block layout used here (DD floppy, 512-byte blocks):
 *   off 0   : type      (T_HEADER = 2 for headers)
 *   off 12  : hash_size (72 for root/dir headers)
 *   off 24..: hash[72]  (block numbers, 0 = empty slot)
 *   off 496 : next_hash (next entry in same hash chain, 0 = end)
 *   off 508 : sec_type  (ST_ROOT=1, ST_USERDIR=2, ST_FILE=-3)
 *
 * Cycle detection via a visited-blocks bitmap. Depth cap = 32 (AmigaDOS
 * convention; deeper trees are pathological/malicious).
 * ═══════════════════════════════════════════════════════════════════════════════ */

/* UFT_AMIGA_T_SHORT + UFT_AMIGA_ST_{ROOT,USERDIR,FILE} are already
 * defined in uft/fs/uft_amigados.h — reuse those. We just add the
 * walker-specific layout constants here. */
#define UFT_AMIGA_HASH_TABLE_SZ  72u
#define UFT_AMIGA_NEXT_HASH_OFF  496u
#define UFT_AMIGA_SEC_TYPE_OFF   508u
#define UFT_AMIGA_HASH_OFF       24u
#define UFT_AMIGA_HIGH_SEQ_OFF   8u   /* file: # of data blocks; ext: same */
#define UFT_AMIGA_DATA_BLK_OFF   24u  /* file/ext: data_blocks[0..71] */
#define UFT_AMIGA_DATA_BLK_MAX   72u
#define UFT_AMIGA_EXTENSION_OFF  504u /* file/ext: next extension block */
#define UFT_AMIGA_MAX_WALK_DEPTH 32
#define UFT_AMIGA_MAX_EXT_DEPTH  256  /* very large file = ~9MB OFS */

typedef struct {
    uint8_t *visited;        /* bitmap: 1 bit per block (header OR data) */
    uint8_t *bam_used;       /* bitmap: BAM-says-used (1) (allocated lazily) */
    size_t   total_blocks;
    size_t   dirs_visited;
    size_t   files_visited;
    size_t   data_blocks;    /* file data blocks reached */
    size_t   extension_blks; /* T_LIST extension blocks reached */
    int      bad_checksums;  /* delta to add to result->bad_checksums */
    int      bad_types;      /* unexpected type/sec_type — delta to error_count */
    int      cycles;         /* header re-visited or data-block reused */
    int      bad_data_ptr;   /* file pointed at out-of-range / zero data block */
} amiga_walk_state_t;

/* Forward decls — walker, file-data walker, extension walker, BAM reconcile. */
static void amiga_walk(uft_amigados_ctx_t *ctx, amiga_walk_state_t *st,
                        uint32_t block_num, int depth);
static void amiga_walk_file_data(uft_amigados_ctx_t *ctx, amiga_walk_state_t *st,
                                  const uint8_t *block, int depth);
static void amiga_walk_extension(uft_amigados_ctx_t *ctx, amiga_walk_state_t *st,
                                  uint32_t block_num, int depth, int ext_depth);

static inline bool amiga_visited_test(const amiga_walk_state_t *st, size_t blk) {
    if (blk >= st->total_blocks) return true;  /* out-of-range ⇒ pretend visited */
    return (st->visited[blk >> 3] & (1u << (blk & 7))) != 0;
}

static inline void amiga_visited_set(amiga_walk_state_t *st, size_t blk) {
    if (blk < st->total_blocks) st->visited[blk >> 3] |= (1u << (blk & 7));
}

static void amiga_walk(uft_amigados_ctx_t *ctx,
                        amiga_walk_state_t *st,
                        uint32_t block_num,
                        int depth) {
    if (depth > UFT_AMIGA_MAX_WALK_DEPTH) {
        st->bad_types++;
        return;
    }
    if (block_num == 0) return;                /* end of chain */
    if (block_num >= st->total_blocks) {
        st->bad_types++;
        return;
    }
    if (amiga_visited_test(st, block_num)) {
        st->cycles++;
        return;
    }
    amiga_visited_set(st, block_num);

    size_t off = (size_t)block_num * UFT_AMIGA_BLOCK_SIZE;
    if (off + UFT_AMIGA_BLOCK_SIZE > ctx->size) {
        st->bad_types++;
        return;
    }
    const uint8_t *block = ctx->data + off;

    if (!amiga_block_checksum_ok(block)) {
        st->bad_checksums++;
        return;  /* corrupt — don't follow further from this block */
    }

    uint32_t type     = amiga_rd_be32(block);
    int32_t  sec_type = (int32_t)amiga_rd_be32(block + UFT_AMIGA_SEC_TYPE_OFF);

    if (type != UFT_AMIGA_T_SHORT) {
        st->bad_types++;
        return;
    }

    if (sec_type == UFT_AMIGA_ST_ROOT || sec_type == UFT_AMIGA_ST_USERDIR) {
        st->dirs_visited++;
        /* Walk hash table — 72 buckets, each may chain via next_hash. */
        for (uint32_t i = 0; i < UFT_AMIGA_HASH_TABLE_SZ; i++) {
            uint32_t slot = amiga_rd_be32(block + UFT_AMIGA_HASH_OFF + i * 4);
            uint32_t cur = slot;
            int chain_depth = 0;
            while (cur != 0 && chain_depth < 4096) {  /* huge chain = pathological */
                amiga_walk(ctx, st, cur, depth + 1);
                /* Read next_hash from the block we just visited. */
                if (cur >= st->total_blocks) break;
                size_t cur_off = (size_t)cur * UFT_AMIGA_BLOCK_SIZE;
                if (cur_off + UFT_AMIGA_BLOCK_SIZE > ctx->size) break;
                cur = amiga_rd_be32(ctx->data + cur_off + UFT_AMIGA_NEXT_HASH_OFF);
                chain_depth++;
            }
            if (chain_depth >= 4096) st->bad_types++;
        }
    } else if (sec_type == UFT_AMIGA_ST_FILE) {
        st->files_visited++;
        /* File-header checksum already verified above. Walk the data-
         * block table + extension chain. We don't validate OFS data-
         * block headers here (T_DATA=8 + checksum + own seq) — that
         * adds significant per-block I/O and is the next refinement. */
        amiga_walk_file_data(ctx, st, block, depth);
    } else {
        /* Unknown sec_type. */
        st->bad_types++;
    }
}

/* Walk the data-block table of a file or T_LIST extension block.
 * `block` points at the file/extension header; high_seq tells us how
 * many of the 72 slots are valid. Each slot is a block number. We mark
 * each in `visited` (a re-visit means the data block was claimed by
 * two files = corruption or deliberate cross-link). After processing
 * the inline table, follow the extension pointer at offset 504 if
 * non-zero, recursing into amiga_walk_extension. */
static void amiga_walk_file_data(uft_amigados_ctx_t *ctx,
                                  amiga_walk_state_t *st,
                                  const uint8_t *block,
                                  int depth) {
    uint32_t high_seq = amiga_rd_be32(block + UFT_AMIGA_HIGH_SEQ_OFF);
    if (high_seq > UFT_AMIGA_DATA_BLK_MAX) {
        st->bad_data_ptr++;
        high_seq = UFT_AMIGA_DATA_BLK_MAX;
    }

    /* AmigaDOS quirk: data_blocks[] is stored REVERSED — slot 0 is the
     * LAST data block in the file. We don't care about order for
     * reachability; we just visit each non-zero entry. */
    for (uint32_t i = 0; i < high_seq; i++) {
        uint32_t blk = amiga_rd_be32(block + UFT_AMIGA_DATA_BLK_OFF + i * 4);
        if (blk == 0 || blk >= st->total_blocks) {
            if (blk != 0) st->bad_data_ptr++;
            continue;
        }
        if (amiga_visited_test(st, blk)) {
            st->cycles++;  /* data block claimed twice */
            continue;
        }
        amiga_visited_set(st, blk);
        st->data_blocks++;
    }

    uint32_t ext = amiga_rd_be32(block + UFT_AMIGA_EXTENSION_OFF);
    if (ext != 0) amiga_walk_extension(ctx, st, ext, depth + 1, 0);
}

static void amiga_walk_extension(uft_amigados_ctx_t *ctx,
                                  amiga_walk_state_t *st,
                                  uint32_t block_num,
                                  int depth,
                                  int ext_depth) {
    if (ext_depth > UFT_AMIGA_MAX_EXT_DEPTH) {
        st->bad_data_ptr++;
        return;
    }
    if (block_num == 0 || block_num >= st->total_blocks) {
        if (block_num != 0) st->bad_data_ptr++;
        return;
    }
    if (amiga_visited_test(st, block_num)) {
        st->cycles++;
        return;
    }
    amiga_visited_set(st, block_num);

    size_t off = (size_t)block_num * UFT_AMIGA_BLOCK_SIZE;
    if (off + UFT_AMIGA_BLOCK_SIZE > ctx->size) {
        st->bad_data_ptr++;
        return;
    }
    const uint8_t *block = ctx->data + off;

    if (!amiga_block_checksum_ok(block)) {
        st->bad_checksums++;
        return;
    }

    /* T_LIST has type=16, sec_type same as the parent file (-3). We
     * accept the block as long as type is T_LIST. */
    uint32_t type = amiga_rd_be32(block);
    if (type != UFT_AMIGA_T_LIST) {
        st->bad_types++;
        return;
    }
    st->extension_blks++;
    /* Same data-block-table layout as file headers — reuse the walker. */
    amiga_walk_file_data(ctx, st, block, depth);
    (void)depth;  /* not used past here, but kept for symmetry */
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * BAM Reconciliation (M2 T7 partial)
 *
 * Compare the directory-tree reachability bitmap (`visited`) against the
 * BAM "free/used" map. Per AmigaDOS spec, BAM bits are 1 = FREE, 0 = USED.
 * Each BAM block holds:
 *   off 0..3: checksum
 *   off 4..511: 127 longwords × 32 bits = 4064 bits, covering 4064 blocks
 * The bitmap covers blocks starting at block 2 (boot blocks 0,1 are
 * permanent reserved and not in the BAM). bm_pages[] in the root block
 * lists which blocks hold BAM data.
 *
 * Discrepancies:
 *   "header found but BAM-marked free" → BAM is lying (count as bad_bam)
 *   "BAM says used but unreachable from root" → orphan / deleted-file
 *      data still on disk = forensically valuable, count as orphan_count
 * ═══════════════════════════════════════════════════════════════════════════════ */

static bool amiga_load_bam(const uft_amigados_ctx_t *ctx,
                            amiga_walk_state_t *st) {
    /* Allocate output bitmap if not already. */
    if (!st->bam_used) {
        st->bam_used = calloc((st->total_blocks + 7) / 8, 1);
        if (!st->bam_used) return false;
    }
    /* Boot blocks 0,1 are permanent reserved — mark them used. */
    if (st->total_blocks >= 1) st->bam_used[0] |= 0x01;
    if (st->total_blocks >= 2) st->bam_used[0] |= 0x02;

    if (ctx->bitmap_count == 0) return false;

    /* Each BAM block holds 4064 bits, starting at block 2. */
    uint32_t logical_block = 2;
    for (size_t bi = 0; bi < ctx->bitmap_count; bi++) {
        uint32_t bam_blk = ctx->bitmap_blocks[bi];
        if (bam_blk == 0 || bam_blk >= st->total_blocks) continue;
        size_t bam_off = (size_t)bam_blk * UFT_AMIGA_BLOCK_SIZE;
        if (bam_off + UFT_AMIGA_BLOCK_SIZE > ctx->size) continue;
        const uint8_t *bam_block = ctx->data + bam_off;

        /* 127 longwords of bitmap data starting at offset 4. */
        for (uint32_t w = 0; w < 127; w++) {
            uint32_t bits = amiga_rd_be32(bam_block + 4 + w * 4);
            for (uint32_t b = 0; b < 32; b++) {
                if (logical_block >= st->total_blocks) break;
                /* AmigaDOS: bit set (1) = FREE, bit clear (0) = USED.
                 * Walk lowest-bit first within each longword. */
                bool free = (bits & (1u << b)) != 0;
                if (!free) {
                    st->bam_used[logical_block >> 3] |=
                        (uint8_t)(1u << (logical_block & 7));
                }
                logical_block++;
            }
            if (logical_block >= st->total_blocks) break;
        }
        if (logical_block >= st->total_blocks) break;
    }
    return true;
}

static void amiga_reconcile_bam(const amiga_walk_state_t *st,
                                  size_t *out_bad_bam,
                                  size_t *out_orphan_blocks) {
    /* bad_bam: header reachable from root but BAM says free.
     * orphan: BAM says used but block not reached from root.
     * Boot blocks 0,1 are excluded (permanent reserved, never in walker). */
    size_t bad = 0, orphan = 0;
    for (size_t blk = 2; blk < st->total_blocks; blk++) {
        bool reached  = (st->visited[blk >> 3] & (1u << (blk & 7))) != 0;
        bool bam_used = st->bam_used &&
                        (st->bam_used[blk >> 3] & (1u << (blk & 7))) != 0;
        if (reached && !bam_used) bad++;
        else if (bam_used && !reached) orphan++;
    }
    *out_bad_bam = bad;
    *out_orphan_blocks = orphan;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Volume Operations
 * ═══════════════════════════════════════════════════════════════════════════════ */

int uft_amiga_get_volume_info(uft_amigados_ctx_t *ctx, 
                               uft_amiga_volume_info_t *info) {
    if (!ctx || !info) return -1;
    
    memset(info, 0, sizeof(*info));
    
    /* Get volume name from context */
    if (ctx->volume_name[0]) {
        strncpy(info->name, ctx->volume_name, sizeof(info->name) - 1);
    }
    
    /* Filesystem type from ctx->fs_type */
    info->dos_type = (uint32_t)ctx->fs_type;
    
    const char *fs_names[] = {"OFS", "FFS", "OFS-INTL", "FFS-INTL", 
                               "OFS-DC", "FFS-DC", "LNFS", "LNFS2"};
    if (ctx->fs_type < 8) {
        strncpy(info->fs_type, fs_names[ctx->fs_type], sizeof(info->fs_type) - 1);
    }
    
    /* Block statistics */
    info->total_blocks = ctx->total_blocks;
    info->used_blocks = 0;  /* Would need bitmap scan */
    info->free_blocks = ctx->total_blocks;
    
    /* Size calculations */
    info->total_bytes = (uint64_t)info->total_blocks * UFT_AMIGA_BLOCK_SIZE;
    
    /* Dates from context */
    info->creation_date = ctx->creation_date;
    
    return 0;
}

int uft_amiga_relabel(uft_amigados_ctx_t *ctx, const char *new_name) {
    if (!ctx || !new_name) return -1;
    if (strlen(new_name) > 30) return -2;
    
    /* Just update context - actual disk write requires full implementation */
    strncpy(ctx->volume_name, new_name, sizeof(ctx->volume_name) - 1);
    
    return 0;  /* Note: Does not persist to disk in stub version */
}

int uft_amiga_format_ext(uft_amigados_ctx_t *ctx, const char *name, 
                         uint8_t dos_type, bool install_boot) {
    (void)ctx; (void)name; (void)dos_type; (void)install_boot;
    return -1;  /* Use uft_amiga_format() from base module */
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Pattern Matching - Stubs
 * ═══════════════════════════════════════════════════════════════════════════════ */

int uft_amiga_find_pattern(uft_amigados_ctx_t *ctx,
                            const char *pattern,
                            bool recursive,
                            uft_amiga_match_cb callback,
                            void *user_data) {
    (void)ctx; (void)pattern; (void)recursive; (void)callback; (void)user_data;
    return -1;  /* Not implemented */
}

int uft_amiga_delete_pattern(uft_amigados_ctx_t *ctx,
                              const char *pattern,
                              bool recursive) {
    (void)ctx; (void)pattern; (void)recursive;
    return -1;  /* Not implemented */
}

int uft_amiga_list_all_ext(uft_amigados_ctx_t *ctx,
                            uft_amiga_match_cb callback,
                            void *user_data) {
    (void)ctx; (void)callback; (void)user_data;
    return -1;  /* Not implemented */
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Pack/Unpack - Stubs
 * ═══════════════════════════════════════════════════════════════════════════════ */

int uft_amiga_unpack_to_host(uft_amigados_ctx_t *ctx,
                              const char *amiga_path,
                              const char *host_path,
                              const uft_amiga_pack_opts_t *opts) {
    (void)ctx; (void)amiga_path; (void)host_path; (void)opts;
    return -1;  /* Not implemented */
}

int uft_amiga_pack_from_host(uft_amigados_ctx_t *ctx,
                              const char *host_path,
                              const char *amiga_path,
                              const uft_amiga_pack_opts_t *opts) {
    (void)ctx; (void)host_path; (void)amiga_path; (void)opts;
    return -1;  /* Not implemented */
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Bootblock - Stubs
 * ═══════════════════════════════════════════════════════════════════════════════ */

int uft_amiga_read_bootblock_ext(uft_amigados_ctx_t *ctx, 
                                  uft_amiga_bootblock_t *boot) {
    if (!ctx || !boot) return -1;
    
    memset(boot, 0, sizeof(*boot));
    
    /* Stub implementation - would need disk access functions */
    boot->valid = false;
    boot->dos_type = 0;
    
    return -1;  /* Not implemented in stub */
}

int uft_amiga_install_bootblock(uft_amigados_ctx_t *ctx, uint32_t dos_type) {
    (void)ctx; (void)dos_type;
    return -1;  /* Not implemented */
}

int uft_amiga_clear_bootblock(uft_amigados_ctx_t *ctx) {
    (void)ctx;
    return -1;  /* Not implemented */
}

int uft_amiga_check_boot_virus(const uft_amiga_bootblock_t *boot) {
    (void)boot;
    return 0;  /* No virus detected (stub) */
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Validation - Stubs
 * ═══════════════════════════════════════════════════════════════════════════════ */

int uft_amiga_validate_ext(uft_amigados_ctx_t *ctx,
                            uft_amiga_validate_result_t *result) {
    if (!ctx || !result) return -1;
    if (!ctx->data || ctx->size < 2 * UFT_AMIGA_BLOCK_SIZE) return -1;

    memset(result, 0, sizeof(*result));

    /* --- 1. Bootblock check via T2 scanner --- */
    uft_bb_scan_result_t bb_result;
    if (uft_bb_scan(ctx->data, UFT_BOOTBLOCK_SIZE, &bb_result) == UFT_OK) {
        if (bb_result.dos_type == UFT_BB_DOS_UNKNOWN) {
            result->error_count++;
            snprintf(result->details + strlen(result->details),
                     sizeof(result->details) - strlen(result->details),
                     "Bootblock: unknown DOS type (raw 0x%08X)\n",
                     bb_result.dos_type_raw);
        }
        if (!bb_result.checksum_ok && !bb_result.all_zeros) {
            result->bad_checksums++;
            snprintf(result->details + strlen(result->details),
                     sizeof(result->details) - strlen(result->details),
                     "Bootblock: checksum mismatch (stored 0x%08X, "
                     "computed 0x%08X)\n",
                     bb_result.stored_checksum, bb_result.computed_checksum);
        }
        if (bb_result.match_count > 0) {
            result->warning_count++;
            snprintf(result->details + strlen(result->details),
                     sizeof(result->details) - strlen(result->details),
                     "Bootblock: %zu virus signature match(es) — %s\n",
                     bb_result.match_count,
                     bb_result.matches[0].virus_name);
        }
    }

    /* --- 2. Root-block checksum --- */
    size_t rb_off = (size_t)ctx->root_block * UFT_AMIGA_BLOCK_SIZE;
    if (ctx->root_block > 0 && rb_off + UFT_AMIGA_BLOCK_SIZE <= ctx->size) {
        if (amiga_block_checksum_ok(ctx->data + rb_off)) {
            result->rootblock_valid = true;
        } else {
            result->bad_checksums++;
            snprintf(result->details + strlen(result->details),
                     sizeof(result->details) - strlen(result->details),
                     "Root block %u: checksum mismatch\n", ctx->root_block);
        }
    } else if (ctx->root_block == 0) {
        /* Default DD root block is 880. */
        size_t fallback = 880u * UFT_AMIGA_BLOCK_SIZE;
        if (fallback + UFT_AMIGA_BLOCK_SIZE <= ctx->size &&
            amiga_block_checksum_ok(ctx->data + fallback)) {
            result->rootblock_valid = true;
        }
    }

    /* --- 3. Bitmap blocks checksum --- */
    if (ctx->bitmap_count > 0) {
        size_t bad = 0;
        for (size_t i = 0; i < ctx->bitmap_count; i++) {
            size_t off = (size_t)ctx->bitmap_blocks[i] * UFT_AMIGA_BLOCK_SIZE;
            if (off + UFT_AMIGA_BLOCK_SIZE > ctx->size) continue;
            if (!amiga_block_checksum_ok(ctx->data + off)) {
                bad++;
                result->bad_checksums++;
            }
        }
        result->bitmap_valid = (bad == 0);
        if (bad > 0) {
            snprintf(result->details + strlen(result->details),
                     sizeof(result->details) - strlen(result->details),
                     "Bitmap: %zu of %zu block(s) with bad checksum\n",
                     bad, ctx->bitmap_count);
        }
    }

    /* --- 4. Directory + file-header chain walk (T3 partial DiskSalv) --- */
    if (result->rootblock_valid && ctx->root_block > 0) {
        size_t total_blocks = ctx->size / UFT_AMIGA_BLOCK_SIZE;
        amiga_walk_state_t st = {0};
        st.total_blocks = total_blocks;
        st.visited = calloc((total_blocks + 7) / 8, 1);

        if (st.visited) {
            amiga_walk(ctx, &st, ctx->root_block, 0);

            result->bad_checksums   += st.bad_checksums;
            result->error_count     += st.bad_types + st.cycles + st.bad_data_ptr;
            result->directory_valid  = (st.bad_checksums == 0 &&
                                        st.bad_types == 0 &&
                                        st.cycles == 0);
            /* Files are valid if every reachable file-header + data-block
             * pointer checksum-passed and no walker-error fired. After the
             * T7-partial-2 commit, this also covers the file-data-block
             * table + extension-block chain. OFS internal data-block
             * checksums (T_DATA=8 with own checksum) are still NOT
             * validated — that's the next refinement. */
            result->files_valid      = result->directory_valid &&
                                       (st.bad_data_ptr == 0);

            if (st.bad_checksums || st.bad_types || st.cycles ||
                st.bad_data_ptr) {
                snprintf(result->details + strlen(result->details),
                         sizeof(result->details) - strlen(result->details),
                         "DirWalk: %zu dir + %zu file + %zu ext + "
                         "%zu data-blocks; bad-checksums=%d bad-types=%d "
                         "cycles=%d bad-data-ptr=%d\n",
                         st.dirs_visited, st.files_visited,
                         st.extension_blks, st.data_blocks,
                         st.bad_checksums, st.bad_types, st.cycles,
                         st.bad_data_ptr);
            }

            /* --- 5. BAM reconciliation (T7 partial #2) --- */
            if (amiga_load_bam(ctx, &st)) {
                size_t bad_bam = 0, orphan = 0;
                amiga_reconcile_bam(&st, &bad_bam, &orphan);
                if (bad_bam) {
                    /* Real corruption: header reachable but BAM says free.
                     * If BAM allocator runs, that block could be reused
                     * and overwrite live-file data. */
                    result->error_count += (int)bad_bam;
                    snprintf(result->details + strlen(result->details),
                             sizeof(result->details) - strlen(result->details),
                             "BAM: %zu reachable block(s) marked FREE — "
                             "data-loss risk if disk gets writes\n",
                             bad_bam);
                }
                if (orphan) {
                    /* Forensically valuable: deleted-file content still
                     * lives on disk. Warning, not error. */
                    result->warning_count += (int)orphan;
                    snprintf(result->details + strlen(result->details),
                             sizeof(result->details) - strlen(result->details),
                             "BAM: %zu orphan block(s) marked USED but "
                             "unreachable from root — possible deleted "
                             "content (forensic recovery candidate)\n",
                             orphan);
                }
            }
            free(st.bam_used);
            free(st.visited);
        } else {
            /* OOM during walker setup — be honest, don't claim valid. */
            result->directory_valid = false;
            result->files_valid     = false;
            result->error_count++;
        }
    } else {
        /* No valid root block ⇒ can't honestly walk. */
        result->directory_valid = false;
        result->files_valid     = false;
    }

    /* Overall verdict. */
    result->valid = (result->error_count == 0) &&
                     result->rootblock_valid &&
                     (result->bad_checksums == 0) &&
                     result->directory_valid &&
                     result->files_valid;
    return 0;
}

int uft_amiga_repair_bitmap(uft_amigados_ctx_t *ctx) {
    (void)ctx;
    /*
     * Intentionally unimplemented. Repair requires walking all
     * header/file-header blocks, reconstructing block usage, writing
     * to a fresh bitmap block, and updating checksums. This needs a
     * verified DiskSalv-compatible header walker which belongs to a
     * separate commit — see XCOPY_INTEGRATION_TODO T3 and T7.
     *
     * Returns -1 (not 0) so callers do not mistake silent no-op for
     * success. UFT rule: never pretend.
     */
    return -1;
}

int uft_amiga_salvage(uft_amigados_ctx_t *ctx,
                       const char *output_dir,
                       int *files_recovered) {
    (void)ctx; (void)output_dir;
    if (files_recovered) *files_recovered = 0;
    /*
     * Intentionally unimplemented. See uft_amiga_repair_bitmap comment.
     * Planned for a follow-up that ports DiskSalv / FixDisk concepts.
     */
    return -1;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * LHA Archive - Stubs
 * ═══════════════════════════════════════════════════════════════════════════════ */

int uft_amiga_lha_list(uft_amigados_ctx_t *ctx,
                        const char *archive_path,
                        uft_amiga_match_cb callback,
                        void *user_data) {
    (void)ctx; (void)archive_path; (void)callback; (void)user_data;
    return -1;  /* Not implemented */
}

int uft_amiga_lha_extract(uft_amigados_ctx_t *ctx,
                           const char *archive_path,
                           const char *dest_path) {
    (void)ctx; (void)archive_path; (void)dest_path;
    return -1;  /* Not implemented */
}

int uft_amiga_lha_extract_to_host(uft_amigados_ctx_t *ctx,
                                   const char *archive_path,
                                   const char *host_path) {
    (void)ctx; (void)archive_path; (void)host_path;
    return -1;  /* Not implemented */
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Batch Operations - Stubs
 * ═══════════════════════════════════════════════════════════════════════════════ */

int uft_amiga_batch_process(const char **paths,
                             int num_paths,
                             uft_amiga_batch_op_t operation,
                             const char *output_dir,
                             uft_amiga_batch_progress_cb progress,
                             void *user_data) {
    (void)paths; (void)num_paths; (void)operation; 
    (void)output_dir; (void)progress; (void)user_data;
    return -1;  /* Not implemented */
}

int uft_amiga_scan_directory(const char *dir_path,
                              bool recursive,
                              char ***found_paths,
                              int *num_found) {
    (void)dir_path; (void)recursive;
    if (found_paths) *found_paths = NULL;
    if (num_found) *num_found = 0;
    return -1;  /* Not implemented */
}

void uft_amiga_free_scan_results(char **paths, int num_paths) {
    if (paths) {
        for (int i = 0; i < num_paths; i++) {
            free(paths[i]);
        }
        free(paths);
    }
}
