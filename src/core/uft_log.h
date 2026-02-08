#ifndef UFT_LOG_H
#define UFT_LOG_H
/**
 * @file uft_log.h
 * @brief Unified Logging System for UFT
 */

#include <stdio.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum uft_log_level {
    UFT_LOG_TRACE = 0,
    UFT_LOG_DEBUG = 1,
    UFT_LOG_INFO  = 2,
    UFT_LOG_WARN  = 3,
    UFT_LOG_ERROR = 4,
    UFT_LOG_FATAL = 5,
    UFT_LOG_OFF   = 6
} uft_log_level_t;

/* Global log level (default: INFO) */
extern uft_log_level_t uft_log_level;

/* Log output file (default: stderr) */
extern FILE* uft_log_file;

/* Core logging function */
void uft_log(uft_log_level_t level, const char* file, int line, 
             const char* func, const char* fmt, ...);

/* Convenience macros */
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

/* Configuration */
void uft_log_set_level(uft_log_level_t level);
void uft_log_set_file(FILE* file);
void uft_log_set_callback(void (*cb)(uft_log_level_t, const char*));

#ifdef __cplusplus
}
#endif

#endif /* UFT_LOG_H */
