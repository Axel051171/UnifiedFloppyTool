/**
 * @file uft_decoder_metrics.c
 * @brief GOD MODE: Unified Decoder Metrics Tracking
 * 
 * Tracks BER, Sync Rate, CRC Rate, and Confidence for
 * reproducible testing and benchmarking.
 * 
 * @author Superman QA - GOD MODE
 * @date 2026
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <stdlib.h>

// ============================================================================
// Metric Types
// ============================================================================

typedef struct {
    // Bit Error Rate
    uint64_t total_bits;
    uint64_t error_bits;
    double ber;
    double ber_log10;           // log10(ber) for display
    
} ber_metric_t;

typedef struct {
    // Sync Lock Rate
    int expected_syncs;
    int found_exact;
    int found_fuzzy;
    int missed;
    double lock_rate;           // (exact + fuzzy) / expected
    double exact_rate;
    double avg_hamming;
    
} sync_metric_t;

typedef struct {
    // CRC/Checksum Pass Rate
    int total_sectors;
    int pass_original;
    int pass_corrected;
    int fail;
    double pass_rate_original;
    double pass_rate_final;
    int corrected_1bit;
    int corrected_2bit;
    
} crc_metric_t;

typedef struct {
    // Overall Confidence Score
    float timing;               // PLL stability
    float sync;                 // Sync quality
    float data;                 // CRC/checksum
    float consistency;          // Multi-rev agreement
    float overall;              // Weighted average
    
} confidence_metric_t;

typedef struct {
    // Retry Statistics
    int total_operations;
    int operations_with_retry;
    int total_retries;
    int max_retries_single;
    double avg_retries;
    double retry_rate;
    
} retry_metric_t;

typedef struct {
    // Combined metrics
    ber_metric_t ber;
    sync_metric_t sync;
    crc_metric_t crc;
    confidence_metric_t confidence;
    retry_metric_t retry;
    
    // Timing
    double decode_time_ms;
    double throughput_mbps;
    
    // Identity
    char test_name[64];
    char format[32];
    int track;
    int head;
    
} decoder_metrics_t;

// ============================================================================
// Metric Calculation
// ============================================================================

void calculate_ber(const uint8_t* decoded, const uint8_t* reference,
                   size_t num_bytes, ber_metric_t* metric) {
    metric->total_bits = num_bytes * 8;
    metric->error_bits = 0;
    
    for (size_t i = 0; i < num_bytes; i++) {
        uint8_t diff = decoded[i] ^ reference[i];
        // Population count
        while (diff) {
            metric->error_bits += diff & 1;
            diff >>= 1;
        }
    }
    
    metric->ber = (metric->total_bits > 0) ?
                  (double)metric->error_bits / metric->total_bits : 0;
    
    metric->ber_log10 = (metric->ber > 0) ? log10(metric->ber) : -10;
}

void update_crc_metrics(crc_metric_t* metric, bool original_ok,
                        bool final_ok, int bits_corrected) {
    metric->total_sectors++;
    
    if (original_ok) {
        metric->pass_original++;
        metric->pass_corrected++;
    } else if (final_ok) {
        metric->pass_corrected++;
        if (bits_corrected == 1) metric->corrected_1bit++;
        else if (bits_corrected == 2) metric->corrected_2bit++;
    } else {
        metric->fail++;
    }
    
    metric->pass_rate_original = (metric->total_sectors > 0) ?
        (double)metric->pass_original / metric->total_sectors * 100 : 0;
    metric->pass_rate_final = (metric->total_sectors > 0) ?
        (double)metric->pass_corrected / metric->total_sectors * 100 : 0;
}

void calculate_confidence(confidence_metric_t* conf) {
    // Weighted average
    const float weights[] = {0.20f, 0.30f, 0.35f, 0.15f};
    
    conf->overall = conf->timing * weights[0] +
                    conf->sync * weights[1] +
                    conf->data * weights[2] +
                    conf->consistency * weights[3];
}

// ============================================================================
// Metrics Reporting
// ============================================================================

void metrics_print_summary(const decoder_metrics_t* m, FILE* out) {
    fprintf(out, "\n");
    fprintf(out, "╔══════════════════════════════════════════════════════════════╗\n");
    fprintf(out, "║  DECODER METRICS: %-40s  ║\n", m->test_name);
    fprintf(out, "╠══════════════════════════════════════════════════════════════╣\n");
    
    // BER
    fprintf(out, "║  BER:         %.2e (10^%.1f)                              ║\n",
            m->ber.ber, m->ber.ber_log10);
    fprintf(out, "║              %llu errors / %llu bits                        ║\n",
            (unsigned long long)m->ber.error_bits,
            (unsigned long long)m->ber.total_bits);
    
    // Sync
    fprintf(out, "╠──────────────────────────────────────────────────────────────╣\n");
    fprintf(out, "║  Sync Rate:   %.1f%% (%d/%d found)                          ║\n",
            m->sync.lock_rate, m->sync.found_exact + m->sync.found_fuzzy,
            m->sync.expected_syncs);
    fprintf(out, "║              Exact: %d, Fuzzy: %d, Missed: %d               ║\n",
            m->sync.found_exact, m->sync.found_fuzzy, m->sync.missed);
    
    // CRC
    fprintf(out, "╠──────────────────────────────────────────────────────────────╣\n");
    fprintf(out, "║  CRC Rate:    %.1f%% original → %.1f%% corrected              ║\n",
            m->crc.pass_rate_original, m->crc.pass_rate_final);
    fprintf(out, "║              1-bit fixes: %d, 2-bit: %d                     ║\n",
            m->crc.corrected_1bit, m->crc.corrected_2bit);
    
    // Confidence
    fprintf(out, "╠──────────────────────────────────────────────────────────────╣\n");
    fprintf(out, "║  Confidence:  %.1f%% overall                                 ║\n",
            m->confidence.overall);
    fprintf(out, "║              Timing: %.1f%%, Sync: %.1f%%                    ║\n",
            m->confidence.timing, m->confidence.sync);
    fprintf(out, "║              Data: %.1f%%, Consistency: %.1f%%               ║\n",
            m->confidence.data, m->confidence.consistency);
    
    // Performance
    fprintf(out, "╠──────────────────────────────────────────────────────────────╣\n");
    fprintf(out, "║  Performance: %.2f ms, %.2f MB/s                            ║\n",
            m->decode_time_ms, m->throughput_mbps);
    
    fprintf(out, "╚══════════════════════════════════════════════════════════════╝\n");
}

void metrics_print_csv_header(FILE* out) {
    fprintf(out, "test,format,track,head,ber,sync_rate,crc_original,crc_final,"
                 "confidence,time_ms,throughput_mbps\n");
}

void metrics_print_csv_row(const decoder_metrics_t* m, FILE* out) {
    fprintf(out, "%s,%s,%d,%d,%.2e,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f\n",
            m->test_name, m->format, m->track, m->head,
            m->ber.ber, m->sync.lock_rate,
            m->crc.pass_rate_original, m->crc.pass_rate_final,
            m->confidence.overall, m->decode_time_ms, m->throughput_mbps);
}

// ============================================================================
// Metrics Collection Session
// ============================================================================

typedef struct {
    decoder_metrics_t* results;
    size_t count;
    size_t capacity;
    
    // Aggregates
    double avg_ber;
    double avg_sync_rate;
    double avg_crc_rate;
    double avg_confidence;
    double total_time_ms;
    
    FILE* csv_log;
} metrics_session_t;

bool metrics_session_init(metrics_session_t* session, size_t capacity,
                          const char* csv_path) {
    session->results = calloc(capacity, sizeof(decoder_metrics_t));
    if (!session->results) return false;
    
    session->count = 0;
    session->capacity = capacity;
    
    if (csv_path) {
        session->csv_log = fopen(csv_path, "w");
        if (session->csv_log) {
            metrics_print_csv_header(session->csv_log);
        }
    }
    
    return true;
}

void metrics_session_add(metrics_session_t* session, const decoder_metrics_t* m) {
    if (session->count >= session->capacity) return;
    
    session->results[session->count++] = *m;
    
    if (session->csv_log) {
        metrics_print_csv_row(m, session->csv_log);
        fflush(session->csv_log);
    }
}

void metrics_session_summarize(metrics_session_t* session) {
    if (session->count == 0) return;
    
    double sum_ber = 0, sum_sync = 0, sum_crc = 0, sum_conf = 0;
    
    for (size_t i = 0; i < session->count; i++) {
        sum_ber += session->results[i].ber.ber;
        sum_sync += session->results[i].sync.lock_rate;
        sum_crc += session->results[i].crc.pass_rate_final;
        sum_conf += session->results[i].confidence.overall;
        session->total_time_ms += session->results[i].decode_time_ms;
    }
    
    session->avg_ber = sum_ber / session->count;
    session->avg_sync_rate = sum_sync / session->count;
    session->avg_crc_rate = sum_crc / session->count;
    session->avg_confidence = sum_conf / session->count;
}

void metrics_session_free(metrics_session_t* session) {
    free(session->results);
    if (session->csv_log) fclose(session->csv_log);
    memset(session, 0, sizeof(*session));
}
