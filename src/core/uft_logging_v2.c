/**
 * @file uft_logging_v2.c
 * @brief Enhanced Logging System Implementation
 */

#include "uft/core/uft_logging_v2.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif

/* ============================================================================
 * Internal State
 * ============================================================================ */

#define LOG_BUFFER_SIZE 1024
#define MAX_LOG_ENTRIES 1000

static struct {
    uft_log_config_t config;
    FILE *log_file;
    uint64_t start_time_us;
    bool initialized;
    
    /* Ring buffer for recent entries */
    uft_log_entry_t entries[MAX_LOG_ENTRIES];
    size_t entry_head;
    size_t entry_count;
    
    /* Statistics */
    uft_log_stats_t stats;
    
} g_log = {0};

/* ============================================================================
 * Time Functions
 * ============================================================================ */

static uint64_t get_time_us(void) {
#ifdef _WIN32
    LARGE_INTEGER freq, count;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&count);
    return (uint64_t)(count.QuadPart * 1000000 / freq.QuadPart);
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000 + tv.tv_usec;
#endif
}

/* ============================================================================
 * ANSI Color Codes
 * ============================================================================ */

static const char* level_color(uft_log_level_t level) {
    switch (level) {
        case UFT_LOG_LEVEL_ERROR:   return "\033[31m";  /* Red */
        case UFT_LOG_LEVEL_WARNING: return "\033[33m";  /* Yellow */
        case UFT_LOG_LEVEL_INFO:    return "\033[32m";  /* Green */
        case UFT_LOG_LEVEL_DEBUG:   return "\033[36m";  /* Cyan */
        case UFT_LOG_LEVEL_TRACE:   return "\033[90m";  /* Gray */
        default: return "";
    }
}

static const char* color_reset(void) {
    return "\033[0m";
}

/* ============================================================================
 * Configuration Functions
 * ============================================================================ */

void uft_log_init(void) {
    if (g_log.initialized) return;
    
    memset(&g_log, 0, sizeof(g_log));
    
    g_log.config.category_mask = UFT_LOG_DEFAULT;
    g_log.config.min_level = UFT_LOG_LEVEL_INFO;
    g_log.config.log_to_stdout = true;
    g_log.config.log_to_stderr = true;
    g_log.config.include_timestamp = true;
    g_log.config.color_output = true;
    
    g_log.start_time_us = get_time_us();
    g_log.initialized = true;
}

void uft_log_shutdown(void) {
    if (!g_log.initialized) return;
    
    if (g_log.log_file) {
        fclose(g_log.log_file);
        g_log.log_file = NULL;
    }
    
    g_log.initialized = false;
}

void uft_log_set_config(const uft_log_config_t *config) {
    if (!config) return;
    
    if (!g_log.initialized) uft_log_init();
    
    /* Close old file if path changed */
    if (g_log.log_file && config->log_file_path != g_log.config.log_file_path) {
        fclose(g_log.log_file);
        g_log.log_file = NULL;
    }
    
    g_log.config = *config;
    
    /* Open new log file */
    if (config->log_to_file && config->log_file_path && !g_log.log_file) {
        g_log.log_file = fopen(config->log_file_path, "a");
    }
}

const uft_log_config_t* uft_log_get_config(void) {
    if (!g_log.initialized) uft_log_init();
    return &g_log.config;
}

void uft_log_set_mask(uint32_t mask) {
    if (!g_log.initialized) uft_log_init();
    g_log.config.category_mask = mask;
}

uint32_t uft_log_get_mask(void) {
    if (!g_log.initialized) uft_log_init();
    return g_log.config.category_mask;
}

void uft_log_set_level(uft_log_level_t level) {
    if (!g_log.initialized) uft_log_init();
    g_log.config.min_level = level;
}

void uft_log_enable_category(uft_log_mask_t category, bool enable) {
    if (!g_log.initialized) uft_log_init();
    
    if (enable) {
        g_log.config.category_mask |= category;
    } else {
        g_log.config.category_mask &= ~category;
    }
}

bool uft_log_is_enabled(uft_log_mask_t category) {
    if (!g_log.initialized) return false;
    return (g_log.config.category_mask & category) != 0;
}

void uft_log_set_file(const char *path) {
    if (!g_log.initialized) uft_log_init();
    
    if (g_log.log_file) {
        fclose(g_log.log_file);
        g_log.log_file = NULL;
    }
    
    if (path) {
        g_log.config.log_file_path = path;
        g_log.config.log_to_file = true;
        g_log.log_file = fopen(path, "a");
    } else {
        g_log.config.log_to_file = false;
    }
}

void uft_log_set_callback(uft_log_callback_t callback, void *user_data) {
    if (!g_log.initialized) uft_log_init();
    g_log.config.callback = callback;
    g_log.config.callback_user_data = user_data;
}

/* ============================================================================
 * Logging Functions
 * ============================================================================ */

void uft_log_v(uft_log_mask_t category, uft_log_level_t level,
               const char *file, int line, const char *func,
               const char *fmt, va_list args) {
    if (!g_log.initialized) uft_log_init();
    
    /* Check if category and level are enabled */
    if (!(g_log.config.category_mask & category)) return;
    if (level > g_log.config.min_level) return;
    
    /* Create entry */
    uft_log_entry_t entry;
    entry.timestamp_us = get_time_us() - g_log.start_time_us;
    entry.category = category;
    entry.level = level;
    entry.source_file = file;
    entry.source_line = line;
    entry.function = func;
    
    vsnprintf(entry.message, sizeof(entry.message), fmt, args);
    
    /* Store in ring buffer */
    size_t idx = g_log.entry_head;
    g_log.entries[idx] = entry;
    g_log.entry_head = (g_log.entry_head + 1) % MAX_LOG_ENTRIES;
    if (g_log.entry_count < MAX_LOG_ENTRIES) {
        g_log.entry_count++;
    }
    
    /* Update statistics */
    g_log.stats.total_messages++;
    if (level == UFT_LOG_LEVEL_ERROR) g_log.stats.error_count++;
    if (level == UFT_LOG_LEVEL_WARNING) g_log.stats.warning_count++;
    
    int cat_idx = 0;
    uint32_t cat = category;
    while (cat > 1 && cat_idx < 8) { cat >>= 1; cat_idx++; }
    if (cat_idx < 8) g_log.stats.by_category[cat_idx]++;
    
    /* Format output */
    char buf[LOG_BUFFER_SIZE + 256];
    int pos = 0;
    
    if (g_log.config.color_output && (g_log.config.log_to_stdout || g_log.config.log_to_stderr)) {
        pos += snprintf(buf + pos, sizeof(buf) - pos, "%s", level_color(level));
    }
    
    if (g_log.config.include_timestamp) {
        double sec = entry.timestamp_us / 1000000.0;
        pos += snprintf(buf + pos, sizeof(buf) - pos, "[%8.3f] ", sec);
    }
    
    /* Category name */
    pos += snprintf(buf + pos, sizeof(buf) - pos, "[%s] ", uft_log_category_name(category));
    
    /* Level */
    pos += snprintf(buf + pos, sizeof(buf) - pos, "%s: ", uft_log_level_name(level));
    
    /* Message */
    pos += snprintf(buf + pos, sizeof(buf) - pos, "%s", entry.message);
    
    /* Source location */
    if (g_log.config.include_source && file) {
        const char *basename = strrchr(file, '/');
        if (!basename) basename = strrchr(file, '\\');
        if (!basename) basename = file; else basename++;
        
        pos += snprintf(buf + pos, sizeof(buf) - pos, " (%s:%d)", basename, line);
    }
    
    if (g_log.config.color_output && (g_log.config.log_to_stdout || g_log.config.log_to_stderr)) {
        pos += snprintf(buf + pos, sizeof(buf) - pos, "%s", color_reset());
    }
    
    pos += snprintf(buf + pos, sizeof(buf) - pos, "\n");
    
    /* Output */
    if (g_log.config.log_to_stdout && level >= UFT_LOG_LEVEL_INFO) {
        fputs(buf, stdout);
        fflush(stdout);
    }
    
    if (g_log.config.log_to_stderr && level <= UFT_LOG_LEVEL_WARNING) {
        fputs(buf, stderr);
    }
    
    if (g_log.config.log_to_file && g_log.log_file) {
        /* Strip colors for file */
        fprintf(g_log.log_file, "[%8.3f] [%s] %s: %s",
                entry.timestamp_us / 1000000.0,
                uft_log_category_name(category),
                uft_log_level_name(level),
                entry.message);
        if (g_log.config.include_source && file) {
            const char *basename = strrchr(file, '/');
            if (!basename) basename = file; else basename++;
            fprintf(g_log.log_file, " (%s:%d)", basename, line);
        }
        fprintf(g_log.log_file, "\n");
        fflush(g_log.log_file);
    }
    
    /* Callback */
    if (g_log.config.callback) {
        g_log.config.callback(&entry, g_log.config.callback_user_data);
    }
}

void uft_log(uft_log_mask_t category, uft_log_level_t level,
             const char *file, int line, const char *func,
             const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    uft_log_v(category, level, file, line, func, fmt, args);
    va_end(args);
}

/* ============================================================================
 * Buffer Access
 * ============================================================================ */

size_t uft_log_get_recent(uft_log_entry_t *entries, size_t max_entries,
                          uint32_t category_filter) {
    if (!g_log.initialized || !entries) return 0;
    
    size_t count = 0;
    size_t start = (g_log.entry_head + MAX_LOG_ENTRIES - g_log.entry_count) % MAX_LOG_ENTRIES;
    
    for (size_t i = 0; i < g_log.entry_count && count < max_entries; i++) {
        size_t idx = (start + i) % MAX_LOG_ENTRIES;
        
        if (category_filter == 0 || (g_log.entries[idx].category & category_filter)) {
            entries[count++] = g_log.entries[idx];
        }
    }
    
    return count;
}

void uft_log_clear_buffer(void) {
    if (!g_log.initialized) return;
    g_log.entry_head = 0;
    g_log.entry_count = 0;
}

int uft_log_export_json(const char *path) {
    if (!g_log.initialized || !path) return -1;
    
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    
    fprintf(f, "{\n");
    fprintf(f, "  \"log_entries\": [\n");
    
    size_t start = (g_log.entry_head + MAX_LOG_ENTRIES - g_log.entry_count) % MAX_LOG_ENTRIES;
    
    for (size_t i = 0; i < g_log.entry_count; i++) {
        size_t idx = (start + i) % MAX_LOG_ENTRIES;
        const uft_log_entry_t *e = &g_log.entries[idx];
        
        fprintf(f, "    {\n");
        fprintf(f, "      \"timestamp_us\": %lu,\n", (unsigned long)e->timestamp_us);
        fprintf(f, "      \"category\": \"%s\",\n", uft_log_category_name(e->category));
        fprintf(f, "      \"level\": \"%s\",\n", uft_log_level_name(e->level));
        fprintf(f, "      \"message\": \"%s\"\n", e->message);
        fprintf(f, "    }%s\n", (i < g_log.entry_count - 1) ? "," : "");
    }
    
    fprintf(f, "  ],\n");
    fprintf(f, "  \"statistics\": {\n");
    fprintf(f, "    \"total_messages\": %lu,\n", (unsigned long)g_log.stats.total_messages);
    fprintf(f, "    \"error_count\": %lu,\n", (unsigned long)g_log.stats.error_count);
    fprintf(f, "    \"warning_count\": %lu\n", (unsigned long)g_log.stats.warning_count);
    fprintf(f, "  }\n");
    fprintf(f, "}\n");
    
    fclose(f);
    return 0;
}

int uft_log_export_html(const char *path) {
    if (!g_log.initialized || !path) return -1;
    
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    
    fprintf(f, "<!DOCTYPE html>\n<html><head>\n");
    fprintf(f, "<title>UFT Log</title>\n");
    fprintf(f, "<style>\n");
    fprintf(f, "body { font-family: monospace; background: #1e1e1e; color: #d4d4d4; }\n");
    fprintf(f, ".error { color: #f44747; }\n");
    fprintf(f, ".warning { color: #cca700; }\n");
    fprintf(f, ".info { color: #4ec9b0; }\n");
    fprintf(f, ".debug { color: #569cd6; }\n");
    fprintf(f, ".trace { color: #808080; }\n");
    fprintf(f, "table { border-collapse: collapse; width: 100%%; }\n");
    fprintf(f, "td, th { padding: 4px 8px; border-bottom: 1px solid #333; }\n");
    fprintf(f, "</style></head><body>\n");
    fprintf(f, "<h1>UFT Log Export</h1>\n");
    fprintf(f, "<table><tr><th>Time</th><th>Category</th><th>Level</th><th>Message</th></tr>\n");
    
    size_t start = (g_log.entry_head + MAX_LOG_ENTRIES - g_log.entry_count) % MAX_LOG_ENTRIES;
    
    for (size_t i = 0; i < g_log.entry_count; i++) {
        size_t idx = (start + i) % MAX_LOG_ENTRIES;
        const uft_log_entry_t *e = &g_log.entries[idx];
        
        const char *css_class = "";
        switch (e->level) {
            case UFT_LOG_LEVEL_ERROR:   css_class = "error"; break;
            case UFT_LOG_LEVEL_WARNING: css_class = "warning"; break;
            case UFT_LOG_LEVEL_INFO:    css_class = "info"; break;
            case UFT_LOG_LEVEL_DEBUG:   css_class = "debug"; break;
            case UFT_LOG_LEVEL_TRACE:   css_class = "trace"; break;
        }
        
        fprintf(f, "<tr class=\"%s\"><td>%.3f</td><td>%s</td><td>%s</td><td>%s</td></tr>\n",
                css_class,
                e->timestamp_us / 1000000.0,
                uft_log_category_name(e->category),
                uft_log_level_name(e->level),
                e->message);
    }
    
    fprintf(f, "</table></body></html>\n");
    fclose(f);
    return 0;
}

/* ============================================================================
 * Statistics
 * ============================================================================ */

void uft_log_get_stats(uft_log_stats_t *stats) {
    if (!g_log.initialized || !stats) return;
    *stats = g_log.stats;
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

const char* uft_log_category_name(uft_log_mask_t category) {
    switch (category) {
        case UFT_LOG_DEVICE: return "DEVICE";
        case UFT_LOG_READ:   return "READ";
        case UFT_LOG_CELL:   return "CELL";
        case UFT_LOG_FORMAT: return "FORMAT";
        case UFT_LOG_WRITE:  return "WRITE";
        case UFT_LOG_VERIFY: return "VERIFY";
        case UFT_LOG_DEBUG:  return "DEBUG";
        case UFT_LOG_TRACE:  return "TRACE";
        default: return "UNKNOWN";
    }
}

const char* uft_log_level_name(uft_log_level_t level) {
    switch (level) {
        case UFT_LOG_LEVEL_ERROR:   return "ERROR";
        case UFT_LOG_LEVEL_WARNING: return "WARN";
        case UFT_LOG_LEVEL_INFO:    return "INFO";
        case UFT_LOG_LEVEL_DEBUG:   return "DEBUG";
        case UFT_LOG_LEVEL_TRACE:   return "TRACE";
        default: return "?";
    }
}

uint32_t uft_log_parse_mask(const char *str) {
    if (!str) return UFT_LOG_DEFAULT;
    
    /* Try numeric */
    char *end;
    long val = strtol(str, &end, 0);
    if (*end == '\0') {
        return (uint32_t)val;
    }
    
    /* Parse comma-separated names */
    uint32_t mask = 0;
    char buf[256];
    strncpy(buf, str, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    
    char *token = strtok(buf, ",");
    while (token) {
        while (*token == ' ') token++;
        
        if (strcasecmp(token, "device") == 0) mask |= UFT_LOG_DEVICE;
        else if (strcasecmp(token, "read") == 0) mask |= UFT_LOG_READ;
        else if (strcasecmp(token, "cell") == 0) mask |= UFT_LOG_CELL;
        else if (strcasecmp(token, "format") == 0) mask |= UFT_LOG_FORMAT;
        else if (strcasecmp(token, "write") == 0) mask |= UFT_LOG_WRITE;
        else if (strcasecmp(token, "verify") == 0) mask |= UFT_LOG_VERIFY;
        else if (strcasecmp(token, "debug") == 0) mask |= UFT_LOG_DEBUG;
        else if (strcasecmp(token, "trace") == 0) mask |= UFT_LOG_TRACE;
        else if (strcasecmp(token, "all") == 0) mask = UFT_LOG_ALL;
        else if (strcasecmp(token, "none") == 0) mask = UFT_LOG_NONE;
        
        token = strtok(NULL, ",");
    }
    
    return mask;
}

const char* uft_log_mask_to_string(uint32_t mask, char *buffer, size_t size) {
    if (!buffer || size == 0) return "";
    
    buffer[0] = '\0';
    size_t pos = 0;
    
    #define ADD_CAT(flag, name) \
        if (mask & flag) { \
            if (pos > 0 && pos < size - 1) buffer[pos++] = ','; \
            size_t len = strlen(name); \
            if (pos + len < size) { memcpy(buffer + pos, name, len); pos += len; buffer[pos] = '\0'; } \
        }
    
    ADD_CAT(UFT_LOG_DEVICE, "device");
    ADD_CAT(UFT_LOG_READ, "read");
    ADD_CAT(UFT_LOG_CELL, "cell");
    ADD_CAT(UFT_LOG_FORMAT, "format");
    ADD_CAT(UFT_LOG_WRITE, "write");
    ADD_CAT(UFT_LOG_VERIFY, "verify");
    ADD_CAT(UFT_LOG_DEBUG, "debug");
    ADD_CAT(UFT_LOG_TRACE, "trace");
    
    #undef ADD_CAT
    
    return buffer;
}
