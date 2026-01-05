/**
 * @file uft_error_handling.h
 * @brief Professional Error Handling Framework
 * 
 * FORENSIC-GRADE ERROR HANDLING
 * 
 * Features:
 * - Error context (file, line, function)
 * - Error chaining (stack traces)
 * - Detailed error messages
 * - Memory-safe cleanup
 * - Thread-safe
 * 
 * @version 3.0.0 (Professional Edition)
 * @date 2024-12-27
 */

#ifndef UFT_ERROR_HANDLING_H
#define UFT_ERROR_HANDLING_H

#include "uft/uft_error.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * ERROR CONTEXT - Track where errors occur
 * ======================================================================== */

#define UFT_MAX_ERROR_MSG 512
#define UFT_MAX_ERROR_STACK 16

typedef struct {
    uft_rc_t code;
    const char* file;
    int line;
    const char* function;
    char message[UFT_MAX_ERROR_MSG];
    
    /* Error chaining (stack trace) */
    uft_rc_t cause_code;
    char cause_message[UFT_MAX_ERROR_MSG];
    
} uft_error_context_t;

/* Thread-local error context */
extern __thread uft_error_context_t g_error_ctx;

/* ========================================================================
 * ERROR MACROS - Use these instead of raw returns!
 * ======================================================================== */

/**
 * Set error with context
 */
#define UFT_SET_ERROR(code, ...) \
    do { \
        g_error_ctx.code = (code); \
        g_error_ctx.file = __FILE__; \
        g_error_ctx.line = __LINE__; \
        g_error_ctx.function = __func__; \
        snprintf(g_error_ctx.message, sizeof(g_error_ctx.message), __VA_ARGS__); \
    } while(0)

/**
 * Return with error context
 */
#define UFT_RETURN_ERROR(code, ...) \
    do { \
        UFT_SET_ERROR(code, __VA_ARGS__); \
        return (code); \
    } while(0)

/**
 * Chain errors (preserve cause)
 */
#define UFT_CHAIN_ERROR(code, cause, ...) \
    do { \
        g_error_ctx.cause_code = (cause); \
        snprintf(g_error_ctx.cause_message, sizeof(g_error_ctx.cause_message), \
                "%s", g_error_ctx.message); \
        UFT_SET_ERROR(code, __VA_ARGS__); \
    } while(0)

/**
 * Check and propagate errors
 */
#define UFT_CHECK_ERROR(expr, ...) \
    do { \
        uft_rc_t _rc = (expr); \
        if (uft_failed(_rc)) { \
            UFT_CHAIN_ERROR(UFT_ERR_INTERNAL, _rc, __VA_ARGS__); \
            return _rc; \
        } \
    } while(0)

/* ========================================================================
 * RESOURCE MANAGEMENT - RAII-style cleanup
 * ======================================================================== */

/**
 * Cleanup function pointer
 */
typedef void (*uft_cleanup_fn_t)(void*);

/**
 * Auto-cleanup wrapper
 * 
 * Usage:
 *   uft_auto_cleanup(file, cleanup_file) FILE* fp = fopen(...);
 *   // fp automatically closed on scope exit
 */
#define uft_auto_cleanup(var, cleanup) \
    __attribute__((cleanup(cleanup))) var

/**
 * Common cleanup functions
 */
static inline void cleanup_free(void* p) {
    void** ptr = (void**)p;
    if (*ptr) {
        free(*ptr);
        *ptr = NULL;
    }
}

static inline void cleanup_fclose(void* p) {
    FILE** fp = (FILE**)p;
    if (*fp) {
        fclose(*fp);
        *fp = NULL;
    }
}

/* ========================================================================
 * ERROR API
 * ======================================================================== */

/**
 * Get last error context
 */
const uft_error_context_t* uft_get_last_error(void);

/**
 * Get detailed error message
 */
const char* uft_get_error_message(void);

/**
 * Print error stack trace
 */
void uft_print_error_stack(FILE* fp);

/**
 * Clear error context
 */
void uft_clear_error(void);

#ifdef __cplusplus
}
#endif

#endif /* UFT_ERROR_HANDLING_H */
