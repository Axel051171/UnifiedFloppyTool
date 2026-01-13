/**
 * @file test_smoke.c
 * @brief Smoke tests for UFT core functionality
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* Include UFT headers */
#include "uft/uft_decode_score.h"
#include "uft/uft_merge_engine.h"

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) do { \
    printf("  TEST: %s ... ", #name); \
    tests_run++; \
    if (test_##name()) { \
        printf("PASS\n"); \
        tests_passed++; \
    } else { \
        printf("FAIL\n"); \
    } \
} while(0)

/* ═══════════════════════════════════════════════════════════════════════════════
 * Score Tests
 * ═══════════════════════════════════════════════════════════════════════════════ */

static int test_score_init(void) {
    uft_decode_score_t score;
    uft_score_init(&score);
    return score.total == 0 && score.crc_score == 0;
}

static int test_score_perfect(void) {
    uft_decode_score_t score;
    uft_score_sector(&score,
                     true,   /* crc_ok */
                     0, 0, 1,/* cylinder, head, sector */
                     80, 18, /* max_cylinder, max_sector */
                     50.0,   /* timing_jitter_ns */
                     200.0,  /* timing_threshold_ns */
                     false,  /* protection_expected */
                     false); /* protection_found */
    
    return score.total >= 90 && score.crc_ok == true;
}

static int test_score_bad_crc(void) {
    uft_decode_score_t score;
    uft_score_sector(&score,
                     false,  /* crc_ok = BAD */
                     0, 0, 1,
                     80, 18,
                     50.0, 200.0,
                     false, false);
    
    return score.crc_score == 0 && score.total < 70;
}

static int test_score_compare(void) {
    uft_decode_score_t a = {0}, b = {0};
    a.total = 80;
    a.crc_ok = true;
    b.total = 70;
    b.crc_ok = true;
    
    return uft_score_compare(&a, &b) > 0;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Merge Tests
 * ═══════════════════════════════════════════════════════════════════════════════ */

static int test_merge_engine_create(void) {
    uft_merge_engine_t *engine = uft_merge_engine_create(NULL);
    int result = (engine != NULL);
    uft_merge_engine_destroy(engine);
    return result;
}

static int test_merge_single_candidate(void) {
    uft_merge_engine_t *engine = uft_merge_engine_create(NULL);
    
    uint8_t data[256];
    memset(data, 0xAA, sizeof(data));
    
    uft_sector_candidate_t candidate = {
        .cylinder = 0,
        .head = 0,
        .sector = 1,
        .data = data,
        .data_size = 256,
        .crc_ok = true,
        .source_revolution = 1
    };
    candidate.score.total = 95;
    candidate.score.crc_ok = true;
    
    uft_merge_add_candidate(engine, &candidate);
    
    uft_merged_track_t track;
    int count = uft_merge_execute(engine, &track);
    
    int result = (count == 1 && track.sector_count == 1);
    
    uft_merged_track_free(&track);
    uft_merge_engine_destroy(engine);
    
    return result;
}

static int test_merge_crc_wins(void) {
    uint8_t good_data[256], bad_data[256];
    memset(good_data, 0xAA, sizeof(good_data));
    memset(bad_data, 0xBB, sizeof(bad_data));
    
    uft_sector_candidate_t candidates[2] = {
        {
            .cylinder = 0, .head = 0, .sector = 1,
            .data = bad_data, .data_size = 256,
            .crc_ok = false, .source_revolution = 1
        },
        {
            .cylinder = 0, .head = 0, .sector = 1,
            .data = good_data, .data_size = 256,
            .crc_ok = true, .source_revolution = 2
        }
    };
    candidates[0].score.total = 50;
    candidates[1].score.total = 95;
    candidates[1].score.crc_ok = true;
    
    uft_merged_sector_t out;
    uft_merge_sector_simple(candidates, 2, UFT_MERGE_CRC_WINS, &out);
    
    /* CRC-OK candidate should win */
    int result = (out.source_revolution == 2 && out.final_score.crc_ok == true);
    
    free(out.data);
    return result;
}

static int test_merge_highest_score(void) {
    uint8_t data1[256], data2[256];
    memset(data1, 0x11, sizeof(data1));
    memset(data2, 0x22, sizeof(data2));
    
    uft_sector_candidate_t candidates[2] = {
        {
            .cylinder = 0, .head = 0, .sector = 1,
            .data = data1, .data_size = 256,
            .crc_ok = true, .source_revolution = 1
        },
        {
            .cylinder = 0, .head = 0, .sector = 1,
            .data = data2, .data_size = 256,
            .crc_ok = true, .source_revolution = 2
        }
    };
    candidates[0].score.total = 70;
    candidates[1].score.total = 90;  /* Higher score */
    
    uft_merged_sector_t out;
    uft_merge_sector_simple(candidates, 2, UFT_MERGE_HIGHEST_SCORE, &out);
    
    /* Higher score should win */
    int result = (out.source_revolution == 2);
    
    free(out.data);
    return result;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Main
 * ═══════════════════════════════════════════════════════════════════════════════ */

int main(void) {
    printf("═══════════════════════════════════════════════════════════════════\n");
    printf("UFT Smoke Tests\n");
    printf("═══════════════════════════════════════════════════════════════════\n\n");
    
    printf("Score Tests:\n");
    TEST(score_init);
    TEST(score_perfect);
    TEST(score_bad_crc);
    TEST(score_compare);
    
    printf("\nMerge Tests:\n");
    TEST(merge_engine_create);
    TEST(merge_single_candidate);
    TEST(merge_crc_wins);
    TEST(merge_highest_score);
    
    printf("\n═══════════════════════════════════════════════════════════════════\n");
    printf("Results: %d/%d tests passed\n", tests_passed, tests_run);
    printf("═══════════════════════════════════════════════════════════════════\n");
    
    return (tests_passed == tests_run) ? 0 : 1;
}
