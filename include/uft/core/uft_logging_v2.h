/**
 * @file uft_logging_v2.h
 * @brief Enhanced Logging System with Category Masks
 * 
 * Based on DTC's -l parameter - provides bitmask-based
 * log level control for fine-grained output filtering.
 */

#ifndef UFT_LOGGING_V2_H
#define UFT_LOGGING_V2_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Log Categories (DTC-compatible bitmask)
 * ============================================================================ */

/**
 * @brief Log category mask values
 * 
 * Add values together to define which categories to log:
 * - 62 = default (read + cell + format + write + verify)
 * - 63 = all except debug
 * - 127 = all including debug
 */
typedef enum {
    UFT_LOG_DEVICE = 0x01,    /**< Device/hardware communication */
    UFT_LOG_READ   = 0x02,    /**< Read operations */
    UFT_LOG_CELL   = 0x04,    /**< Cell-level analysis */
    UFT_LOG_FORMAT = 0x08,    /**< Format detection/parsing */
    UFT_LOG_WRITE  = 0x10,    /**< Write operations */
    UFT_LOG_VERIFY = 0x20,    /**< Verify operations */
    UFT_LOG_DEBUG  = 0x40,    /**< Debug information */
    UFT_LOG_TRACE  = 0x80,    /**< Detailed trace (very verbose) */
    
    /* Convenience combinations */
    UFT_LOG_NONE    = 0x00,
    UFT_LOG_DEFAULT = 0x3E,   /**< read + cell + format + write + verify */
    UFT_LOG_ALL     = 0x7F,   /**< All except trace */
    UFT_LOG_VERBOSE = 0xFF    /**< Everything */
} uft_log_mask_t;

/**
 * @brief Log severity levels
 */
typedef enum {
    UFT_LOG_LEVEL_ERROR = 0,   /**< Errors only */
    UFT_LOG_LEVEL_WARNING,     /**< Warnings and errors */
    UFT_LOG_LEVEL_INFO,        /**< Informational messages */
    UFT_LOG_LEVEL_DEBUG,       /**< Debug messages */
    UFT_LOG_LEVEL_TRACE        /**< Trace-level detail */
} uft_log_level_t;

/* ============================================================================
 * Log Entry Structure
 * ============================================================================ */

/**
 * @brief Single log entry
 */
typedef struct {
    uint64_t timestamp_us;     /**< Microseconds since start */
    uft_log_mask_t category;   /**< Category mask */
    uft_log_level_t level;     /**< Severity level */
    const char *source_file;   /**< Source file name */
    int source_line;           /**< Source line number */
    const char *function;      /**< Function name */
    char message[1024];        /**< Log message */
} uft_log_entry_t;

/**
 * @brief Log callback function type
 */
typedef void (*uft_log_callback_t)(const uft_log_entry_t *entry, void *user_data);

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * @brief Logging configuration
 */
typedef struct {
    uint32_t category_mask;    /**< Active category mask */
    uft_log_level_t min_level; /**< Minimum level to log */
    bool log_to_stdout;        /**< Output to stdout */
    bool log_to_stderr;        /**< Output errors to stderr */
    bool log_to_file;          /**< Output to file */
    const char *log_file_path; /**< Log file path */
    bool include_timestamp;    /**< Include timestamp in output */
    bool include_source;       /**< Include source location */
    bool color_output;         /**< Use ANSI colors (terminal) */
    uft_log_callback_t callback; /**< Custom callback */
    void *callback_user_data;  /**< User data for callback */
} uft_log_config_t;

/* ============================================================================
 * Global Configuration Functions
 * ============================================================================ */

/**
 * @brief Initialize logging with default settings
 */
void uft_log_init(void);

/**
 * @brief Shutdown logging, flush buffers
 */
void uft_log_shutdown(void);

/**
 * @brief Set logging configuration
 */
void uft_log_set_config(const uft_log_config_t *config);

/**
 * @brief Get current configuration
 */
const uft_log_config_t* uft_log_get_config(void);

/**
 * @brief Set category mask (like DTC -l parameter)
 */
void uft_log_set_mask(uint32_t mask);

/**
 * @brief Get current category mask
 */
uint32_t uft_log_get_mask(void);

/**
 * @brief Set minimum log level
 */
void uft_log_set_level(uft_log_level_t level);

/**
 * @brief Enable/disable specific category
 */
void uft_log_enable_category(uft_log_mask_t category, bool enable);

/**
 * @brief Check if category is enabled
 */
bool uft_log_is_enabled(uft_log_mask_t category);

/**
 * @brief Set log file path
 */
void uft_log_set_file(const char *path);

/**
 * @brief Set custom callback
 */
void uft_log_set_callback(uft_log_callback_t callback, void *user_data);

/* ============================================================================
 * Logging Functions
 * ============================================================================ */

/**
 * @brief Log message with category
 */
void uft_log(uft_log_mask_t category, uft_log_level_t level,
             const char *file, int line, const char *func,
             const char *fmt, ...);

/**
 * @brief Log with va_list
 */
void uft_log_v(uft_log_mask_t category, uft_log_level_t level,
               const char *file, int line, const char *func,
               const char *fmt, va_list args);

/* ============================================================================
 * Convenience Macros
 * ============================================================================ */

#define UFT_LOG(cat, level, ...) \
    uft_log(cat, level, __FILE__, __LINE__, __func__, __VA_ARGS__)

/* Category-specific macros */
#define UFT_LOG_DEV(...)    UFT_LOG(UFT_LOG_DEVICE, UFT_LOG_LEVEL_INFO, __VA_ARGS__)
#define UFT_LOG_READ(...)   UFT_LOG(UFT_LOG_READ, UFT_LOG_LEVEL_INFO, __VA_ARGS__)
#define UFT_LOG_CELL(...)   UFT_LOG(UFT_LOG_CELL, UFT_LOG_LEVEL_INFO, __VA_ARGS__)
#define UFT_LOG_FMT(...)    UFT_LOG(UFT_LOG_FORMAT, UFT_LOG_LEVEL_INFO, __VA_ARGS__)
#define UFT_LOG_WRITE(...)  UFT_LOG(UFT_LOG_WRITE, UFT_LOG_LEVEL_INFO, __VA_ARGS__)
#define UFT_LOG_VFY(...)    UFT_LOG(UFT_LOG_VERIFY, UFT_LOG_LEVEL_INFO, __VA_ARGS__)
#define UFT_LOG_DBG(...)    UFT_LOG(UFT_LOG_DEBUG, UFT_LOG_LEVEL_DEBUG, __VA_ARGS__)
#define UFT_LOG_TRC(...)    UFT_LOG(UFT_LOG_TRACE, UFT_LOG_LEVEL_TRACE, __VA_ARGS__)

/* Level-specific macros */
#define UFT_ERROR(cat, ...)   UFT_LOG(cat, UFT_LOG_LEVEL_ERROR, __VA_ARGS__)
#define UFT_WARN(cat, ...)    UFT_LOG(cat, UFT_LOG_LEVEL_WARNING, __VA_ARGS__)
#define UFT_INFO(cat, ...)    UFT_LOG(cat, UFT_LOG_LEVEL_INFO, __VA_ARGS__)
#define UFT_DEBUG(cat, ...)   UFT_LOG(cat, UFT_LOG_LEVEL_DEBUG, __VA_ARGS__)
#define UFT_TRACE(cat, ...)   UFT_LOG(cat, UFT_LOG_LEVEL_TRACE, __VA_ARGS__)

/* ============================================================================
 * Log Buffer Access (for GUI/Export)
 * ============================================================================ */

/**
 * @brief Get recent log entries
 * 
 * @param entries      Output array
 * @param max_entries  Maximum entries to retrieve
 * @param category     Filter by category (0 = all)
 * @return Number of entries returned
 */
size_t uft_log_get_recent(uft_log_entry_t *entries, size_t max_entries,
                          uint32_t category_filter);

/**
 * @brief Clear log buffer
 */
void uft_log_clear_buffer(void);

/**
 * @brief Export log to JSON file
 */
int uft_log_export_json(const char *path);

/**
 * @brief Export log to HTML file
 */
int uft_log_export_html(const char *path);

/* ============================================================================
 * Statistics
 * ============================================================================ */

/**
 * @brief Log statistics
 */
typedef struct {
    uint64_t total_messages;
    uint64_t error_count;
    uint64_t warning_count;
    uint64_t by_category[8];  /**< Count per category */
} uft_log_stats_t;

/**
 * @brief Get log statistics
 */
void uft_log_get_stats(uft_log_stats_t *stats);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get category name
 */
const char* uft_log_category_name(uft_log_mask_t category);

/**
 * @brief Get level name
 */
const char* uft_log_level_name(uft_log_level_t level);

/**
 * @brief Parse mask from string (e.g., "62" or "read,format,write")
 */
uint32_t uft_log_parse_mask(const char *str);

/**
 * @brief Format mask as string
 */
const char* uft_log_mask_to_string(uint32_t mask, char *buffer, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* UFT_LOGGING_V2_H */
