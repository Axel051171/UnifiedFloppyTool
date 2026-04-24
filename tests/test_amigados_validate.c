/**
 * @file test_amigados_validate.c
 * @brief Unit tests for uft_amiga_validate_ext (M2 T3).
 *
 * Builds a synthetic 880-block AmigaDOS image in memory and verifies
 * that the validator catches bootblock errors, root-block checksum
 * mismatches, and reports honestly when no structural walk was
 * performed.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "uft/fs/uft_amigados.h"
#include "uft/fs/uft_amigados_extended.h"
#include "uft/fs/uft_bootblock_scanner.h"

static int _pass = 0, _fail = 0, _last_fail = 0;
#define TEST(name) static void test_##name(void)
#define RUN(name)  do { printf("  [TEST] %-38s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

#define DD_BLOCK_COUNT   1760u      /* 880 × 2 (2 sides) */
#define BLOCK_SIZE       512u
#define DD_IMAGE_SIZE    (DD_BLOCK_COUNT * BLOCK_SIZE)   /* 901120 */

static void write_be32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >>  8); p[3] = (uint8_t)(v      );
}

static uint32_t rd_be32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] <<  8) |  (uint32_t)p[3];
}

/* Compute standard AmigaDOS block checksum (different from bootblock). */
static uint32_t block_checksum(const uint8_t *block) {
    uint32_t sum = 0;
    for (size_t i = 0; i < BLOCK_SIZE; i += 4) sum += rd_be32(block + i);
    return (uint32_t)(-(int32_t)sum);
}

/* Build a synthetic DD ADF with bootblock + root block at 880. */
static uint8_t *build_valid_adf(void) {
    uint8_t *img = calloc(1, DD_IMAGE_SIZE);
    if (!img) return NULL;

    /* Bootblock: DOS\0 + checksum placeholder + root block 880 */
    img[0] = 'D'; img[1] = 'O'; img[2] = 'S'; img[3] = 0;
    write_be32(img + 8, 880);
    /* Some boot code so looks_like_code returns true */
    img[12] = 0x60; img[13] = 0x00;
    /* Compute and store bootblock checksum */
    uint32_t bb_sum = uft_bb_compute_checksum(img);
    write_be32(img + 4, bb_sum);

    /* Root block at block 880: type=T_HEADER (2), secondary=T_ROOT (1) */
    uint8_t *rb = img + 880u * BLOCK_SIZE;
    write_be32(rb + 0, 2);        /* type = 2 (T_HEADER) */
    write_be32(rb + 4, 0);        /* header key (0 for root) */
    write_be32(rb + 12, 72);      /* hash table size */
    /* Secondary type at end of block: offset BLOCK_SIZE - 4 = 508 */
    write_be32(rb + BLOCK_SIZE - 4, 1);   /* secondary type = T_ROOT */
    /* Checksum at offset 20 (word 5) — zero first then compute */
    write_be32(rb + 20, 0);
    write_be32(rb + 20, block_checksum(rb));

    return img;
}

TEST(rejects_null_inputs) {
    uft_amigados_ctx_t ctx = {0};
    uft_amiga_validate_result_t r;
    ASSERT(uft_amiga_validate_ext(NULL, &r)    == -1);
    ASSERT(uft_amiga_validate_ext(&ctx, NULL)  == -1);
}

TEST(rejects_too_small_image) {
    uint8_t tiny[100] = {0};
    uft_amigados_ctx_t ctx = { .data = tiny, .size = sizeof(tiny) };
    uft_amiga_validate_result_t r;
    ASSERT(uft_amiga_validate_ext(&ctx, &r) == -1);
}

TEST(valid_adf_passes) {
    uint8_t *img = build_valid_adf();
    ASSERT(img != NULL);

    uft_amigados_ctx_t ctx = {
        .data = img,
        .size = DD_IMAGE_SIZE,
        .root_block = 880,
        .bitmap_count = 0,
    };
    uft_amiga_validate_result_t r;
    ASSERT(uft_amiga_validate_ext(&ctx, &r) == 0);
    ASSERT(r.rootblock_valid);
    ASSERT(r.bad_checksums == 0);
    ASSERT(r.valid);
    free(img);
}

TEST(detects_bootblock_checksum_error) {
    uint8_t *img = build_valid_adf();
    ASSERT(img != NULL);

    /* Corrupt a non-checksum byte of the bootblock. */
    img[100] ^= 0xFF;

    uft_amigados_ctx_t ctx = {
        .data = img, .size = DD_IMAGE_SIZE, .root_block = 880,
    };
    uft_amiga_validate_result_t r;
    ASSERT(uft_amiga_validate_ext(&ctx, &r) == 0);
    ASSERT(r.bad_checksums >= 1);
    ASSERT(!r.valid);
    free(img);
}

TEST(detects_root_block_corruption) {
    uint8_t *img = build_valid_adf();
    ASSERT(img != NULL);

    /* Flip a byte inside the root block. */
    uint8_t *rb = img + 880u * BLOCK_SIZE;
    rb[100] ^= 0x55;

    uft_amigados_ctx_t ctx = {
        .data = img, .size = DD_IMAGE_SIZE, .root_block = 880,
    };
    uft_amiga_validate_result_t r;
    ASSERT(uft_amiga_validate_ext(&ctx, &r) == 0);
    ASSERT(!r.rootblock_valid);
    ASSERT(r.bad_checksums >= 1);
    ASSERT(!r.valid);
    free(img);
}

TEST(unknown_dos_type_flagged) {
    uint8_t *img = build_valid_adf();
    ASSERT(img != NULL);

    /* Overwrite DOS\x with garbage, recompute checksum to isolate
     * the DOS-type check from checksum error. */
    img[0] = 'X'; img[1] = 'Y'; img[2] = 'Z'; img[3] = 9;
    write_be32(img + 4, 0);
    write_be32(img + 4, uft_bb_compute_checksum(img));

    uft_amigados_ctx_t ctx = {
        .data = img, .size = DD_IMAGE_SIZE, .root_block = 880,
    };
    uft_amiga_validate_result_t r;
    ASSERT(uft_amiga_validate_ext(&ctx, &r) == 0);
    ASSERT(r.error_count >= 1);
    ASSERT(!r.valid);
    free(img);
}

TEST(repair_and_salvage_return_not_implemented) {
    /* T3 honesty: these functions MUST return -1 (not 0) until real
     * implementation lands. */
    uft_amigados_ctx_t ctx = {0};
    int files = -42;
    ASSERT(uft_amiga_repair_bitmap(&ctx) == -1);
    ASSERT(uft_amiga_salvage(&ctx, "/tmp", &files) == -1);
    ASSERT(files == 0);  /* zeroed on entry */
}

int main(void) {
    printf("=== uft_amiga_validate_ext tests ===\n");
    RUN(rejects_null_inputs);
    RUN(rejects_too_small_image);
    RUN(valid_adf_passes);
    RUN(detects_bootblock_checksum_error);
    RUN(detects_root_block_corruption);
    RUN(unknown_dos_type_flagged);
    RUN(repair_and_salvage_return_not_implemented);
    printf("Results: %d passed, %d failed\n", _pass, _fail);
    return _fail ? 1 : 0;
}
