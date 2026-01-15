/**
 * @file test_operation_result.c
 * @brief Unit tests for Unified Operation Result System (W-P2-001)
 * 
 * Tests cover:
 * - Result initialization
 * - Status setting (success, partial, error)
 * - Detail appending
 * - Progress and timing
 * - String conversions
 * - Specialized initializers
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>

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
#define ASSERT_STR_CONTAINS(haystack, needle) ASSERT(strstr(haystack, needle) != NULL)

/*============================================================================
 * EXTERNAL DECLARATIONS
 *============================================================================*/

#include "uft/uft_operation_result.h"

/*============================================================================
 * TESTS: INITIALIZATION
 *============================================================================*/

TEST(result_init_basic) {
    uft_operation_result_t result;
    
    uft_result_init(&result, UFT_OP_READ);
    
    ASSERT_EQ(result.operation, UFT_OP_READ);
    ASSERT_EQ(result.status, UFT_STATUS_PENDING);
    ASSERT_EQ(result.error_code, UFT_SUCCESS);
    ASSERT_EQ(result.tracks.total, 0);
    ASSERT_EQ(result.sectors.total, 0);
}

TEST(result_init_all_operations) {
    uft_operation_result_t result;
    
    uft_operation_type_t ops[] = {
        UFT_OP_READ, UFT_OP_DECODE, UFT_OP_ANALYZE,
        UFT_OP_WRITE, UFT_OP_CONVERT, UFT_OP_VERIFY,
        UFT_OP_RECOVER, UFT_OP_COPY, UFT_OP_DETECT,
        UFT_OP_VALIDATE
    };
    
    for (size_t i = 0; i < sizeof(ops)/sizeof(ops[0]); i++) {
        uft_result_init(&result, ops[i]);
        ASSERT_EQ(result.operation, ops[i]);
        ASSERT_EQ(result.status, UFT_STATUS_PENDING);
    }
}

TEST(result_init_null_safe) {
    /* Should not crash */
    uft_result_init(NULL, UFT_OP_READ);
}

/*============================================================================
 * TESTS: STATUS SETTING
 *============================================================================*/

TEST(result_set_success) {
    uft_operation_result_t result;
    uft_result_init(&result, UFT_OP_READ);
    
    uft_result_set_success(&result, "Read completed successfully");
    
    ASSERT_EQ(result.status, UFT_STATUS_SUCCESS);
    ASSERT_EQ(result.error_code, UFT_SUCCESS);
    ASSERT_STR_CONTAINS(result.message, "Read completed");
}

TEST(result_set_partial) {
    uft_operation_result_t result;
    uft_result_init(&result, UFT_OP_READ);
    
    uft_result_set_partial(&result, "Read with 3 bad sectors");
    
    ASSERT_EQ(result.status, UFT_STATUS_PARTIAL);
    ASSERT_EQ(result.error_code, UFT_SUCCESS);  /* Partial is not an error */
    ASSERT_STR_CONTAINS(result.message, "bad sectors");
}

TEST(result_set_error) {
    uft_operation_result_t result;
    uft_result_init(&result, UFT_OP_READ);
    
    uft_result_set_error(&result, UFT_ERR_CRC, "CRC error on track 5");
    
    ASSERT_EQ(result.status, UFT_STATUS_FAILED);
    ASSERT_EQ(result.error_code, UFT_ERR_CRC);
    ASSERT_STR_CONTAINS(result.message, "CRC");
}

TEST(result_set_error_default_message) {
    uft_operation_result_t result;
    uft_result_init(&result, UFT_OP_READ);
    
    uft_result_set_error(&result, UFT_ERR_IO, NULL);
    
    ASSERT_EQ(result.status, UFT_STATUS_FAILED);
    ASSERT_EQ(result.error_code, UFT_ERR_IO);
    /* Should have default error string */
    ASSERT_TRUE(strlen(result.message) > 0);
}

/*============================================================================
 * TESTS: DETAIL APPENDING
 *============================================================================*/

TEST(result_append_detail_single) {
    uft_operation_result_t result;
    uft_result_init(&result, UFT_OP_ANALYZE);
    
    uft_result_append_detail(&result, "Track 5: weak bits detected");
    
    ASSERT_STR_CONTAINS(result.detail, "Track 5");
    ASSERT_STR_CONTAINS(result.detail, "weak bits");
}

TEST(result_append_detail_multiple) {
    uft_operation_result_t result;
    uft_result_init(&result, UFT_OP_ANALYZE);
    
    uft_result_append_detail(&result, "Line 1");
    uft_result_append_detail(&result, "Line 2");
    uft_result_append_detail(&result, "Line 3");
    
    ASSERT_STR_CONTAINS(result.detail, "Line 1");
    ASSERT_STR_CONTAINS(result.detail, "Line 2");
    ASSERT_STR_CONTAINS(result.detail, "Line 3");
    
    /* Should have newlines between entries */
    ASSERT_TRUE(strchr(result.detail, '\n') != NULL);
}

TEST(result_append_detail_null_safe) {
    uft_operation_result_t result;
    uft_result_init(&result, UFT_OP_ANALYZE);
    
    uft_result_append_detail(&result, NULL);
    uft_result_append_detail(NULL, "test");
    
    /* Should not crash, detail should be empty */
    ASSERT_EQ(strlen(result.detail), 0);
}

/*============================================================================
 * TESTS: PROGRESS AND TIMING
 *============================================================================*/

TEST(result_progress_basic) {
    uft_operation_result_t result;
    uft_result_init(&result, UFT_OP_READ);
    
    uft_result_set_progress(&result, 0.5f);
    
    ASSERT_EQ(result.status, UFT_STATUS_RUNNING);
    ASSERT_TRUE(result.timing.progress >= 0.49f && result.timing.progress <= 0.51f);
}

TEST(result_progress_clamping) {
    uft_operation_result_t result;
    uft_result_init(&result, UFT_OP_READ);
    
    /* Test below 0 */
    uft_result_set_progress(&result, -0.5f);
    ASSERT_TRUE(result.timing.progress >= 0.0f);
    
    /* Test above 1 */
    uft_result_set_progress(&result, 1.5f);
    ASSERT_TRUE(result.timing.progress <= 1.0f);
}

TEST(result_timing_start_stop) {
    uft_operation_result_t result;
    uft_result_init(&result, UFT_OP_READ);
    
    uft_result_start_timing(&result);
    ASSERT_EQ(result.status, UFT_STATUS_RUNNING);
    ASSERT_TRUE(result.timing.start_time > 0);
    
    /* Small delay */
    usleep(10000);  /* 10ms */
    
    uft_result_stop_timing(&result);
    ASSERT_TRUE(result.timing.end_time >= result.timing.start_time);
    ASSERT_TRUE(result.timing.progress >= 0.99f);
}

/*============================================================================
 * TESTS: STRING CONVERSIONS
 *============================================================================*/

TEST(operation_type_strings) {
    ASSERT_STR_CONTAINS(uft_operation_type_str(UFT_OP_READ), "Read");
    ASSERT_STR_CONTAINS(uft_operation_type_str(UFT_OP_WRITE), "Write");
    ASSERT_STR_CONTAINS(uft_operation_type_str(UFT_OP_VERIFY), "Verify");
    ASSERT_STR_CONTAINS(uft_operation_type_str(UFT_OP_CONVERT), "Convert");
}

TEST(operation_status_strings) {
    ASSERT_STR_CONTAINS(uft_operation_status_str(UFT_STATUS_SUCCESS), "Success");
    ASSERT_STR_CONTAINS(uft_operation_status_str(UFT_STATUS_FAILED), "Failed");
    ASSERT_STR_CONTAINS(uft_operation_status_str(UFT_STATUS_PARTIAL), "Partial");
    ASSERT_STR_CONTAINS(uft_operation_status_str(UFT_STATUS_RUNNING), "Running");
}

TEST(result_summary_generation) {
    uft_operation_result_t result;
    uft_result_init(&result, UFT_OP_READ);
    
    result.tracks.total = 80;
    result.tracks.good = 78;
    result.tracks.bad = 2;
    result.sectors.total = 1440;
    result.sectors.good = 1435;
    result.sectors.crc_error = 3;
    result.sectors.missing = 2;
    
    uft_result_set_partial(&result, "Read with errors");
    
    char summary[512];
    size_t len = uft_result_summary(&result, summary, sizeof(summary));
    
    ASSERT_TRUE(len > 0);
    ASSERT_STR_CONTAINS(summary, "Read");
    ASSERT_STR_CONTAINS(summary, "Partial");
    ASSERT_STR_CONTAINS(summary, "78");  /* good tracks */
    ASSERT_STR_CONTAINS(summary, "1435"); /* good sectors */
}

/*============================================================================
 * TESTS: SPECIALIZED INITIALIZERS
 *============================================================================*/

TEST(result_not_implemented) {
    uft_operation_result_t result;
    
    uft_result_not_implemented(&result, UFT_OP_WRITE, "HD write support");
    
    ASSERT_EQ(result.operation, UFT_OP_WRITE);
    ASSERT_EQ(result.status, UFT_STATUS_NOT_IMPLEMENTED);
    ASSERT_STR_CONTAINS(result.message, "Not implemented");
    ASSERT_STR_CONTAINS(result.detail, "TODO");
}

TEST(result_no_hardware) {
    uft_operation_result_t result;
    
    uft_result_no_hardware(&result, "Greaseweazle");
    
    ASSERT_EQ(result.status, UFT_STATUS_FAILED);
    ASSERT_STR_CONTAINS(result.message, "Greaseweazle");
    ASSERT_STR_CONTAINS(result.message, "not connected");
    ASSERT_STR_CONTAINS(result.detail, "USB");
}

TEST(result_cancelled) {
    uft_operation_result_t result;
    uft_result_init(&result, UFT_OP_COPY);
    uft_result_start_timing(&result);
    
    uft_result_cancelled(&result);
    
    ASSERT_EQ(result.status, UFT_STATUS_CANCELLED);
    ASSERT_STR_CONTAINS(result.message, "cancelled");
}

/*============================================================================
 * TESTS: MACROS
 *============================================================================*/

TEST(result_macros) {
    uft_operation_result_t result;
    
    /* Test UFT_RESULT_OK */
    uft_result_init(&result, UFT_OP_READ);
    ASSERT_FALSE(UFT_RESULT_OK(&result));  /* Pending is not OK */
    
    uft_result_set_success(&result, "Done");
    ASSERT_TRUE(UFT_RESULT_OK(&result));
    
    /* Test UFT_RESULT_FAILED */
    uft_result_set_error(&result, UFT_ERR_IO, "Error");
    ASSERT_TRUE(UFT_RESULT_FAILED(&result));
    
    /* Test UFT_RESULT_COMPLETED */
    uft_result_set_partial(&result, "Partial");
    ASSERT_TRUE(UFT_RESULT_COMPLETED(&result));
    
    uft_result_set_success(&result, "Success");
    ASSERT_TRUE(UFT_RESULT_COMPLETED(&result));
}

TEST(sector_error_rate_macro) {
    uft_operation_result_t result;
    uft_result_init(&result, UFT_OP_READ);
    
    /* No sectors - should be 0 */
    float rate = UFT_SECTOR_ERROR_RATE(&result);
    ASSERT_TRUE(rate < 0.01f);
    
    /* Some errors */
    result.sectors.total = 100;
    result.sectors.crc_error = 5;
    result.sectors.missing = 5;
    
    rate = UFT_SECTOR_ERROR_RATE(&result);
    ASSERT_TRUE(rate >= 0.09f && rate <= 0.11f);  /* Should be ~0.10 */
}

/*============================================================================
 * TESTS: STATISTICS
 *============================================================================*/

TEST(result_statistics_tracks) {
    uft_operation_result_t result;
    uft_result_init(&result, UFT_OP_READ);
    
    result.tracks.total = 160;
    result.tracks.good = 155;
    result.tracks.weak = 3;
    result.tracks.bad = 2;
    result.tracks.skipped = 0;
    
    ASSERT_EQ(result.tracks.total, 160);
    ASSERT_EQ(result.tracks.good + result.tracks.weak + result.tracks.bad, 160);
}

TEST(result_statistics_sectors) {
    uft_operation_result_t result;
    uft_result_init(&result, UFT_OP_READ);
    
    result.sectors.total = 2880;
    result.sectors.good = 2870;
    result.sectors.crc_error = 5;
    result.sectors.header_error = 2;
    result.sectors.missing = 3;
    result.sectors.recovered = 4;
    result.sectors.weak_bits = 1;
    
    ASSERT_EQ(result.sectors.total, 2880);
}

TEST(result_statistics_bytes) {
    uft_operation_result_t result;
    uft_result_init(&result, UFT_OP_READ);
    
    result.bytes.total_read = 1474560;  /* 1.44MB floppy */
    result.bytes.good = 1474000;
    result.bytes.uncertain = 500;
    result.bytes.bad = 60;
    
    ASSERT_EQ(result.bytes.total_read, 1474560);
}

/*============================================================================
 * MAIN
 *============================================================================*/

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    
    printf("\n═══════════════════════════════════════════════════════════════════\n");
    printf("  UFT Operation Result Tests (W-P2-001)\n");
    printf("═══════════════════════════════════════════════════════════════════\n\n");
    
    /* Initialization */
    printf("[SUITE] Initialization\n");
    RUN_TEST(result_init_basic);
    RUN_TEST(result_init_all_operations);
    RUN_TEST(result_init_null_safe);
    
    /* Status setting */
    printf("\n[SUITE] Status Setting\n");
    RUN_TEST(result_set_success);
    RUN_TEST(result_set_partial);
    RUN_TEST(result_set_error);
    RUN_TEST(result_set_error_default_message);
    
    /* Detail appending */
    printf("\n[SUITE] Detail Appending\n");
    RUN_TEST(result_append_detail_single);
    RUN_TEST(result_append_detail_multiple);
    RUN_TEST(result_append_detail_null_safe);
    
    /* Progress and timing */
    printf("\n[SUITE] Progress & Timing\n");
    RUN_TEST(result_progress_basic);
    RUN_TEST(result_progress_clamping);
    RUN_TEST(result_timing_start_stop);
    
    /* String conversions */
    printf("\n[SUITE] String Conversions\n");
    RUN_TEST(operation_type_strings);
    RUN_TEST(operation_status_strings);
    RUN_TEST(result_summary_generation);
    
    /* Specialized initializers */
    printf("\n[SUITE] Specialized Initializers\n");
    RUN_TEST(result_not_implemented);
    RUN_TEST(result_no_hardware);
    RUN_TEST(result_cancelled);
    
    /* Macros */
    printf("\n[SUITE] Convenience Macros\n");
    RUN_TEST(result_macros);
    RUN_TEST(sector_error_rate_macro);
    
    /* Statistics */
    printf("\n[SUITE] Statistics\n");
    RUN_TEST(result_statistics_tracks);
    RUN_TEST(result_statistics_sectors);
    RUN_TEST(result_statistics_bytes);
    
    /* Summary */
    printf("\n═══════════════════════════════════════════════════════════════════\n");
    printf("  Results: %d passed, %d failed (of %d)\n", 
           tests_passed, tests_run - tests_passed, tests_run);
    printf("═══════════════════════════════════════════════════════════════════\n\n");
    
    return (tests_passed == tests_run) ? 0 : 1;
}
