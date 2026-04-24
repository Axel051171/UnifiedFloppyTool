/**
 * @file test_adf_bam.c
 * @brief Tests for the AmigaDOS BAM reader (M2 T5).
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "uft/fs/uft_adf_bam.h"

static int _pass = 0, _fail = 0, _last_fail = 0;
#define TEST(name) static void test_##name(void)
#define RUN(name)  do { printf("  [TEST] %-38s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

#define DD_SIZE       901120u          /* 1760 × 512 */
#define DD_ROOT       880u
#define DD_BITMAP1    881u             /* Conventional DD bitmap placement */

static void write_be32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >>  8); p[3] = (uint8_t)(v      );
}

/* Build an empty DD ADF with:
 *   - "DOS\0" bootblock prefix
 *   - root_block pointer → 880
 *   - root block with bitmap_pages[0] → 881
 *   - bitmap block 881 with all bits set to 1 (everything FREE)
 * Returns heap-allocated image (caller frees).
 */
static uint8_t *build_empty_dd_adf(void) {
    uint8_t *img = calloc(1, DD_SIZE);
    if (!img) return NULL;

    /* Bootblock: DOS\0 + root block = 880 */
    img[0] = 'D'; img[1] = 'O'; img[2] = 'S'; img[3] = 0;
    write_be32(img + 8, DD_ROOT);

    /* Root block (880): write bitmap_pages[0] = 881 */
    write_be32(img + DD_ROOT * UFT_ADF_BLOCK_SIZE + 316, DD_BITMAP1);

    /* Bitmap block (881): all 0xFF = all blocks FREE */
    uint8_t *bmp = img + DD_BITMAP1 * UFT_ADF_BLOCK_SIZE;
    memset(bmp + 4, 0xFF, UFT_ADF_BLOCK_SIZE - 4);

    return img;
}

/* Mark a specific block as USED (bit = 0) in the bitmap at block 881. */
static void mark_block_used(uint8_t *img, uint32_t block) {
    if (block < 2) return;
    uint32_t rel = block - 2;
    uint32_t long_idx = rel / 32;
    uint32_t bit_in_long = rel % 32;
    uint32_t byte_off_in_long = bit_in_long / 8;
    uint32_t be_byte = 3 - byte_off_in_long;
    size_t byte_pos = DD_BITMAP1 * UFT_ADF_BLOCK_SIZE + 4 +
                       (size_t)long_idx * 4 + be_byte;
    img[byte_pos] &= (uint8_t)~(1u << (bit_in_long % 8));
}

TEST(rejects_null_inputs) {
    uft_adf_bam_t bam;
    uint8_t buf[UFT_ADF_BLOCK_SIZE] = {0};
    ASSERT(uft_adf_bam_init(NULL, buf, UFT_ADF_BLOCK_SIZE) == UFT_ERR_INVALID_ARG);
    ASSERT(uft_adf_bam_init(&bam, NULL, UFT_ADF_BLOCK_SIZE) == UFT_ERR_INVALID_ARG);
}

TEST(rejects_too_small) {
    uft_adf_bam_t bam;
    uint8_t small[512] = {0};
    ASSERT(uft_adf_bam_init(&bam, small, sizeof(small)) == UFT_ERR_INVALID_ARG);
}

TEST(rejects_non_amiga_image) {
    uft_adf_bam_t bam;
    uint8_t *img = calloc(1, DD_SIZE);
    ASSERT(img != NULL);
    /* Not DOS\x */
    img[0] = 'P'; img[1] = 'F'; img[2] = 'S'; img[3] = 0;
    ASSERT(uft_adf_bam_init(&bam, img, DD_SIZE) == UFT_ERR_FORMAT);
    free(img);
}

TEST(empty_disk_all_free) {
    uint8_t *img = build_empty_dd_adf();
    ASSERT(img != NULL);
    uft_adf_bam_t bam;
    ASSERT(uft_adf_bam_init(&bam, img, DD_SIZE) == UFT_OK);
    ASSERT(bam.total_blocks == 1760);
    ASSERT(bam.bitmap_count >= 1);

    /* Bootblock: always used (not in bitmap) */
    ASSERT(uft_adf_bam_is_block_used(&bam, 0));
    ASSERT(uft_adf_bam_is_block_used(&bam, 1));

    /* Block 2 with all-FREE bitmap should be free */
    ASSERT(!uft_adf_bam_is_block_used(&bam, 2));
    ASSERT(!uft_adf_bam_is_block_used(&bam, 100));
    ASSERT(!uft_adf_bam_is_block_used(&bam, 1759));

    /* Block beyond total: treated as used (safe default) */
    ASSERT(uft_adf_bam_is_block_used(&bam, 10000));
    free(img);
}

TEST(marked_blocks_are_reported_used) {
    uint8_t *img = build_empty_dd_adf();
    ASSERT(img != NULL);

    /* Mark a pattern of blocks as used. */
    mark_block_used(img, 2);
    mark_block_used(img, 100);
    mark_block_used(img, 880);
    mark_block_used(img, 1500);

    uft_adf_bam_t bam;
    ASSERT(uft_adf_bam_init(&bam, img, DD_SIZE) == UFT_OK);

    ASSERT(uft_adf_bam_is_block_used(&bam, 2));
    ASSERT(uft_adf_bam_is_block_used(&bam, 100));
    ASSERT(uft_adf_bam_is_block_used(&bam, 880));
    ASSERT(uft_adf_bam_is_block_used(&bam, 1500));

    /* Neighbours should NOT be affected */
    ASSERT(!uft_adf_bam_is_block_used(&bam, 3));
    ASSERT(!uft_adf_bam_is_block_used(&bam, 101));
    ASSERT(!uft_adf_bam_is_block_used(&bam, 1501));
    free(img);
}

TEST(count_populates_stats) {
    uint8_t *img = build_empty_dd_adf();
    ASSERT(img != NULL);

    mark_block_used(img, 10);
    mark_block_used(img, 11);
    mark_block_used(img, 12);

    uft_adf_bam_t bam;
    ASSERT(uft_adf_bam_init(&bam, img, DD_SIZE) == UFT_OK);
    ASSERT(uft_adf_bam_count(&bam) == UFT_OK);
    ASSERT(bam.stats_valid);
    /* Used = 2 (bootblock) + 3 marked = 5 */
    ASSERT(bam.used_count == 5);
    ASSERT(bam.free_count == 1760 - 5);
    free(img);
}

TEST(count_all_free_gives_two_used_bootblock_only) {
    uint8_t *img = build_empty_dd_adf();
    ASSERT(img != NULL);
    uft_adf_bam_t bam;
    ASSERT(uft_adf_bam_init(&bam, img, DD_SIZE) == UFT_OK);
    ASSERT(uft_adf_bam_count(&bam) == UFT_OK);
    ASSERT(bam.used_count == 2);                 /* just blocks 0 and 1 */
    ASSERT(bam.free_count == 1760 - 2);
    free(img);
}

TEST(count_all_used_has_no_free) {
    uint8_t *img = build_empty_dd_adf();
    ASSERT(img != NULL);
    /* Zero the bitmap entirely → all blocks USED */
    memset(img + DD_BITMAP1 * UFT_ADF_BLOCK_SIZE + 4, 0x00,
           UFT_ADF_BLOCK_SIZE - 4);

    uft_adf_bam_t bam;
    ASSERT(uft_adf_bam_init(&bam, img, DD_SIZE) == UFT_OK);
    ASSERT(uft_adf_bam_count(&bam) == UFT_OK);
    ASSERT(bam.free_count == 0);
    ASSERT(bam.used_count == 1760);
    free(img);
}

TEST(null_bam_is_used_default) {
    ASSERT(uft_adf_bam_is_block_used(NULL, 5));
}

int main(void) {
    printf("=== AmigaDOS BAM reader tests ===\n");
    RUN(rejects_null_inputs);
    RUN(rejects_too_small);
    RUN(rejects_non_amiga_image);
    RUN(empty_disk_all_free);
    RUN(marked_blocks_are_reported_used);
    RUN(count_populates_stats);
    RUN(count_all_free_gives_two_used_bootblock_only);
    RUN(count_all_used_has_no_free);
    RUN(null_bam_is_used_default);
    printf("Results: %d passed, %d failed\n", _pass, _fail);
    return _fail ? 1 : 0;
}
