/**
 * @file uft_logging.h
 * @brief Professional Logging & Telemetry System
 * 
 * FORENSIC-GRADE LOGGING
 * 
 * Features:
 * - Multiple log levels
 * - Thread-safe
 * - Structured logging (JSON)
 * - Performance metrics
 * - File + console output
 * - Rotation support
 * 
 * @version 3.0.0 (Professional Edition)
 * @date 2024-12-27
 */

#ifndef UFT_LOGGING_H
#define UFT_LOGGING_H

#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * LOG LEVELS
 * ======================================================================== */

typedef enum {
    UFT_LOG_TRACE = 0,   /**< Detailed flux timings */
    UFT_LOG_DEBUG = 1,   /**< Debugging info */
    UFT_LOG_INFO = 2,    /**< Progress info */
    UFT_LOG_WARN = 3,    /**< Warnings (retries, etc) */
    UFT_LOG_ERROR = 4,   /**< Errors */
    UFT_LOG_FATAL = 5    /**< Fatal errors */
} uft_log_level_t;

/* ========================================================================
 * LOG CONFIGURATION
 * ======================================================================== */

typedef struct {
    uft_log_level_t min_level;      /**< Minimum level to log */
    bool log_to_file;               /**< Enable file logging */
    bool log_to_console;            /**< Enable console logging */
    bool structured_json;           /**< JSON format */
    const char* log_file_path;      /**< Log file path */
    size_t max_file_size;           /**< Max size before rotation */
    uint32_t max_rotations;         /**< Number of rotated files */
    
    /* Thread safety */
    pthread_mutex_t mutex;
    
} uft_log_config_t;

/* ========================================================================
 * LOGGING MACROS - Use these!
 * ======================================================================== */

#define UFT_LOG_TRACE(...) \
    uft_log(UFT_LOG_TRACE, __FILE__, __LINE__, __func__, __VA_ARGS__)

#define UFT_LOG_DEBUG(...) \
    uft_log(UFT_LOG_DEBUG, __FILE__, __LINE__, __func__, __VA_ARGS__)

#define UFT_LOG_INFO(...) \
    uft_log(UFT_LOG_INFO, __FILE__, __LINE__, __func__, __VA_ARGS__)

#define UFT_LOG_WARN(...) \
    uft_log(UFT_LOG_WARN, __FILE__, __LINE__, __func__, __VA_ARGS__)

#define UFT_LOG_ERROR(...) \
    uft_log(UFT_LOG_ERROR, __FILE__, __LINE__, __func__, __VA_ARGS__)

#define UFT_LOG_FATAL(...) \
    uft_log(UFT_LOG_FATAL, __FILE__, __LINE__, __func__, __VA_ARGS__)

/* ========================================================================
 * TELEMETRY - Performance metrics
 * ======================================================================== */

typedef struct {
    /* I/O statistics */
    uint64_t bytes_read;
    uint64_t bytes_written;
    uint64_t read_errors;
    uint64_t write_errors;
    
    /* Timing */
    uint64_t start_time_us;
    uint64_t end_time_us;
    uint64_t total_time_us;
    
    /* Track statistics */
    uint32_t tracks_processed;
    uint32_t tracks_failed;
    uint32_t retries;
    
    /* Flux statistics */
    uint64_t flux_transitions;
    uint32_t min_flux_ns;
    uint32_t max_flux_ns;
    uint32_t avg_flux_ns;
    
    /* Quality metrics */
    uint32_t weak_bits_found;
    uint32_t dpm_anomalies;
    uint32_t crc_errors;
    
} uft_telemetry_t;

/* ========================================================================
 * API FUNCTIONS
 * ======================================================================== */

/**
 * Initialize logging system
 */
uft_rc_t uft_log_init(const uft_log_config_t* config);

/**
 * Shutdown logging
 */
void uft_log_shutdown(void);

/**
 * Log message (use macros instead!)
 */
void uft_log(
    uft_log_level_t level,
    const char* file,
    int line,
    const char* func,
    const char* fmt,
    ...
) __attribute__((format(printf, 5, 6)));

/**
 * Log structured data (JSON)
 */
void uft_log_json(
    uft_log_level_t level,
    const char* event,
    const char* json_data
);

/**
 * Telemetry API
 */
uft_telemetry_t* uft_telemetry_create(void);
void uft_telemetry_update(uft_telemetry_t* telem, const char* key, uint64_t value);
void uft_telemetry_log(const uft_telemetry_t* telem);
void uft_telemetry_destroy(uft_telemetry_t** telem);

/**
 * Performance timing
 */
#define UFT_TIME_START(var) uint64_t var = uft_get_time_us()
#define UFT_TIME_END(var) (uft_get_time_us() - (var))
#define UFT_TIME_LOG(var, ...) \
    UFT_LOG_INFO(__VA_ARGS__, UFT_TIME_END(var) / 1000.0)

uint64_t uft_get_time_us(void);

#ifdef __cplusplus
}
#endif

#endif /* UFT_LOGGING_H */
