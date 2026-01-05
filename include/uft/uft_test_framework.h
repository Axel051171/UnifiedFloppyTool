/**
 * @file uft_test_framework.h
 * @brief Comprehensive Test Framework
 * 
 * TEST-KATEGORIEN:
 * ════════════════════════════════════════════════════════════════════════════
 * 
 *   1. GOLDEN TESTS (Regression)
 *      - Bekannte gute Images pro Format
 *      - Hash-Vergleich nach Verarbeitung
 *      - Sektor-für-Sektor Verifikation
 * 
 *   2. FUZZ TESTS (Security)
 *      - AFL/libFuzzer Harnesses
 *      - Mutation-basiert
 *      - Coverage-guided
 * 
 *   3. PROPERTY TESTS (Invarianten)
 *      - Roundtrip: decode(encode(x)) == x
 *      - Idempotenz: parse(parse(x)) == parse(x)
 *      - Bounds: alle Offsets in Range
 * 
 *   4. SECURITY TESTS (Crash-Klassen)
 *      - Out-of-bounds Read/Write
 *      - Integer Overflow/Underflow
 *      - Null Pointer Dereference
 *      - Use-after-free
 *      - Double-free
 * 
 * BUG → TEST → FIX WORKFLOW:
 * ════════════════════════════════════════════════════════════════════════════
 * 
 *   ┌─────────────┐    ┌─────────────┐    ┌─────────────┐    ┌─────────────┐
 *   │  Bug found  │ →  │ Create      │ →  │  Fix bug    │ →  │  Test       │
 *   │  (crash/    │    │ repro test  │    │             │    │  passes     │
 *   │   wrong)    │    │             │    │             │    │             │
 *   └─────────────┘    └─────────────┘    └─────────────┘    └─────────────┘
 *          │                  │                  │                  │
 *          ▼                  ▼                  ▼                  ▼
 *   tests/regression/  tests/regression/  src/...         CI/CD gate
 *   issue_NNN.bin      test_issue_NNN.c
 */

#ifndef UFT_TEST_FRAMEWORK_H
#define UFT_TEST_FRAMEWORK_H

#include "uft_types.h"
#include "uft_error.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Test Result Types
// ============================================================================

typedef enum uft_test_result {
    UFT_TEST_PASS       = 0,
    UFT_TEST_FAIL       = 1,
    UFT_TEST_SKIP       = 2,
    UFT_TEST_CRASH      = 3,
    UFT_TEST_TIMEOUT    = 4,
} uft_test_result_t;

typedef struct uft_test_stats {
    int     total;
    int     passed;
    int     failed;
    int     skipped;
    int     crashed;
    double  duration_ms;
} uft_test_stats_t;

// ============================================================================
// Golden Test Definition
// ============================================================================

typedef struct uft_golden_test {
    const char*     name;
    const char*     description;
    uft_format_t    format;
    const char*     variant;        // "D64-35", "ADF-DD", etc.
    
    // Test file
    const char*     input_path;
    size_t          expected_size;
    uint32_t        expected_crc32;
    
    // Expected results
    int             expected_cylinders;
    int             expected_heads;
    int             expected_sectors;
    int             expected_errors;
    
    // Sector hashes (for detailed verification)
    const uint32_t* sector_crc32s;  // NULL or array of CRCs
    int             sector_count;
    
    // Test flags
    bool            test_read;
    bool            test_write;
    bool            test_roundtrip;
    bool            test_conversion;
} uft_golden_test_t;

// ============================================================================
// Fuzz Target Definition
// ============================================================================

typedef enum uft_fuzz_target {
    UFT_FUZZ_FORMAT_PROBE,      // Format detection
    UFT_FUZZ_D64_PARSER,
    UFT_FUZZ_ADF_PARSER,
    UFT_FUZZ_SCP_PARSER,
    UFT_FUZZ_G64_PARSER,
    UFT_FUZZ_HFE_PARSER,
    UFT_FUZZ_IMG_PARSER,
    UFT_FUZZ_IPF_PARSER,
    UFT_FUZZ_PLL_DECODER,
    UFT_FUZZ_GCR_DECODER,
    UFT_FUZZ_MFM_DECODER,
} uft_fuzz_target_t;

typedef struct uft_fuzz_config {
    uft_fuzz_target_t   target;
    size_t              max_input_size;
    size_t              min_input_size;
    int                 timeout_ms;
    bool                detect_leaks;
    bool                detect_ub;
} uft_fuzz_config_t;

// ============================================================================
// Security Test Definition
// ============================================================================

typedef enum uft_crash_class {
    UFT_CRASH_OOB_READ,         // Out-of-bounds read
    UFT_CRASH_OOB_WRITE,        // Out-of-bounds write
    UFT_CRASH_INT_OVERFLOW,     // Integer overflow
    UFT_CRASH_INT_UNDERFLOW,    // Integer underflow
    UFT_CRASH_NULL_DEREF,       // Null pointer dereference
    UFT_CRASH_USE_AFTER_FREE,   // Use after free
    UFT_CRASH_DOUBLE_FREE,      // Double free
    UFT_CRASH_STACK_OVERFLOW,   // Stack buffer overflow
    UFT_CRASH_HEAP_OVERFLOW,    // Heap buffer overflow
    UFT_CRASH_DIV_BY_ZERO,      // Division by zero
    UFT_CRASH_ASSERTION,        // Assertion failure
} uft_crash_class_t;

typedef struct uft_security_test {
    const char*         name;
    uft_crash_class_t   expected_crash;
    const char*         description;
    
    // Malformed input
    const uint8_t*      data;
    size_t              size;
    
    // What to test
    uft_fuzz_target_t   target;
    
    // Expected behavior
    bool                should_crash;       // false = should handle gracefully
    uft_error_t         expected_error;     // If should_crash == false
} uft_security_test_t;

// ============================================================================
// Regression Test (Bug Repro)
// ============================================================================

typedef struct uft_regression_test {
    const char*     issue_id;       // "ISSUE-123", "CVE-2024-XXX"
    const char*     description;
    const char*     input_path;     // Path to repro file
    uft_crash_class_t crash_class;
    bool            fixed;          // true after fix
    const char*     fix_commit;     // Git commit hash
} uft_regression_test_t;

// ============================================================================
// Test Runner API
// ============================================================================

/**
 * @brief Run all golden tests
 */
uft_test_result_t uft_test_run_golden(uft_test_stats_t* stats);

/**
 * @brief Run golden test for specific format
 */
uft_test_result_t uft_test_run_golden_format(uft_format_t format,
                                              uft_test_stats_t* stats);

/**
 * @brief Run single golden test
 */
uft_test_result_t uft_test_run_golden_single(const uft_golden_test_t* test);

/**
 * @brief Run all security tests
 */
uft_test_result_t uft_test_run_security(uft_test_stats_t* stats);

/**
 * @brief Run all regression tests
 */
uft_test_result_t uft_test_run_regression(uft_test_stats_t* stats);

/**
 * @brief Create regression test from crash
 */
uft_error_t uft_test_create_regression(const char* issue_id,
                                        const uint8_t* crash_input,
                                        size_t crash_size,
                                        uft_crash_class_t crash_class,
                                        const char* description);

// ============================================================================
// Fuzzing API
// ============================================================================

/**
 * @brief Initialize fuzzing for target
 */
void uft_fuzz_init(uft_fuzz_target_t target);

/**
 * @brief Fuzz entry point (called by AFL/libFuzzer)
 */
int uft_fuzz_one_input(const uint8_t* data, size_t size);

/**
 * @brief Get fuzz config for target
 */
const uft_fuzz_config_t* uft_fuzz_get_config(uft_fuzz_target_t target);

// ============================================================================
// Coverage API
// ============================================================================

typedef struct uft_coverage_stats {
    int     lines_total;
    int     lines_covered;
    int     branches_total;
    int     branches_covered;
    double  line_coverage_pct;
    double  branch_coverage_pct;
} uft_coverage_stats_t;

void uft_coverage_reset(void);
void uft_coverage_report(uft_coverage_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif // UFT_TEST_FRAMEWORK_H
