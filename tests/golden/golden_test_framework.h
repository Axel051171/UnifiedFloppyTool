/**
 * @file golden_test_framework.h
 * @brief Golden Test Framework for Parser Regression Testing
 * 
 * P2-008: Known-good images as test fixtures
 * 
 * Usage:
 * 1. Place golden images in tests/golden/images/
 * 2. Place expected results in tests/golden/expected/
 * 3. Run: ./run_golden_tests
 */

#ifndef UFT_GOLDEN_TEST_H
#define UFT_GOLDEN_TEST_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Test Case Structure
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef enum {
    GOLDEN_TEST_CHECKSUM,       /* Compare checksums only */
    GOLDEN_TEST_SECTOR_CRC,     /* Compare sector CRCs */
    GOLDEN_TEST_FULL_COMPARE,   /* Full byte-by-byte compare */
    GOLDEN_TEST_METADATA,       /* Compare metadata JSON */
    GOLDEN_TEST_SECTOR_COUNT,   /* Just count readable sectors */
} golden_test_mode_t;

typedef struct {
    const char      *name;              /* Test name */
    const char      *image_path;        /* Path to golden image */
    const char      *expected_path;     /* Path to expected output */
    const char      *format;            /* Expected format (e.g. "adf", "d64") */
    golden_test_mode_t mode;
    
    /* Expected values */
    uint32_t        expected_crc32;     /* Expected CRC32 */
    int             expected_sectors;   /* Expected sector count */
    int             expected_errors;    /* Expected error count */
    double          min_quality;        /* Minimum quality score */
    
    /* Optional */
    const char      *description;
    bool            skip;               /* Skip this test */
} golden_test_case_t;

typedef struct {
    int             passed;
    int             failed;
    int             skipped;
    int             total;
    double          elapsed_ms;
    char            **failures;         /* List of failure messages */
    int             failure_count;
} golden_test_results_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * API
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Run a single golden test
 */
bool golden_test_run(const golden_test_case_t *tc, char *error_msg, size_t msg_size);

/**
 * @brief Run all tests in array
 */
void golden_test_run_all(const golden_test_case_t *tests, int count,
                         golden_test_results_t *results);

/**
 * @brief Discover and run all tests in directory
 */
void golden_test_discover(const char *test_dir, golden_test_results_t *results);

/**
 * @brief Generate expected output for a test case
 */
int golden_test_generate(const golden_test_case_t *tc, const char *output_path);

/**
 * @brief Print test results
 */
void golden_test_print_results(const golden_test_results_t *results);

/**
 * @brief Free results memory
 */
void golden_test_free_results(golden_test_results_t *results);

/**
 * @brief Export results to JUnit XML
 */
int golden_test_export_junit(const golden_test_results_t *results, 
                             const char *xml_path);

#ifdef __cplusplus
}
#endif

#endif /* UFT_GOLDEN_TEST_H */
