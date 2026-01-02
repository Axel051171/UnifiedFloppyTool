#ifndef UFT_ERROR_H
#define UFT_ERROR_H

/**
 * @file uft_error.h
 * @brief Unified error handling for UnifiedFloppyTool
 * 
 * This header defines the standard error codes and handling mechanisms
 * used throughout UFT. All public APIs should return uft_rc_t.
 * 
 * @version 2.10.0
 * @date 2024-12-26
 */

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Standard UFT return codes
 * 
 * All public UFT functions should return one of these codes.
 * Success is 0, all errors are negative.
 */
typedef enum {
    /** Operation completed successfully */
    UFT_SUCCESS = 0,
    
    /* Argument errors (-1 to -9) */
    /** Invalid argument provided (NULL pointer, out of range, etc.) */
    UFT_ERR_INVALID_ARG = -1,
    /** Required buffer too small */
    UFT_ERR_BUFFER_TOO_SMALL = -2,
    /** Invalid path or filename */
    UFT_ERR_INVALID_PATH = -3,
    
    /* I/O errors (-10 to -19) */
    /** General I/O error */
    UFT_ERR_IO = -10,
    /** File not found */
    UFT_ERR_FILE_NOT_FOUND = -11,
    /** Permission denied */
    UFT_ERR_PERMISSION = -12,
    /** File already exists */
    UFT_ERR_FILE_EXISTS = -13,
    /** End of file reached */
    UFT_ERR_EOF = -14,
    
    /* Format errors (-20 to -29) */
    /** Unknown or invalid format */
    UFT_ERR_FORMAT = -20,
    /** Format detection failed */
    UFT_ERR_FORMAT_DETECT = -21,
    /** Unsupported format variant */
    UFT_ERR_FORMAT_VARIANT = -22,
    /** Corrupted or invalid data */
    UFT_ERR_CORRUPTED = -23,
    /** CRC/checksum mismatch */
    UFT_ERR_CRC = -24,
    
    /* Resource errors (-30 to -39) */
    /** Memory allocation failed */
    UFT_ERR_MEMORY = -30,
    /** Resource limit exceeded */
    UFT_ERR_RESOURCE = -31,
    /** Resource busy */
    UFT_ERR_BUSY = -32,
    
    /* Feature errors (-40 to -49) */
    /** Feature not supported */
    UFT_ERR_NOT_SUPPORTED = -40,
    /** Feature not implemented */
    UFT_ERR_NOT_IMPLEMENTED = -41,
    /** Operation not permitted in current state */
    UFT_ERR_NOT_PERMITTED = -42,
    
    /* Hardware errors (-50 to -59) */
    /** Hardware communication error */
    UFT_ERR_HARDWARE = -50,
    /** USB error */
    UFT_ERR_USB = -51,
    /** Device not found */
    UFT_ERR_DEVICE_NOT_FOUND = -52,
    /** Timeout */
    UFT_ERR_TIMEOUT = -53,
    
    /* Internal errors (-90 to -99) */
    /** Internal error (should not happen) */
    UFT_ERR_INTERNAL = -90,
    /** Assertion failed */
    UFT_ERR_ASSERTION = -91,
    
    /** Unknown error */
    UFT_ERR_UNKNOWN = -100
} uft_rc_t;

/**
 * @brief Extended error context
 * 
 * Provides additional information about errors beyond the return code.
 * Can be embedded in context structures or used standalone.
 */
typedef struct {
    /** Primary error code */
    uft_rc_t code;
    
    /** System errno if applicable (0 if not) */
    int sys_errno;
    
    /** File/line where error occurred (for debugging) */
    const char* file;
    int line;
    
    /** Human-readable error message (optional, can be NULL) */
    char message[256];
} uft_error_ctx_t;

/**
 * @brief Convert error code to string
 * 
 * @param rc Error code
 * @return Static string describing the error (never NULL)
 */
const char* uft_strerror(uft_rc_t rc);

/**
 * @brief Check if return code indicates success
 * 
 * @param rc Return code to check
 * @return true if success, false if error
 */
static inline bool uft_success(uft_rc_t rc) {
    return rc == UFT_SUCCESS;
}

/**
 * @brief Check if return code indicates error
 * 
 * @param rc Return code to check
 * @return true if error, false if success
 */
static inline bool uft_failed(uft_rc_t rc) {
    return rc != UFT_SUCCESS;
}

/**
 * @brief Macro to check for NULL arguments
 * 
 * Place at beginning of functions to validate pointers.
 * Returns UFT_ERR_INVALID_ARG if pointer is NULL.
 * 
 * Example:
 * @code
 * uft_rc_t function(ctx_t* ctx, data_t* data) {
 *     UFT_CHECK_NULL(ctx);
 *     UFT_CHECK_NULL(data);
 *     // ... rest of function
 * }
 * @endcode
 */
#define UFT_CHECK_NULL(ptr) \
    do { \
        if (!(ptr)) { \
            return UFT_ERR_INVALID_ARG; \
        } \
    } while(0)

/**
 * @brief Macro to check multiple NULL arguments
 * 
 * More efficient than multiple UFT_CHECK_NULL calls.
 * 
 * Example:
 * @code
 * UFT_CHECK_NULLS(ctx, data, output);
 * @endcode
 */
#define UFT_CHECK_NULLS(...) \
    do { \
        void* ptrs[] = { __VA_ARGS__ }; \
        for (size_t i = 0; i < sizeof(ptrs)/sizeof(ptrs[0]); i++) { \
            if (!ptrs[i]) return UFT_ERR_INVALID_ARG; \
        } \
    } while(0)

/**
 * @brief Macro to propagate errors
 * 
 * If expression returns error, propagate it immediately.
 * 
 * Example:
 * @code
 * UFT_PROPAGATE(validate_input(data));
 * UFT_PROPAGATE(process_data(data));
 * @endcode
 */
#define UFT_PROPAGATE(expr) \
    do { \
        uft_rc_t _rc = (expr); \
        if (uft_failed(_rc)) return _rc; \
    } while(0)

/**
 * @brief Macro to set error context with file/line info
 * 
 * Example:
 * @code
 * UFT_SET_ERROR(ctx->error, UFT_ERR_CORRUPTED, "Invalid sector count");
 * @endcode
 */
#define UFT_SET_ERROR(err_ctx, err_code, msg, ...) \
    do { \
        (err_ctx).code = (err_code); \
        (err_ctx).file = __FILE__; \
        (err_ctx).line = __LINE__; \
        snprintf((err_ctx).message, sizeof((err_ctx).message), (msg), ##__VA_ARGS__); \
    } while(0)

#ifdef __cplusplus
}
#endif

#endif /* UFT_ERROR_H */
