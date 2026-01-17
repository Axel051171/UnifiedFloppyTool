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

#include <stdio.h>

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

/** @brief Alias for compatibility - mark as defined to avoid redefinition */
#ifndef UFT_ERROR_T_DEFINED
#define UFT_ERROR_T_DEFINED
typedef uft_rc_t uft_error_t;
#endif

/** @brief Alias for NULL pointer error */
#define UFT_ERROR_NULL_POINTER UFT_ERR_INVALID_ARG

/** @brief Alias for invalid parameter (same as INVALID_ARG) */
#define UFT_ERR_INVALID_PARAM UFT_ERR_INVALID_ARG

/** @brief Standard OK return */
#define UFT_OK UFT_SUCCESS

/* Legacy error code aliases */
#define UFT_ERROR_NO_MEMORY      UFT_ERR_MEMORY
#define UFT_ERR_NOMEM            UFT_ERR_MEMORY  /* CI check compatibility */
#define UFT_ERROR_NOT_SUPPORTED  UFT_ERR_NOT_SUPPORTED
#define UFT_ERROR_FILE_OPEN      UFT_ERR_IO
#define UFT_ERROR_DISK_PROTECTED UFT_ERR_NOT_PERMITTED
#define UFT_ERROR_TRACK_NOT_FOUND UFT_ERR_FORMAT
#define UFT_ERROR_SECTOR_NOT_FOUND UFT_ERR_FORMAT
#define UFT_ERROR_CRC_ERROR      UFT_ERR_CRC
#define UFT_ERROR_INVALID_ARG    UFT_ERR_INVALID_ARG
#define UFT_ERROR_CANCELLED      UFT_ERR_TIMEOUT
#define UFT_ERROR_FILE_WRITE     UFT_ERR_IO
#define UFT_ERROR_FILE_READ      UFT_ERR_IO
#define UFT_ERROR_IO             UFT_ERR_IO
#define UFT_ERROR_TOOL_FAILED    UFT_ERR_INTERNAL
#define UFT_ERROR               UFT_ERR_INTERNAL
#define UFT_ERROR_UNKNOWN_ENCODING UFT_ERR_FORMAT
#define UFT_ERROR_PLUGIN_LOAD    UFT_ERR_INTERNAL
#define UFT_ERROR_PLUGIN_NOT_FOUND UFT_ERR_NOT_SUPPORTED
#define UFT_ERROR_BUFFER_TOO_SMALL UFT_ERR_BUFFER_TOO_SMALL
#define UFT_ERROR_NOT_IMPLEMENTED UFT_ERR_NOT_IMPLEMENTED
#define UFT_ERROR_OUT_OF_RANGE   UFT_ERR_INVALID_ARG
#define UFT_ERROR_TIMEOUT        UFT_ERR_TIMEOUT
#define UFT_ERROR_ALLOC_FAILED   UFT_ERR_MEMORY
#define UFT_ERROR_FILE_NOT_FOUND UFT_ERR_FILE_NOT_FOUND
#define UFT_ERROR_FILE_EXISTS    UFT_ERR_FILE_EXISTS
#define UFT_ERROR_FILE_SEEK      UFT_ERR_IO
#define UFT_ERROR_FILE_CORRUPT   UFT_ERR_CORRUPTED
#define UFT_ERROR_FILE_TOO_LARGE UFT_ERR_IO
#define UFT_ERROR_FILE_PERMISSION UFT_ERR_PERMISSION
#define UFT_ERROR_FORMAT_UNKNOWN UFT_ERR_FORMAT
#define UFT_ERROR_FORMAT_INVALID UFT_ERR_FORMAT
#define UFT_ERROR_FORMAT_MISMATCH UFT_ERR_FORMAT_VARIANT
#define UFT_ERROR_DISK_READ      UFT_ERR_IO
#define UFT_ERROR_DISK_WRITE     UFT_ERR_IO
#define UFT_ERROR_DISK_NOTREADY  UFT_ERR_TIMEOUT
#define UFT_ERROR_TRACK_READ     UFT_ERR_IO
#define UFT_ERROR_TRACK_WRITE    UFT_ERR_IO
#define UFT_ERROR_HARDWARE       UFT_ERR_HARDWARE
#define UFT_ERROR_USB            UFT_ERR_USB
#define UFT_ERROR_BAD_CHECKSUM   UFT_ERR_CRC
#define UFT_ERROR_BAD_HEADER     UFT_ERR_FORMAT
#define UFT_ERROR_BAD_MAGIC      UFT_ERR_FORMAT
#define UFT_ERROR_DATA_CRC_ERROR UFT_ERR_CRC
#define UFT_ERROR_DECODE_FAILED  UFT_ERR_FORMAT
#define UFT_ERROR_DEVICE_BUSY    UFT_ERR_TIMEOUT
#define UFT_ERROR_DEVICE_ERROR   UFT_ERR_HARDWARE
#define UFT_ERROR_DEVICE_NOT_FOUND UFT_ERR_DEVICE_NOT_FOUND
#define UFT_ERROR_DEVICE_OFFLINE UFT_ERR_HARDWARE
#define UFT_ERROR_DISK_CHANGED   UFT_ERR_IO
#define UFT_ERROR_DISK_FULL      UFT_ERR_IO
#define UFT_ERROR_DISK_NOT_READY UFT_ERR_TIMEOUT
#define UFT_ERROR_DISK_REMOVED   UFT_ERR_IO
#define UFT_ERROR_DRIVE_BUSY     UFT_ERR_TIMEOUT
#define UFT_ERROR_DRIVE_NOT_FOUND UFT_ERR_DEVICE_NOT_FOUND
#define UFT_ERROR_ENCODE_FAILED  UFT_ERR_FORMAT
#define UFT_ERROR_FORMAT_CONVERT UFT_ERR_FORMAT
#define UFT_ERROR_FORMAT_VERSION UFT_ERR_FORMAT_VARIANT
#define UFT_ERROR_FUZZY_BITS     UFT_ERR_CRC
#define UFT_ERROR_ID_CRC_ERROR   UFT_ERR_CRC
#define UFT_ERROR_MOTOR_ERROR    UFT_ERR_HARDWARE
#define UFT_ERROR_NO_DISK        UFT_ERR_IO
#define UFT_ERROR_NO_INDEX       UFT_ERR_FORMAT
#define UFT_ERROR_NO_SYNC        UFT_ERR_FORMAT
#define UFT_ERROR_PLL_FAILED     UFT_ERR_FORMAT
#define UFT_ERROR_PLUGIN_INIT    UFT_ERR_INTERNAL
#define UFT_ERROR_PLUGIN_VERSION UFT_ERR_INTERNAL
#define UFT_ERROR_READ_PROTECTED UFT_ERR_NOT_PERMITTED
#define UFT_ERROR_SEEK_ERROR     UFT_ERR_IO
#define UFT_ERROR_STACK_OVERFLOW UFT_ERR_MEMORY
#define UFT_ERROR_UNFORMATTED    UFT_ERR_FORMAT
#define UFT_ERROR_UNKNOWN        UFT_ERR_INTERNAL
#define UFT_ERROR_USB_ERROR      UFT_ERR_USB
#define UFT_ERROR_VERIFY_FAILED  UFT_ERR_CRC
#define UFT_ERROR_WEAK_BITS      UFT_ERR_CRC
#define UFT_ERROR_DISK_NOT_OPEN  UFT_ERR_IO
#define UFT_ERROR_FORMAT_UNSUPPORTED UFT_ERR_NOT_SUPPORTED
#define UFT_ERROR_INVALID_ARGUMENT UFT_ERR_INVALID_ARG
#define UFT_ERROR_OVERFLOW       UFT_ERR_INVALID_ARG
#define UFT_ERROR_CRC            UFT_ERR_CRC
#define UFT_ERROR_INTERNAL       UFT_ERR_INTERNAL

/**
 * @brief Extended error context
 * 
 * Provides additional information about errors beyond the return code.
 * Can be embedded in context structures or used standalone.
 */
typedef struct uft_error_ctx {
    /** Primary error code */
    uft_rc_t code;
    
    /** System errno if applicable (0 if not) */
    int sys_errno;
    
    /** File/line where error occurred (for debugging) */
    const char* file;
    int line;
    
    /** Human-readable error message (optional, can be NULL) */
    char message[256];
    
    /** Function name where error occurred */
    const char* function;
    
    /** Extra context (optional) */
    const char* extra;
} uft_error_ctx_t;

/** @brief Alias for context type */
typedef uft_error_ctx_t uft_error_context_t;

/** @brief Error info structure for lookup table */
typedef struct uft_error_info {
    uft_rc_t code;       /**< Error code */
    const char* name;    /**< Error name string */
    const char* message; /**< Error description */
    const char* category;  /**< Error category */
} uft_error_info_t;

/**
 * @brief Convert error code to string
 * 
 * @param rc Error code
 * @return Static string describing the error (never NULL)
 */
const char* uft_strerror(uft_rc_t rc);

/** @brief Alias for uft_strerror */
#define uft_error_string(rc) uft_strerror(rc)

/** @brief Check if return code indicates failure */
#define UFT_FAILED(rc) ((rc) < 0)

/** @brief Check if return code indicates success */
#define UFT_SUCCEEDED(rc) ((rc) >= 0)

/** @brief Set error context (TLS) */
void uft_error_set_context(const char* file, int line,
                            const char* function, const char* message);

/** @brief Get current error context (TLS) */
const uft_error_context_t* uft_error_get_context(void);

/** @brief Clear error context (TLS) */
void uft_error_clear_context(void);

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
/* Include compat aliases at end */
#include "core/uft_error_compat.h"
