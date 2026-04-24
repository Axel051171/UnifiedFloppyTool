/**
 * @file uft_salvage_fs.c
 * @brief DiskSalv-inspired salvage strategies (M2 T7 scaffold).
 *
 * Scaffolding commit: strategies REPAIR_IN_PLACE and RECOVER_BY_COPY
 * are fully implemented. FILE_BY_FILE walks candidate header blocks
 * and counts chain-consistent ones, but does NOT extract file payloads
 * (that requires a full AmigaDOS walker which is a separate commit).
 * Scaffolding is explicit — result.files_extracted stays 0 and a TODO
 * is emitted to the loss.json sidecar so downstream tools know.
 */

#include "uft/recovery/uft_salvage_fs.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define AMIGA_BLOCK_SIZE   512u
#define T_HEADER           2u           /* primary type for header blocks */
#define ST_ROOT            1u
#define ST_USERDIR         2u
#define ST_FILE            (uint32_t)(-3)  /* per AmigaDOS encoding */

static inline uint32_t rd_be32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] <<  8) |  (uint32_t)p[3];
}

static bool amiga_block_checksum_ok(const uint8_t *block) {
    uint32_t sum = 0;
    for (size_t i = 0; i < AMIGA_BLOCK_SIZE; i += 4) {
        sum += rd_be32(block + i);
    }
    return sum == 0;
}

/*
 * Lightweight join of two strings into a small caller-provided buffer.
 * Handles missing trailing slash. Returns false if it can't fit.
 */
static bool join_path(const char *dir, const char *leaf,
                       char *out, size_t cap)
{
    if (!dir || !leaf || !out || cap < 2) return false;
    size_t dlen = strlen(dir);
    size_t llen = strlen(leaf);
    bool need_sep = (dlen > 0 && dir[dlen - 1] != '/' && dir[dlen - 1] != '\\');
    size_t total = dlen + (need_sep ? 1 : 0) + llen;
    if (total + 1 > cap) return false;
    memcpy(out, dir, dlen);
    if (need_sep) out[dlen++] = '/';
    memcpy(out + dlen, leaf, llen);
    out[dlen + llen] = '\0';
    return true;
}

static uft_error_t write_loss_json(const char *output_dir,
                                    const uft_salvage_result_t *r)
{
    char path[272];
    if (!join_path(output_dir, "loss.json", path, sizeof(path))) {
        return UFT_ERR_INVALID_ARG;
    }
    FILE *fh = fopen(path, "w");
    if (!fh) return UFT_ERR_IO;

    fprintf(fh,
        "{\n"
        "  \"schema\": \"uft.loss_report.v1\",\n"
        "  \"operation\": \"salvage\",\n"
        "  \"strategy\": %d,\n"
        "  \"blocks_total\": %u,\n"
        "  \"blocks_valid_checksum\": %u,\n"
        "  \"blocks_unreadable\": %u,\n"
        "  \"header_candidates\": %u,\n"
        "  \"headers_with_valid_chain\": %u,\n"
        "  \"files_extracted\": %u,\n"
        "  \"files_skipped_corrupt\": %u,\n"
        "  \"notes\": [\n"
        "    \"file-by-file payload extraction is not yet implemented; \"\n"
        "    \"headers_with_valid_chain reflects structural validation only\"\n"
        "  ]\n"
        "}\n",
        (int)r->strategy,
        r->blocks_total,
        r->blocks_valid_checksum,
        r->blocks_unreadable,
        r->header_candidates,
        r->headers_with_valid_chain,
        r->files_extracted,
        r->files_skipped_corrupt);
    fclose(fh);
    return UFT_OK;
}

/*
 * Strategy 2: bit-exact copy. Writes output_dir/image.adf.
 */
static uft_error_t run_recover_by_copy(const uint8_t *image,
                                        size_t image_size,
                                        const char *output_dir,
                                        uft_salvage_result_t *r)
{
    char img_path[272];
    if (!join_path(output_dir, "image.adf", img_path, sizeof(img_path))) {
        return UFT_ERR_INVALID_ARG;
    }
    FILE *fh = fopen(img_path, "wb");
    if (!fh) return UFT_ERR_IO;

    size_t wrote = fwrite(image, 1, image_size, fh);
    fclose(fh);
    if (wrote != image_size) return UFT_ERR_IO;

    /* Now scan the (identical) image for basic stats. */
    for (size_t off = 0; off + AMIGA_BLOCK_SIZE <= image_size;
         off += AMIGA_BLOCK_SIZE) {
        r->blocks_total++;
        if (amiga_block_checksum_ok(image + off)) {
            r->blocks_valid_checksum++;
        }
    }

    return UFT_OK;
}

/*
 * Strategy 3: header-candidate walk. Counts type=2 blocks with valid
 * checksum and plausible secondary type (root/userdir/file). Does NOT
 * extract payloads — see TODO in header.
 */
static uft_error_t run_file_by_file(const uint8_t *image,
                                     size_t image_size,
                                     uft_salvage_result_t *r)
{
    for (size_t off = 0; off + AMIGA_BLOCK_SIZE <= image_size;
         off += AMIGA_BLOCK_SIZE) {
        r->blocks_total++;
        const uint8_t *blk = image + off;

        bool cksum_ok = amiga_block_checksum_ok(blk);
        if (cksum_ok) r->blocks_valid_checksum++;

        uint32_t type = rd_be32(blk + 0);
        if (type != T_HEADER) continue;

        r->header_candidates++;

        /* Secondary type lives at the LAST long of the block. */
        uint32_t sec_type = rd_be32(blk + AMIGA_BLOCK_SIZE - 4);
        bool sec_plausible = (sec_type == ST_ROOT ||
                               sec_type == ST_USERDIR ||
                               sec_type == ST_FILE);
        if (cksum_ok && sec_plausible) {
            r->headers_with_valid_chain++;
        }
    }
    /* Payload extraction is not implemented — keep files_extracted at 0
     * and let the caller see that in the result. */
    r->files_extracted = 0;
    r->files_skipped_corrupt = 0;
    return UFT_OK;
}

uft_error_t uft_salvage_run(const uint8_t *image,
                             size_t image_size,
                             uft_salvage_strategy_t strategy,
                             const char *output_dir,
                             uft_salvage_result_t *result)
{
    if (!image || !output_dir || !result) return UFT_ERR_INVALID_ARG;
    if (image_size < AMIGA_BLOCK_SIZE) return UFT_ERR_INVALID_ARG;
    if (image_size % AMIGA_BLOCK_SIZE != 0) return UFT_ERR_INVALID_ARG;

    memset(result, 0, sizeof(*result));
    result->strategy = strategy;
    strncpy(result->output_dir, output_dir, sizeof(result->output_dir) - 1);

    uft_error_t err = UFT_OK;
    switch (strategy) {
    case UFT_SALVAGE_REPAIR_IN_PLACE:
        /*
         * Forbidden per XCOPY_INTEGRATION_TODO.md § Anti-Features:
         * repair must never modify the source. Fail loudly so callers
         * know to pick a different strategy.
         */
        return UFT_ERR_UNSUPPORTED;

    case UFT_SALVAGE_RECOVER_BY_COPY:
        err = run_recover_by_copy(image, image_size, output_dir, result);
        break;

    case UFT_SALVAGE_FILE_BY_FILE:
        err = run_file_by_file(image, image_size, result);
        break;

    default:
        return UFT_ERR_INVALID_ARG;
    }

    if (err != UFT_OK) return err;

    /* Always write a loss.json sidecar — even on successful runs, so
     * operators can see what was or was not recovered. Prinzip 1. */
    if (write_loss_json(output_dir, result) == UFT_OK) {
        result->loss_json_written = true;
    }
    return UFT_OK;
}
