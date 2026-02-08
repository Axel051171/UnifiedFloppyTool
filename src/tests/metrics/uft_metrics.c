/**
 * @file uft_metrics.c
 * @brief Algorithm quality metrics implementation
 */

#include "uft_metrics.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/* ============================================================================
 * BER Calculation
 * ============================================================================ */

static inline uint8_t get_bit(const uint8_t *data, size_t pos) {
    return (data[pos / 8] >> (7 - (pos % 8))) & 1;
}

uft_ber_metrics_t uft_ber_calculate(const uint8_t *expected,
                                    const uint8_t *actual,
                                    size_t len_bits) {
    uft_ber_metrics_t m = {0};
    m.total_bits = len_bits;
    m.first_error_pos = len_bits;  /* No error yet */
    
    size_t burst_len = 0;
    
    for (size_t i = 0; i < len_bits; i++) {
        uint8_t exp_bit = get_bit(expected, i);
        uint8_t act_bit = get_bit(actual, i);
        
        if (exp_bit != act_bit) {
            m.error_bits++;
            m.flip_errors++;
            
            if (m.first_error_pos == len_bits) {
                m.first_error_pos = i;
            }
            m.last_error_pos = i;
            
            burst_len++;
            if (burst_len > m.max_error_burst) {
                m.max_error_burst = burst_len;
            }
        } else {
            burst_len = 0;
        }
    }
    
    m.ber = (m.total_bits > 0) ? 
            (double)m.error_bits / m.total_bits : 0;
    
    return m;
}

uft_ber_metrics_t uft_ber_calculate_aligned(const uint8_t *expected,
                                             const uint8_t *actual,
                                             size_t len_bits,
                                             int max_shift) {
    uft_ber_metrics_t best = uft_ber_calculate(expected, actual, len_bits);
    
    /* Try different alignments */
    for (int shift = -max_shift; shift <= max_shift; shift++) {
        if (shift == 0) continue;
        
        size_t adj_len = len_bits;
        const uint8_t *exp_ptr = expected;
        const uint8_t *act_ptr = actual;
        size_t exp_offset = 0;
        size_t act_offset = 0;
        
        if (shift > 0) {
            act_offset = (size_t)shift;
            adj_len -= shift;
        } else {
            exp_offset = (size_t)(-shift);
            adj_len += shift;
        }
        
        /* Manual comparison with offset */
        size_t errors = 0;
        for (size_t i = 0; i < adj_len; i++) {
            uint8_t exp_bit = get_bit(exp_ptr, exp_offset + i);
            uint8_t act_bit = get_bit(act_ptr, act_offset + i);
            if (exp_bit != act_bit) errors++;
        }
        
        double ber = (double)errors / adj_len;
        if (ber < best.ber) {
            best = uft_ber_calculate(expected, actual, len_bits);
            best.ber = ber;
            best.error_bits = errors;
            /* Note: other fields may not be accurate with shift */
        }
    }
    
    return best;
}

/* ============================================================================
 * Sync Metrics
 * ============================================================================ */

uft_sync_metrics_t uft_sync_metrics_calculate(
    const size_t *expected_positions, size_t expected_count,
    const size_t *detected_positions, size_t detected_count,
    size_t tolerance) {
    
    uft_sync_metrics_t m = {0};
    m.total_syncs = expected_count;
    m.min_lock_time_bits = SIZE_MAX;
    
    /* Mark which expected syncs were found */
    bool *found = calloc(expected_count, sizeof(bool));
    bool *matched = calloc(detected_count, sizeof(bool));
    
    /* Match detected to expected */
    for (size_t d = 0; d < detected_count; d++) {
        bool is_match = false;
        
        for (size_t e = 0; e < expected_count; e++) {
            if (found[e]) continue;
            
            size_t diff = (detected_positions[d] > expected_positions[e]) ?
                          detected_positions[d] - expected_positions[e] :
                          expected_positions[e] - detected_positions[d];
            
            if (diff <= tolerance) {
                found[e] = true;
                matched[d] = true;
                m.found_syncs++;
                is_match = true;
                break;
            }
        }
        
        if (!is_match) {
            m.false_positives++;
        }
    }
    
    /* Count missed syncs */
    for (size_t e = 0; e < expected_count; e++) {
        if (!found[e]) {
            m.missed_syncs++;
        }
    }
    
    free(found);
    free(matched);
    
    /* Calculate rates */
    if (m.total_syncs > 0) {
        m.detection_rate = (double)m.found_syncs / m.total_syncs;
        m.recall = m.detection_rate;
    }
    
    if (m.found_syncs + m.false_positives > 0) {
        m.precision = (double)m.found_syncs / 
                     (m.found_syncs + m.false_positives);
    }
    
    if (m.precision + m.recall > 0) {
        m.f1_score = 2 * m.precision * m.recall / (m.precision + m.recall);
    }
    
    return m;
}

/* ============================================================================
 * CRC Metrics
 * ============================================================================ */

void uft_crc_metrics_init(uft_crc_metrics_t *m) {
    if (!m) return;
    memset(m, 0, sizeof(*m));
}

void uft_crc_metrics_record(uft_crc_metrics_t *m,
                            bool header_ok,
                            bool data_ok,
                            int retry_count) {
    if (!m) return;
    
    m->total_sectors++;
    
    if (header_ok) m->header_crc_pass++;
    if (data_ok) m->data_crc_pass++;
    if (header_ok && data_ok) m->both_crc_pass++;
    
    if (header_ok && !data_ok) m->data_only_fail++;
    if (!header_ok && data_ok) m->header_only_fail++;
    if (!header_ok && !data_ok) m->both_fail++;
    
    if (retry_count == 0 && header_ok && data_ok) {
        m->pass_first_try++;
    } else if (retry_count > 0 && retry_count < 8) {
        m->pass_after_retry[retry_count]++;
    }
    
    if ((size_t)retry_count > m->max_retries_needed) {
        m->max_retries_needed = retry_count;
    }
}

void uft_crc_metrics_finalize(uft_crc_metrics_t *m) {
    if (!m || m->total_sectors == 0) return;
    
    m->header_pass_rate = (double)m->header_crc_pass / m->total_sectors;
    m->data_pass_rate = (double)m->data_crc_pass / m->total_sectors;
    m->overall_pass_rate = (double)m->both_crc_pass / m->total_sectors;
    
    /* Calculate average retries */
    size_t total_retries = 0;
    size_t retry_ops = 0;
    for (int i = 1; i < 8; i++) {
        total_retries += m->pass_after_retry[i] * i;
        retry_ops += m->pass_after_retry[i];
    }
    if (retry_ops > 0) {
        m->avg_retries = (double)total_retries / retry_ops;
    }
}

/* ============================================================================
 * Confidence Metrics
 * ============================================================================ */

uft_confidence_metrics_t uft_confidence_calculate(
    const uint8_t *confidence_map, size_t bit_count) {
    
    uft_confidence_metrics_t m = {0};
    m.total_bits = bit_count;
    m.min = 255;
    
    if (!confidence_map || bit_count == 0) return m;
    
    /* Build histogram and calculate sum */
    double sum = 0;
    for (size_t i = 0; i < bit_count; i++) {
        uint8_t c = confidence_map[i];
        m.histogram[c]++;
        sum += c;
        
        if (c < m.min) m.min = c;
        if (c > m.max) m.max = c;
        
        if (c < 128) m.weak_bits++;
        else if (c < 200) m.marginal_bits++;
        else m.strong_bits++;
    }
    
    /* Mean */
    m.mean = sum / bit_count;
    
    /* Variance and stddev */
    double var_sum = 0;
    for (size_t i = 0; i < bit_count; i++) {
        double diff = confidence_map[i] - m.mean;
        var_sum += diff * diff;
    }
    m.stddev = sqrt(var_sum / bit_count);
    
    /* Median (from histogram) */
    size_t mid = bit_count / 2;
    size_t count = 0;
    for (int i = 0; i < 256; i++) {
        count += m.histogram[i];
        if (count >= mid) {
            m.median = (double)i;
            break;
        }
    }
    
    /* Ratios */
    m.weak_ratio = (double)m.weak_bits / bit_count;
    m.strong_ratio = (double)m.strong_bits / bit_count;
    
    return m;
}

/* ============================================================================
 * Retry Metrics
 * ============================================================================ */

void uft_retry_metrics_init(uft_retry_metrics_t *m) {
    if (!m) return;
    memset(m, 0, sizeof(*m));
}

void uft_retry_metrics_record(uft_retry_metrics_t *m,
                              bool success,
                              int retries,
                              uft_retry_reason_t reason) {
    if (!m) return;
    
    m->total_operations++;
    
    if (success) {
        if (retries == 0) {
            m->successful_first_try++;
        } else {
            m->required_retries += retries;
        }
    } else {
        m->permanent_failures++;
    }
    
    if ((size_t)retries > m->max_retries) {
        m->max_retries = retries;
    }
    
    /* Record reason */
    switch (reason) {
        case UFT_RETRY_CRC:     m->retry_crc_error++; break;
        case UFT_RETRY_SYNC:    m->retry_sync_lost++; break;
        case UFT_RETRY_TIMEOUT: m->retry_timeout++; break;
        case UFT_RETRY_WEAK:    m->retry_weak_bit++; break;
        case UFT_RETRY_OTHER:   m->retry_other++; break;
        default: break;
    }
}

void uft_retry_metrics_finalize(uft_retry_metrics_t *m) {
    if (!m || m->total_operations == 0) return;
    
    m->first_try_rate = (double)m->successful_first_try / m->total_operations;
    m->success_rate = (double)(m->total_operations - m->permanent_failures) / 
                      m->total_operations;
    
    size_t ops_with_retry = m->total_operations - m->successful_first_try - 
                            m->permanent_failures;
    if (ops_with_retry > 0) {
        m->avg_retries_when_needed = (double)m->required_retries / ops_with_retry;
    }
}

/* ============================================================================
 * Quality Report
 * ============================================================================ */

void uft_quality_report_generate(uft_quality_report_t *report) {
    if (!report) return;
    
    /* Calculate weighted quality score */
    double score = 0;
    
    /* BER component (40% weight) */
    if (report->ber.ber < 0.0001) score += 40;
    else if (report->ber.ber < 0.001) score += 35;
    else if (report->ber.ber < 0.01) score += 25;
    else if (report->ber.ber < 0.1) score += 10;
    
    /* CRC component (30% weight) */
    score += report->crc.overall_pass_rate * 30;
    
    /* Sync component (15% weight) */
    score += report->sync.f1_score * 15;
    
    /* Confidence component (10% weight) */
    score += report->confidence.strong_ratio * 10;
    
    /* Retry component (5% weight) */
    score += report->retry.first_try_rate * 5;
    
    report->quality_score = score;
    
    /* Assign grade */
    if (score >= 90) report->quality_grade = "A";
    else if (score >= 80) report->quality_grade = "B";
    else if (score >= 70) report->quality_grade = "C";
    else if (score >= 60) report->quality_grade = "D";
    else report->quality_grade = "F";
}

void uft_quality_report_print(const uft_quality_report_t *report) {
    if (!report) return;
    
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║           UFT ALGORITHM QUALITY REPORT                     ║\n");
    printf("╠════════════════════════════════════════════════════════════╣\n");
    printf("║ Overall Score: %5.1f / 100    Grade: %s                     ║\n",
           report->quality_score, report->quality_grade);
    printf("╠════════════════════════════════════════════════════════════╣\n");
    
    printf("║ BIT ERROR RATE                                             ║\n");
    printf("║   BER: %.6f (%zu errors / %zu bits)                     \n",
           report->ber.ber, report->ber.error_bits, report->ber.total_bits);
    printf("║   Max burst: %zu bits                                      \n",
           report->ber.max_error_burst);
    
    printf("╠════════════════════════════════════════════════════════════╣\n");
    printf("║ SYNC DETECTION                                             ║\n");
    printf("║   Detection: %.1f%% (%zu/%zu)                               \n",
           report->sync.detection_rate * 100,
           report->sync.found_syncs, report->sync.total_syncs);
    printf("║   Precision: %.1f%%  Recall: %.1f%%  F1: %.3f              \n",
           report->sync.precision * 100,
           report->sync.recall * 100,
           report->sync.f1_score);
    
    printf("╠════════════════════════════════════════════════════════════╣\n");
    printf("║ CRC PASS RATE                                              ║\n");
    printf("║   Header: %.1f%%  Data: %.1f%%  Both: %.1f%%                \n",
           report->crc.header_pass_rate * 100,
           report->crc.data_pass_rate * 100,
           report->crc.overall_pass_rate * 100);
    printf("║   First try: %zu/%zu (%.1f%%)                               \n",
           report->crc.pass_first_try, report->crc.total_sectors,
           (double)report->crc.pass_first_try / report->crc.total_sectors * 100);
    
    printf("╠════════════════════════════════════════════════════════════╣\n");
    printf("║ CONFIDENCE                                                 ║\n");
    printf("║   Mean: %.1f  Stddev: %.1f  Median: %.0f                   \n",
           report->confidence.mean,
           report->confidence.stddev,
           report->confidence.median);
    printf("║   Weak: %.1f%%  Strong: %.1f%%                              \n",
           report->confidence.weak_ratio * 100,
           report->confidence.strong_ratio * 100);
    
    printf("╠════════════════════════════════════════════════════════════╣\n");
    printf("║ RETRY PERFORMANCE                                          ║\n");
    printf("║   First try rate: %.1f%%                                   \n",
           report->retry.first_try_rate * 100);
    printf("║   Max retries: %zu  Avg when needed: %.1f                  \n",
           report->retry.max_retries,
           report->retry.avg_retries_when_needed);
    
    printf("╚════════════════════════════════════════════════════════════╝\n\n");
}

int uft_quality_report_to_json(const uft_quality_report_t *report,
                               char *buffer, size_t buffer_size) {
    if (!report || !buffer) return -1;
    
    return snprintf(buffer, buffer_size,
        "{\n"
        "  \"quality_score\": %.2f,\n"
        "  \"quality_grade\": \"%s\",\n"
        "  \"ber\": { \"value\": %.8f, \"errors\": %zu, \"total\": %zu },\n"
        "  \"sync\": { \"detection_rate\": %.4f, \"f1_score\": %.4f },\n"
        "  \"crc\": { \"pass_rate\": %.4f, \"first_try\": %zu },\n"
        "  \"confidence\": { \"mean\": %.2f, \"weak_ratio\": %.4f },\n"
        "  \"retry\": { \"first_try_rate\": %.4f, \"max\": %zu }\n"
        "}\n",
        report->quality_score,
        report->quality_grade,
        report->ber.ber, report->ber.error_bits, report->ber.total_bits,
        report->sync.detection_rate, report->sync.f1_score,
        report->crc.overall_pass_rate, report->crc.pass_first_try,
        report->confidence.mean, report->confidence.weak_ratio,
        report->retry.first_try_rate, report->retry.max_retries);
}

/* ============================================================================
 * Test Suite
 * ============================================================================ */

void uft_test_suite_init(uft_test_suite_t *suite) {
    if (!suite) return;
    memset(suite, 0, sizeof(*suite));
    
    suite->capacity = 32;
    suite->cases = calloc(suite->capacity, sizeof(uft_test_case_t));
}

int uft_test_suite_add(uft_test_suite_t *suite,
                       const char *name,
                       const char *input,
                       const char *reference) {
    if (!suite || !name) return -1;
    
    if (suite->case_count >= suite->capacity) {
        suite->capacity *= 2;
        suite->cases = realloc(suite->cases,
                              suite->capacity * sizeof(uft_test_case_t));
    }
    
    uft_test_case_t *tc = &suite->cases[suite->case_count++];
    memset(tc, 0, sizeof(*tc));
    tc->name = name;
    tc->input_file = input;
    tc->reference_file = reference;
    
    /* Default thresholds */
    tc->max_ber = 0.01;      /* 1% BER max */
    tc->min_crc_rate = 0.95; /* 95% CRC pass */
    tc->min_sync_rate = 0.99;/* 99% sync detection */
    tc->max_retries = 3;
    
    return (int)(suite->case_count - 1);
}

void uft_test_case_set_thresholds(uft_test_case_t *tc,
                                   double max_ber,
                                   double min_crc_rate,
                                   double min_sync_rate,
                                   int max_retries) {
    if (!tc) return;
    tc->max_ber = max_ber;
    tc->min_crc_rate = min_crc_rate;
    tc->min_sync_rate = min_sync_rate;
    tc->max_retries = max_retries;
}

void uft_test_suite_report(const uft_test_suite_t *suite) {
    if (!suite) return;
    
    printf("\n=== TEST SUITE RESULTS ===\n");
    printf("Passed: %zu / %zu\n", suite->passed, suite->case_count);
    printf("\n");
    
    for (size_t i = 0; i < suite->case_count; i++) {
        const uft_test_case_t *tc = &suite->cases[i];
        printf("[%s] %s", tc->passed ? "PASS" : "FAIL", tc->name);
        if (!tc->passed && tc->failure_reason) {
            printf(" - %s", tc->failure_reason);
        }
        printf("\n");
    }
    
    if (suite->case_count > 0) {
        printf("\nAggregate Metrics:\n");
        printf("  Avg BER: %.6f\n", suite->total_ber / suite->case_count);
        printf("  Avg CRC Rate: %.1f%%\n", 
               suite->total_crc_rate / suite->case_count * 100);
        printf("  Avg Sync Rate: %.1f%%\n",
               suite->total_sync_rate / suite->case_count * 100);
    }
}

void uft_test_suite_free(uft_test_suite_t *suite) {
    if (!suite) return;
    free(suite->cases);
    memset(suite, 0, sizeof(*suite));
}
