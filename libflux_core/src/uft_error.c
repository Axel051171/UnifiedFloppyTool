/**
 * @file uft_error.c
 * @brief Unified error handling implementation
 */

#include "uft/uft_error.h"

const char* uft_strerror(uft_rc_t rc) {
    switch (rc) {
        case UFT_SUCCESS:
            return "Success";
        
        /* Argument errors */
        case UFT_ERR_INVALID_ARG:
            return "Invalid argument";
        case UFT_ERR_BUFFER_TOO_SMALL:
            return "Buffer too small";
        case UFT_ERR_INVALID_PATH:
            return "Invalid path";
        
        /* I/O errors */
        case UFT_ERR_IO:
            return "I/O error";
        case UFT_ERR_FILE_NOT_FOUND:
            return "File not found";
        case UFT_ERR_PERMISSION:
            return "Permission denied";
        case UFT_ERR_FILE_EXISTS:
            return "File already exists";
        case UFT_ERR_EOF:
            return "End of file";
        
        /* Format errors */
        case UFT_ERR_FORMAT:
            return "Invalid format";
        case UFT_ERR_FORMAT_DETECT:
            return "Format detection failed";
        case UFT_ERR_FORMAT_VARIANT:
            return "Unsupported format variant";
        case UFT_ERR_CORRUPTED:
            return "Corrupted data";
        case UFT_ERR_CRC:
            return "CRC mismatch";
        
        /* Resource errors */
        case UFT_ERR_MEMORY:
            return "Memory allocation failed";
        case UFT_ERR_RESOURCE:
            return "Resource limit exceeded";
        case UFT_ERR_BUSY:
            return "Resource busy";
        
        /* Feature errors */
        case UFT_ERR_NOT_SUPPORTED:
            return "Not supported";
        case UFT_ERR_NOT_IMPLEMENTED:
            return "Not implemented";
        case UFT_ERR_NOT_PERMITTED:
            return "Operation not permitted";
        
        /* Hardware errors */
        case UFT_ERR_HARDWARE:
            return "Hardware communication error";
        case UFT_ERR_USB:
            return "USB error";
        case UFT_ERR_DEVICE_NOT_FOUND:
            return "Device not found";
        case UFT_ERR_TIMEOUT:
            return "Timeout";
        
        /* Internal errors */
        case UFT_ERR_INTERNAL:
            return "Internal error";
        case UFT_ERR_ASSERTION:
            return "Assertion failed";
        
        case UFT_ERR_UNKNOWN:
        default:
            return "Unknown error";
    }
}
