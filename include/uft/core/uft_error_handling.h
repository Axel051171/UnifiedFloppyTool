/**
 * @file uft_error_handling.h
 * @brief Logging and error handling utilities
 */

#ifndef UFT_CORE_ERROR_HANDLING_H
#define UFT_CORE_ERROR_HANDLING_H

#include <stdio.h>
#include "uft/uft_error.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Log levels
 */
typedef enum {
    UFT_LOG_DEBUG = 0,
    UFT_LOG_INFO  = 1,
    UFT_LOG_WARN  = 2,
    UFT_LOG_ERROR = 3,
    UFT_LOG_FATAL = 4
} uft_log_level_t;

/**
 * @brief Log callback function type
 */
typedef void (*uft_log_callback_t)(uft_log_level_t level, 
                                    const char* file, int line,
                                    const char* func, const char* message);

/**
 * @brief Global log level (extern)
 */
extern uft_log_level_t g_uft_log_level;

/* Undefine macro from uft_error.h - we define our own function */
#ifdef uft_error_string
#undef uft_error_string
#endif

/**
 * @brief Set custom log callback
 */
void uft_set_log_callback(uft_log_callback_t callback);

/**
 * @brief Set minimum log level
 */
void uft_set_log_level(uft_log_level_t level);

/**
 * @brief Internal logging function (use macros instead)
 */
void uft_log_internal(uft_log_level_t level, const char* file, int line,
                       const char* func, const char* fmt, ...);

/**
 * @brief Get error string
 */

/**
 * @brief Get error details with user message and suggestion
 */
/**
 * @brief Get error name string
 */
const char* uft_error_string(uft_error_t err);
const char* uft_error_details(uft_error_t err,
                               const char** user_msg,
                               const char** suggestion);

/* Logging macros */
#define UFT_LOG_DEBUG(...) \
    uft_log_internal(UFT_LOG_DEBUG, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define UFT_LOG_INFO(...) \
    uft_log_internal(UFT_LOG_INFO, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define UFT_LOG_WARN(...) \
    uft_log_internal(UFT_LOG_WARN, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define UFT_LOG_ERROR(...) \
    uft_log_internal(UFT_LOG_ERROR, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define UFT_LOG_FATAL(...) \
    uft_log_internal(UFT_LOG_FATAL, __FILE__, __LINE__, __func__, __VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif /* UFT_CORE_ERROR_HANDLING_H */
