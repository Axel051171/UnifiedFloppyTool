/**
 * @file uft_log.h
 * @brief Structured JSON Logging System (W-P3-002)
 * 
 * Features:
 * - Multiple log levels (ERROR, WARN, INFO, DEBUG, TRACE)
 * - JSON and plain text output
 * - File and console logging
 * - Contextual logging (component, operation)
 * - Performance metrics
 * 
 * @version 1.0.0
 * @date 2026-01-15
 */

#ifndef UFT_LOG_H
#define UFT_LOG_H

#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * LOG LEVELS
 *===========================================================================*/

typedef enum {
    UFT_LOG_NONE  = 0,
    UFT_LOG_ERROR = 1,
    UFT_LOG_WARN  = 2,
    UFT_LOG_INFO  = 3,
    UFT_LOG_DEBUG = 4,
    UFT_LOG_TRACE = 5
} uft_log_level_t;

/*===========================================================================
 * LOG OUTPUT
 *===========================================================================*/

typedef enum {
    UFT_LOG_OUTPUT_NONE    = 0,
    UFT_LOG_OUTPUT_CONSOLE = 1,
    UFT_LOG_OUTPUT_FILE    = 2,
    UFT_LOG_OUTPUT_BOTH    = 3
} uft_log_output_t;

typedef enum {
    UFT_LOG_FORMAT_PLAIN,
    UFT_LOG_FORMAT_JSON,
    UFT_LOG_FORMAT_JSONL     /* JSON Lines (one per line) */
} uft_log_format_t;

/*===========================================================================
 * CONFIGURATION
 *===========================================================================*/

typedef struct {
    uft_log_level_t level;
    uft_log_output_t output;
    uft_log_format_t format;
    const char *log_file;
    bool include_timestamp;
    bool include_location;
    bool colorize;
    size_t max_file_size;    /**< Max log file size (0 = unlimited) */
    int max_files;           /**< Max rotated files to keep */
} uft_log_config_t;

#define UFT_LOG_CONFIG_DEFAULT { \
    .level = UFT_LOG_INFO, \
    .output = UFT_LOG_OUTPUT_CONSOLE, \
    .format = UFT_LOG_FORMAT_PLAIN, \
    .log_file = NULL, \
    .include_timestamp = true, \
    .include_location = false, \
    .colorize = true, \
    .max_file_size = 10 * 1024 * 1024, \
    .max_files = 5 \
}

/*===========================================================================
 * LOG CONTEXT
 *===========================================================================*/

typedef struct {
    const char *component;   /**< Component name (e.g., "PLL", "Format") */
    const char *operation;   /**< Current operation (e.g., "read_track") */
    int track;               /**< Track number (-1 if N/A) */
    int side;                /**< Side number (-1 if N/A) */
    int sector;              /**< Sector number (-1 if N/A) */
} uft_log_context_t;

#define UFT_LOG_CONTEXT_INIT { NULL, NULL, -1, -1, -1 }

/*===========================================================================
 * LIFECYCLE
 *===========================================================================*/

/**
 * @brief Initialize logging system
 */
int uft_log_init(const uft_log_config_t *config);

/**
 * @brief Shutdown logging system
 */
void uft_log_shutdown(void);

/**
 * @brief Reconfigure logging
 */
int uft_log_configure(const uft_log_config_t *config);

/**
 * @brief Get current configuration
 */
const uft_log_config_t* uft_log_get_config(void);

/**
 * @brief Set log level
 */
void uft_log_set_level(uft_log_level_t level);

/**
 * @brief Get log level
 */
uft_log_level_t uft_log_get_level(void);

/*===========================================================================
 * LOGGING FUNCTIONS
 *===========================================================================*/

/**
 * @brief Log message with context
 */
void uft_log_msg(
    uft_log_level_t level,
    const uft_log_context_t *ctx,
    const char *file,
    int line,
    const char *func,
    const char *fmt, ...);

/**
 * @brief Log message (va_list version)
 */
void uft_log_msgv(
    uft_log_level_t level,
    const uft_log_context_t *ctx,
    const char *file,
    int line,
    const char *func,
    const char *fmt,
    va_list args);

/*===========================================================================
 * CONVENIENCE MACROS
 *===========================================================================*/

#define UFT_LOG(level, ...) \
    uft_log_msg(level, NULL, __FILE__, __LINE__, __func__, __VA_ARGS__)

#define UFT_LOG_CTX(level, ctx, ...) \
    uft_log_msg(level, ctx, __FILE__, __LINE__, __func__, __VA_ARGS__)

#define UFT_ERROR(...) UFT_LOG(UFT_LOG_ERROR, __VA_ARGS__)
#define UFT_WARN(...)  UFT_LOG(UFT_LOG_WARN, __VA_ARGS__)
#define UFT_INFO(...)  UFT_LOG(UFT_LOG_INFO, __VA_ARGS__)
#define UFT_DEBUG(...) UFT_LOG(UFT_LOG_DEBUG, __VA_ARGS__)
#define UFT_TRACE(...) UFT_LOG(UFT_LOG_TRACE, __VA_ARGS__)

/* Context-aware logging */
#define UFT_ERROR_CTX(ctx, ...) UFT_LOG_CTX(UFT_LOG_ERROR, ctx, __VA_ARGS__)
#define UFT_WARN_CTX(ctx, ...)  UFT_LOG_CTX(UFT_LOG_WARN, ctx, __VA_ARGS__)
#define UFT_INFO_CTX(ctx, ...)  UFT_LOG_CTX(UFT_LOG_INFO, ctx, __VA_ARGS__)
#define UFT_DEBUG_CTX(ctx, ...) UFT_LOG_CTX(UFT_LOG_DEBUG, ctx, __VA_ARGS__)
#define UFT_TRACE_CTX(ctx, ...) UFT_LOG_CTX(UFT_LOG_TRACE, ctx, __VA_ARGS__)

/*===========================================================================
 * STRUCTURED LOGGING
 *===========================================================================*/

/**
 * @brief Log key-value pair
 */
void uft_log_kv(uft_log_level_t level, const char *key, const char *value);
void uft_log_kv_int(uft_log_level_t level, const char *key, int value);
void uft_log_kv_float(uft_log_level_t level, const char *key, double value);
void uft_log_kv_bool(uft_log_level_t level, const char *key, bool value);

/**
 * @brief Start structured log entry
 */
void uft_log_begin(uft_log_level_t level, const char *event);

/**
 * @brief Add field to current entry
 */
void uft_log_field_str(const char *key, const char *value);
void uft_log_field_int(const char *key, int value);
void uft_log_field_float(const char *key, double value);
void uft_log_field_bool(const char *key, bool value);

/**
 * @brief End and emit structured log entry
 */
void uft_log_end(void);

/*===========================================================================
 * PERFORMANCE LOGGING
 *===========================================================================*/

/**
 * @brief Performance timer handle
 */
typedef struct uft_log_timer uft_log_timer_t;

/**
 * @brief Start performance timer
 */
uft_log_timer_t* uft_log_timer_start(const char *operation);

/**
 * @brief Stop timer and log duration
 */
void uft_log_timer_stop(uft_log_timer_t *timer);

/**
 * @brief Log performance metric
 */
void uft_log_metric(const char *name, double value, const char *unit);

/*===========================================================================
 * PROGRESS LOGGING
 *===========================================================================*/

/**
 * @brief Log progress update
 */
void uft_log_progress(const char *operation, int current, int total);

/*===========================================================================
 * UTILITIES
 *===========================================================================*/

/**
 * @brief Get level name
 */
const char* uft_log_level_name(uft_log_level_t level);

/**
 * @brief Parse level from string
 */
uft_log_level_t uft_log_level_parse(const char *str);

/**
 * @brief Flush log buffers
 */
void uft_log_flush(void);

/**
 * @brief Rotate log file
 */
int uft_log_rotate(void);

#ifdef __cplusplus
}
#endif

#endif /* UFT_LOG_H */
