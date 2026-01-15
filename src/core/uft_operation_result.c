/**
 * @file uft_operation_result.c
 * @brief Implementation of unified operation result system
 * 
 * @version 1.0.0
 * @date 2026-01-08
 */

#include "uft/uft_operation_result.h"
#include <string.h>
#include <stdio.h>
#include <time.h>

/* ============================================================================
 * Error String Implementation (required by uft_error.h)
 * ============================================================================ */

const char* uft_strerror(uft_rc_t rc)
{
    switch (rc) {
        case UFT_SUCCESS:             return "Success";
        case UFT_ERR_INTERNAL:        return "Internal error";
        case UFT_ERR_INVALID_ARG:     return "Invalid argument";
        case UFT_ERR_BUFFER_TOO_SMALL: return "Buffer too small";
        case UFT_ERR_MEMORY:          return "Memory allocation failed";
        case UFT_ERR_RESOURCE:        return "Resource unavailable";
        case UFT_ERR_BUSY:            return "Resource busy";
        case UFT_ERR_FORMAT:          return "Format error";
        case UFT_ERR_FORMAT_DETECT:   return "Format detection failed";
        case UFT_ERR_FORMAT_VARIANT:  return "Format variant error";
        case UFT_ERR_CORRUPTED:       return "Data corrupted";
        case UFT_ERR_CRC:             return "CRC error";
        case UFT_ERR_IO:              return "I/O error";
        case UFT_ERR_FILE_NOT_FOUND:  return "File not found";
        case UFT_ERR_PERMISSION:      return "Permission denied";
        case UFT_ERR_FILE_EXISTS:     return "File already exists";
        case UFT_ERR_EOF:             return "End of file";
        case UFT_ERR_TIMEOUT:         return "Operation timeout";
        case UFT_ERR_NOT_SUPPORTED:   return "Not supported";
        case UFT_ERR_NOT_IMPLEMENTED: return "Not implemented";
        case UFT_ERR_NOT_PERMITTED:   return "Operation not permitted";
        case UFT_ERR_HARDWARE:        return "Hardware error";
        case UFT_ERR_USB:             return "USB error";
        case UFT_ERR_DEVICE_NOT_FOUND: return "Device not found";
        case UFT_ERR_ASSERTION:       return "Assertion failed";
        default:                      return "Unknown error";
    }
}

/* ============================================================================
 * String Tables
 * ============================================================================ */

static const char* operation_type_strings[] = {
    "Unknown",
    "Read",
    "Decode",
    "Analyze",
    "Write",
    "Convert",
    "Verify",
    "Recover",
    "Copy",
    "Detect",
    "Validate"
};

static const char* operation_status_strings[] = {
    "Pending",
    "Running",
    "Success",
    "Partial",
    "Failed",
    "Cancelled",
    "Timeout",
    "Not Implemented"
};

/* ============================================================================
 * Core Functions
 * ============================================================================ */

void uft_result_init(uft_operation_result_t* result, uft_operation_type_t operation)
{
    if (!result) return;
    
    memset(result, 0, sizeof(*result));
    result->operation = operation;
    result->status = UFT_STATUS_PENDING;
    result->error_code = UFT_SUCCESS;
}

void uft_result_set_success(uft_operation_result_t* result, const char* message)
{
    if (!result) return;
    
    result->status = UFT_STATUS_SUCCESS;
    result->error_code = UFT_SUCCESS;
    
    if (message) {
        strncpy(result->message, message, UFT_RESULT_MSG_MAX - 1);
        result->message[UFT_RESULT_MSG_MAX - 1] = '\0';
    }
}

void uft_result_set_partial(uft_operation_result_t* result, const char* message)
{
    if (!result) return;
    
    result->status = UFT_STATUS_PARTIAL;
    result->error_code = UFT_SUCCESS;  /* Partial is not an error */
    
    if (message) {
        strncpy(result->message, message, UFT_RESULT_MSG_MAX - 1);
        result->message[UFT_RESULT_MSG_MAX - 1] = '\0';
    }
}

void uft_result_set_error(uft_operation_result_t* result, 
                          uft_rc_t code, const char* message)
{
    if (!result) return;
    
    result->status = UFT_STATUS_FAILED;
    result->error_code = code;
    
    if (message) {
        strncpy(result->message, message, UFT_RESULT_MSG_MAX - 1);
        result->message[UFT_RESULT_MSG_MAX - 1] = '\0';
    } else {
        /* Use default error string */
        strncpy(result->message, uft_strerror(code), UFT_RESULT_MSG_MAX - 1);
        result->message[UFT_RESULT_MSG_MAX - 1] = '\0';
    }
}

void uft_result_append_detail(uft_operation_result_t* result, const char* detail)
{
    if (!result || !detail) return;
    
    size_t current_len = strlen(result->detail);
    size_t detail_len = strlen(detail);
    size_t available = UFT_RESULT_DETAIL_MAX - current_len - 2;  /* -2 for newline + null */
    
    if (available > 0 && detail_len > 0) {
        if (current_len > 0) {
            result->detail[current_len] = '\n';
            current_len++;
        }
        
        size_t to_copy = (detail_len < available) ? detail_len : available;
        memcpy(result->detail + current_len, detail, to_copy);
        result->detail[current_len + to_copy] = '\0';
    }
}

void uft_result_set_progress(uft_operation_result_t* result, float progress)
{
    if (!result) return;
    
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    
    result->timing.progress = progress;
    result->status = UFT_STATUS_RUNNING;
    
    /* Update estimated time */
    if (progress > 0.01f && result->timing.start_time > 0) {
        time_t now = time(NULL);
        uint32_t elapsed = (uint32_t)(now - result->timing.start_time) * 1000;
        result->timing.elapsed_ms = elapsed;
        
        if (progress < 1.0f) {
            result->timing.estimated_ms = (uint32_t)(elapsed / progress - elapsed);
        } else {
            result->timing.estimated_ms = 0;
        }
    }
}

void uft_result_start_timing(uft_operation_result_t* result)
{
    if (!result) return;
    
    result->timing.start_time = time(NULL);
    result->timing.end_time = 0;
    result->timing.elapsed_ms = 0;
    result->timing.estimated_ms = 0;
    result->timing.progress = 0.0f;
    result->status = UFT_STATUS_RUNNING;
}

void uft_result_stop_timing(uft_operation_result_t* result)
{
    if (!result) return;
    
    result->timing.end_time = time(NULL);
    result->timing.elapsed_ms = 
        (uint32_t)(result->timing.end_time - result->timing.start_time) * 1000;
    result->timing.estimated_ms = 0;
    result->timing.progress = 1.0f;
}

/* ============================================================================
 * String Functions
 * ============================================================================ */

const char* uft_operation_type_str(uft_operation_type_t op)
{
    if (op < 0 || op > UFT_OP_VALIDATE) {
        return "Invalid";
    }
    return operation_type_strings[op];
}

const char* uft_operation_status_str(uft_operation_status_t status)
{
    if (status < 0 || status > UFT_STATUS_NOT_IMPLEMENTED) {
        return "Invalid";
    }
    return operation_status_strings[status];
}

size_t uft_result_summary(const uft_operation_result_t* result,
                          char* buffer, size_t buffer_size)
{
    if (!result || !buffer || buffer_size == 0) return 0;
    
    int written = snprintf(buffer, buffer_size,
        "%s %s: %s\n"
        "Tracks: %u/%u good, %u bad\n"
        "Sectors: %u/%u good, %u CRC errors, %u missing\n"
        "Time: %u ms",
        uft_operation_type_str(result->operation),
        uft_operation_status_str(result->status),
        result->message[0] ? result->message : "(no message)",
        result->tracks.good, result->tracks.total, result->tracks.bad,
        result->sectors.good, result->sectors.total, 
        result->sectors.crc_error, result->sectors.missing,
        result->timing.elapsed_ms
    );
    
    return (written > 0 && (size_t)written < buffer_size) ? (size_t)written : 0;
}

/* ============================================================================
 * Specialized Initializers
 * ============================================================================ */

void uft_result_not_implemented(uft_operation_result_t* result,
                                uft_operation_type_t operation,
                                const char* feature_name)
{
    if (!result) return;
    
    uft_result_init(result, operation);
    result->status = UFT_STATUS_NOT_IMPLEMENTED;
    result->error_code = UFT_ERR_NOT_IMPLEMENTED;
    
    snprintf(result->message, UFT_RESULT_MSG_MAX,
             "%s: Not implemented yet", 
             feature_name ? feature_name : "Feature");
    
    /* TODO marker for tracking */
    snprintf(result->detail, UFT_RESULT_DETAIL_MAX,
             "TODO: Implement %s\n"
             "This is a stub operation that returns without performing any action.",
             feature_name ? feature_name : "this feature");
}

void uft_result_no_hardware(uft_operation_result_t* result,
                            const char* device_name)
{
    if (!result) return;
    
    uft_result_init(result, UFT_OP_UNKNOWN);
    result->status = UFT_STATUS_FAILED;
    result->error_code = UFT_ERR_DEVICE_NOT_FOUND;
    
    snprintf(result->message, UFT_RESULT_MSG_MAX,
             "%s not connected or not responding", 
             device_name ? device_name : "Device");
    
    snprintf(result->detail, UFT_RESULT_DETAIL_MAX,
             "Please check:\n"
             "1. Device is properly connected via USB\n"
             "2. Device drivers are installed\n"
             "3. No other application is using the device\n"
             "4. Device power is on");
}

void uft_result_cancelled(uft_operation_result_t* result)
{
    if (!result) return;
    
    result->status = UFT_STATUS_CANCELLED;
    result->error_code = UFT_ERR_TIMEOUT;  /* Closest error code */
    
    strncpy(result->message, "Operation cancelled by user", UFT_RESULT_MSG_MAX - 1);
    result->message[UFT_RESULT_MSG_MAX - 1] = '\0';
    
    /* Stop timing */
    uft_result_stop_timing(result);
}
