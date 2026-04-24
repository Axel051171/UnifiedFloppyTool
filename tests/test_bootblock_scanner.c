/**
 * @file test_bootblock_scanner.c
 * @brief Tests for the Amiga bootblock scanner (M2 T2).
 */

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "uft/fs/uft_bootblock_scanner.h"

static int _pass = 0, _fail = 0, _last_fail = 0;
#define TEST(name) static void test_##name(void)
#define RUN(name)  do { printf("  [TEST] %-38s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

/* Build a minimal bootblock with given DOS type variant and checksum. */
static void build_bootblock(uint8_t *bb, uint8_t dos_variant,
                             uint32_t stored_checksum, uint32_t root_block) {
    memset(bb, 0, UFT_BOOTBLOCK_SIZE);
    /* DOS\x at offset 0 */
    bb[0] = 'D'; bb[1] = 'O'; bb[2] = 'S'; bb[3] = dos_variant;
    /* stored checksum at offset 4..7, BE32 */
    bb[4] = (uint8_t)(stored_checksum >> 24);
    bb[5] = (uint8_t)(stored_checksum >> 16);
    bb[6] = (uint8_t)(stored_checksum >>  8);
    bb[7] = (uint8_t)(stored_checksum      );
    /* root block at offset 8..11, BE32 */
    bb[8]  = (uint8_t)(root_block >> 24);
    bb[9]  = (uint8_t)(root_block >> 16);
    bb[10] = (uint8_t)(root_block >>  8);
    bb[11] = (uint8_t)(root_block      );
}

TEST(rejects_null_buffer) {
    uft_bb_scan_result_t r;
    ASSERT(uft_bb_scan(NULL, UFT_BOOTBLOCK_SIZE, &r) == UFT_ERR_INVALID_ARG);
}

TEST(rejects_null_result) {
    uint8_t bb[UFT_BOOTBLOCK_SIZE] = {0};
    ASSERT(uft_bb_scan(bb, UFT_BOOTBLOCK_SIZE, NULL) == UFT_ERR_INVALID_ARG);
}

TEST(rejects_wrong_size) {
    uint8_t bb[UFT_BOOTBLOCK_SIZE] = {0};
    uft_bb_scan_result_t r;
    ASSERT(uft_bb_scan(bb, 512, &r) == UFT_ERR_INVALID_ARG);
    ASSERT(uft_bb_scan(bb, UFT_BOOTBLOCK_SIZE - 1, &r) == UFT_ERR_INVALID_ARG);
    ASSERT(uft_bb_scan(bb, UFT_BOOTBLOCK_SIZE + 1, &r) == UFT_ERR_INVALID_ARG);
}

TEST(all_zeros_detected) {
    uint8_t bb[UFT_BOOTBLOCK_SIZE] = {0};
    uft_bb_scan_result_t r;
    ASSERT(uft_bb_scan(bb, sizeof(bb), &r) == UFT_OK);
    ASSERT(r.all_zeros);
    ASSERT(r.dos_type == UFT_BB_DOS_UNKNOWN);
    ASSERT(!r.looks_like_code);
    ASSERT(r.match_count == 0);
}

TEST(ofs_bootblock_classified) {
    uint8_t bb[UFT_BOOTBLOCK_SIZE];
    build_bootblock(bb, 0, 0, 880);
    uft_bb_scan_result_t r;
    ASSERT(uft_bb_scan(bb, sizeof(bb), &r) == UFT_OK);
    ASSERT(r.dos_type == UFT_BB_DOS_OFS);
    ASSERT(r.root_block == 880);
}

TEST(ffs_bootblock_classified) {
    uint8_t bb[UFT_BOOTBLOCK_SIZE];
    build_bootblock(bb, 1, 0, 880);
    uft_bb_scan_result_t r;
    ASSERT(uft_bb_scan(bb, sizeof(bb), &r) == UFT_OK);
    ASSERT(r.dos_type == UFT_BB_DOS_FFS);
}

TEST(ofs_intl_bootblock_classified) {
    uint8_t bb[UFT_BOOTBLOCK_SIZE];
    build_bootblock(bb, 2, 0, 880);
    uft_bb_scan_result_t r;
    ASSERT(uft_bb_scan(bb, sizeof(bb), &r) == UFT_OK);
    ASSERT(r.dos_type == UFT_BB_DOS_OFS_INTL);
}

TEST(ffs_dc_bootblock_classified) {
    uint8_t bb[UFT_BOOTBLOCK_SIZE];
    build_bootblock(bb, 5, 0, 880);
    uft_bb_scan_result_t r;
    ASSERT(uft_bb_scan(bb, sizeof(bb), &r) == UFT_OK);
    ASSERT(r.dos_type == UFT_BB_DOS_FFS_DC);
}

TEST(unknown_dos_variant_flagged) {
    uint8_t bb[UFT_BOOTBLOCK_SIZE];
    build_bootblock(bb, 99, 0, 880);
    uft_bb_scan_result_t r;
    ASSERT(uft_bb_scan(bb, sizeof(bb), &r) == UFT_OK);
    ASSERT(r.dos_type == UFT_BB_DOS_UNKNOWN);
}

TEST(pfs_prefix_not_dos) {
    uint8_t bb[UFT_BOOTBLOCK_SIZE] = {0};
    bb[0] = 'P'; bb[1] = 'F'; bb[2] = 'S'; bb[3] = 0;
    uft_bb_scan_result_t r;
    ASSERT(uft_bb_scan(bb, sizeof(bb), &r) == UFT_OK);
    ASSERT(r.dos_type == UFT_BB_DOS_UNKNOWN);
}

TEST(checksum_of_zeros_is_all_ones) {
    /* All-zero bootblock: sum = 0, ~0 = 0xFFFFFFFF */
    uint8_t bb[UFT_BOOTBLOCK_SIZE] = {0};
    ASSERT(uft_bb_compute_checksum(bb) == 0xFFFFFFFFu);
}

TEST(checksum_validates_correct_stored_value) {
    /* Build a bootblock, then set stored checksum to the computed
     * value and verify the scanner reports checksum_ok == true. */
    uint8_t bb[UFT_BOOTBLOCK_SIZE];
    build_bootblock(bb, 0, 0, 880);
    /* Put some non-zero content so sum is non-trivial */
    bb[64] = 0x60; bb[65] = 0x00; bb[66] = 0x01; bb[67] = 0x00;

    uint32_t expected = uft_bb_compute_checksum(bb);

    build_bootblock(bb, 0, expected, 880);
    bb[64] = 0x60; bb[65] = 0x00; bb[66] = 0x01; bb[67] = 0x00;

    uft_bb_scan_result_t r;
    ASSERT(uft_bb_scan(bb, sizeof(bb), &r) == UFT_OK);
    ASSERT(r.stored_checksum == expected);
    ASSERT(r.computed_checksum == expected);
    ASSERT(r.checksum_ok);
}

TEST(checksum_detects_modified_bootblock) {
    uint8_t bb[UFT_BOOTBLOCK_SIZE];
    build_bootblock(bb, 0, 0, 880);
    bb[64] = 0x60;
    uint32_t orig = uft_bb_compute_checksum(bb);

    build_bootblock(bb, 0, orig, 880);
    bb[64] = 0x60;

    /* Now modify a byte AFTER checksum was stored — checksum must fail. */
    bb[100] = 0xAA;

    uft_bb_scan_result_t r;
    ASSERT(uft_bb_scan(bb, sizeof(bb), &r) == UFT_OK);
    ASSERT(!r.checksum_ok);
}

TEST(m68k_opcode_detected) {
    /* BRA.S at offset 12 — past DOS header + checksum + root_block. */
    uint8_t bb[UFT_BOOTBLOCK_SIZE];
    build_bootblock(bb, 0, 0, 880);
    bb[12] = 0x60; bb[13] = 0x00;
    uft_bb_scan_result_t r;
    ASSERT(uft_bb_scan(bb, sizeof(bb), &r) == UFT_OK);
    ASSERT(r.looks_like_code);
}

TEST(virus_db_consulted_but_no_matches_on_clean_bootblock) {
    /* With all virus signatures PENDING, no matches should be found
     * on an otherwise-clean bootblock. */
    uint8_t bb[UFT_BOOTBLOCK_SIZE];
    build_bootblock(bb, 0, 0, 880);
    bb[12] = 0x60; bb[13] = 0x00;
    uft_bb_scan_result_t r;
    ASSERT(uft_bb_scan(bb, sizeof(bb), &r) == UFT_OK);
    ASSERT(r.match_count == 0);
}

int main(void) {
    printf("=== Bootblock scanner tests ===\n");
    RUN(rejects_null_buffer);
    RUN(rejects_null_result);
    RUN(rejects_wrong_size);
    RUN(all_zeros_detected);
    RUN(ofs_bootblock_classified);
    RUN(ffs_bootblock_classified);
    RUN(ofs_intl_bootblock_classified);
    RUN(ffs_dc_bootblock_classified);
    RUN(unknown_dos_variant_flagged);
    RUN(pfs_prefix_not_dos);
    RUN(checksum_of_zeros_is_all_ones);
    RUN(checksum_validates_correct_stored_value);
    RUN(checksum_detects_modified_bootblock);
    RUN(m68k_opcode_detected);
    RUN(virus_db_consulted_but_no_matches_on_clean_bootblock);
    printf("Results: %d passed, %d failed\n", _pass, _fail);
    return _fail ? 1 : 0;
}
