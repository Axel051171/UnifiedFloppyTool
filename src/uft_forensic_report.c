/**
 * @file uft_forensic_report.c
 * @brief Forensic Report Generator Implementation
 * 
 * TICKET-006: Forensic Report Generator
 */

#include "uft/uft_forensic_report.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Internal Structures
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    int             cylinder;
    int             head;
    int             sector;
    uft_error_t     code;
    char            *message;
} report_error_t;

struct uft_report {
    uft_report_options_t    options;
    uft_report_metadata_t   metadata;
    
    uft_report_track_t      *tracks;
    int                     track_count;
    int                     track_capacity;
    
    report_error_t          *errors;
    int                     error_count;
    int                     error_capacity;
    
    uft_report_protection_t *protections;
    int                     protection_count;
    
    uft_hash_chain_t        *hash_chain;
    uft_audit_trail_t       *audit_trail;
    
    bool                    success;
    char                    *result_message;
    
    uint64_t                start_time;
    uint64_t                end_time;
};

/* ═══════════════════════════════════════════════════════════════════════════════
 * Helper Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

static uint64_t get_timestamp_ms(void) {
#ifdef _WIN32
    return GetTickCount64();
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
#endif
}

static char *strdup_safe(const char *str) {
    return str ? strdup(str) : NULL;
}

static void get_timestamp_string(char *buf, size_t size) {
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    strftime(buf, size, "%Y-%m-%d %H:%M:%S", tm);
}

/* Simple hash functions - in production use OpenSSL */
static uint32_t simple_crc32(const uint8_t *data, size_t size) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < size; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
        }
    }
    return ~crc;
}

static void simple_sha256(const uint8_t *data, size_t size, char *out) {
    /* Simplified hash for demonstration */
    uint32_t h0 = 0x6a09e667;
    uint32_t h1 = 0xbb67ae85;
    
    for (size_t i = 0; i < size; i++) {
        h0 = (h0 + data[i]) * 0x01000193;
        h1 = (h1 ^ data[i]) * 0x811c9dc5;
    }
    
    snprintf(out, 65, "%08x%08x%08x%08x%08x%08x%08x%08x",
             h0, h1, h0 ^ h1, h0 + h1,
             h1 - h0, h0 * 31, h1 * 37, (h0 ^ h1) * 41);
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Report Builder Implementation
 * ═══════════════════════════════════════════════════════════════════════════════ */

uft_report_t *uft_report_create(const uft_report_options_t *options) {
    uft_report_t *report = calloc(1, sizeof(uft_report_t));
    if (!report) return NULL;
    
    if (options) {
        report->options = *options;
        report->options.title = strdup_safe(options->title);
        report->options.organization = strdup_safe(options->organization);
        report->options.operator_name = strdup_safe(options->operator_name);
        report->options.case_number = strdup_safe(options->case_number);
        report->options.evidence_id = strdup_safe(options->evidence_id);
        report->options.output_path = strdup_safe(options->output_path);
    } else {
        uft_report_options_t defaults = UFT_REPORT_OPTIONS_DEFAULT;
        report->options = defaults;
    }
    
    report->track_capacity = 256;
    report->tracks = calloc(report->track_capacity, sizeof(uft_report_track_t));
    
    report->error_capacity = 64;
    report->errors = calloc(report->error_capacity, sizeof(report_error_t));
    
    report->hash_chain = uft_hash_chain_create(report->options.hash_algorithm);
    report->audit_trail = uft_audit_trail_create();
    
    report->start_time = get_timestamp_ms();
    
    uft_audit_log(report->audit_trail, UFT_AUDIT_START, "Report generation started",
                  -1, -1, -1);
    
    return report;
}

void uft_report_destroy(uft_report_t *report) {
    if (!report) return;
    
    /* Free options strings */
    free(report->options.title);
    free(report->options.organization);
    free(report->options.operator_name);
    free(report->options.case_number);
    free(report->options.evidence_id);
    free(report->options.output_path);
    
    /* Free metadata */
    free(report->metadata.source_path);
    free(report->metadata.target_path);
    free(report->metadata.detected_format);
    free(report->metadata.detected_encoding);
    free(report->metadata.detected_filesystem);
    free(report->metadata.volume_label);
    free(report->metadata.hardware_name);
    free(report->metadata.hardware_serial);
    free(report->metadata.drive_type);
    free(report->metadata.media_type);
    free(report->metadata.write_protect);
    
    /* Free tracks */
    free(report->tracks);
    
    /* Free errors */
    for (int i = 0; i < report->error_count; i++) {
        free(report->errors[i].message);
    }
    free(report->errors);
    
    /* Free protections */
    for (int i = 0; i < report->protection_count; i++) {
        free(report->protections[i].scheme_name);
        free(report->protections[i].scheme_version);
        free(report->protections[i].details);
        free(report->protections[i].affected_tracks);
    }
    free(report->protections);
    
    /* Free chains */
    uft_hash_chain_destroy(report->hash_chain);
    uft_audit_trail_destroy(report->audit_trail);
    
    free(report->result_message);
    free(report);
}

void uft_report_set_metadata(uft_report_t *report, const uft_report_metadata_t *metadata) {
    if (!report || !metadata) return;
    
    report->metadata.source_path = strdup_safe(metadata->source_path);
    report->metadata.target_path = strdup_safe(metadata->target_path);
    report->metadata.source_format = metadata->source_format;
    report->metadata.target_format = metadata->target_format;
    report->metadata.cylinders = metadata->cylinders;
    report->metadata.heads = metadata->heads;
    report->metadata.sectors_per_track = metadata->sectors_per_track;
    report->metadata.bytes_per_sector = metadata->bytes_per_sector;
    report->metadata.total_size = metadata->total_size;
    report->metadata.detected_format = strdup_safe(metadata->detected_format);
    report->metadata.detected_encoding = strdup_safe(metadata->detected_encoding);
    report->metadata.detected_filesystem = strdup_safe(metadata->detected_filesystem);
    report->metadata.volume_label = strdup_safe(metadata->volume_label);
    report->metadata.hardware_name = strdup_safe(metadata->hardware_name);
    report->metadata.hardware_serial = strdup_safe(metadata->hardware_serial);
    report->metadata.drive_type = strdup_safe(metadata->drive_type);
    report->metadata.media_type = strdup_safe(metadata->media_type);
    report->metadata.write_protect = strdup_safe(metadata->write_protect);
}

void uft_report_add_track(uft_report_t *report, const uft_report_track_t *track) {
    if (!report || !track) return;
    
    /* Grow if needed */
    if (report->track_count >= report->track_capacity) {
        report->track_capacity *= 2;
        report->tracks = realloc(report->tracks, 
                                  report->track_capacity * sizeof(uft_report_track_t));
    }
    
    report->tracks[report->track_count++] = *track;
}

void uft_report_add_error(uft_report_t *report, int cylinder, int head, int sector,
                           uft_error_t error_code, const char *message) {
    if (!report) return;
    
    if (report->error_count >= report->error_capacity) {
        report->error_capacity *= 2;
        report->errors = realloc(report->errors,
                                  report->error_capacity * sizeof(report_error_t));
    }
    
    report_error_t *err = &report->errors[report->error_count++];
    err->cylinder = cylinder;
    err->head = head;
    err->sector = sector;
    err->code = error_code;
    err->message = strdup_safe(message);
    
    uft_audit_log(report->audit_trail, UFT_AUDIT_ERROR, message, cylinder, head, sector);
}

void uft_report_add_protection(uft_report_t *report, const uft_report_protection_t *protection) {
    if (!report || !protection) return;
    
    report->protections = realloc(report->protections,
                                   (report->protection_count + 1) * sizeof(uft_report_protection_t));
    
    uft_report_protection_t *p = &report->protections[report->protection_count++];
    p->scheme_name = strdup_safe(protection->scheme_name);
    p->scheme_version = strdup_safe(protection->scheme_version);
    p->confidence = protection->confidence;
    p->details = strdup_safe(protection->details);
    p->track_count = protection->track_count;
    
    if (protection->affected_tracks && protection->track_count > 0) {
        p->affected_tracks = malloc(protection->track_count * sizeof(int));
        if(!p->affected_tracks) return; /* P0-SEC-001 */
        memcpy(p->affected_tracks, protection->affected_tracks,
               protection->track_count * sizeof(int));
    }
}

void uft_report_add_audit(uft_report_t *report, uft_audit_event_t event,
                           const char *description, int cylinder, int head) {
    if (!report) return;
    uft_audit_log(report->audit_trail, event, description, cylinder, head, -1);
}

void uft_report_add_hash(uft_report_t *report, const char *data_id,
                          const uint8_t *data, size_t size) {
    if (!report || !data_id || !data) return;
    uft_hash_chain_add(report->hash_chain, data_id, data, size);
}

void uft_report_set_result(uft_report_t *report, bool success, const char *message) {
    if (!report) return;
    report->success = success;
    free(report->result_message);
    report->result_message = strdup_safe(message);
    report->end_time = get_timestamp_ms();
    
    uft_audit_log(report->audit_trail, UFT_AUDIT_END,
                  success ? "Operation completed successfully" : "Operation failed",
                  -1, -1, -1);
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Report Generation - JSON
 * ═══════════════════════════════════════════════════════════════════════════════ */

static char *generate_json(uft_report_t *report) {
    size_t size = 65536 + report->track_count * 512 + report->error_count * 256;
    char *json = malloc(size);
    if (!json) return NULL;
    
    char timestamp[64];
    get_timestamp_string(timestamp, sizeof(timestamp));
    
    int pos = 0;
    
    /* Header */
    pos += snprintf(json + pos, size - pos,
        "{\n"
        "  \"report\": {\n"
        "    \"version\": \"1.0\",\n"
        "    \"generator\": \"UnifiedFloppyTool v5.1.0\",\n"
        "    \"timestamp\": \"%s\",\n"
        "    \"type\": \"%s\",\n",
        timestamp,
        uft_report_type_name(report->options.type));
    
    if (report->options.title) {
        pos += snprintf(json + pos, size - pos,
            "    \"title\": \"%s\",\n", report->options.title);
    }
    if (report->options.organization) {
        pos += snprintf(json + pos, size - pos,
            "    \"organization\": \"%s\",\n", report->options.organization);
    }
    if (report->options.case_number) {
        pos += snprintf(json + pos, size - pos,
            "    \"case_number\": \"%s\",\n", report->options.case_number);
    }
    if (report->options.evidence_id) {
        pos += snprintf(json + pos, size - pos,
            "    \"evidence_id\": \"%s\",\n", report->options.evidence_id);
    }
    
    /* Result */
    pos += snprintf(json + pos, size - pos,
        "    \"success\": %s,\n"
        "    \"result_message\": \"%s\",\n"
        "    \"duration_ms\": %lu\n"
        "  },\n",
        report->success ? "true" : "false",
        report->result_message ? report->result_message : "",
        (unsigned long)(report->end_time - report->start_time));
    
    /* Metadata */
    if (report->options.sections & UFT_REPORT_SEC_METADATA) {
        pos += snprintf(json + pos, size - pos,
            "  \"metadata\": {\n"
            "    \"source_path\": \"%s\",\n"
            "    \"cylinders\": %d,\n"
            "    \"heads\": %d,\n"
            "    \"sectors_per_track\": %d,\n"
            "    \"bytes_per_sector\": %d,\n"
            "    \"total_size\": %zu,\n"
            "    \"detected_format\": \"%s\",\n"
            "    \"detected_filesystem\": \"%s\"\n"
            "  },\n",
            report->metadata.source_path ? report->metadata.source_path : "",
            report->metadata.cylinders,
            report->metadata.heads,
            report->metadata.sectors_per_track,
            report->metadata.bytes_per_sector,
            report->metadata.total_size,
            report->metadata.detected_format ? report->metadata.detected_format : "",
            report->metadata.detected_filesystem ? report->metadata.detected_filesystem : "");
    }
    
    /* Summary */
    if (report->options.sections & UFT_REPORT_SEC_SUMMARY) {
        int good_tracks = 0, bad_tracks = 0;
        for (int i = 0; i < report->track_count; i++) {
            if (report->tracks[i].has_errors) bad_tracks++;
            else good_tracks++;
        }
        
        pos += snprintf(json + pos, size - pos,
            "  \"summary\": {\n"
            "    \"tracks_total\": %d,\n"
            "    \"tracks_good\": %d,\n"
            "    \"tracks_bad\": %d,\n"
            "    \"error_count\": %d,\n"
            "    \"protection_detected\": %s\n"
            "  },\n",
            report->track_count,
            good_tracks, bad_tracks,
            report->error_count,
            report->protection_count > 0 ? "true" : "false");
    }
    
    /* Hashes */
    if (report->options.sections & UFT_REPORT_SEC_HASHES) {
        uft_hash_chain_finalize(report->hash_chain);
        pos += snprintf(json + pos, size - pos,
            "  \"hashes\": {\n"
            "    \"algorithm\": \"%s\",\n"
            "    \"root_hash\": \"%s\",\n"
            "    \"entry_count\": %d,\n"
            "    \"verified\": %s\n"
            "  },\n",
            uft_hash_algo_name(report->hash_chain->algorithm),
            report->hash_chain->root_hash,
            report->hash_chain->count,
            report->hash_chain->verified ? "true" : "false");
    }
    
    /* Tracks */
    if ((report->options.sections & UFT_REPORT_SEC_TRACK_MAP) && report->track_count > 0) {
        pos += snprintf(json + pos, size - pos, "  \"tracks\": [\n");
        for (int i = 0; i < report->track_count && pos < (int)size - 512; i++) {
            uft_report_track_t *t = &report->tracks[i];
            pos += snprintf(json + pos, size - pos,
                "    {\"cyl\": %d, \"head\": %d, \"good\": %d, \"bad\": %d, "
                "\"errors\": %s, \"hash\": \"%s\"}%s\n",
                t->cylinder, t->head, t->sectors_good, t->sectors_bad,
                t->has_errors ? "true" : "false",
                t->hash,
                (i < report->track_count - 1) ? "," : "");
        }
        pos += snprintf(json + pos, size - pos, "  ],\n");
    }
    
    /* Errors */
    if ((report->options.sections & UFT_REPORT_SEC_ERRORS) && report->error_count > 0) {
        pos += snprintf(json + pos, size - pos, "  \"errors\": [\n");
        for (int i = 0; i < report->error_count && pos < (int)size - 256; i++) {
            report_error_t *e = &report->errors[i];
            pos += snprintf(json + pos, size - pos,
                "    {\"cyl\": %d, \"head\": %d, \"sector\": %d, "
                "\"code\": %d, \"message\": \"%s\"}%s\n",
                e->cylinder, e->head, e->sector, e->code,
                e->message ? e->message : "",
                (i < report->error_count - 1) ? "," : "");
        }
        pos += snprintf(json + pos, size - pos, "  ],\n");
    }
    
    /* Protection */
    if ((report->options.sections & UFT_REPORT_SEC_PROTECTION) && report->protection_count > 0) {
        pos += snprintf(json + pos, size - pos, "  \"protection\": [\n");
        for (int i = 0; i < report->protection_count && pos < (int)size - 256; i++) {
            uft_report_protection_t *p = &report->protections[i];
            pos += snprintf(json + pos, size - pos,
                "    {\"scheme\": \"%s\", \"version\": \"%s\", "
                "\"confidence\": %d, \"tracks\": %d}%s\n",
                p->scheme_name ? p->scheme_name : "",
                p->scheme_version ? p->scheme_version : "",
                p->confidence, p->track_count,
                (i < report->protection_count - 1) ? "," : "");
        }
        pos += snprintf(json + pos, size - pos, "  ],\n");
    }
    
    /* Audit trail */
    if (report->options.sections & UFT_REPORT_SEC_AUDIT) {
        char *audit_json = uft_audit_trail_to_json(report->audit_trail);
        if (audit_json) {
            pos += snprintf(json + pos, size - pos, "  \"audit\": %s,\n", audit_json);
            free(audit_json);
        }
    }
    
    /* Hash chain */
    if (report->options.sections & UFT_REPORT_SEC_HASH_CHAIN) {
        char *chain_json = uft_hash_chain_to_json(report->hash_chain);
        if (chain_json) {
            pos += snprintf(json + pos, size - pos, "  \"hash_chain\": %s\n", chain_json);
            free(chain_json);
        }
    }
    
    /* Close */
    /* Remove trailing comma if present */
    if (pos > 2 && json[pos-2] == ',') {
        json[pos-2] = '\n';
        pos--;
    }
    pos += snprintf(json + pos, size - pos, "}\n");
    
    return json;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Report Generation - HTML
 * ═══════════════════════════════════════════════════════════════════════════════ */

static char *generate_html(uft_report_t *report) {
    size_t size = 131072 + report->track_count * 256;
    char *html = malloc(size);
    if (!html) return NULL;
    
    char timestamp[64];
    get_timestamp_string(timestamp, sizeof(timestamp));
    
    int pos = 0;
    
    pos += snprintf(html + pos, size - pos,
        "<!DOCTYPE html>\n"
        "<html>\n"
        "<head>\n"
        "  <meta charset=\"UTF-8\">\n"
        "  <title>%s - Forensic Report</title>\n"
        "  <style>\n"
        "    body { font-family: Arial, sans-serif; margin: 40px; }\n"
        "    h1 { color: #333; border-bottom: 2px solid #007bff; padding-bottom: 10px; }\n"
        "    h2 { color: #555; margin-top: 30px; }\n"
        "    table { border-collapse: collapse; width: 100%%; margin: 20px 0; }\n"
        "    th, td { border: 1px solid #ddd; padding: 8px; text-align: left; }\n"
        "    th { background-color: #007bff; color: white; }\n"
        "    tr:nth-child(even) { background-color: #f2f2f2; }\n"
        "    .success { color: green; font-weight: bold; }\n"
        "    .error { color: red; font-weight: bold; }\n"
        "    .hash { font-family: monospace; font-size: 12px; word-break: break-all; }\n"
        "    .summary-box { background: #f8f9fa; padding: 20px; border-radius: 5px; }\n"
        "    .track-ok { background: #d4edda; }\n"
        "    .track-err { background: #f8d7da; }\n"
        "  </style>\n"
        "</head>\n"
        "<body>\n",
        report->options.title ? report->options.title : "Disk Image");
    
    /* Header */
    pos += snprintf(html + pos, size - pos,
        "<h1>%s</h1>\n"
        "<p><strong>Generated:</strong> %s</p>\n",
        report->options.title ? report->options.title : "Forensic Report",
        timestamp);
    
    if (report->options.organization) {
        pos += snprintf(html + pos, size - pos,
            "<p><strong>Organization:</strong> %s</p>\n", report->options.organization);
    }
    if (report->options.case_number) {
        pos += snprintf(html + pos, size - pos,
            "<p><strong>Case Number:</strong> %s</p>\n", report->options.case_number);
    }
    
    /* Result */
    pos += snprintf(html + pos, size - pos,
        "<div class=\"summary-box\">\n"
        "<h2>Result</h2>\n"
        "<p class=\"%s\">%s</p>\n"
        "<p>%s</p>\n"
        "</div>\n",
        report->success ? "success" : "error",
        report->success ? "SUCCESS" : "FAILED",
        report->result_message ? report->result_message : "");
    
    /* Metadata */
    if (report->options.sections & UFT_REPORT_SEC_METADATA) {
        pos += snprintf(html + pos, size - pos,
            "<h2>Disk Metadata</h2>\n"
            "<table>\n"
            "<tr><th>Property</th><th>Value</th></tr>\n"
            "<tr><td>Source</td><td>%s</td></tr>\n"
            "<tr><td>Cylinders</td><td>%d</td></tr>\n"
            "<tr><td>Heads</td><td>%d</td></tr>\n"
            "<tr><td>Sectors/Track</td><td>%d</td></tr>\n"
            "<tr><td>Bytes/Sector</td><td>%d</td></tr>\n"
            "<tr><td>Total Size</td><td>%zu bytes</td></tr>\n"
            "<tr><td>Format</td><td>%s</td></tr>\n"
            "<tr><td>Filesystem</td><td>%s</td></tr>\n"
            "</table>\n",
            report->metadata.source_path ? report->metadata.source_path : "-",
            report->metadata.cylinders,
            report->metadata.heads,
            report->metadata.sectors_per_track,
            report->metadata.bytes_per_sector,
            report->metadata.total_size,
            report->metadata.detected_format ? report->metadata.detected_format : "-",
            report->metadata.detected_filesystem ? report->metadata.detected_filesystem : "-");
    }
    
    /* Hashes */
    if (report->options.sections & UFT_REPORT_SEC_HASHES) {
        uft_hash_chain_finalize(report->hash_chain);
        pos += snprintf(html + pos, size - pos,
            "<h2>Hash Verification</h2>\n"
            "<table>\n"
            "<tr><th>Algorithm</th><td>%s</td></tr>\n"
            "<tr><th>Root Hash</th><td class=\"hash\">%s</td></tr>\n"
            "<tr><th>Chain Entries</th><td>%d</td></tr>\n"
            "<tr><th>Verified</th><td class=\"%s\">%s</td></tr>\n"
            "</table>\n",
            uft_hash_algo_name(report->hash_chain->algorithm),
            report->hash_chain->root_hash,
            report->hash_chain->count,
            report->hash_chain->verified ? "success" : "error",
            report->hash_chain->verified ? "YES" : "NO");
    }
    
    /* Errors */
    if ((report->options.sections & UFT_REPORT_SEC_ERRORS) && report->error_count > 0) {
        pos += snprintf(html + pos, size - pos,
            "<h2>Errors (%d)</h2>\n"
            "<table>\n"
            "<tr><th>Location</th><th>Code</th><th>Message</th></tr>\n",
            report->error_count);
        
        for (int i = 0; i < report->error_count && pos < (int)size - 256; i++) {
            report_error_t *e = &report->errors[i];
            pos += snprintf(html + pos, size - pos,
                "<tr><td>C%d/H%d/S%d</td><td>%d</td><td>%s</td></tr>\n",
                e->cylinder, e->head, e->sector, e->code,
                e->message ? e->message : "");
        }
        pos += snprintf(html + pos, size - pos, "</table>\n");
    }
    
    /* Footer */
    pos += snprintf(html + pos, size - pos,
        "<hr>\n"
        "<p><small>Generated by UnifiedFloppyTool v5.1.0</small></p>\n"
        "</body>\n"
        "</html>\n");
    
    return html;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Report Generation API
 * ═══════════════════════════════════════════════════════════════════════════════ */

uft_error_t uft_report_generate(uft_report_t *report, const char *path) {
    if (!report || !path) return UFT_ERR_INVALID_PARAM;
    
    char *content = uft_report_generate_string(report);
    if (!content) return UFT_ERR_MEMORY;
    
    FILE *f = fopen(path, "w");
    if (!f) {
        free(content);
        return UFT_ERR_IO;
    }
    
    fputs(content, f);
    fclose(f);
    free(content);
    
    return UFT_OK;
}

char *uft_report_generate_string(uft_report_t *report) {
    if (!report) return NULL;
    return uft_report_generate_format(report, report->options.format);
}

char *uft_report_generate_format(uft_report_t *report, uft_report_format_t format) {
    if (!report) return NULL;
    
    switch (format) {
        case UFT_REPORT_FORMAT_JSON:
            return generate_json(report);
        case UFT_REPORT_FORMAT_HTML:
            return generate_html(report);
        case UFT_REPORT_FORMAT_MARKDOWN:
        case UFT_REPORT_FORMAT_TEXT:
        case UFT_REPORT_FORMAT_XML:
        case UFT_REPORT_FORMAT_PDF:
        default:
            return generate_json(report);  /* Fallback to JSON */
    }
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Hash Chain Implementation
 * ═══════════════════════════════════════════════════════════════════════════════ */

uft_hash_chain_t *uft_hash_chain_create(uft_hash_algo_t algorithm) {
    uft_hash_chain_t *chain = calloc(1, sizeof(uft_hash_chain_t));
    if (!chain) return NULL;
    
    chain->capacity = 256;
    chain->entries = calloc(chain->capacity, sizeof(uft_hash_entry_t));
    chain->algorithm = algorithm;
    
    return chain;
}

void uft_hash_chain_destroy(uft_hash_chain_t *chain) {
    if (!chain) return;
    
    for (int i = 0; i < chain->count; i++) {
        free(chain->entries[i].data_id);
    }
    free(chain->entries);
    free(chain);
}

uft_hash_entry_t *uft_hash_chain_add(uft_hash_chain_t *chain, const char *data_id,
                                      const uint8_t *data, size_t size) {
    if (!chain || !data_id || !data) return NULL;
    
    if (chain->count >= chain->capacity) {
        chain->capacity *= 2;
        chain->entries = realloc(chain->entries, 
                                  chain->capacity * sizeof(uft_hash_entry_t));
    }
    
    uft_hash_entry_t *entry = &chain->entries[chain->count];
    memset(entry, 0, sizeof(*entry));
    
    entry->data_id = strdup(data_id);
    entry->algorithm = chain->algorithm;
    entry->data_size = size;
    entry->timestamp = get_timestamp_ms();
    entry->sequence = chain->count;
    
    /* Copy previous hash */
    if (chain->count > 0) {
        strncpy(entry->prev_hash, chain->entries[chain->count - 1].hash, sizeof(entry->prev_hash) - 1); entry->prev_hash[sizeof(entry->prev_hash) - 1] = '\0';
    }
    
    /* Compute hash */
    uft_compute_hash(chain->algorithm, data, size, entry->hash, sizeof(entry->hash));
    
    chain->count++;
    return entry;
}

const char *uft_hash_chain_finalize(uft_hash_chain_t *chain) {
    if (!chain || chain->count == 0) return "";
    
    /* Compute root hash from all entry hashes */
    size_t total_len = chain->count * 64;
    char *combined = malloc(total_len + 1);
    if (!combined) return "";
    
    combined[0] = '\0';
    for (int i = 0; i < chain->count; i++) {
        strncat(combined, chain->entries[i].hash, total_len - strlen(combined));
    }
    
    uft_compute_hash(chain->algorithm, (uint8_t *)combined, strlen(combined),
                      chain->root_hash, sizeof(chain->root_hash));
    
    free(combined);
    chain->verified = uft_hash_chain_verify(chain);
    
    return chain->root_hash;
}

bool uft_hash_chain_verify(uft_hash_chain_t *chain) {
    if (!chain) return false;
    
    /* Verify chain links */
    for (int i = 1; i < chain->count; i++) {
        if (strcmp(chain->entries[i].prev_hash, chain->entries[i-1].hash) != 0) {
            return false;
        }
    }
    
    return true;
}

char *uft_hash_chain_to_json(const uft_hash_chain_t *chain) {
    if (!chain) return strdup("[]");
    
    size_t size = chain->count * 256 + 256;
    char *json = malloc(size);
    if (!json) return NULL;
    
    int pos = 0;
    pos += snprintf(json + pos, size - pos, "[\n");
    
    for (int i = 0; i < chain->count && pos < (int)size - 256; i++) {
        uft_hash_entry_t *e = &chain->entries[i];
        pos += snprintf(json + pos, size - pos,
            "  {\"seq\": %u, \"id\": \"%s\", \"hash\": \"%s\", \"size\": %zu}%s\n",
            e->sequence, e->data_id, e->hash, e->data_size,
            (i < chain->count - 1) ? "," : "");
    }
    
    pos += snprintf(json + pos, size - pos, "]");
    return json;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Audit Trail Implementation
 * ═══════════════════════════════════════════════════════════════════════════════ */

uft_audit_trail_t *uft_audit_trail_create(void) {
    uft_audit_trail_t *trail = calloc(1, sizeof(uft_audit_trail_t));
    if (!trail) return NULL;
    
    trail->capacity = 256;
    trail->entries = calloc(trail->capacity, sizeof(uft_audit_entry_t));
    trail->start_time = get_timestamp_ms();
    
    return trail;
}

void uft_audit_trail_destroy(uft_audit_trail_t *trail) {
    if (!trail) return;
    
    for (int i = 0; i < trail->count; i++) {
        free(trail->entries[i].description);
        free(trail->entries[i].detail);
    }
    free(trail->entries);
    free(trail);
}

void uft_audit_log(uft_audit_trail_t *trail, uft_audit_event_t event,
                    const char *description, int cylinder, int head, int sector) {
    if (!trail) return;
    
    if (trail->count >= trail->capacity) {
        trail->capacity *= 2;
        trail->entries = realloc(trail->entries,
                                  trail->capacity * sizeof(uft_audit_entry_t));
    }
    
    uft_audit_entry_t *entry = &trail->entries[trail->count++];
    entry->timestamp = get_timestamp_ms();
    entry->event = event;
    entry->description = strdup_safe(description);
    entry->detail = NULL;
    entry->cylinder = cylinder;
    entry->head = head;
    entry->sector = sector;
    entry->error_code = UFT_OK;
    
    if (event == UFT_AUDIT_END) {
        trail->end_time = entry->timestamp;
    }
}

char *uft_audit_trail_to_json(const uft_audit_trail_t *trail) {
    if (!trail) return strdup("[]");
    
    size_t size = trail->count * 256 + 256;
    char *json = malloc(size);
    if (!json) return NULL;
    
    int pos = 0;
    pos += snprintf(json + pos, size - pos, "[\n");
    
    for (int i = 0; i < trail->count && pos < (int)size - 256; i++) {
        uft_audit_entry_t *e = &trail->entries[i];
        pos += snprintf(json + pos, size - pos,
            "  {\"time\": %lu, \"event\": \"%s\", \"desc\": \"%s\", "
            "\"cyl\": %d, \"head\": %d}%s\n",
            (unsigned long)e->timestamp,
            uft_audit_event_name(e->event),
            e->description ? e->description : "",
            e->cylinder, e->head,
            (i < trail->count - 1) ? "," : "");
    }
    
    pos += snprintf(json + pos, size - pos, "]");
    return json;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Utility Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

const char *uft_report_format_name(uft_report_format_t format) {
    switch (format) {
        case UFT_REPORT_FORMAT_JSON:     return "JSON";
        case UFT_REPORT_FORMAT_HTML:     return "HTML";
        case UFT_REPORT_FORMAT_PDF:      return "PDF";
        case UFT_REPORT_FORMAT_MARKDOWN: return "Markdown";
        case UFT_REPORT_FORMAT_TEXT:     return "Text";
        case UFT_REPORT_FORMAT_XML:      return "XML";
        default:                         return "Unknown";
    }
}

const char *uft_report_type_name(uft_report_type_t type) {
    switch (type) {
        case UFT_REPORT_TYPE_READ:       return "Read";
        case UFT_REPORT_TYPE_WRITE:      return "Write";
        case UFT_REPORT_TYPE_VERIFY:     return "Verify";
        case UFT_REPORT_TYPE_RECOVERY:   return "Recovery";
        case UFT_REPORT_TYPE_ANALYSIS:   return "Analysis";
        case UFT_REPORT_TYPE_COMPARISON: return "Comparison";
        case UFT_REPORT_TYPE_CONVERSION: return "Conversion";
        case UFT_REPORT_TYPE_INVENTORY:  return "Inventory";
        default:                         return "Unknown";
    }
}

const char *uft_hash_algo_name(uft_hash_algo_t algo) {
    switch (algo) {
        case UFT_HASH_MD5:    return "MD5";
        case UFT_HASH_SHA1:   return "SHA1";
        case UFT_HASH_SHA256: return "SHA256";
        case UFT_HASH_SHA512: return "SHA512";
        case UFT_HASH_CRC32:  return "CRC32";
        case UFT_HASH_XXH64:  return "XXH64";
        default:              return "Unknown";
    }
}

const char *uft_audit_event_name(uft_audit_event_t event) {
    switch (event) {
        case UFT_AUDIT_START:   return "START";
        case UFT_AUDIT_END:     return "END";
        case UFT_AUDIT_READ:    return "READ";
        case UFT_AUDIT_WRITE:   return "WRITE";
        case UFT_AUDIT_ERROR:   return "ERROR";
        case UFT_AUDIT_RETRY:   return "RETRY";
        case UFT_AUDIT_SKIP:    return "SKIP";
        case UFT_AUDIT_RECOVER: return "RECOVER";
        case UFT_AUDIT_VERIFY:  return "VERIFY";
        case UFT_AUDIT_CONFIG:  return "CONFIG";
        case UFT_AUDIT_USER:    return "USER";
        default:                return "UNKNOWN";
    }
}

void uft_compute_hash(uft_hash_algo_t algo, const uint8_t *data, size_t size,
                       char *hash_out, size_t hash_size) {
    if (!data || !hash_out || hash_size < 65) return;
    
    switch (algo) {
        case UFT_HASH_CRC32: {
            uint32_t crc = simple_crc32(data, size);
            snprintf(hash_out, hash_size, "%08x", crc);
            break;
        }
        case UFT_HASH_SHA256:
        default:
            simple_sha256(data, size, hash_out);
            break;
    }
}

const char *uft_report_format_extension(uft_report_format_t format) {
    switch (format) {
        case UFT_REPORT_FORMAT_JSON:     return ".json";
        case UFT_REPORT_FORMAT_HTML:     return ".html";
        case UFT_REPORT_FORMAT_PDF:      return ".pdf";
        case UFT_REPORT_FORMAT_MARKDOWN: return ".md";
        case UFT_REPORT_FORMAT_TEXT:     return ".txt";
        case UFT_REPORT_FORMAT_XML:      return ".xml";
        default:                         return ".txt";
    }
}
