/**
 * @file uft_adf_bam.h
 * @brief AmigaDOS Block Allocation Map (BAM) reader.
 *
 * Enables BAM-aware (BAMCOPY) reads that skip unused blocks.
 * Inspired by X-Copy's BamCopy routine — copying hundreds of disks
 * is 3-4× faster when empty sectors are skipped.
 *
 * Forensic note (Prinzip 1): BAM-aware skip is a DOS-level semantic
 * optimisation. For flux-level captures the entire track is always
 * read regardless of BAM state — marginal bits in "free" sectors
 * can still carry historical data (XCopy integration §5).
 *
 * Part of MASTER_PLAN.md M2 T5.
 */
#ifndef UFT_ADF_BAM_H
#define UFT_ADF_BAM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "uft/uft_error.h"

#ifdef __cplusplus
extern "C" {
#endif

/** AmigaDOS block size. */
#define UFT_ADF_BLOCK_SIZE        512u

/** Standard DD root block number. */
#define UFT_ADF_DD_ROOT_BLOCK     880u

/** Maximum bitmap-block pointers tracked (25 in root, up to 25 in extensions). */
#define UFT_ADF_MAX_BITMAP_BLOCKS 50u

/**
 * BAM reader state. Initialised once per mounted image; lookups are
 * O(1) read-only queries.
 */
typedef struct {
    const uint8_t *image;
    size_t         image_size;
    uint32_t       root_block;
    uint32_t       total_blocks;

    /* Up to UFT_ADF_MAX_BITMAP_BLOCKS bitmap-block numbers. The root
     * block carries 25 entries starting at offset 316; further
     * bitmap-extension blocks can add more. */
    uint32_t       bitmap_blocks[UFT_ADF_MAX_BITMAP_BLOCKS];
    size_t         bitmap_count;

    /* Cached statistics — populated by uft_adf_bam_count(). */
    uint32_t       used_count;
    uint32_t       free_count;
    bool           stats_valid;
} uft_adf_bam_t;

/**
 * Initialise a BAM reader from an ADF image.
 *
 * Parses the root block (default 880 for DD) and extracts bitmap-
 * block pointers. Does NOT yet enumerate bitmap contents — queries
 * read bitmap blocks on demand. Call uft_adf_bam_count() once if
 * used/free totals are needed.
 *
 * @return UFT_OK on success,
 *         UFT_ERR_INVALID_ARG on NULL inputs or image too small,
 *         UFT_ERR_FORMAT if the image is not a recognised ADF.
 */
uft_error_t uft_adf_bam_init(uft_adf_bam_t *bam,
                              const uint8_t *image,
                              size_t image_size);

/**
 * Query whether a block is in use.
 *
 * Bit semantics: AmigaDOS stores 1 = FREE, 0 = USED. The boot block
 * (blocks 0 and 1) is not covered by the bitmap and is always
 * reported as USED. The root block itself is reported per its
 * actual bitmap entry.
 *
 * @return true if the block is in use OR if the bitmap cannot be
 *         located for this block number. Returning true-on-doubt is
 *         the safe default (never skip a possibly-used block).
 */
bool uft_adf_bam_is_block_used(const uft_adf_bam_t *bam,
                                uint32_t block_num);

/**
 * Populate @c used_count and @c free_count by walking the whole bitmap.
 * Sets @c stats_valid = true. Safe to call multiple times.
 */
uft_error_t uft_adf_bam_count(uft_adf_bam_t *bam);

#ifdef __cplusplus
}
#endif

#endif /* UFT_ADF_BAM_H */
