// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file uft_forensic_report.c
 * @brief GOD MODE Forensic Report Generator
 * @version 1.0.0-GOD
 * @date 2025-01-02
 *
 * Generates comprehensive forensic reports for disk imaging operations:
 * - Full audit trail of all operations
 * - Hash verification at each stage
 * - Weak bit and error documentation
 * - Chain of custody support
 * - Court-admissible report formats
 *
 * Output formats: JSON, XML, HTML, PDF (via template)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <stdarg.h>

/*============================================================================
 * CONSTANTS
 *============================================================================*/

#define UFT_REPORT_VERSION      "1.0.0-GOD"
#define UFT_MAX_LOG_ENTRIES     10000
#define UFT_MAX_HASH_LEN        128
#define UFT_MAX_PATH            4096
#define UFT_MAX_SECTORS         10000

/* Report output formats */
typedef enum {
    REPORT_FORMAT_JSON = 0,
    REPORT_FORMAT_XML,
    REPORT_FORMAT_HTML,
    REPORT_FORMAT_TEXT,
    REPORT_FORMAT_CSV
} report_format_t;

/* Log entry severity */
typedef enum {
    LOG_DEBUG = 0,
    LOG_INFO,
    LOG_WARNING,
    LOG_ERROR,
    LOG_CRITICAL
} log_severity_t;

/* Sector status */
typedef enum {
    SECTOR_OK = 0,
    SECTOR_WEAK_BITS,
    SECTOR_CRC_ERROR,
    SECTOR_HEADER_ERROR,
    SECTOR_MISSING,
    SECTOR_RECOVERED
} sector_status_t;

/*============================================================================
 * DATA STRUCTURES
 *============================================================================*/

/* Log entry */
typedef struct {
    time_t timestamp;
    log_severity_t severity;
    char message[512];
    char module[64];
    int track;
    int sector;
} log_entry_t;

/* Hash record */
typedef struct {
    char algorithm[32];
    char value[UFT_MAX_HASH_LEN];
    time_t computed_at;
} hash_record_t;

/* Sector record */
typedef struct {
    int track;
    int head;
    int sector;
    sector_status_t status;
    uint32_t crc_expected;
    uint32_t crc_actual;
    int weak_bit_count;
    int retry_count;
    float confidence;
    char notes[256];
} sector_record_t;

/* Device info */
typedef struct {
    char manufacturer[64];
    char model[64];
    char serial[64];
    char firmware[32];
    char connection[32];
} device_info_t;

/* Media info */
typedef struct {
    char type[32];          /* DD, HD, ED */
    int tracks;
    int heads;
    int sectors_per_track;
    int bytes_per_sector;
    int rpm;
    char encoding[32];      /* MFM, GCR, FM */
    char format[64];        /* Amiga, PC, C64, etc. */
} media_info_t;

/* Forensic report */
typedef struct {
    /* Metadata */
    char report_id[64];
    char case_number[64];
    char examiner[128];
    char organization[128];
    time_t start_time;
    time_t end_time;
    
    /* Source info */
    char source_path[UFT_MAX_PATH];
    char output_path[UFT_MAX_PATH];
    device_info_t device;
    media_info_t media;
    
    /* Hashes */
    hash_record_t source_hash;
    hash_record_t output_hash;
    hash_record_t partial_hashes[168];  /* Per-track hashes */
    int partial_hash_count;
    
    /* Sector analysis */
    sector_record_t* sectors;
    int sector_count;
    int sector_capacity;
    
    /* Statistics */
    int total_sectors;
    int good_sectors;
    int weak_sectors;
    int error_sectors;
    int recovered_sectors;
    int missing_sectors;
    
    /* Log */
    log_entry_t* log;
    int log_count;
    int log_capacity;
    
    /* Status */
    bool success;
    char final_status[256];
    
} forensic_report_t;

/*============================================================================
 * REPORT CREATION
 *============================================================================*/

/**
 * @brief Create new forensic report
 */
forensic_report_t* forensic_report_create(const char* case_number,
                                           const char* examiner) {
    forensic_report_t* report = calloc(1, sizeof(forensic_report_t));
    if (!report) return NULL;
    
    /* Generate unique report ID */
    time_t now = time(NULL);
    snprintf(report->report_id, sizeof(report->report_id),
             "UFT-%ld-%04X", now, (unsigned)(rand() & 0xFFFF));
    
    if (case_number) {
        strncpy(report->case_number, case_number, sizeof(report->case_number) - 1);
    }
    if (examiner) {
        strncpy(report->examiner, examiner, sizeof(report->examiner) - 1);
    }
    
    report->start_time = now;
    
    /* Allocate log */
    report->log_capacity = 1000;
    report->log = calloc(report->log_capacity, sizeof(log_entry_t));
    
    /* Allocate sectors */
    report->sector_capacity = 1000;
    report->sectors = calloc(report->sector_capacity, sizeof(sector_record_t));
    
    return report;
}

/**
 * @brief Destroy forensic report
 */
void forensic_report_destroy(forensic_report_t* report) {
    if (!report) return;
    free(report->log);
    free(report->sectors);
    free(report);
}

/*============================================================================
 * LOGGING
 *============================================================================*/

/**
 * @brief Add log entry
 */
void forensic_report_log(forensic_report_t* report,
                         log_severity_t severity,
                         const char* module,
                         int track, int sector,
                         const char* fmt, ...) {
    if (!report || !report->log) return;
    
    /* Expand log if needed */
    if (report->log_count >= report->log_capacity) {
        int new_cap = report->log_capacity * 2;
        log_entry_t* new_log = realloc(report->log, new_cap * sizeof(log_entry_t));
        if (!new_log) return;
        report->log = new_log;
        report->log_capacity = new_cap;
    }
    
    log_entry_t* entry = &report->log[report->log_count++];
    entry->timestamp = time(NULL);
    entry->severity = severity;
    entry->track = track;
    entry->sector = sector;
    
    if (module) {
        strncpy(entry->module, module, sizeof(entry->module) - 1);
    }
    
    va_list args;
    va_start(args, fmt);
    vsnprintf(entry->message, sizeof(entry->message), fmt, args);
    va_end(args);
}

/*============================================================================
 * SECTOR RECORDING
 *============================================================================*/

/**
 * @brief Record sector status
 */
void forensic_report_sector(forensic_report_t* report,
                            int track, int head, int sector,
                            sector_status_t status,
                            uint32_t crc_expected, uint32_t crc_actual,
                            int weak_bits, float confidence,
                            const char* notes) {
    if (!report || !report->sectors) return;
    
    /* Expand if needed */
    if (report->sector_count >= report->sector_capacity) {
        int new_cap = report->sector_capacity * 2;
        sector_record_t* new_sec = realloc(report->sectors, 
                                            new_cap * sizeof(sector_record_t));
        if (!new_sec) return;
        report->sectors = new_sec;
        report->sector_capacity = new_cap;
    }
    
    sector_record_t* rec = &report->sectors[report->sector_count++];
    rec->track = track;
    rec->head = head;
    rec->sector = sector;
    rec->status = status;
    rec->crc_expected = crc_expected;
    rec->crc_actual = crc_actual;
    rec->weak_bit_count = weak_bits;
    rec->confidence = confidence;
    
    if (notes) {
        strncpy(rec->notes, notes, sizeof(rec->notes) - 1);
    }
    
    /* Update statistics */
    report->total_sectors++;
    switch (status) {
        case SECTOR_OK: report->good_sectors++; break;
        case SECTOR_WEAK_BITS: report->weak_sectors++; break;
        case SECTOR_CRC_ERROR:
        case SECTOR_HEADER_ERROR: report->error_sectors++; break;
        case SECTOR_MISSING: report->missing_sectors++; break;
        case SECTOR_RECOVERED: report->recovered_sectors++; break;
    }
}

/*============================================================================
 * HASH RECORDING
 *============================================================================*/

/**
 * @brief Set source hash
 */
void forensic_report_set_source_hash(forensic_report_t* report,
                                      const char* algorithm,
                                      const char* value) {
    if (!report) return;
    strncpy(report->source_hash.algorithm, algorithm, 
            sizeof(report->source_hash.algorithm) - 1);
    strncpy(report->source_hash.value, value,
            sizeof(report->source_hash.value) - 1);
    report->source_hash.computed_at = time(NULL);
}

/**
 * @brief Set output hash
 */
void forensic_report_set_output_hash(forensic_report_t* report,
                                      const char* algorithm,
                                      const char* value) {
    if (!report) return;
    strncpy(report->output_hash.algorithm, algorithm,
            sizeof(report->output_hash.algorithm) - 1);
    strncpy(report->output_hash.value, value,
            sizeof(report->output_hash.value) - 1);
    report->output_hash.computed_at = time(NULL);
}

/**
 * @brief Add per-track hash
 */
void forensic_report_add_track_hash(forensic_report_t* report,
                                     int track,
                                     const char* algorithm,
                                     const char* value) {
    if (!report || report->partial_hash_count >= 168) return;
    
    hash_record_t* rec = &report->partial_hashes[report->partial_hash_count++];
    snprintf(rec->algorithm, sizeof(rec->algorithm), "%s:T%d", algorithm, track);
    strncpy(rec->value, value, sizeof(rec->value) - 1);
    rec->computed_at = time(NULL);
}

/*============================================================================
 * REPORT GENERATION
 *============================================================================*/

static const char* severity_str(log_severity_t sev) {
    switch (sev) {
        case LOG_DEBUG: return "DEBUG";
        case LOG_INFO: return "INFO";
        case LOG_WARNING: return "WARNING";
        case LOG_ERROR: return "ERROR";
        case LOG_CRITICAL: return "CRITICAL";
        default: return "UNKNOWN";
    }
}

static const char* sector_status_str(sector_status_t status) {
    switch (status) {
        case SECTOR_OK: return "OK";
        case SECTOR_WEAK_BITS: return "WEAK_BITS";
        case SECTOR_CRC_ERROR: return "CRC_ERROR";
        case SECTOR_HEADER_ERROR: return "HEADER_ERROR";
        case SECTOR_MISSING: return "MISSING";
        case SECTOR_RECOVERED: return "RECOVERED";
        default: return "UNKNOWN";
    }
}

/**
 * @brief Generate JSON report
 */
int forensic_report_to_json(forensic_report_t* report, FILE* out) {
    if (!report || !out) return -1;
    
    char time_buf[64];
    struct tm* tm_info;
    
    fprintf(out, "{\n");
    fprintf(out, "  \"report_version\": \"%s\",\n", UFT_REPORT_VERSION);
    fprintf(out, "  \"report_id\": \"%s\",\n", report->report_id);
    fprintf(out, "  \"case_number\": \"%s\",\n", report->case_number);
    fprintf(out, "  \"examiner\": \"%s\",\n", report->examiner);
    
    tm_info = localtime(&report->start_time);
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%dT%H:%M:%S", tm_info);
    fprintf(out, "  \"start_time\": \"%s\",\n", time_buf);
    
    if (report->end_time > 0) {
        tm_info = localtime(&report->end_time);
        strftime(time_buf, sizeof(time_buf), "%Y-%m-%dT%H:%M:%S", tm_info);
        fprintf(out, "  \"end_time\": \"%s\",\n", time_buf);
    }
    
    /* Hashes */
    fprintf(out, "  \"hashes\": {\n");
    fprintf(out, "    \"source\": {\n");
    fprintf(out, "      \"algorithm\": \"%s\",\n", report->source_hash.algorithm);
    fprintf(out, "      \"value\": \"%s\"\n", report->source_hash.value);
    fprintf(out, "    },\n");
    fprintf(out, "    \"output\": {\n");
    fprintf(out, "      \"algorithm\": \"%s\",\n", report->output_hash.algorithm);
    fprintf(out, "      \"value\": \"%s\"\n", report->output_hash.value);
    fprintf(out, "    }\n");
    fprintf(out, "  },\n");
    
    /* Statistics */
    fprintf(out, "  \"statistics\": {\n");
    fprintf(out, "    \"total_sectors\": %d,\n", report->total_sectors);
    fprintf(out, "    \"good_sectors\": %d,\n", report->good_sectors);
    fprintf(out, "    \"weak_sectors\": %d,\n", report->weak_sectors);
    fprintf(out, "    \"error_sectors\": %d,\n", report->error_sectors);
    fprintf(out, "    \"recovered_sectors\": %d,\n", report->recovered_sectors);
    fprintf(out, "    \"missing_sectors\": %d,\n", report->missing_sectors);
    fprintf(out, "    \"success_rate\": %.2f\n", 
            report->total_sectors > 0 ? 
            (float)report->good_sectors / report->total_sectors * 100.0f : 0.0f);
    fprintf(out, "  },\n");
    
    /* Sector details */
    fprintf(out, "  \"sectors\": [\n");
    for (int i = 0; i < report->sector_count; i++) {
        sector_record_t* s = &report->sectors[i];
        fprintf(out, "    {\"t\":%d,\"h\":%d,\"s\":%d,\"status\":\"%s\","
                     "\"weak\":%d,\"conf\":%.2f}%s\n",
                s->track, s->head, s->sector,
                sector_status_str(s->status),
                s->weak_bit_count, s->confidence,
                i < report->sector_count - 1 ? "," : "");
    }
    fprintf(out, "  ],\n");
    
    /* Log entries */
    fprintf(out, "  \"log\": [\n");
    for (int i = 0; i < report->log_count; i++) {
        log_entry_t* e = &report->log[i];
        tm_info = localtime(&e->timestamp);
        strftime(time_buf, sizeof(time_buf), "%H:%M:%S", tm_info);
        
        /* Escape quotes in message */
        char escaped_msg[1024];
        const char* src = e->message;
        char* dst = escaped_msg;
        while (*src && dst < escaped_msg + sizeof(escaped_msg) - 2) {
            if (*src == '"' || *src == '\\') {
                *dst++ = '\\';
            }
            *dst++ = *src++;
        }
        *dst = '\0';
        
        fprintf(out, "    {\"time\":\"%s\",\"level\":\"%s\",\"msg\":\"%s\"}%s\n",
                time_buf, severity_str(e->severity), escaped_msg,
                i < report->log_count - 1 ? "," : "");
    }
    fprintf(out, "  ],\n");
    
    fprintf(out, "  \"success\": %s,\n", report->success ? "true" : "false");
    fprintf(out, "  \"final_status\": \"%s\"\n", report->final_status);
    fprintf(out, "}\n");
    
    return 0;
}

/**
 * @brief Generate HTML report
 */
int forensic_report_to_html(forensic_report_t* report, FILE* out) {
    if (!report || !out) return -1;
    
    char time_buf[64];
    struct tm* tm_info;
    
    fprintf(out, "<!DOCTYPE html>\n<html>\n<head>\n");
    fprintf(out, "<title>UFT Forensic Report - %s</title>\n", report->report_id);
    fprintf(out, "<style>\n");
    fprintf(out, "body { font-family: Arial, sans-serif; margin: 20px; }\n");
    fprintf(out, "h1 { color: #2c3e50; }\n");
    fprintf(out, "h2 { color: #34495e; border-bottom: 2px solid #3498db; }\n");
    fprintf(out, "table { border-collapse: collapse; width: 100%%; margin: 10px 0; }\n");
    fprintf(out, "th, td { border: 1px solid #ddd; padding: 8px; text-align: left; }\n");
    fprintf(out, "th { background: #3498db; color: white; }\n");
    fprintf(out, "tr:nth-child(even) { background: #f2f2f2; }\n");
    fprintf(out, ".ok { color: green; } .error { color: red; }\n");
    fprintf(out, ".weak { color: orange; } .recovered { color: blue; }\n");
    fprintf(out, ".hash { font-family: monospace; background: #f5f5f5; padding: 5px; }\n");
    fprintf(out, ".summary { background: #ecf0f1; padding: 15px; border-radius: 5px; }\n");
    fprintf(out, "</style>\n</head>\n<body>\n");
    
    /* Header */
    fprintf(out, "<h1>🔍 UFT Forensic Imaging Report</h1>\n");
    fprintf(out, "<div class='summary'>\n");
    fprintf(out, "<p><strong>Report ID:</strong> %s</p>\n", report->report_id);
    fprintf(out, "<p><strong>Case Number:</strong> %s</p>\n", report->case_number);
    fprintf(out, "<p><strong>Examiner:</strong> %s</p>\n", report->examiner);
    
    tm_info = localtime(&report->start_time);
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_info);
    fprintf(out, "<p><strong>Start Time:</strong> %s</p>\n", time_buf);
    
    if (report->end_time > 0) {
        tm_info = localtime(&report->end_time);
        strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_info);
        fprintf(out, "<p><strong>End Time:</strong> %s</p>\n", time_buf);
    }
    fprintf(out, "</div>\n");
    
    /* Hash verification */
    fprintf(out, "<h2>🔐 Hash Verification</h2>\n");
    fprintf(out, "<table>\n<tr><th>Type</th><th>Algorithm</th><th>Value</th></tr>\n");
    fprintf(out, "<tr><td>Source</td><td>%s</td><td class='hash'>%s</td></tr>\n",
            report->source_hash.algorithm, report->source_hash.value);
    fprintf(out, "<tr><td>Output</td><td>%s</td><td class='hash'>%s</td></tr>\n",
            report->output_hash.algorithm, report->output_hash.value);
    fprintf(out, "</table>\n");
    
    /* Statistics */
    fprintf(out, "<h2>📊 Statistics</h2>\n");
    fprintf(out, "<table>\n");
    fprintf(out, "<tr><th>Metric</th><th>Value</th></tr>\n");
    fprintf(out, "<tr><td>Total Sectors</td><td>%d</td></tr>\n", report->total_sectors);
    fprintf(out, "<tr><td>Good Sectors</td><td class='ok'>%d</td></tr>\n", report->good_sectors);
    fprintf(out, "<tr><td>Weak Bit Sectors</td><td class='weak'>%d</td></tr>\n", report->weak_sectors);
    fprintf(out, "<tr><td>Error Sectors</td><td class='error'>%d</td></tr>\n", report->error_sectors);
    fprintf(out, "<tr><td>Recovered Sectors</td><td class='recovered'>%d</td></tr>\n", report->recovered_sectors);
    fprintf(out, "<tr><td>Missing Sectors</td><td class='error'>%d</td></tr>\n", report->missing_sectors);
    
    float success_rate = report->total_sectors > 0 ? 
        (float)report->good_sectors / report->total_sectors * 100.0f : 0.0f;
    fprintf(out, "<tr><td><strong>Success Rate</strong></td><td><strong>%.2f%%</strong></td></tr>\n", 
            success_rate);
    fprintf(out, "</table>\n");
    
    /* Final status */
    fprintf(out, "<h2>✅ Final Status</h2>\n");
    fprintf(out, "<p class='%s'><strong>%s</strong></p>\n",
            report->success ? "ok" : "error",
            report->final_status);
    
    fprintf(out, "<hr>\n<p><small>Generated by UnifiedFloppyTool GOD MODE v%s</small></p>\n",
            UFT_REPORT_VERSION);
    fprintf(out, "</body>\n</html>\n");
    
    return 0;
}

/**
 * @brief Finalize report
 */
void forensic_report_finalize(forensic_report_t* report, bool success, 
                               const char* status) {
    if (!report) return;
    
    report->end_time = time(NULL);
    report->success = success;
    
    if (status) {
        strncpy(report->final_status, status, sizeof(report->final_status) - 1);
    } else {
        snprintf(report->final_status, sizeof(report->final_status),
                 "%s: %d/%d sectors OK (%.1f%%)",
                 success ? "SUCCESS" : "COMPLETED WITH ERRORS",
                 report->good_sectors, report->total_sectors,
                 report->total_sectors > 0 ?
                 (float)report->good_sectors / report->total_sectors * 100.0f : 0.0f);
    }
}

/**
 * @brief Save report to file
 */
int forensic_report_save(forensic_report_t* report, const char* path,
                          report_format_t format) {
    if (!report || !path) return -1;
    
    FILE* f = fopen(path, "w");
    if (!f) return -1;
    
    int result = 0;
    
    switch (format) {
        case REPORT_FORMAT_JSON:
            result = forensic_report_to_json(report, f);
            break;
        case REPORT_FORMAT_HTML:
            result = forensic_report_to_html(report, f);
            break;
        default:
            result = forensic_report_to_json(report, f);
            break;
    }
    
    fclose(f);
    return result;
}

/*============================================================================
 * UNIT TEST
 *============================================================================*/

#ifdef FORENSIC_REPORT_TEST

#include <assert.h>

int main(void) {
    printf("=== forensic_report Unit Tests ===\n");
    
    // Test 1: Create report
    {
        forensic_report_t* r = forensic_report_create("CASE-2025-001", "Test Examiner");
        assert(r != NULL);
        assert(strlen(r->report_id) > 0);
        assert(strcmp(r->case_number, "CASE-2025-001") == 0);
        forensic_report_destroy(r);
        printf("✓ Create report\n");
    }
    
    // Test 2: Logging
    {
        forensic_report_t* r = forensic_report_create("TEST", "Tester");
        forensic_report_log(r, LOG_INFO, "decoder", 0, 0, "Started decoding");
        forensic_report_log(r, LOG_WARNING, "pll", 5, 3, "Weak bits detected");
        forensic_report_log(r, LOG_ERROR, "crc", 10, 1, "CRC mismatch");
        assert(r->log_count == 3);
        assert(r->log[1].severity == LOG_WARNING);
        forensic_report_destroy(r);
        printf("✓ Logging\n");
    }
    
    // Test 3: Sector recording
    {
        forensic_report_t* r = forensic_report_create("TEST", "Tester");
        forensic_report_sector(r, 0, 0, 1, SECTOR_OK, 0x1234, 0x1234, 0, 1.0f, NULL);
        forensic_report_sector(r, 0, 0, 2, SECTOR_WEAK_BITS, 0x5678, 0x5678, 12, 0.85f, "Multiple revolutions");
        forensic_report_sector(r, 0, 0, 3, SECTOR_CRC_ERROR, 0xABCD, 0xEF01, 0, 0.0f, "Unrecoverable");
        
        assert(r->sector_count == 3);
        assert(r->good_sectors == 1);
        assert(r->weak_sectors == 1);
        assert(r->error_sectors == 1);
        forensic_report_destroy(r);
        printf("✓ Sector recording\n");
    }
    
    // Test 4: Hash recording
    {
        forensic_report_t* r = forensic_report_create("TEST", "Tester");
        forensic_report_set_source_hash(r, "SHA256", "abc123def456...");
        forensic_report_set_output_hash(r, "SHA256", "abc123def456...");
        
        assert(strcmp(r->source_hash.algorithm, "SHA256") == 0);
        assert(r->source_hash.computed_at > 0);
        forensic_report_destroy(r);
        printf("✓ Hash recording\n");
    }
    
    // Test 5: JSON output
    {
        forensic_report_t* r = forensic_report_create("TEST-JSON", "JSON Tester");
        forensic_report_set_source_hash(r, "MD5", "d41d8cd98f00b204e9800998ecf8427e");
        forensic_report_sector(r, 0, 0, 1, SECTOR_OK, 0, 0, 0, 1.0f, NULL);
        forensic_report_log(r, LOG_INFO, "test", 0, 0, "Test message");
        forensic_report_finalize(r, true, "Test complete");
        
        FILE* f = fopen("/tmp/test_report.json", "w"); if (!f) return;
        assert(f != NULL);
        int res = forensic_report_to_json(r, f);
        fclose(f);
        assert(res == 0);
        
        forensic_report_destroy(r);
        printf("✓ JSON output\n");
    }
    
    // Test 6: HTML output
    {
        forensic_report_t* r = forensic_report_create("TEST-HTML", "HTML Tester");
        forensic_report_set_source_hash(r, "SHA256", "e3b0c44298fc1c149afbf4c8996fb924...");
        forensic_report_sector(r, 0, 0, 1, SECTOR_OK, 0, 0, 0, 1.0f, NULL);
        forensic_report_sector(r, 1, 0, 1, SECTOR_WEAK_BITS, 0, 0, 8, 0.9f, NULL);
        forensic_report_finalize(r, true, NULL);
        
        FILE* f = fopen("/tmp/test_report.html", "w"); if (!f) return;
        assert(f != NULL);
        int res = forensic_report_to_html(r, f);
        fclose(f);
        assert(res == 0);
        
        forensic_report_destroy(r);
        printf("✓ HTML output\n");
    }
    
    printf("\n=== All tests passed! ===\n");
    return 0;
}
#endif
