/**
 * @file test_salvage_fs.c
 * @brief Tests for DiskSalv-inspired salvage strategies (M2 T7).
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "uft/recovery/uft_salvage_fs.h"

static int _pass = 0, _fail = 0, _last_fail = 0;
#define TEST(name) static void test_##name(void)
#define RUN(name)  do { printf("  [TEST] %-40s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

#define BLOCK_SIZE   512u
#define DD_SIZE      901120u

static const char *test_dir(void) {
#ifdef _WIN32
    static const char *d = "C:\\Windows\\Temp";
#else
    static const char *d = "/tmp";
#endif
    return d;
}

static void write_be32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >>  8); p[3] = (uint8_t)(v      );
}

static uint32_t rd_be32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] <<  8) |  (uint32_t)p[3];
}

static uint32_t block_checksum(const uint8_t *block) {
    uint32_t sum = 0;
    for (size_t i = 0; i < BLOCK_SIZE; i += 4) sum += rd_be32(block + i);
    return (uint32_t)(-(int32_t)sum);
}

/* Place a type=2/sec=1 (root-like) header block at the given block idx. */
static void place_root_header(uint8_t *img, uint32_t block_idx) {
    uint8_t *b = img + (size_t)block_idx * BLOCK_SIZE;
    memset(b, 0, BLOCK_SIZE);
    write_be32(b + 0, 2);                          /* type = T_HEADER */
    write_be32(b + BLOCK_SIZE - 4, 1);             /* sec type = ST_ROOT */
    /* Zero checksum field then compute */
    write_be32(b + 20, 0);
    write_be32(b + 20, block_checksum(b));
}

TEST(rejects_null_inputs) {
    uft_salvage_result_t r;
    uint8_t buf[512];
    ASSERT(uft_salvage_run(NULL, 512, UFT_SALVAGE_RECOVER_BY_COPY,
                            test_dir(), &r) == UFT_ERR_INVALID_ARG);
    ASSERT(uft_salvage_run(buf, 512, UFT_SALVAGE_RECOVER_BY_COPY,
                            NULL, &r) == UFT_ERR_INVALID_ARG);
    ASSERT(uft_salvage_run(buf, 512, UFT_SALVAGE_RECOVER_BY_COPY,
                            test_dir(), NULL) == UFT_ERR_INVALID_ARG);
}

TEST(rejects_non_block_aligned_size) {
    uint8_t buf[513];
    uft_salvage_result_t r;
    ASSERT(uft_salvage_run(buf, 513, UFT_SALVAGE_RECOVER_BY_COPY,
                            test_dir(), &r) == UFT_ERR_INVALID_ARG);
}

TEST(repair_in_place_always_forbidden) {
    uint8_t *img = calloc(1, DD_SIZE);
    ASSERT(img);
    uft_salvage_result_t r;
    ASSERT(uft_salvage_run(img, DD_SIZE, UFT_SALVAGE_REPAIR_IN_PLACE,
                            test_dir(), &r) == UFT_ERR_UNSUPPORTED);
    free(img);
}

TEST(recover_by_copy_writes_identical_image) {
    uint8_t *img = calloc(1, DD_SIZE);
    ASSERT(img);
    /* Fill with recognisable pattern */
    for (size_t i = 0; i < DD_SIZE; i++) img[i] = (uint8_t)(i & 0xFF);

    uft_salvage_result_t r;
    ASSERT(uft_salvage_run(img, DD_SIZE, UFT_SALVAGE_RECOVER_BY_COPY,
                            test_dir(), &r) == UFT_OK);
    ASSERT(r.strategy == UFT_SALVAGE_RECOVER_BY_COPY);
    ASSERT(r.blocks_total == DD_SIZE / BLOCK_SIZE);
    ASSERT(r.loss_json_written);

    /* Verify output file exists and matches source byte-for-byte */
    char path[300];
    snprintf(path, sizeof(path), "%s/image.adf", test_dir());
    FILE *fh = fopen(path, "rb");
    ASSERT(fh != NULL);

    uint8_t *back = malloc(DD_SIZE);
    ASSERT(back);
    size_t readback = fread(back, 1, DD_SIZE, fh);
    fclose(fh);
    ASSERT(readback == DD_SIZE);
    ASSERT(memcmp(img, back, DD_SIZE) == 0);

    free(back);
    free(img);
}

TEST(file_by_file_counts_header_candidates) {
    uint8_t *img = calloc(1, DD_SIZE);
    ASSERT(img);
    /* Place 3 root-header candidates */
    place_root_header(img, 880);
    place_root_header(img, 881);
    place_root_header(img, 1000);

    uft_salvage_result_t r;
    ASSERT(uft_salvage_run(img, DD_SIZE, UFT_SALVAGE_FILE_BY_FILE,
                            test_dir(), &r) == UFT_OK);
    ASSERT(r.strategy == UFT_SALVAGE_FILE_BY_FILE);
    ASSERT(r.header_candidates == 3);
    ASSERT(r.headers_with_valid_chain == 3);
    /* Extraction is intentionally not implemented yet */
    ASSERT(r.files_extracted == 0);
    free(img);
}

TEST(file_by_file_honest_zero_extracted) {
    /* Even with tons of "valid" headers, files_extracted is 0 because
     * payload extraction is documented-TODO. This prevents callers
     * from mistakenly thinking FILE_BY_FILE is complete. */
    uint8_t *img = calloc(1, DD_SIZE);
    ASSERT(img);
    for (uint32_t i = 100; i < 200; i++) place_root_header(img, i);

    uft_salvage_result_t r;
    ASSERT(uft_salvage_run(img, DD_SIZE, UFT_SALVAGE_FILE_BY_FILE,
                            test_dir(), &r) == UFT_OK);
    ASSERT(r.header_candidates >= 100);
    ASSERT(r.files_extracted == 0);                /* KEY: honest zero */
    free(img);
}

TEST(unknown_strategy_rejected) {
    uint8_t *img = calloc(1, DD_SIZE);
    ASSERT(img);
    uft_salvage_result_t r;
    ASSERT(uft_salvage_run(img, DD_SIZE, (uft_salvage_strategy_t)99,
                            test_dir(), &r) == UFT_ERR_INVALID_ARG);
    free(img);
}

int main(void) {
    printf("=== uft_salvage_fs tests ===\n");
    RUN(rejects_null_inputs);
    RUN(rejects_non_block_aligned_size);
    RUN(repair_in_place_always_forbidden);
    RUN(recover_by_copy_writes_identical_image);
    RUN(file_by_file_counts_header_candidates);
    RUN(file_by_file_honest_zero_extracted);
    RUN(unknown_strategy_rejected);
    printf("Results: %d passed, %d failed\n", _pass, _fail);
    return _fail ? 1 : 0;
}
