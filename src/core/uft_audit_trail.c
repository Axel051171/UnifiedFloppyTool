/* Feature test macros for gethostname */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE 1
#endif
/**
 * @file uft_audit_trail.c
 * @brief UFT Audit Trail System Implementation
 * 
 * @version 3.2.0
 * @date 2026-01-04
 * 
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 UnifiedFloppyTool Contributors
 */

#include "uft/uft_audit_trail.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#include <unistd.h>
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * Global Session
 * ═══════════════════════════════════════════════════════════════════════════ */

static uft_audit_session_t* g_global_session = NULL;

void uft_audit_set_global(uft_audit_session_t* session) {
    g_global_session = session;
}

uft_audit_session_t* uft_audit_get_global(void) {
    return g_global_session;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Time Utilities
 * ═══════════════════════════════════════════════════════════════════════════ */

static uint64_t get_timestamp_us(void) {
#ifdef _WIN32
    LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    return (uint64_t)(counter.QuadPart * 1000000 / freq.QuadPart);
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000 + (uint64_t)tv.tv_usec;
#endif
}

static void generate_uuid(uint8_t* uuid) {
    /* Simple pseudo-random UUID (not cryptographically secure) */
    uint64_t ts = get_timestamp_us();
    for (int i = 0; i < 16; i++) {
        uuid[i] = (uint8_t)((ts >> (i * 4)) ^ (rand() & 0xFF));
    }
    /* Set version 4 */
    uuid[6] = (uuid[6] & 0x0F) | 0x40;
    uuid[8] = (uuid[8] & 0x3F) | 0x80;
}

char* uft_audit_format_time(time_t timestamp, char* buffer, size_t size) {
    if (!buffer || size < 20) return NULL;
    struct tm* tm = localtime(&timestamp);
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", tm);
    return buffer;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Event & Severity Names
 * ═══════════════════════════════════════════════════════════════════════════ */

const char* uft_audit_event_name(uft_audit_event_t event) {
    switch (event) {
        case UFT_AE_SESSION_START:      return "SESSION_START";
        case UFT_AE_SESSION_END:        return "SESSION_END";
        case UFT_AE_CONFIG_CHANGE:      return "CONFIG_CHANGE";
        case UFT_AE_FILE_OPEN:          return "FILE_OPEN";
        case UFT_AE_FILE_CLOSE:         return "FILE_CLOSE";
        case UFT_AE_FILE_READ:          return "FILE_READ";
        case UFT_AE_FILE_WRITE:         return "FILE_WRITE";
        case UFT_AE_FILE_CREATE:        return "FILE_CREATE";
        case UFT_AE_FILE_DELETE:        return "FILE_DELETE";
        case UFT_AE_FORMAT_DETECT:      return "FORMAT_DETECT";
        case UFT_AE_FORMAT_VERIFY:      return "FORMAT_VERIFY";
        case UFT_AE_FORMAT_CONVERT:     return "FORMAT_CONVERT";
        case UFT_AE_TRACK_READ:         return "TRACK_READ";
        case UFT_AE_TRACK_WRITE:        return "TRACK_WRITE";
        case UFT_AE_TRACK_DECODE:       return "TRACK_DECODE";
        case UFT_AE_TRACK_ENCODE:       return "TRACK_ENCODE";
        case UFT_AE_TRACK_REPAIR:       return "TRACK_REPAIR";
        case UFT_AE_SECTOR_READ:        return "SECTOR_READ";
        case UFT_AE_SECTOR_WRITE:       return "SECTOR_WRITE";
        case UFT_AE_SECTOR_VERIFY:      return "SECTOR_VERIFY";
        case UFT_AE_SECTOR_REPAIR:      return "SECTOR_REPAIR";
        case UFT_AE_HW_CONNECT:         return "HW_CONNECT";
        case UFT_AE_HW_DISCONNECT:      return "HW_DISCONNECT";
        case UFT_AE_HW_CALIBRATE:       return "HW_CALIBRATE";
        case UFT_AE_HW_READ_FLUX:       return "HW_READ_FLUX";
        case UFT_AE_HW_WRITE_FLUX:      return "HW_WRITE_FLUX";
        case UFT_AE_RECOVERY_START:     return "RECOVERY_START";
        case UFT_AE_RECOVERY_SUCCESS:   return "RECOVERY_SUCCESS";
        case UFT_AE_RECOVERY_FAIL:      return "RECOVERY_FAIL";
        case UFT_AE_RECOVERY_PARTIAL:   return "RECOVERY_PARTIAL";
        case UFT_AE_ERROR:              return "ERROR";
        case UFT_AE_WARNING:            return "WARNING";
        case UFT_AE_CRC_MISMATCH:       return "CRC_MISMATCH";
        case UFT_AE_DATA_LOSS:          return "DATA_LOSS";
        case UFT_AE_CHECKSUM_INPUT:     return "CHECKSUM_INPUT";
        case UFT_AE_CHECKSUM_OUTPUT:    return "CHECKSUM_OUTPUT";
        case UFT_AE_HASH_COMPUTED:      return "HASH_COMPUTED";
        default:                        return "UNKNOWN";
    }
}

const char* uft_audit_severity_name(uft_audit_severity_t severity) {
    switch (severity) {
        case UFT_AUDIT_DEBUG:       return "DEBUG";
        case UFT_AUDIT_INFO:        return "INFO";
        case UFT_AUDIT_WARNING:     return "WARNING";
        case UFT_AUDIT_ERROR:       return "ERROR";
        case UFT_AUDIT_CRITICAL:    return "CRITICAL";
        default:                    return "UNKNOWN";
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Session Management
 * ═══════════════════════════════════════════════════════════════════════════ */

uft_audit_session_t* uft_audit_session_create(const char* log_path, uint32_t flags) {
    uft_audit_session_t* session = (uft_audit_session_t*)calloc(1, sizeof(*session));
    if (!session) return NULL;
    
    generate_uuid(session->session_id);
    session->start_time = time(NULL);
    session->session_start_us = get_timestamp_us();
    session->flags = flags;
    session->min_severity = UFT_AUDIT_INFO;
    session->auto_flush = true;
    
    snprintf(session->uft_version, sizeof(session->uft_version), "3.2.0");
    
#ifdef _WIN32
    DWORD size = sizeof(session->hostname);
    GetComputerNameA(session->hostname, &size);
    snprintf(session->os_info, sizeof(session->os_info), "Windows");
#else
    gethostname(session->hostname, sizeof(session->hostname));
    snprintf(session->os_info, sizeof(session->os_info), "Unix");
#endif
    
    /* Initial entry allocation */
    session->entry_capacity = 1024;
    session->entries = (uft_audit_entry_t*)calloc(session->entry_capacity, sizeof(uft_audit_entry_t));
    if (!session->entries) {
        free(session);
        return NULL;
    }
    
    /* Open log file if requested */
    if (log_path && (flags & (UFT_AUDIT_FLAG_TEXT_LOG | UFT_AUDIT_FLAG_BINARY_LOG))) {
        strncpy(session->log_path, log_path, sizeof(session->log_path) - 1);
        session->log_file = fopen(log_path, "w");
    if (!session->log_file) { free(session->entries); free(session); return NULL; }
        if (session->log_file && (flags & UFT_AUDIT_FLAG_TEXT_LOG)) {
            char timebuf[32];
            fprintf(session->log_file, "# UFT Audit Log\n");
            fprintf(session->log_file, "# Session: %s\n", 
                    uft_audit_format_time(session->start_time, timebuf, sizeof(timebuf)));
            fprintf(session->log_file, "# Version: %s\n", session->uft_version);
            fprintf(session->log_file, "# Host: %s\n\n", session->hostname);
        }
    }
    
    /* Log session start */
    uft_audit_log(session, UFT_AE_SESSION_START, UFT_AUDIT_INFO, "Audit session started");
    
    return session;
}

int uft_audit_session_end(uft_audit_session_t* session) {
    if (!session) return -1;
    
    session->end_time = time(NULL);
    uft_audit_log(session, UFT_AE_SESSION_END, UFT_AUDIT_INFO, "Audit session ended");
    
    if (session->log_file) {
        char timebuf[32];
        fprintf(session->log_file, "\n# Session ended: %s\n",
                uft_audit_format_time(session->end_time, timebuf, sizeof(timebuf)));
        fprintf(session->log_file, "# Total events: %zu\n", session->entry_count);
        fclose(session->log_file);
        session->log_file = NULL;
    }
    
    return 0;
}

void uft_audit_session_free(uft_audit_session_t* session) {
    if (!session) return;
    
    if (session->log_file) {
        fclose(session->log_file);
    }
    
    /* Free extended data in entries */
    for (size_t i = 0; i < session->entry_count; i++) {
        free(session->entries[i].ext_data);
    }
    
    free(session->entries);
    free(session);
}

void uft_audit_set_min_severity(uft_audit_session_t* session, uft_audit_severity_t severity) {
    if (session) {
        session->min_severity = severity;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Entry Allocation
 * ═══════════════════════════════════════════════════════════════════════════ */

static uft_audit_entry_t* allocate_entry(uft_audit_session_t* session) {
    if (!session) return NULL;
    
    /* Grow if needed */
    if (session->entry_count >= session->entry_capacity) {
        size_t new_cap = session->entry_capacity * 2;
        if (new_cap > UFT_AUDIT_MAX_ENTRIES) {
            new_cap = UFT_AUDIT_MAX_ENTRIES;
        }
        if (session->entry_count >= new_cap) {
            return NULL;  /* At limit */
        }
        
        uft_audit_entry_t* new_entries = (uft_audit_entry_t*)realloc(
            session->entries, new_cap * sizeof(uft_audit_entry_t));
        if (!new_entries) return NULL;
        
        session->entries = new_entries;
        session->entry_capacity = new_cap;
    }
    
    uft_audit_entry_t* entry = &session->entries[session->entry_count++];
    memset(entry, 0, sizeof(*entry));
    
    entry->sequence = ++session->next_sequence;
    entry->timestamp_us = get_timestamp_us() - session->session_start_us;
    entry->wall_time = time(NULL);
    
    return entry;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Write Entry to Log
 * ═══════════════════════════════════════════════════════════════════════════ */

static void write_entry_to_log(uft_audit_session_t* session, const uft_audit_entry_t* entry) {
    if (!session || !session->log_file || !entry) return;
    
    char timebuf[32];
    uft_audit_format_time(entry->wall_time, timebuf, sizeof(timebuf));
    
    fprintf(session->log_file, "[%s] [%s] [%s] %s",
            timebuf,
            uft_audit_severity_name(entry->severity),
            uft_audit_event_name(entry->event),
            entry->description);
    
    if (entry->cylinder != 0 || entry->head != 0) {
        fprintf(session->log_file, " (C%u H%u", entry->cylinder, entry->head);
        if (entry->sector != 0) {
            fprintf(session->log_file, " S%u", entry->sector);
        }
        fprintf(session->log_file, ")");
    }
    
    if (entry->file_path[0]) {
        fprintf(session->log_file, " [%s]", entry->file_path);
    }
    
    fprintf(session->log_file, "\n");
    
    if (session->auto_flush) {
        fflush(session->log_file);
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Event Logging
 * ═══════════════════════════════════════════════════════════════════════════ */

uint64_t uft_audit_log(
    uft_audit_session_t* session,
    uft_audit_event_t event,
    uft_audit_severity_t severity,
    const char* description
) {
    if (!session || severity < session->min_severity) return 0;
    
    uft_audit_entry_t* entry = allocate_entry(session);
    if (!entry) return 0;
    
    entry->event = event;
    entry->severity = severity;
    
    if (description) {
        strncpy(entry->description, description, sizeof(entry->description) - 1);
    }
    
    write_entry_to_log(session, entry);
    
    return entry->sequence;
}

uint64_t uft_audit_log_track(
    uft_audit_session_t* session,
    uft_audit_event_t event,
    uft_audit_severity_t severity,
    uint8_t cylinder,
    uint8_t head,
    const char* description
) {
    if (!session || severity < session->min_severity) return 0;
    
    uft_audit_entry_t* entry = allocate_entry(session);
    if (!entry) return 0;
    
    entry->event = event;
    entry->severity = severity;
    entry->cylinder = cylinder;
    entry->head = head;
    
    if (description) {
        strncpy(entry->description, description, sizeof(entry->description) - 1);
    }
    
    write_entry_to_log(session, entry);
    
    return entry->sequence;
}

uint64_t uft_audit_log_sector(
    uft_audit_session_t* session,
    uft_audit_event_t event,
    uft_audit_severity_t severity,
    uint8_t cylinder,
    uint8_t head,
    uint8_t sector,
    const char* description
) {
    if (!session || severity < session->min_severity) return 0;
    
    uft_audit_entry_t* entry = allocate_entry(session);
    if (!entry) return 0;
    
    entry->event = event;
    entry->severity = severity;
    entry->cylinder = cylinder;
    entry->head = head;
    entry->sector = sector;
    
    if (description) {
        strncpy(entry->description, description, sizeof(entry->description) - 1);
    }
    
    write_entry_to_log(session, entry);
    
    return entry->sequence;
}

uint64_t uft_audit_log_file(
    uft_audit_session_t* session,
    uft_audit_event_t event,
    const char* file_path,
    size_t bytes,
    int result
) {
    if (!session) return 0;
    
    uft_audit_severity_t severity = (result == 0) ? UFT_AUDIT_INFO : UFT_AUDIT_WARNING;
    
    uft_audit_entry_t* entry = allocate_entry(session);
    if (!entry) return 0;
    
    entry->event = event;
    entry->severity = severity;
    entry->result_code = result;
    entry->bytes_affected = (uint32_t)bytes;
    
    if (file_path) {
        strncpy(entry->file_path, file_path, sizeof(entry->file_path) - 1);
    }
    
    snprintf(entry->description, sizeof(entry->description),
             "File operation: %zu bytes, result: %d", bytes, result);
    
    write_entry_to_log(session, entry);
    
    return entry->sequence;
}

uint64_t uft_audit_log_checksum(
    uft_audit_session_t* session,
    uft_audit_event_t event,
    const char* file_path,
    const char* hash_type,
    const char* hash_value
) {
    if (!session) return 0;
    
    uft_audit_entry_t* entry = allocate_entry(session);
    if (!entry) return 0;
    
    entry->event = event;
    entry->severity = UFT_AUDIT_INFO;
    
    if (file_path) {
        strncpy(entry->file_path, file_path, sizeof(entry->file_path) - 1);
    }
    
    snprintf(entry->description, sizeof(entry->description),
             "%s: %s", hash_type ? hash_type : "HASH", hash_value ? hash_value : "");
    
    write_entry_to_log(session, entry);
    
    return entry->sequence;
}

uint64_t uft_audit_log_data(
    uft_audit_session_t* session,
    uft_audit_event_t event,
    uft_audit_severity_t severity,
    const char* description,
    const void* data,
    size_t data_size
) {
    if (!session || severity < session->min_severity) return 0;
    
    uft_audit_entry_t* entry = allocate_entry(session);
    if (!entry) return 0;
    
    entry->event = event;
    entry->severity = severity;
    
    if (description) {
        strncpy(entry->description, description, sizeof(entry->description) - 1);
    }
    
    /* Store extended data if requested and size reasonable */
    if (data && data_size > 0 && data_size <= UFT_AUDIT_MAX_DATA_SIZE && session->include_data) {
        entry->ext_data = (uint8_t*)malloc(data_size);
        if (entry->ext_data) {
            memcpy(entry->ext_data, data, data_size);
            entry->ext_data_size = data_size;
        }
    }
    
    write_entry_to_log(session, entry);
    
    return entry->sequence;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Query Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

const uft_audit_entry_t* uft_audit_get_entry(
    const uft_audit_session_t* session,
    uint64_t sequence
) {
    if (!session || sequence == 0) return NULL;
    
    /* Binary search (entries are in sequence order) */
    size_t low = 0;
    size_t high = session->entry_count;
    
    while (low < high) {
        size_t mid = (low + high) / 2;
        if (session->entries[mid].sequence < sequence) {
            low = mid + 1;
        } else if (session->entries[mid].sequence > sequence) {
            high = mid;
        } else {
            return &session->entries[mid];
        }
    }
    
    return NULL;
}

size_t uft_audit_count_entries(
    const uft_audit_session_t* session,
    uint32_t event_mask,
    uft_audit_severity_t min_severity
) {
    if (!session) return 0;
    
    size_t count = 0;
    for (size_t i = 0; i < session->entry_count; i++) {
        const uft_audit_entry_t* entry = &session->entries[i];
        
        if (entry->severity < min_severity) continue;
        if (event_mask != 0 && !((1 << (entry->event & 0x0F)) & event_mask)) continue;
        
        count++;
    }
    
    return count;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Export Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

int uft_audit_export_json(const uft_audit_session_t* session, const char* path) {
    if (!session || !path) return -1;
    
    FILE* f = fopen(path, "w");
    if (!f) return -1;
    
    char timebuf[32];
    
    fprintf(f, "{\n");
    fprintf(f, "  \"session\": {\n");
    fprintf(f, "    \"version\": \"%s\",\n", session->uft_version);
    fprintf(f, "    \"hostname\": \"%s\",\n", session->hostname);
    fprintf(f, "    \"start_time\": \"%s\",\n", 
            uft_audit_format_time(session->start_time, timebuf, sizeof(timebuf)));
    if (session->end_time) {
        fprintf(f, "    \"end_time\": \"%s\",\n",
                uft_audit_format_time(session->end_time, timebuf, sizeof(timebuf)));
    }
    fprintf(f, "    \"entry_count\": %zu\n", session->entry_count);
    fprintf(f, "  },\n");
    
    fprintf(f, "  \"entries\": [\n");
    for (size_t i = 0; i < session->entry_count; i++) {
        const uft_audit_entry_t* e = &session->entries[i];
        fprintf(f, "    {\n");
        fprintf(f, "      \"sequence\": %lu,\n", (unsigned long)e->sequence);
        fprintf(f, "      \"timestamp_us\": %lu,\n", (unsigned long)e->timestamp_us);
        fprintf(f, "      \"event\": \"%s\",\n", uft_audit_event_name(e->event));
        fprintf(f, "      \"severity\": \"%s\",\n", uft_audit_severity_name(e->severity));
        fprintf(f, "      \"description\": \"%s\"", e->description);
        
        if (e->cylinder || e->head || e->sector) {
            fprintf(f, ",\n      \"location\": {\"cyl\": %d, \"head\": %d, \"sector\": %d}",
                    e->cylinder, e->head, e->sector);
        }
        if (e->file_path[0]) {
            fprintf(f, ",\n      \"file\": \"%s\"", e->file_path);
        }
        
        fprintf(f, "\n    }%s\n", (i < session->entry_count - 1) ? "," : "");
    }
    fprintf(f, "  ]\n");
    fprintf(f, "}\n");
    
    fclose(f);
    return 0;
}

int uft_audit_export_markdown(const uft_audit_session_t* session, const char* path) {
    if (!session || !path) return -1;
    
    FILE* f = fopen(path, "w");
    if (!f) return -1;
    
    char timebuf[32];
    
    fprintf(f, "# UFT Audit Report\n\n");
    fprintf(f, "## Session Information\n\n");
    fprintf(f, "| Property | Value |\n");
    fprintf(f, "|----------|-------|\n");
    fprintf(f, "| UFT Version | %s |\n", session->uft_version);
    fprintf(f, "| Hostname | %s |\n", session->hostname);
    fprintf(f, "| Start Time | %s |\n", 
            uft_audit_format_time(session->start_time, timebuf, sizeof(timebuf)));
    if (session->end_time) {
        fprintf(f, "| End Time | %s |\n",
                uft_audit_format_time(session->end_time, timebuf, sizeof(timebuf)));
    }
    fprintf(f, "| Total Events | %zu |\n\n", session->entry_count);
    
    /* Count by severity */
    int counts[5] = {0};
    for (size_t i = 0; i < session->entry_count; i++) {
        if (session->entries[i].severity <= UFT_AUDIT_CRITICAL) {
            counts[session->entries[i].severity]++;
        }
    }
    
    fprintf(f, "## Event Summary\n\n");
    fprintf(f, "| Severity | Count |\n");
    fprintf(f, "|----------|-------|\n");
    fprintf(f, "| Critical | %d |\n", counts[UFT_AUDIT_CRITICAL]);
    fprintf(f, "| Error | %d |\n", counts[UFT_AUDIT_ERROR]);
    fprintf(f, "| Warning | %d |\n", counts[UFT_AUDIT_WARNING]);
    fprintf(f, "| Info | %d |\n", counts[UFT_AUDIT_INFO]);
    fprintf(f, "| Debug | %d |\n\n", counts[UFT_AUDIT_DEBUG]);
    
    /* List errors and warnings */
    fprintf(f, "## Issues\n\n");
    bool has_issues = false;
    for (size_t i = 0; i < session->entry_count; i++) {
        const uft_audit_entry_t* e = &session->entries[i];
        if (e->severity >= UFT_AUDIT_WARNING) {
            has_issues = true;
            fprintf(f, "- **[%s]** %s", 
                    uft_audit_severity_name(e->severity), e->description);
            if (e->file_path[0]) {
                fprintf(f, " (`%s`)", e->file_path);
            }
            fprintf(f, "\n");
        }
    }
    if (!has_issues) {
        fprintf(f, "*No issues detected.*\n");
    }
    
    fclose(f);
    return 0;
}

void uft_audit_print_summary(const uft_audit_session_t* session, FILE* out) {
    if (!session) return;
    if (!out) out = stdout;
    
    char timebuf[32];
    
    fprintf(out, "=== UFT Audit Session Summary ===\n");
    fprintf(out, "Version: %s\n", session->uft_version);
    fprintf(out, "Started: %s\n", 
            uft_audit_format_time(session->start_time, timebuf, sizeof(timebuf)));
    fprintf(out, "Events:  %zu\n", session->entry_count);
    
    /* Count by severity */
    int errors = 0, warnings = 0;
    for (size_t i = 0; i < session->entry_count; i++) {
        if (session->entries[i].severity == UFT_AUDIT_ERROR ||
            session->entries[i].severity == UFT_AUDIT_CRITICAL) {
            errors++;
        } else if (session->entries[i].severity == UFT_AUDIT_WARNING) {
            warnings++;
        }
    }
    
    fprintf(out, "Errors:  %d\n", errors);
    fprintf(out, "Warnings: %d\n", warnings);
    fprintf(out, "================================\n");
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Load/Verify (Stub implementations)
 * ═══════════════════════════════════════════════════════════════════════════ */

uft_audit_session_t* uft_audit_session_load(const char* path) {
    /* TODO: Implement binary log loading */
    (void)path;
    return NULL;
}

int uft_audit_verify_session(const uft_audit_session_t* session) {
    if (!session) return -1;
    
    /* Verify sequence numbers are monotonic */
    for (size_t i = 1; i < session->entry_count; i++) {
        if (session->entries[i].sequence <= session->entries[i-1].sequence) {
            return -2;  /* Sequence error */
        }
    }
    
    return 0;
}
