/**
 * @file uft_logging.c
 * @brief Logging & Telemetry Implementation
 * 
 * @version 3.0.0
 * @date 2024-12-27
 */

#include "uft_logging.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

/* Global log configuration */
static uft_log_config_t g_log_config = {
    .min_level = UFT_LOG_INFO,
    .log_to_console = true,
    .log_to_file = false,
    .structured_json = false,
    .mutex = PTHREAD_MUTEX_INITIALIZER
};

static FILE* g_log_file = NULL;

/* Log level names */
static const char* log_level_names[] = {
    "TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"
};

/* Log level colors (ANSI) */
static const char* log_level_colors[] = {
    "\x1b[37m",  // TRACE: white
    "\x1b[36m",  // DEBUG: cyan
    "\x1b[32m",  // INFO: green
    "\x1b[33m",  // WARN: yellow
    "\x1b[31m",  // ERROR: red
    "\x1b[35m"   // FATAL: magenta
};

static const char* color_reset = "\x1b[0m";

/**
 * Initialize logging system
 */
uft_rc_t uft_log_init(const uft_log_config_t* config) {
    if (!config) {
        return UFT_ERR_INVALID_ARG;
    }
    
    pthread_mutex_lock(&g_log_config.mutex);
    
    memcpy(&g_log_config, config, sizeof(uft_log_config_t));
    g_log_config.mutex = PTHREAD_MUTEX_INITIALIZER;
    
    if (config->log_to_file && config->log_file_path) {
        g_log_file = fopen(config->log_file_path, "a");
        if (!g_log_file) {
            pthread_mutex_unlock(&g_log_config.mutex);
            return UFT_ERR_NOT_FOUND;
        }
    }
    
    pthread_mutex_unlock(&g_log_config.mutex);
    
    return UFT_SUCCESS;
}

/**
 * Shutdown logging
 */
void uft_log_shutdown(void) {
    pthread_mutex_lock(&g_log_config.mutex);
    
    if (g_log_file) {
        fclose(g_log_file);
        g_log_file = NULL;
    }
    
    pthread_mutex_unlock(&g_log_config.mutex);
}

/**
 * Get current time in microseconds
 */
uint64_t uft_get_time_us(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000ULL + (uint64_t)tv.tv_usec;
}

/**
 * Log message
 */
void uft_log(
    uft_log_level_t level,
    const char* file,
    int line,
    const char* func,
    const char* fmt,
    ...
) {
    if (level < g_log_config.min_level) {
        return;
    }
    
    pthread_mutex_lock(&g_log_config.mutex);
    
    /* Get timestamp */
    char timestamp[32];
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);
    
    /* Format message */
    char message[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(message, sizeof(message), fmt, args);
    va_end(args);
    
    /* Extract filename (without path) */
    const char* filename = strrchr(file, '/');
    filename = filename ? filename + 1 : file;
    
    if (g_log_config.structured_json) {
        /* JSON format */
        char json[2048];
        snprintf(json, sizeof(json),
                "{\"timestamp\":\"%s\",\"level\":\"%s\",\"file\":\"%s\","
                "\"line\":%d,\"function\":\"%s\",\"message\":\"%s\"}\n",
                timestamp,
                log_level_names[level],
                filename,
                line,
                func,
                message);
        
        if (g_log_config.log_to_console) {
            printf("%s", json);
        }
        
        if (g_log_config.log_to_file && g_log_file) {
            fprintf(g_log_file, "%s", json);
            fflush(g_log_file);
        }
    } else {
        /* Text format */
        if (g_log_config.log_to_console) {
            printf("%s[%s] %s %s:%d %s() - %s%s\n",
                   log_level_colors[level],
                   log_level_names[level],
                   timestamp,
                   filename,
                   line,
                   func,
                   message,
                   color_reset);
        }
        
        if (g_log_config.log_to_file && g_log_file) {
            fprintf(g_log_file, "[%s] %s %s:%d %s() - %s\n",
                    log_level_names[level],
                    timestamp,
                    filename,
                    line,
                    func,
                    message);
            fflush(g_log_file);
        }
    }
    
    pthread_mutex_unlock(&g_log_config.mutex);
}

/**
 * Log structured data (JSON)
 */
void uft_log_json(
    uft_log_level_t level,
    const char* event,
    const char* json_data
) {
    if (level < g_log_config.min_level) {
        return;
    }
    
    pthread_mutex_lock(&g_log_config.mutex);
    
    char timestamp[32];
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);
    
    char output[4096];
    snprintf(output, sizeof(output),
            "{\"timestamp\":\"%s\",\"level\":\"%s\",\"event\":\"%s\",\"data\":%s}\n",
            timestamp,
            log_level_names[level],
            event,
            json_data);
    
    if (g_log_config.log_to_console) {
        printf("%s", output);
    }
    
    if (g_log_config.log_to_file && g_log_file) {
        fprintf(g_log_file, "%s", output);
        fflush(g_log_file);
    }
    
    pthread_mutex_unlock(&g_log_config.mutex);
}

/**
 * Telemetry API
 */
uft_telemetry_t* uft_telemetry_create(void) {
    uft_telemetry_t* telem = calloc(1, sizeof(uft_telemetry_t));
    if (telem) {
        telem->start_time_us = uft_get_time_us();
        telem->min_flux_ns = UINT32_MAX;
    }
    return telem;
}

void uft_telemetry_update(uft_telemetry_t* telem, const char* key, uint64_t value) {
    if (!telem) return;
    
    if (strcmp(key, "bytes_read") == 0) {
        telem->bytes_read += value;
    } else if (strcmp(key, "bytes_written") == 0) {
        telem->bytes_written += value;
    } else if (strcmp(key, "flux_transitions") == 0) {
        telem->flux_transitions += value;
    } else if (strcmp(key, "tracks_processed") == 0) {
        telem->tracks_processed++;
    } else if (strcmp(key, "weak_bits") == 0) {
        telem->weak_bits_found += value;
    }
}

void uft_telemetry_log(const uft_telemetry_t* telem) {
    if (!telem) return;
    
    uint64_t duration_us = uft_get_time_us() - telem->start_time_us;
    double duration_s = duration_us / 1000000.0;
    
    UFT_LOG_INFO("=== TELEMETRY ===");
    UFT_LOG_INFO("Duration: %.2f seconds", duration_s);
    UFT_LOG_INFO("Bytes read: %llu", (unsigned long long)telem->bytes_read);
    UFT_LOG_INFO("Bytes written: %llu", (unsigned long long)telem->bytes_written);
    UFT_LOG_INFO("Flux transitions: %llu", (unsigned long long)telem->flux_transitions);
    UFT_LOG_INFO("Tracks processed: %u", telem->tracks_processed);
    UFT_LOG_INFO("Weak bits found: %u", telem->weak_bits_found);
    
    if (telem->tracks_processed > 0) {
        double tracks_per_sec = telem->tracks_processed / duration_s;
        UFT_LOG_INFO("Performance: %.2f tracks/sec", tracks_per_sec);
    }
}

void uft_telemetry_destroy(uft_telemetry_t** telem) {
    if (telem && *telem) {
        free(*telem);
        *telem = NULL;
    }
}
