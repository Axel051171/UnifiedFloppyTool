/**
 * @file uft_json_logger.c
 * @brief JSON Logging Implementation
 */

#include "uft_json_logger.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

struct uft_logger {
    uft_logger_config_t config;
    FILE                *file;
    pthread_mutex_t     mutex;
    uint64_t            log_count;
};

static uft_logger_t *g_global_logger = NULL;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Implementation
 * ═══════════════════════════════════════════════════════════════════════════════ */

uft_logger_t* uft_logger_create(const uft_logger_config_t *config) {
    uft_logger_t *logger = calloc(1, sizeof(uft_logger_t));
    if (!logger) return NULL;
    
    if (config) {
        logger->config = *config;
    } else {
        logger->config.min_level = UFT_LOG_INFO;
        logger->config.json_enabled = true;
        logger->config.console_enabled = true;
        logger->config.timestamp_enabled = true;
    }
    
    pthread_mutex_init(&logger->mutex, NULL);
    
    if (logger->config.file_enabled && logger->config.log_path) {
        logger->file = fopen(logger->config.log_path, "a");
    }
    
    return logger;
}

void uft_logger_destroy(uft_logger_t *logger) {
    if (!logger) return;
    
    pthread_mutex_lock(&logger->mutex);
    if (logger->file) {
        fclose(logger->file);
    }
    pthread_mutex_unlock(&logger->mutex);
    
    pthread_mutex_destroy(&logger->mutex);
    free(logger);
}

void uft_logger_set_level(uft_logger_t *logger, uft_log_level_t level) {
    if (logger) logger->config.min_level = level;
}

void uft_logger_set_json(uft_logger_t *logger, bool enabled) {
    if (logger) logger->config.json_enabled = enabled;
}

const char* uft_log_level_name(uft_log_level_t level) {
    static const char* names[] = {
        "TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"
    };
    if (level >= 0 && level <= UFT_LOG_FATAL) {
        return names[level];
    }
    return "UNKNOWN";
}

static void get_timestamp(char *buf, size_t size) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(buf, size, "%Y-%m-%dT%H:%M:%S", tm_info);
}

static void escape_json_string(const char *src, char *dst, size_t dst_size) {
    size_t j = 0;
    for (size_t i = 0; src[i] && j + 2 < dst_size; i++) {
        char c = src[i];
        if (c == '"' || c == '\\') {
            dst[j++] = '\\';
        } else if (c == '\n') {
            dst[j++] = '\\';
            c = 'n';
        } else if (c == '\r') {
            dst[j++] = '\\';
            c = 'r';
        } else if (c == '\t') {
            dst[j++] = '\\';
            c = 't';
        }
        dst[j++] = c;
    }
    dst[j] = '\0';
}

void uft_log(uft_logger_t *logger, uft_log_level_t level,
             const char *source, int line,
             const char *fmt, ...) {
    if (!logger || level < logger->config.min_level) return;
    
    pthread_mutex_lock(&logger->mutex);
    
    char message[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(message, sizeof(message), fmt, args);
    va_end(args);
    
    char timestamp[32] = {0};
    if (logger->config.timestamp_enabled) {
        get_timestamp(timestamp, sizeof(timestamp));
    }
    
    /* JSON output */
    if (logger->config.json_enabled) {
        char escaped[1024];
        escape_json_string(message, escaped, sizeof(escaped));
        
        char json[2048];
        int len = snprintf(json, sizeof(json),
            "{\"timestamp\":\"%s\",\"level\":\"%s\",\"message\":\"%s\"",
            timestamp, uft_log_level_name(level), escaped);
        
        if (logger->config.source_enabled && source) {
            len += snprintf(json + len, sizeof(json) - len,
                ",\"source\":\"%s\",\"line\":%d", source, line);
        }
        
        snprintf(json + len, sizeof(json) - len, ",\"seq\":%lu}\n",
                 (unsigned long)logger->log_count);
        
        if (logger->config.console_enabled) {
            fputs(json, stderr);
        }
        if (logger->file) {
            fputs(json, logger->file);
            fflush(logger->file);
        }
    } else {
        /* Plain text output */
        char output[2048];
        if (logger->config.source_enabled && source) {
            snprintf(output, sizeof(output), "[%s] [%s] %s:%d: %s\n",
                     timestamp, uft_log_level_name(level), source, line, message);
        } else {
            snprintf(output, sizeof(output), "[%s] [%s] %s\n",
                     timestamp, uft_log_level_name(level), message);
        }
        
        if (logger->config.console_enabled) {
            fputs(output, stderr);
        }
        if (logger->file) {
            fputs(output, logger->file);
            fflush(logger->file);
        }
    }
    
    logger->log_count++;
    pthread_mutex_unlock(&logger->mutex);
}

void uft_log_json(uft_logger_t *logger, uft_log_level_t level,
                  const char *event, const char *json_data) {
    if (!logger || level < logger->config.min_level) return;
    
    pthread_mutex_lock(&logger->mutex);
    
    char timestamp[32] = {0};
    get_timestamp(timestamp, sizeof(timestamp));
    
    char output[4096];
    snprintf(output, sizeof(output),
        "{\"timestamp\":\"%s\",\"level\":\"%s\",\"event\":\"%s\",\"data\":%s}\n",
        timestamp, uft_log_level_name(level), event, json_data ? json_data : "null");
    
    if (logger->config.console_enabled) {
        fputs(output, stderr);
    }
    if (logger->file) {
        fputs(output, logger->file);
        fflush(logger->file);
    }
    
    logger->log_count++;
    pthread_mutex_unlock(&logger->mutex);
}

void uft_logger_set_global(uft_logger_t *logger) {
    g_global_logger = logger;
}

uft_logger_t* uft_logger_get_global(void) {
    return g_global_logger;
}
