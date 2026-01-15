/**
 * @file test_merge_engine.c
 * @brief Unit tests for Multi-Read Merge Engine (W-P1-004)
 * 
 * Tests cover:
 * - Engine lifecycle (create/destroy/reset)
 * - Candidate addition
 * - Merge strategies (CRC wins, highest score, majority, latest)
 * - Track-level merge
 * - Simple sector merge
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include "uft/uft_merge_engine.h"

/*============================================================================
 * TEST FRAMEWORK
 *============================================================================*/

static int tests_run = 0;
static int tests_passed = 0;
static int current_test_failed = 0;

#define TEST(name) static void test_##name(void)
#define RUN_TEST(name) do { \
    printf("  [TEST] %s ... ", #name); \
    tests_run++; \
    current_test_failed = 0; \
    test_##name(); \
    if (!current_test_failed) { \
        tests_passed++; \
        printf("PASS\n"); \
    } \
} while(0)

#define ASSERT(cond) do { \
    if (!(cond)) { \
        printf("FAIL\n    Assertion failed: %s\n    at %s:%d\n", \
               #cond, __FILE__, __LINE__); \
        current_test_failed = 1; \
        return; \
    } \
} while(0)

#define ASSERT_EQ(a, b) ASSERT((a) == (b))
#define ASSERT_NE(a, b) ASSERT((a) != (b))
#define ASSERT_TRUE(x) ASSERT(x)
#define ASSERT_FALSE(x) ASSERT(!(x))
#define ASSERT_NOT_NULL(x) ASSERT((x) != NULL)
#define ASSERT_NULL(x) ASSERT((x) == NULL)

/*============================================================================
 * HELPER FUNCTIONS
 *============================================================================*/

static uft_sector_candidate_t make_candidate(
    int cyl, int head, int sector, int rev,
    bool crc_ok, uint8_t score, const uint8_t *data, size_t data_size)
{
    uft_sector_candidate_t c = {0};
    c.cylinder = cyl;
    c.head = head;
    c.sector = sector;
    c.source_revolution = rev;
    c.crc_ok = crc_ok;
    c.score.total = score;
    c.score.crc_ok = crc_ok;
    c.score.crc_score = crc_ok ? 40 : 0;
    c.data = (uint8_t *)data;
    c.data_size = data_size;
    return c;
}

/*============================================================================
 * TESTS: LIFECYCLE
 *============================================================================*/

TEST(engine_create_default) {
    uft_merge_engine_t *engine = uft_merge_engine_create(NULL);
    ASSERT_NOT_NULL(engine);
    uft_merge_engine_destroy(engine);
}

TEST(engine_create_with_config) {
    uft_merge_config_t config = {
        .strategy = UFT_MERGE_HIGHEST_SCORE,
        .min_agreements = 2,
        .max_revolutions = 10
    };
    
    uft_merge_engine_t *engine = uft_merge_engine_create(&config);
    ASSERT_NOT_NULL(engine);
    uft_merge_engine_destroy(engine);
}

TEST(engine_destroy_null) {
    /* Should not crash */
    uft_merge_engine_destroy(NULL);
}

TEST(engine_reset) {
    uft_merge_engine_t *engine = uft_merge_engine_create(NULL);
    ASSERT_NOT_NULL(engine);
    
    /* Add some data */
    uint8_t data[512] = {0};
    uft_sector_candidate_t c = make_candidate(0, 0, 1, 0, true, 80, data, 512);
    ASSERT_EQ(uft_merge_add_candidate(engine, &c), 0);
    
    /* Reset */
    uft_merge_reset(engine);
    
    /* Should be able to add again */
    ASSERT_EQ(uft_merge_add_candidate(engine, &c), 0);
    
    uft_merge_engine_destroy(engine);
}

/*============================================================================
 * TESTS: CANDIDATE ADDITION
 *============================================================================*/

TEST(add_single_candidate) {
    uft_merge_engine_t *engine = uft_merge_engine_create(NULL);
    ASSERT_NOT_NULL(engine);
    
    uint8_t data[512] = {0xAA};
    uft_sector_candidate_t c = make_candidate(5, 0, 3, 1, true, 85, data, 512);
    
    ASSERT_EQ(uft_merge_add_candidate(engine, &c), 0);
    
    uft_merge_engine_destroy(engine);
}

TEST(add_multiple_candidates_same_sector) {
    uft_merge_engine_t *engine = uft_merge_engine_create(NULL);
    ASSERT_NOT_NULL(engine);
    
    uint8_t data1[512] = {0xAA};
    uint8_t data2[512] = {0xBB};
    uint8_t data3[512] = {0xCC};
    
    uft_sector_candidate_t c1 = make_candidate(0, 0, 1, 0, false, 50, data1, 512);
    uft_sector_candidate_t c2 = make_candidate(0, 0, 1, 1, true, 90, data2, 512);
    uft_sector_candidate_t c3 = make_candidate(0, 0, 1, 2, true, 85, data3, 512);
    
    ASSERT_EQ(uft_merge_add_candidate(engine, &c1), 0);
    ASSERT_EQ(uft_merge_add_candidate(engine, &c2), 0);
    ASSERT_EQ(uft_merge_add_candidate(engine, &c3), 0);
    
    uft_merge_engine_destroy(engine);
}

TEST(add_multiple_sectors) {
    uft_merge_engine_t *engine = uft_merge_engine_create(NULL);
    ASSERT_NOT_NULL(engine);
    
    uint8_t data[512] = {0};
    
    for (int s = 1; s <= 9; s++) {
        uft_sector_candidate_t c = make_candidate(0, 0, s, 0, true, 80, data, 512);
        ASSERT_EQ(uft_merge_add_candidate(engine, &c), 0);
    }
    
    uft_merge_engine_destroy(engine);
}

TEST(add_null_candidate) {
    uft_merge_engine_t *engine = uft_merge_engine_create(NULL);
    ASSERT_NOT_NULL(engine);
    
    ASSERT_EQ(uft_merge_add_candidate(engine, NULL), -1);
    ASSERT_EQ(uft_merge_add_candidate(NULL, NULL), -1);
    
    uft_merge_engine_destroy(engine);
}

/*============================================================================
 * TESTS: MERGE STRATEGIES
 *============================================================================*/

TEST(merge_crc_wins_strategy) {
    uft_merge_config_t config = { .strategy = UFT_MERGE_CRC_WINS };
    uft_merge_engine_t *engine = uft_merge_engine_create(&config);
    ASSERT_NOT_NULL(engine);
    
    uint8_t data_bad[512] = {0xFF};
    uint8_t data_good[512] = {0xAA};
    
    /* Add bad CRC first, then good */
    uft_sector_candidate_t c1 = make_candidate(0, 0, 1, 0, false, 90, data_bad, 512);
    uft_sector_candidate_t c2 = make_candidate(0, 0, 1, 1, true, 70, data_good, 512);
    
    uft_merge_add_candidate(engine, &c1);
    uft_merge_add_candidate(engine, &c2);
    
    uft_merged_track_t track;
    int count = uft_merge_execute(engine, &track);
    
    ASSERT_EQ(count, 1);
    ASSERT_NOT_NULL(track.sectors);
    ASSERT_TRUE(track.sectors[0].final_score.crc_ok);  /* CRC OK should win */
    ASSERT_EQ(track.sectors[0].source_revolution, 1);   /* From rev 1 */
    
    uft_merged_track_free(&track);
    uft_merge_engine_destroy(engine);
}

TEST(merge_highest_score_strategy) {
    uft_merge_config_t config = { .strategy = UFT_MERGE_HIGHEST_SCORE };
    uft_merge_engine_t *engine = uft_merge_engine_create(&config);
    ASSERT_NOT_NULL(engine);
    
    uint8_t data1[512] = {0x11};
    uint8_t data2[512] = {0x22};
    uint8_t data3[512] = {0x33};
    
    uft_sector_candidate_t c1 = make_candidate(0, 0, 1, 0, false, 60, data1, 512);
    uft_sector_candidate_t c2 = make_candidate(0, 0, 1, 1, false, 95, data2, 512);  /* Highest */
    uft_sector_candidate_t c3 = make_candidate(0, 0, 1, 2, false, 80, data3, 512);
    
    uft_merge_add_candidate(engine, &c1);
    uft_merge_add_candidate(engine, &c2);
    uft_merge_add_candidate(engine, &c3);
    
    uft_merged_track_t track;
    int count = uft_merge_execute(engine, &track);
    
    ASSERT_EQ(count, 1);
    ASSERT_EQ(track.sectors[0].final_score.total, 95);
    ASSERT_EQ(track.sectors[0].source_revolution, 1);
    
    uft_merged_track_free(&track);
    uft_merge_engine_destroy(engine);
}

TEST(merge_latest_strategy) {
    uft_merge_config_t config = { .strategy = UFT_MERGE_LATEST };
    uft_merge_engine_t *engine = uft_merge_engine_create(&config);
    ASSERT_NOT_NULL(engine);
    
    uint8_t data1[512] = {0x11};
    uint8_t data2[512] = {0x22};
    uint8_t data3[512] = {0x33};
    
    uft_sector_candidate_t c1 = make_candidate(0, 0, 1, 0, true, 90, data1, 512);
    uft_sector_candidate_t c2 = make_candidate(0, 0, 1, 1, true, 85, data2, 512);
    uft_sector_candidate_t c3 = make_candidate(0, 0, 1, 2, true, 80, data3, 512);  /* Latest */
    
    uft_merge_add_candidate(engine, &c1);
    uft_merge_add_candidate(engine, &c2);
    uft_merge_add_candidate(engine, &c3);
    
    uft_merged_track_t track;
    int count = uft_merge_execute(engine, &track);
    
    ASSERT_EQ(count, 1);
    ASSERT_EQ(track.sectors[0].source_revolution, 2);  /* Latest wins */
    
    uft_merged_track_free(&track);
    uft_merge_engine_destroy(engine);
}

/*============================================================================
 * TESTS: TRACK MERGE
 *============================================================================*/

TEST(merge_full_track) {
    /* Use CRC_WINS to ensure CRC-OK sectors win */
    uft_merge_config_t config = { .strategy = UFT_MERGE_CRC_WINS };
    uft_merge_engine_t *engine = uft_merge_engine_create(&config);
    ASSERT_NOT_NULL(engine);
    
    uint8_t data[512] = {0};
    
    /* Add 9 sectors from 3 revolutions */
    for (int s = 1; s <= 9; s++) {
        for (int r = 0; r < 3; r++) {
            bool crc_ok = (r == 1);  /* Rev 1 always has good CRC */
            uint8_t score = 70 + r * 10;
            uft_sector_candidate_t c = make_candidate(5, 0, s, r, crc_ok, score, data, 512);
            uft_merge_add_candidate(engine, &c);
        }
    }
    
    uft_merged_track_t track;
    int count = uft_merge_execute(engine, &track);
    
    ASSERT_EQ(count, 9);
    ASSERT_EQ(track.cylinder, 5);
    ASSERT_EQ(track.head, 0);
    ASSERT_EQ(track.sector_count, 9);
    ASSERT_EQ(track.good_sectors, 9);  /* All from rev 1 with good CRC */
    
    uft_merged_track_free(&track);
    uft_merge_engine_destroy(engine);
}

TEST(merge_track_with_failures) {
    uft_merge_config_t config = { .strategy = UFT_MERGE_HIGHEST_SCORE };
    uft_merge_engine_t *engine = uft_merge_engine_create(&config);
    ASSERT_NOT_NULL(engine);
    
    uint8_t data[512] = {0};
    
    /* Add some good and some bad sectors */
    for (int s = 1; s <= 5; s++) {
        bool crc_ok = (s <= 3);  /* First 3 are good */
        uint8_t score = crc_ok ? 90 : 40;
        uft_sector_candidate_t c = make_candidate(10, 1, s, 0, crc_ok, score, data, 512);
        uft_merge_add_candidate(engine, &c);
    }
    
    uft_merged_track_t track;
    int count = uft_merge_execute(engine, &track);
    
    ASSERT_EQ(count, 5);
    ASSERT_EQ(track.good_sectors, 3);
    ASSERT_EQ(track.failed_sectors, 2);
    
    uft_merged_track_free(&track);
    uft_merge_engine_destroy(engine);
}

TEST(merge_empty_track) {
    uft_merge_engine_t *engine = uft_merge_engine_create(NULL);
    ASSERT_NOT_NULL(engine);
    
    uft_merged_track_t track;
    int count = uft_merge_execute(engine, &track);
    
    ASSERT_EQ(count, 0);
    ASSERT_EQ(track.sector_count, 0);
    
    uft_merged_track_free(&track);
    uft_merge_engine_destroy(engine);
}

/*============================================================================
 * TESTS: SIMPLE SECTOR MERGE
 *============================================================================*/

TEST(simple_sector_merge) {
    uint8_t data1[512] = {0x11};
    uint8_t data2[512] = {0x22};
    
    uft_sector_candidate_t candidates[2];
    candidates[0] = make_candidate(0, 0, 1, 0, false, 50, data1, 512);
    candidates[1] = make_candidate(0, 0, 1, 1, true, 90, data2, 512);
    
    uft_merged_sector_t result;
    int rc = uft_merge_sector_simple(candidates, 2, UFT_MERGE_CRC_WINS, &result);
    
    ASSERT_EQ(rc, 0);
    ASSERT_TRUE(result.final_score.crc_ok);
    ASSERT_EQ(result.source_revolution, 1);
    
    free(result.data);
}

TEST(simple_sector_merge_null) {
    uft_merged_sector_t result;
    
    ASSERT_EQ(uft_merge_sector_simple(NULL, 0, UFT_MERGE_CRC_WINS, &result), -1);
    ASSERT_EQ(uft_merge_sector_simple(NULL, 2, UFT_MERGE_CRC_WINS, &result), -1);
}

/*============================================================================
 * TESTS: STATISTICS
 *============================================================================*/

TEST(track_statistics) {
    uft_merge_engine_t *engine = uft_merge_engine_create(NULL);
    ASSERT_NOT_NULL(engine);
    
    uint8_t data[512] = {0};
    
    /* 3 good sectors, 2 bad */
    for (int s = 1; s <= 5; s++) {
        bool crc_ok = (s <= 3);
        uft_sector_candidate_t c1 = make_candidate(0, 0, s, 0, crc_ok, crc_ok ? 90 : 30, data, 512);
        uft_merge_add_candidate(engine, &c1);
        
        /* Add second read for bad sectors */
        if (!crc_ok) {
            uft_sector_candidate_t c2 = make_candidate(0, 0, s, 1, false, 35, data, 512);
            uft_merge_add_candidate(engine, &c2);
        }
    }
    
    uft_merged_track_t track;
    uft_merge_execute(engine, &track);
    
    ASSERT_EQ(track.sector_count, 5);
    ASSERT_EQ(track.good_sectors, 3);
    ASSERT_TRUE(track.track_score.confidence > 50);  /* >50% good */
    
    /* Check individual sector stats */
    for (int i = 0; i < track.sector_count; i++) {
        ASSERT_TRUE(track.sectors[i].total_candidates >= 1);
    }
    
    uft_merged_track_free(&track);
    uft_merge_engine_destroy(engine);
}

/*============================================================================
 * MAIN
 *============================================================================*/

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    
    printf("\n═══════════════════════════════════════════════════════════════════\n");
    printf("  UFT Merge Engine Tests (W-P1-004)\n");
    printf("═══════════════════════════════════════════════════════════════════\n\n");
    
    /* Lifecycle */
    printf("[SUITE] Lifecycle\n");
    RUN_TEST(engine_create_default);
    RUN_TEST(engine_create_with_config);
    RUN_TEST(engine_destroy_null);
    RUN_TEST(engine_reset);
    
    /* Candidate Addition */
    printf("\n[SUITE] Candidate Addition\n");
    RUN_TEST(add_single_candidate);
    RUN_TEST(add_multiple_candidates_same_sector);
    RUN_TEST(add_multiple_sectors);
    RUN_TEST(add_null_candidate);
    
    /* Merge Strategies */
    printf("\n[SUITE] Merge Strategies\n");
    RUN_TEST(merge_crc_wins_strategy);
    RUN_TEST(merge_highest_score_strategy);
    RUN_TEST(merge_latest_strategy);
    
    /* Track Merge */
    printf("\n[SUITE] Track Merge\n");
    RUN_TEST(merge_full_track);
    RUN_TEST(merge_track_with_failures);
    RUN_TEST(merge_empty_track);
    
    /* Simple Sector Merge */
    printf("\n[SUITE] Simple Sector Merge\n");
    RUN_TEST(simple_sector_merge);
    RUN_TEST(simple_sector_merge_null);
    
    /* Statistics */
    printf("\n[SUITE] Statistics\n");
    RUN_TEST(track_statistics);
    
    /* Summary */
    printf("\n═══════════════════════════════════════════════════════════════════\n");
    printf("  Results: %d passed, %d failed (of %d)\n", 
           tests_passed, tests_run - tests_passed, tests_run);
    printf("═══════════════════════════════════════════════════════════════════\n\n");
    
    return (tests_passed == tests_run) ? 0 : 1;
}
