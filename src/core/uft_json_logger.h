/**
 * @file uft_json_logger.h
 * @brief Structured JSON Logging
 * 
 * P3-002: JSON-Format für strukturiertes Logging
 */

#ifndef UFT_JSON_LOGGER_H
#define UFT_JSON_LOGGER_H

#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Log Levels
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef enum {
    UFT_LOG_TRACE = 0,
    UFT_LOG_DEBUG = 1,
    UFT_LOG_INFO = 2,
    UFT_LOG_WARN = 3,
    UFT_LOG_ERROR = 4,
    UFT_LOG_FATAL = 5,
} uft_log_level_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Logger Configuration
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    uft_log_level_t min_level;
    bool            json_enabled;
    bool            file_enabled;
    bool            console_enabled;
    bool            timestamp_enabled;
    bool            source_enabled;
    const char      *log_path;
    size_t          max_file_size;
    int             max_files;
} uft_logger_config_t;

typedef struct uft_logger uft_logger_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * API
 * ═══════════════════════════════════════════════════════════════════════════════ */

uft_logger_t* uft_logger_create(const uft_logger_config_t *config);
void uft_logger_destroy(uft_logger_t *logger);

void uft_logger_set_level(uft_logger_t *logger, uft_log_level_t level);
void uft_logger_set_json(uft_logger_t *logger, bool enabled);

/* Core logging */
void uft_log(uft_logger_t *logger, uft_log_level_t level,
             const char *source, int line,
             const char *fmt, ...);

void uft_log_json(uft_logger_t *logger, uft_log_level_t level,
                  const char *event, const char *json_data);

/* Convenience macros */
#define UFT_TRACE(logger, ...) \
    uft_log(logger, UFT_LOG_TRACE, __FILE__, __LINE__, __VA_ARGS__)

#define UFT_DEBUG(logger, ...) \
    uft_log(logger, UFT_LOG_DEBUG, __FILE__, __LINE__, __VA_ARGS__)

#define UFT_INFO(logger, ...) \
    uft_log(logger, UFT_LOG_INFO, __FILE__, __LINE__, __VA_ARGS__)

#define UFT_WARN(logger, ...) \
    uft_log(logger, UFT_LOG_WARN, __FILE__, __LINE__, __VA_ARGS__)

#define UFT_ERROR(logger, ...) \
    uft_log(logger, UFT_LOG_ERROR, __FILE__, __LINE__, __VA_ARGS__)

#define UFT_FATAL(logger, ...) \
    uft_log(logger, UFT_LOG_FATAL, __FILE__, __LINE__, __VA_ARGS__)

/* Structured JSON logging */
#define UFT_LOG_EVENT(logger, level, event, ...) \
    uft_log_event(logger, level, event, __VA_ARGS__)

void uft_log_event(uft_logger_t *logger, uft_log_level_t level,
                   const char *event, ...);

/* Get level name */
const char* uft_log_level_name(uft_log_level_t level);

/* Global logger (optional) */
void uft_logger_set_global(uft_logger_t *logger);
uft_logger_t* uft_logger_get_global(void);

#ifdef __cplusplus
}
#endif

#endif /* UFT_JSON_LOGGER_H */
