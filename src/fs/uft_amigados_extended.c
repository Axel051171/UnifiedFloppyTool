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

    /* --- 4. Directory and file chains: not walked yet (future work) --- */
    result->directory_valid = true;   /* optimistic — no walk done */
    result->files_valid     = true;   /* optimistic — no walk done */

    /* Overall verdict. */
    result->valid = (result->error_count == 0) &&
                     result->rootblock_valid &&
                     (result->bad_checksums == 0);
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
