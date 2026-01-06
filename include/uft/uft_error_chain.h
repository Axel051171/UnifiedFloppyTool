/**
 * @file uft_error_chain.h
 * @brief Structured Error Chain and Context API
 * 
 * TICKET-010: Error Chain / Context
 * Structured error handling with context stack and debugging info
 * 
 * @version 5.1.0
 * @date 2026-01-03
 */

#ifndef UFT_ERROR_CHAIN_H
#define UFT_ERROR_CHAIN_H

#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include "uft_types.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Error Severity and Categories
 * ═══════════════════════════════════════════════════════════════════════════════ */

/** Error severity levels */
typedef enum {
    UFT_SEVERITY_DEBUG,     /**< Debug information */
    UFT_SEVERITY_INFO,      /**< Informational */
    UFT_SEVERITY_WARNING,   /**< Warning - operation continues */
    UFT_SEVERITY_ERROR,     /**< Error - operation failed */
    UFT_SEVERITY_FATAL,     /**< Fatal - cannot continue */
} uft_severity_t;

/** Error categories */
typedef enum {
    UFT_ECAT_NONE = 0,
    UFT_ECAT_IO,            /**< I/O errors (file, device) */
    UFT_ECAT_MEMORY,        /**< Memory allocation */
    UFT_ECAT_FORMAT,        /**< Format parsing errors */
    UFT_ECAT_HARDWARE,      /**< Hardware communication */
    UFT_ECAT_PARAM,         /**< Invalid parameters */
    UFT_ECAT_STATE,         /**< Invalid state */
    UFT_ECAT_TIMEOUT,       /**< Operation timeout */
    UFT_ECAT_PROTOCOL,      /**< Protocol errors */
    UFT_ECAT_CRC,           /**< CRC/checksum errors */
    UFT_ECAT_ENCODING,      /**< Encoding errors */
    UFT_ECAT_SYSTEM,        /**< System/OS errors */
    UFT_ECAT_USER,          /**< User-caused errors */
    UFT_ECAT_INTERNAL,      /**< Internal/logic errors */
} uft_error_category_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Error Info Structures
 * ═══════════════════════════════════════════════════════════════════════════════ */

/** Source location information */
typedef struct {
    const char      *file;          /**< Source file name */
    const char      *function;      /**< Function name */
    int             line;           /**< Line number */
} uft_source_loc_t;

/** Single error entry in chain */
typedef struct uft_error_entry {
    uft_error_t             code;           /**< Error code */
    uft_severity_t          severity;       /**< Severity level */
    uft_error_category_t    category;       /**< Error category */
    
    char                    *message;       /**< Error message */
    char                    *detail;        /**< Detailed description */
    char                    *suggestion;    /**< Suggested fix */
    
    uft_source_loc_t        location;       /**< Source location */
    uint64_t                timestamp;      /**< When error occurred (ms) */
    
    /* Context data */
    int                     context_int;    /**< Integer context */
    const char              *context_str;   /**< String context */
    void                    *context_ptr;   /**< Pointer context */
    
    struct uft_error_entry  *cause;         /**< Underlying cause */
    struct uft_error_entry  *next;          /**< Next in chain */
} uft_error_entry_t;

/** Error context (thread-local error state) */
typedef struct {
    uft_error_entry_t   *chain;         /**< Error chain (most recent first) */
    uft_error_entry_t   *last;          /**< Last error */
    int                 count;          /**< Number of errors */
    
    /* Context stack */
    const char          *context_stack[16]; /**< Operation context stack */
    int                 context_depth;      /**< Current stack depth */
    
    /* Configuration */
    uft_severity_t      min_severity;   /**< Minimum severity to capture */
    int                 max_entries;    /**< Maximum entries to keep */
    bool                capture_trace;  /**< Capture stack traces */
} uft_error_context_t;

/** Error callback for notifications */
typedef void (*uft_error_callback_t)(const uft_error_entry_t *error, void *user_data);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Context Management
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Initialize error context
 * @return New error context
 */
uft_error_context_t *uft_error_context_create(void);

/**
 * @brief Destroy error context
 * @param ctx Context to destroy
 */
void uft_error_context_destroy(uft_error_context_t *ctx);

/**
 * @brief Get thread-local error context
 * @return Current thread's error context
 */
uft_error_context_t *uft_error_context_get(void);

/**
 * @brief Set thread-local error context
 * @param ctx Context to use (NULL to use default)
 */
void uft_error_context_set(uft_error_context_t *ctx);

/**
 * @brief Clear all errors in context
 * @param ctx Context (NULL for thread-local)
 */
void uft_error_clear(uft_error_context_t *ctx);

/**
 * @brief Configure error context
 * @param ctx Context (NULL for thread-local)
 * @param min_severity Minimum severity to capture
 * @param max_entries Maximum entries to keep
 * @param capture_trace Whether to capture stack traces
 */
void uft_error_configure(uft_error_context_t *ctx, uft_severity_t min_severity,
                          int max_entries, bool capture_trace);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Context Stack Operations
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Push operation context onto stack
 * @param ctx Context (NULL for thread-local)
 * @param operation Operation description
 */
void uft_error_push_context(uft_error_context_t *ctx, const char *operation);

/**
 * @brief Pop operation context from stack
 * @param ctx Context (NULL for thread-local)
 */
void uft_error_pop_context(uft_error_context_t *ctx);

/**
 * @brief Get current context string
 * @param ctx Context (NULL for thread-local)
 * @return Current context description (do not free)
 */
const char *uft_error_current_context(uft_error_context_t *ctx);

/**
 * @brief Get full context path
 * @param ctx Context (NULL for thread-local)
 * @param separator Separator between contexts
 * @return Allocated string (caller must free)
 */
char *uft_error_context_path(uft_error_context_t *ctx, const char *separator);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Error Reporting Macros
 * ═══════════════════════════════════════════════════════════════════════════════ */

/** Report error with source location */
#define UFT_ERROR(code, msg) \
    uft_error_report_loc(NULL, (code), UFT_SEVERITY_ERROR, \
                         __FILE__, __func__, __LINE__, (msg))

/** Report error with format string */
#define UFT_ERRORF(code, fmt, ...) \
    uft_error_reportf_loc(NULL, (code), UFT_SEVERITY_ERROR, \
                          __FILE__, __func__, __LINE__, (fmt), ##__VA_ARGS__)

/** Report warning */
#define UFT_WARN(msg) \
    uft_error_report_loc(NULL, UFT_OK, UFT_SEVERITY_WARNING, \
                         __FILE__, __func__, __LINE__, (msg))

/** Report warning with format */
#define UFT_WARNF(fmt, ...) \
    uft_error_reportf_loc(NULL, UFT_OK, UFT_SEVERITY_WARNING, \
                          __FILE__, __func__, __LINE__, (fmt), ##__VA_ARGS__)

/** Report info */
#define UFT_INFO(msg) \
    uft_error_report_loc(NULL, UFT_OK, UFT_SEVERITY_INFO, \
                         __FILE__, __func__, __LINE__, (msg))

/** Report debug */
#define UFT_DEBUG(msg) \
    uft_error_report_loc(NULL, UFT_OK, UFT_SEVERITY_DEBUG, \
                         __FILE__, __func__, __LINE__, (msg))

/** Push context with auto-pop on scope exit (C11 or GCC/Clang) */
#ifdef __GNUC__
#define UFT_CONTEXT(op) \
    __attribute__((cleanup(uft_error_pop_context_cleanup))) \
    const char *_uft_ctx_##__LINE__ = (uft_error_push_context(NULL, op), op)
#else
#define UFT_CONTEXT(op) uft_error_push_context(NULL, op)
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Error Reporting Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Report an error
 * @param ctx Context (NULL for thread-local)
 * @param code Error code
 * @param severity Severity level
 * @param message Error message
 * @return The error code (for chaining)
 */
uft_error_t uft_error_report(uft_error_context_t *ctx, uft_error_t code,
                              uft_severity_t severity, const char *message);

/**
 * @brief Report error with source location
 */
uft_error_t uft_error_report_loc(uft_error_context_t *ctx, uft_error_t code,
                                  uft_severity_t severity,
                                  const char *file, const char *func, int line,
                                  const char *message);

/**
 * @brief Report error with format string
 */
uft_error_t uft_error_reportf(uft_error_context_t *ctx, uft_error_t code,
                               uft_severity_t severity, const char *fmt, ...);

/**
 * @brief Report error with format string and location
 */
uft_error_t uft_error_reportf_loc(uft_error_context_t *ctx, uft_error_t code,
                                   uft_severity_t severity,
                                   const char *file, const char *func, int line,
                                   const char *fmt, ...);

/**
 * @brief Report error with full details
 * @param ctx Context (NULL for thread-local)
 * @param code Error code
 * @param severity Severity level
 * @param category Error category
 * @param message Error message
 * @param detail Detailed description
 * @param suggestion Suggested fix
 * @param cause Underlying cause (can be NULL)
 * @return The error code
 */
uft_error_t uft_error_report_full(uft_error_context_t *ctx, uft_error_t code,
                                   uft_severity_t severity, uft_error_category_t category,
                                   const char *message, const char *detail,
                                   const char *suggestion, uft_error_entry_t *cause);

/**
 * @brief Wrap an existing error with additional context
 * @param ctx Context (NULL for thread-local)
 * @param code New error code
 * @param message Additional message
 * @return The error code
 */
uft_error_t uft_error_wrap(uft_error_context_t *ctx, uft_error_t code,
                            const char *message);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Error Query Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Check if errors exist
 * @param ctx Context (NULL for thread-local)
 * @return true if errors present
 */
bool uft_error_has_errors(uft_error_context_t *ctx);

/**
 * @brief Get error count
 * @param ctx Context (NULL for thread-local)
 * @return Number of errors
 */
int uft_error_count(uft_error_context_t *ctx);

/**
 * @brief Get last error
 * @param ctx Context (NULL for thread-local)
 * @return Last error entry (NULL if none)
 */
const uft_error_entry_t *uft_error_last(uft_error_context_t *ctx);

/**
 * @brief Get error chain
 * @param ctx Context (NULL for thread-local)
 * @return First error in chain
 */
const uft_error_entry_t *uft_error_chain(uft_error_context_t *ctx);

/**
 * @brief Get last error code
 * @param ctx Context (NULL for thread-local)
 * @return Error code (UFT_OK if no errors)
 */
uft_error_t uft_error_code(uft_error_context_t *ctx);

/**
 * @brief Get last error message
 * @param ctx Context (NULL for thread-local)
 * @return Error message (empty string if no errors)
 */
const char *uft_error_message(uft_error_context_t *ctx);

/**
 * @brief Find errors by category
 * @param ctx Context (NULL for thread-local)
 * @param category Category to find
 * @return First matching error (NULL if not found)
 */
const uft_error_entry_t *uft_error_find_category(uft_error_context_t *ctx,
                                                   uft_error_category_t category);

/**
 * @brief Find errors by severity
 * @param ctx Context (NULL for thread-local)
 * @param min_severity Minimum severity
 * @return First matching error (NULL if not found)
 */
const uft_error_entry_t *uft_error_find_severity(uft_error_context_t *ctx,
                                                   uft_severity_t min_severity);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Error Callbacks
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Set error callback
 * @param ctx Context (NULL for thread-local)
 * @param callback Callback function
 * @param user_data User data passed to callback
 */
void uft_error_set_callback(uft_error_context_t *ctx, uft_error_callback_t callback,
                             void *user_data);

/**
 * @brief Remove error callback
 * @param ctx Context (NULL for thread-local)
 */
void uft_error_remove_callback(uft_error_context_t *ctx);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Error Output
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Print error chain to stdout
 * @param ctx Context (NULL for thread-local)
 */
void uft_error_print(uft_error_context_t *ctx);

/**
 * @brief Print error chain with full details
 * @param ctx Context (NULL for thread-local)
 */
void uft_error_print_full(uft_error_context_t *ctx);

/**
 * @brief Format error as string
 * @param entry Error entry
 * @return Formatted string (caller must free)
 */
char *uft_error_format(const uft_error_entry_t *entry);

/**
 * @brief Format full error chain as string
 * @param ctx Context (NULL for thread-local)
 * @return Formatted string (caller must free)
 */
char *uft_error_format_chain(uft_error_context_t *ctx);

/**
 * @brief Export errors as JSON
 * @param ctx Context (NULL for thread-local)
 * @param pretty Pretty-print output
 * @return JSON string (caller must free)
 */
char *uft_error_to_json(uft_error_context_t *ctx, bool pretty);

/**
 * @brief Save error log to file
 * @param ctx Context (NULL for thread-local)
 * @param path File path
 * @return UFT_OK on success
 */
uft_error_t uft_error_save_log(uft_error_context_t *ctx, const char *path);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Utility Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

/** Get severity name */
const char *uft_severity_name(uft_severity_t severity);

/** Get category name */
const char *uft_error_category_name(uft_error_category_t category);

/** Get error code name */
const char *uft_error_code_name(uft_error_t code);

/** Get error description */
const char *uft_error_description(uft_error_t code);

/** Classify error by code */
uft_error_category_t uft_error_classify(uft_error_t code);

/** Map system errno to UFT error */
uft_error_t uft_error_from_errno(int errno_val);

/** Map system errno to UFT error with message */
uft_error_t uft_error_from_errno_msg(int errno_val, char *msg_out, size_t msg_size);

/** Helper for cleanup attribute */
void uft_error_pop_context_cleanup(const char **dummy);

#ifdef __cplusplus
}
#endif

#endif /* UFT_ERROR_CHAIN_H */
