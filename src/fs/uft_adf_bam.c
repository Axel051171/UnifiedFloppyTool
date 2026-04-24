/**
 * @file uft_adf_bam.c
 * @brief AmigaDOS BAM reader implementation (M2 T5).
 */

#include "uft/fs/uft_adf_bam.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define DOS_TYPE_BASE             0x444F5300u
#define ROOT_BITMAP_PTR_OFFSET    316u  /* offset of bm_pages[0] in root */
#define ROOT_BITMAP_PTR_COUNT     25u   /* bm_pages[0..24] */
#define BITMAP_CHECKSUM_OFFSET    0u    /* first long = checksum */
#define BITMAP_DATA_OFFSET        4u    /* data starts at byte 4 */
#define BITMAP_DATA_BYTES         (UFT_ADF_BLOCK_SIZE - 4u)   /* 508 */
#define BITMAP_BITS_PER_BLOCK     (BITMAP_DATA_BYTES * 8u)    /* 4064 */

static inline uint32_t rd_be32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] <<  8) |  (uint32_t)p[3];
}

uft_error_t uft_adf_bam_init(uft_adf_bam_t *bam,
                              const uint8_t *image,
                              size_t image_size)
{
    if (bam == NULL || image == NULL) return UFT_ERR_INVALID_ARG;
    if (image_size < 2 * UFT_ADF_BLOCK_SIZE) return UFT_ERR_INVALID_ARG;

    memset(bam, 0, sizeof(*bam));
    bam->image = image;
    bam->image_size = image_size;
    bam->total_blocks = (uint32_t)(image_size / UFT_ADF_BLOCK_SIZE);

    /* Verify this looks like an ADF (DOS\x prefix in bootblock). */
    if ((rd_be32(image) & 0xFFFFFF00u) != DOS_TYPE_BASE) {
        return UFT_ERR_FORMAT;
    }

    /* Root block: try the pointer stored in bootblock offset 8, else
     * fall back to canonical 880 for DD. */
    uint32_t candidate = rd_be32(image + 8);
    if (candidate == 0 || candidate >= bam->total_blocks) {
        candidate = UFT_ADF_DD_ROOT_BLOCK;
    }
    if ((size_t)candidate * UFT_ADF_BLOCK_SIZE + UFT_ADF_BLOCK_SIZE >
        image_size) {
        return UFT_ERR_FORMAT;
    }
    bam->root_block = candidate;

    /* Extract 25 bitmap-page pointers from the root block. */
    const uint8_t *rb = image + (size_t)bam->root_block * UFT_ADF_BLOCK_SIZE;
    for (uint32_t i = 0; i < ROOT_BITMAP_PTR_COUNT; i++) {
        if (bam->bitmap_count >= UFT_ADF_MAX_BITMAP_BLOCKS) break;
        uint32_t ptr = rd_be32(rb + ROOT_BITMAP_PTR_OFFSET + i * 4u);
        if (ptr == 0) continue;
        if (ptr >= bam->total_blocks) continue;
        bam->bitmap_blocks[bam->bitmap_count++] = ptr;
    }

    /* If no bitmap blocks were found, the filesystem is marked
     * invalid — but we don't fail init. Queries just return "used"
     * (safe default). */
    return UFT_OK;
}

/*
 * Locate the bitmap block + bit position for a given data block.
 * @return bitmap block number if found, 0 (not possible as bitmap
 *         block) if the lookup falls outside what we have.
 */
static bool locate_bam_bit(const uft_adf_bam_t *bam, uint32_t block_num,
                            const uint8_t **out_byte_ptr,
                            uint32_t *out_bit_in_byte)
{
    /* Bitmap bit indices are relative to block 2 (blocks 0 and 1 are
     * the bootblock and not tracked). So the bitmap bit for block N
     * lives at bit (N - 2). */
    if (block_num < 2) return false;
    uint32_t rel = block_num - 2u;

    uint32_t bitmap_idx = rel / BITMAP_BITS_PER_BLOCK;
    uint32_t bit_index  = rel % BITMAP_BITS_PER_BLOCK;

    if (bitmap_idx >= bam->bitmap_count) return false;
    uint32_t bm_block = bam->bitmap_blocks[bitmap_idx];
    size_t off = (size_t)bm_block * UFT_ADF_BLOCK_SIZE;
    if (off + UFT_ADF_BLOCK_SIZE > bam->image_size) return false;

    /* Within a bitmap block: u32 longs, little-endian bit order per
     * standard AmigaDOS (bit 0 of first data long = first tracked
     * block). Each long is BE32 on disk, but bits within are LSB-
     * first for the bitmap semantic. */
    uint32_t long_idx = bit_index / 32u;
    uint32_t bit_in_long = bit_index % 32u;

    size_t byte_off = off + BITMAP_DATA_OFFSET +
                       (size_t)long_idx * 4u +
                       (size_t)(bit_in_long / 8u);
    /* Reverse byte order within the long: BE layout of u32. */
    uint32_t byte_off_in_long = bit_in_long / 8u;
    uint32_t be_byte_index = 3u - byte_off_in_long;
    size_t real_byte = off + BITMAP_DATA_OFFSET +
                        (size_t)long_idx * 4u +
                        (size_t)be_byte_index;
    (void)byte_off;

    if (real_byte >= bam->image_size) return false;
    *out_byte_ptr = bam->image + real_byte;
    *out_bit_in_byte = bit_in_long % 8u;
    return true;
}

bool uft_adf_bam_is_block_used(const uft_adf_bam_t *bam,
                                uint32_t block_num)
{
    if (bam == NULL) return true;
    if (block_num < 2) return true;              /* bootblock */
    if (block_num >= bam->total_blocks) return true;

    const uint8_t *bp;
    uint32_t bit_in_byte;
    if (!locate_bam_bit(bam, block_num, &bp, &bit_in_byte)) {
        return true;  /* safe default: treat as used */
    }
    /* 1 = free, 0 = used (AmigaDOS convention). */
    bool is_free = (((*bp) >> bit_in_byte) & 1u) != 0u;
    return !is_free;
}

uft_error_t uft_adf_bam_count(uft_adf_bam_t *bam) {
    if (bam == NULL) return UFT_ERR_INVALID_ARG;
    bam->used_count = 0;
    bam->free_count = 0;

    /* Blocks 0 and 1 are the bootblock and not tracked — count as used. */
    if (bam->total_blocks >= 1) bam->used_count += 2;
    for (uint32_t b = 2; b < bam->total_blocks; b++) {
        if (uft_adf_bam_is_block_used(bam, b)) {
            bam->used_count++;
        } else {
            bam->free_count++;
        }
    }
    bam->stats_valid = true;
    return UFT_OK;
}
