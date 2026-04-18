/**
 * @file uft_metrics.h
 * @brief Algorithm quality metrics and testing framework
 * 
 * Provides reproducible metrics for evaluating decoder quality:
 * - BER (Bit Error Rate)
 * - Sync Lock Quality
 * - CRC Pass Rate
 * - Confidence Distribution
 * - Retry Rate
 */

#ifndef UFT_METRICS_H
#define UFT_METRICS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Bit Error Rate (BER)
 * ============================================================================ */

typedef struct {
    size_t total_bits;
    size_t error_bits;
    double ber;                 /**< error_bits / total_bits */
    
    /* Error classification */
    size_t insertion_errors;    /**< Extra bits (run length +1) */
    size_t deletion_errors;     /**< Missing bits (run length -1) */
    size_t flip_errors;         /**< Bit value inverted */
    
    /* Position info */
    size_t first_error_pos;
    size_t last_error_pos;
    size_t max_error_burst;     /**< Longest consecutive errors */
    
} uft_ber_metrics_t;

/**
 * @brief Calculate BER by comparing expected vs actual
 */
uft_ber_metrics_t uft_ber_calculate(const uint8_t *expected,
                                    const uint8_t *actual,
                                    size_t len_bits);

/**
 * @brief Calculate BER with tolerance for timing shifts
 */
uft_ber_metrics_t uft_ber_calculate_aligned(const uint8_t *expected,
                                             const uint8_t *actual,
                                             size_t len_bits,
                                             int max_shift);

/* ============================================================================
 * Sync Lock Quality
 * ============================================================================ */

typedef struct {
    size_t total_syncs;         /**< Expected number of syncs */
    size_t found_syncs;         /**< Actually found */
    size_t false_positives;     /**< Wrong detections */
    size_t missed_syncs;        /**< Not found */
    
    double detection_rate;      /**< found / total */
    double precision;           /**< found / (found + false_pos) */
    double recall;              /**< found / (found + missed) */
    double f1_score;            /**< Harmonic mean of precision/recall */
    
    double avg_lock_time_bits;  /**< Average bits to achieve lock */
    double max_lock_time_bits;
    double min_lock_time_bits;
    
    size_t lock_losses;         /**< Times lock was lost */
    double avg_lock_duration;   /**< Average bits of stable lock */
    
} uft_sync_metrics_t;

/**
 * @brief Calculate sync detection quality
 * @param expected_positions Array of expected sync positions
 * @param expected_count Number of expected syncs
 * @param detected_positions Array of detected sync positions
 * @param detected_count Number of detected syncs
 * @param tolerance Position tolerance in bits
 */
uft_sync_metrics_t uft_sync_metrics_calculate(
    const size_t *expected_positions, size_t expected_count,
    const size_t *detected_positions, size_t detected_count,
    size_t tolerance);

/* ============================================================================
 * CRC Pass Rate
 * ============================================================================ */

typedef struct {
    size_t total_sectors;
    size_t header_crc_pass;
    size_t data_crc_pass;
    size_t both_crc_pass;       /**< Header AND data pass */
    
    double header_pass_rate;
    double data_pass_rate;
    double overall_pass_rate;
    
    /* By retry count */
    size_t pass_first_try;
    size_t pass_after_retry[8]; /**< Index = retry number */
    size_t max_retries_needed;
    double avg_retries;
    
    /* Error analysis */
    size_t header_only_fail;
    size_t data_only_fail;
    size_t both_fail;
    
} uft_crc_metrics_t;

/**
 * @brief Initialize CRC metrics
 */
void uft_crc_metrics_init(uft_crc_metrics_t *m);

/**
 * @brief Record a sector read result
 */
void uft_crc_metrics_record(uft_crc_metrics_t *m,
                            bool header_ok,
                            bool data_ok,
                            int retry_count);

/**
 * @brief Finalize metrics (calculate rates)
 */
void uft_crc_metrics_finalize(uft_crc_metrics_t *m);

/* ============================================================================
 * Confidence Distribution
 * ============================================================================ */

typedef struct {
    size_t histogram[256];      /**< Count per confidence level */
    size_t total_bits;
    
    double mean;
    double stddev;
    double median;
    uint8_t min;
    uint8_t max;
    
    size_t weak_bits;           /**< Confidence < 128 */
    size_t marginal_bits;       /**< 128 <= confidence < 200 */
    size_t strong_bits;         /**< Confidence >= 200 */
    
    double weak_ratio;
    double strong_ratio;
    
} uft_confidence_metrics_t;

/**
 * @brief Calculate confidence distribution
 */
uft_confidence_metrics_t uft_confidence_calculate(
    const uint8_t *confidence_map, size_t bit_count);

/* ============================================================================
 * Retry Rate
 * ============================================================================ */

typedef struct {
    size_t total_operations;
    size_t successful_first_try;
    size_t required_retries;
    size_t permanent_failures;
    
    double first_try_rate;
    double success_rate;        /**< Eventually successful */
    double avg_retries_when_needed;
    size_t max_retries;
    
    /* Retry reason breakdown */
    size_t retry_crc_error;
    size_t retry_sync_lost;
    size_t retry_timeout;
    size_t retry_weak_bit;
    size_t retry_other;
    
} uft_retry_metrics_t;

/**
 * @brief Initialize retry metrics
 */
void uft_retry_metrics_init(uft_retry_metrics_t *m);

/**
 * @brief Record an operation result
 * @param m Metrics structure
 * @param success Final success status
 * @param retries Number of retries needed
 * @param reason Retry reason (if any)
 */
typedef enum {
    UFT_RETRY_NONE,
    UFT_RETRY_CRC,
    UFT_RETRY_SYNC,
    UFT_RETRY_TIMEOUT,
    UFT_RETRY_WEAK,
    UFT_RETRY_OTHER
} uft_retry_reason_t;

void uft_retry_metrics_record(uft_retry_metrics_t *m,
                              bool success,
                              int retries,
                              uft_retry_reason_t reason);

/**
 * @brief Finalize retry metrics
 */
void uft_retry_metrics_finalize(uft_retry_metrics_t *m);

/* ============================================================================
 * Combined Quality Score
 * ============================================================================ */

typedef struct {
    uft_ber_metrics_t ber;
    uft_sync_metrics_t sync;
    uft_crc_metrics_t crc;
    uft_confidence_metrics_t confidence;
    uft_retry_metrics_t retry;
    
    /* Weighted overall score 0-100 */
    double quality_score;
    const char *quality_grade;  /**< "A", "B", "C", "D", "F" */
    
} uft_quality_report_t;

/**
 * @brief Generate comprehensive quality report
 */
void uft_quality_report_generate(uft_quality_report_t *report);

/**
 * @brief Print quality report
 */
void uft_quality_report_print(const uft_quality_report_t *report);

/**
 * @brief Export report as JSON
 */
int uft_quality_report_to_json(const uft_quality_report_t *report,
                               char *buffer, size_t buffer_size);

/* ============================================================================
 * Test Suite
 * ============================================================================ */

typedef struct {
    const char *name;
    const char *description;
    const char *input_file;
    const char *reference_file;
    
    /* Expected metrics (pass/fail thresholds) */
    double max_ber;
    double min_crc_rate;
    double min_sync_rate;
    int max_retries;
    
    /* Actual results */
    uft_quality_report_t result;
    bool passed;
    const char *failure_reason;
    
} uft_test_case_t;

typedef struct {
    uft_test_case_t *cases;
    size_t case_count;
    size_t capacity;
    
    size_t passed;
    size_t failed;
    
    /* Aggregate metrics */
    double total_ber;
    double total_crc_rate;
    double total_sync_rate;
    
} uft_test_suite_t;

/**
 * @brief Initialize test suite
 */
void uft_test_suite_init(uft_test_suite_t *suite);

/**
 * @brief Add test case
 */
int uft_test_suite_add(uft_test_suite_t *suite,
                       const char *name,
                       const char *input,
                       const char *reference);

/**
 * @brief Set thresholds for a test case
 */
void uft_test_case_set_thresholds(uft_test_case_t *tc,
                                   double max_ber,
                                   double min_crc_rate,
                                   double min_sync_rate,
                                   int max_retries);

/**
 * @brief Run all tests
 */
int uft_test_suite_run(uft_test_suite_t *suite);

/**
 * @brief Print test results
 */
void uft_test_suite_report(const uft_test_suite_t *suite);

/**
 * @brief Free test suite resources
 */
void uft_test_suite_free(uft_test_suite_t *suite);

#ifdef __cplusplus
}
#endif

#endif /* UFT_METRICS_H */
