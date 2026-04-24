/**
 * @file test_disk_quickscan.c
 * @brief Tests for uft_disk_quickscan (M2 T6).
 */

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "uft/analysis/uft_disk_quickscan.h"

static int _pass = 0, _fail = 0, _last_fail = 0;
#define TEST(name) static void test_##name(void)
#define RUN(name)  do { printf("  [TEST] %-38s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

TEST(classify_unreadable_when_no_syncs) {
    ASSERT(uft_qscan_classify_track(0, 0, 0, 0) == UFT_QSTAT_UNREADABLE);
    ASSERT(uft_qscan_classify_track(0, 5, 0, 0) == UFT_QSTAT_UNREADABLE);
}

TEST(classify_blank_when_syncs_no_sectors) {
    ASSERT(uft_qscan_classify_track(10, 0, 0, 0) == UFT_QSTAT_BLANK);
}

TEST(classify_degraded_on_crc_fail) {
    ASSERT(uft_qscan_classify_track(11, 8, 3, 0) == UFT_QSTAT_DEGRADED);
}

TEST(classify_degraded_on_missing) {
    ASSERT(uft_qscan_classify_track(11, 8, 0, 3) == UFT_QSTAT_DEGRADED);
}

TEST(classify_good_all_ok) {
    ASSERT(uft_qscan_classify_track(11, 11, 0, 0) == UFT_QSTAT_GOOD);
}

TEST(analyse_rejects_null_inputs) {
    uft_qscan_track_t t = {0};
    uint8_t buf[16] = {0};
    ASSERT(uft_qscan_analyse_bitstream(NULL, 128, 0x4489, 11, &t)
           == UFT_ERR_INVALID_ARG);
    ASSERT(uft_qscan_analyse_bitstream(buf, 128, 0x4489, 11, NULL)
           == UFT_ERR_INVALID_ARG);
    ASSERT(uft_qscan_analyse_bitstream(buf, 0, 0x4489, 11, &t)
           == UFT_ERR_INVALID_ARG);
}

TEST(analyse_zero_bits_is_unreadable) {
    /* 512 bits of 0x00 — no sync pattern anywhere. */
    uint8_t buf[64] = {0};
    uft_qscan_track_t t = { .track_num = 7 };
    ASSERT(uft_qscan_analyse_bitstream(buf, 64 * 8, 0x4489, 11, &t) == UFT_OK);
    ASSERT(t.track_num == 7);                  /* preserved */
    ASSERT(t.syncs_found == 0);
    ASSERT(t.status == UFT_QSTAT_UNREADABLE);
    ASSERT(t.sectors_missing == 11);
}

TEST(analyse_finds_embedded_sync) {
    /* Build a buffer with a 0x4489 sync pattern embedded at bit 64.
     * Plus 48 bits of address + CRC after it. Checking only that
     * the scanner sees a sync, not the CRC validity. */
    uint8_t buf[32] = {0};
    /* Put 0x4489 at bits 64..79 → bytes 8..9 */
    buf[8] = 0x44;
    buf[9] = 0x89;
    uft_qscan_track_t t = { .track_num = 0 };
    ASSERT(uft_qscan_analyse_bitstream(buf, 32 * 8, 0x4489, 11, &t) == UFT_OK);
    ASSERT(t.syncs_found >= 1);
}

TEST(summarise_with_empty_result_is_zero_percent) {
    uft_qscan_result_t r = {0};
    uft_qscan_summarise(&r);
    ASSERT(r.health_percent == 0.0f);
    ASSERT(r.total_good == 0);
}

TEST(summarise_counts_each_category) {
    uft_qscan_result_t r = {0};
    r.track_count = 4;
    r.tracks[0].status = UFT_QSTAT_GOOD;
    r.tracks[1].status = UFT_QSTAT_DEGRADED;
    r.tracks[2].status = UFT_QSTAT_UNREADABLE;
    r.tracks[3].status = UFT_QSTAT_BLANK;
    r.tracks[0].elapsed_ms = 100;
    r.tracks[1].elapsed_ms = 200;
    r.tracks[2].elapsed_ms = 400;
    r.tracks[3].elapsed_ms = 50;

    uft_qscan_summarise(&r);
    ASSERT(r.total_good == 1);
    ASSERT(r.total_degraded == 1);
    ASSERT(r.total_unreadable == 1);
    ASSERT(r.total_blank == 1);
    ASSERT(r.total_elapsed_ms == 750);
    ASSERT(r.health_percent == 25.0f);          /* 1/4 = 25% */
}

TEST(summarise_ignores_unscanned) {
    /* Unscanned (UNKNOWN) tracks don't count toward denominator. */
    uft_qscan_result_t r = {0};
    r.track_count = 4;
    r.tracks[0].status = UFT_QSTAT_GOOD;
    r.tracks[1].status = UFT_QSTAT_GOOD;
    /* r.tracks[2] and [3] stay UNKNOWN */

    uft_qscan_summarise(&r);
    ASSERT(r.total_good == 2);
    ASSERT(r.health_percent == 100.0f);         /* 2 good / 2 scanned */
}

TEST(summarise_null_is_safe) {
    uft_qscan_summarise(NULL);  /* must not crash */
}

int main(void) {
    printf("=== uft_disk_quickscan tests ===\n");
    RUN(classify_unreadable_when_no_syncs);
    RUN(classify_blank_when_syncs_no_sectors);
    RUN(classify_degraded_on_crc_fail);
    RUN(classify_degraded_on_missing);
    RUN(classify_good_all_ok);
    RUN(analyse_rejects_null_inputs);
    RUN(analyse_zero_bits_is_unreadable);
    RUN(analyse_finds_embedded_sync);
    RUN(summarise_with_empty_result_is_zero_percent);
    RUN(summarise_counts_each_category);
    RUN(summarise_ignores_unscanned);
    RUN(summarise_null_is_safe);
    printf("Results: %d passed, %d failed\n", _pass, _fail);
    return _fail ? 1 : 0;
}
