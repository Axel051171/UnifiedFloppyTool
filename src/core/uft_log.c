/**
 * @file uft_log.c
 * @brief Implementation of the structured logging system.
 *
 * Previously uft_log.h declared the API but had zero implementation.
 * UFT_ERROR/UFT_WARN/UFT_INFO macros would produce linker errors on use.
 *
 * This minimal implementation:
 *   - Outputs to stderr by default (compatible with fprintf(stderr) callers)
 *   - Supports level filtering (UFT_LOG_ERROR < UFT_LOG_WARN < ...)
 *   - Plain-text format with optional timestamp + location
 *   - Safe to call before uft_log_init() — uses defaults
 *
 * Phase P4 of API consolidation: provides the missing implementation so
 * callers can finally migrate from fprintf(stderr, ...) to UFT_ERROR(...).
 */
#include "uft/uft_log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ============================================================================
 * Global state (simple: one logger per process)
 * ============================================================================ */

static uft_log_config_t g_cfg = UFT_LOG_CONFIG_DEFAULT;
static FILE            *g_file_handle = NULL;
static bool             g_initialized = false;

static const char *level_label(uft_log_level_t l) {
    switch (l) {
        case UFT_LOG_ERROR: return "ERROR";
        case UFT_LOG_WARN:  return "WARN";
        case UFT_LOG_INFO:  return "INFO";
        case UFT_LOG_DEBUG: return "DEBUG";
        case UFT_LOG_TRACE: return "TRACE";
        default:            return "?";
    }
}

static const char *level_color(uft_log_level_t l) {
    switch (l) {
        case UFT_LOG_ERROR: return "\x1b[31m";    /* red */
        case UFT_LOG_WARN:  return "\x1b[33m";    /* yellow */
        case UFT_LOG_INFO:  return "\x1b[32m";    /* green */
        case UFT_LOG_DEBUG: return "\x1b[36m";    /* cyan */
        case UFT_LOG_TRACE: return "\x1b[90m";    /* gray */
        default:            return "";
    }
}

/* ============================================================================
 * Lifecycle
 * ============================================================================ */

int uft_log_init(const uft_log_config_t *config) {
    if (config) g_cfg = *config;
    /* Open file if requested */
    if ((g_cfg.output == UFT_LOG_OUTPUT_FILE ||
         g_cfg.output == UFT_LOG_OUTPUT_BOTH) && g_cfg.log_file) {
        g_file_handle = fopen(g_cfg.log_file, "a");
        if (!g_file_handle) return -1;
    }
    g_initialized = true;
    return 0;
}

void uft_log_shutdown(void) {
    if (g_file_handle) {
        fclose(g_file_handle);
        g_file_handle = NULL;
    }
    g_initialized = false;
}

int uft_log_configure(const uft_log_config_t *config) {
    uft_log_shutdown();
    return uft_log_init(config);
}

const uft_log_config_t* uft_log_get_config(void) {
    return &g_cfg;
}

void uft_log_set_level(uft_log_level_t level) {
    g_cfg.level = level;
}

uft_log_level_t uft_log_get_level(void) {
    return g_cfg.level;
}

/* ============================================================================
 * Core logging
 * ============================================================================ */

static void write_to_sinks(const char *line) {
    if (g_cfg.output == UFT_LOG_OUTPUT_NONE) return;

    if (g_cfg.output == UFT_LOG_OUTPUT_CONSOLE ||
        g_cfg.output == UFT_LOG_OUTPUT_BOTH) {
        fputs(line, stderr);
        fputc('\n', stderr);
    }
    if ((g_cfg.output == UFT_LOG_OUTPUT_FILE ||
         g_cfg.output == UFT_LOG_OUTPUT_BOTH) && g_file_handle) {
        fputs(line, g_file_handle);
        fputc('\n', g_file_handle);
        fflush(g_file_handle);
    }
}

void uft_log_msgv(
    uft_log_level_t level,
    const uft_log_context_t *ctx,
    const char *file,
    int line,
    const char *func,
    const char *fmt,
    va_list args)
{
    /* Filter: Enum is DEBUG=0 < INFO=1 < WARN=2 < ERROR=3 < NONE=4.
     * Pass only if level >= configured threshold. */
    if ((int)level < (int)g_cfg.level) return;

    char buf[2048];
    size_t off = 0;

    /* Timestamp */
    if (g_cfg.include_timestamp) {
        time_t now = time(NULL);
        struct tm tm_buf;
#if defined(_WIN32)
        localtime_s(&tm_buf, &now);
#else
        localtime_r(&now, &tm_buf);
#endif
        off += strftime(buf + off, sizeof(buf) - off, "%H:%M:%S ", &tm_buf);
    }

    /* Color + level */
    const char *color = g_cfg.colorize ? level_color(level) : "";
    const char *reset = g_cfg.colorize ? "\x1b[0m" : "";
    int n = snprintf(buf + off, sizeof(buf) - off, "%s[%s]%s ",
                     color, level_label(level), reset);
    if (n > 0) off += (size_t)n;

    /* Component/operation context */
    if (ctx && ctx->component) {
        n = snprintf(buf + off, sizeof(buf) - off, "[%s", ctx->component);
        if (n > 0) off += (size_t)n;
        if (ctx->operation) {
            n = snprintf(buf + off, sizeof(buf) - off, "/%s", ctx->operation);
            if (n > 0) off += (size_t)n;
        }
        n = snprintf(buf + off, sizeof(buf) - off, "] ");
        if (n > 0) off += (size_t)n;
    }

    /* Message itself */
    if (off < sizeof(buf)) {
        int m = vsnprintf(buf + off, sizeof(buf) - off, fmt, args);
        if (m > 0) off += (size_t)m;
    }

    /* File:line (optional) */
    if (g_cfg.include_location && file && off + 64 < sizeof(buf)) {
        n = snprintf(buf + off, sizeof(buf) - off, "  (%s:%d in %s)",
                     file, line, func ? func : "?");
        if (n > 0) off += (size_t)n;
    }

    write_to_sinks(buf);
}

void uft_log_msg(
    uft_log_level_t level,
    const uft_log_context_t *ctx,
    const char *file,
    int line,
    const char *func,
    const char *fmt,
    ...)
{
    va_list args;
    va_start(args, fmt);
    uft_log_msgv(level, ctx, file, line, func, fmt, args);
    va_end(args);
}
