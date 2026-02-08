#ifndef UFT_ERROR_CODES_H
#define UFT_ERROR_CODES_H
/**
 * @file uft_error_codes.h
 * @brief Unified Error Codes for UFT
 * 
 * All modules should use these error codes for consistency.
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef enum uft_error {
    /* Success */
    UFT_OK                      = 0,
    
    /* General Errors (1-99) */
    UFT_ERROR_UNKNOWN           = 1,
    UFT_ERROR_INVALID_PARAM     = 2,
    UFT_ERROR_NULL_POINTER      = 3,
    UFT_ERROR_OUT_OF_MEMORY     = 4,
    UFT_ERROR_BUFFER_TOO_SMALL  = 5,
    UFT_ERROR_NOT_IMPLEMENTED   = 6,
    UFT_ERROR_NOT_SUPPORTED     = 7,
    UFT_ERROR_TIMEOUT           = 8,
    UFT_ERROR_CANCELLED         = 9,
    UFT_ERROR_SECURITY          = 10,   /**< Security violation (path traversal, etc.) */
    
    /* File I/O Errors (100-199) */
    UFT_ERROR_FILE_NOT_FOUND    = 100,
    UFT_ERROR_FILE_OPEN         = 101,
    UFT_ERROR_FILE_READ         = 102,
    UFT_ERROR_FILE_WRITE        = 103,
    UFT_ERROR_FILE_SEEK         = 104,
    UFT_ERROR_FILE_TRUNCATED    = 105,
    UFT_ERROR_FILE_TOO_LARGE    = 106,
    UFT_ERROR_PATH_TOO_LONG     = 107,
    UFT_ERROR_PERMISSION_DENIED = 108,
    
    /* Format Errors (200-299) */
    UFT_ERROR_FORMAT_UNKNOWN    = 200,
    UFT_ERROR_FORMAT_INVALID    = 201,
    UFT_ERROR_FORMAT_VERSION    = 202,
    UFT_ERROR_FORMAT_CORRUPTED  = 203,
    UFT_ERROR_MAGIC_MISMATCH    = 204,
    UFT_ERROR_CHECKSUM          = 205,
    UFT_ERROR_CRC               = 206,
    
    /* Disk/Track Errors (300-399) */
    UFT_ERROR_DISK_NOT_READY    = 300,
    UFT_ERROR_DISK_PROTECTED    = 301,
    UFT_ERROR_TRACK_NOT_FOUND   = 302,
    UFT_ERROR_SECTOR_NOT_FOUND  = 303,
    UFT_ERROR_GEOMETRY          = 304,
    UFT_ERROR_BAD_SECTOR        = 305,
    UFT_ERROR_SYNC_NOT_FOUND    = 306,
    UFT_ERROR_ENCODING          = 307,
    
    /* Hardware Errors (400-499) */
    UFT_ERROR_HW_NOT_FOUND      = 400,
    UFT_ERROR_HW_INIT           = 401,
    UFT_ERROR_HW_COMM           = 402,
    UFT_ERROR_HW_TIMEOUT        = 403,
    UFT_ERROR_HW_BUSY           = 404,
    
    /* Protection Errors (500-599) */
    UFT_ERROR_PROTECTED         = 500,
    UFT_ERROR_COPY_PROTECTED    = 501,
    UFT_ERROR_WEAK_BITS         = 502,
    
} uft_error_t;

/** Get human-readable error string */
const char* uft_error_str(uft_error_t err);

/** Check if error code indicates success */
#define UFT_SUCCESS(e) ((e) == UFT_OK)

/** Check if error code indicates failure */
#define UFT_FAILED(e)  ((e) != UFT_OK)

#ifdef __cplusplus
}
#endif

#endif /* UFT_ERROR_CODES_H */
