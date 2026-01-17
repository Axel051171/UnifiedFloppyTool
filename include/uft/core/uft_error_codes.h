/**
 * @file uft_error_codes.h
 * @brief Unified Error Codes (P2-ARCH-006)
 * 
 * Central error code definitions for all UFT subsystems.
 * Consolidates: UFT_ERR_*, UFT_IR_ERR_*, UFT_DEC_ERR_*, UFT_WEB_ERR_*
 * 
 * Error code ranges:
 *   0        = Success
 *  -1..-99   = Generic errors
 * -100..-199 = I/O errors
 * -200..-299 = Format/Parse errors
 * -300..-399 = Decode errors
 * -400..-499 = Hardware errors
 * -500..-599 = Memory errors
 * -600..-699 = Protection errors
 * -700..-799 = Validation errors
 * 
 * @version 1.0.0
 * @date 2026-01-05
 */

#ifndef UFT_ERROR_CODES_H
#define UFT_ERROR_CODES_H

#include <stdbool.h>

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Success
 *===========================================================================*/

#define UFT_OK                      0       /**< Success */
#define UFT_SUCCESS                 0       /**< Success (alias) */

/*===========================================================================
 * Generic Errors (-1 to -99)
 *===========================================================================*/

#define UFT_E_GENERIC              -1       /**< Generic/unspecified error */
#define UFT_E_INVALID_ARG          -2       /**< Invalid argument */
#define UFT_E_NULL_PTR             -3       /**< NULL pointer */
#define UFT_E_BUFFER_TOO_SMALL     -4       /**< Buffer too small */
#define UFT_E_NOT_IMPLEMENTED      -5       /**< Not implemented */
#define UFT_E_NOT_SUPPORTED        -6       /**< Operation not supported */
#define UFT_E_NOT_FOUND            -7       /**< Item not found */
#define UFT_E_ALREADY_EXISTS       -8       /**< Item already exists */
#define UFT_E_BUSY                 -9       /**< Resource busy */
#define UFT_E_TIMEOUT             -10       /**< Operation timed out */
#define UFT_E_CANCELLED           -11       /**< Operation cancelled */
#define UFT_E_OVERFLOW            -12       /**< Overflow (integer, buffer) */
#define UFT_E_UNDERFLOW           -13       /**< Underflow */
#define UFT_E_RANGE               -14       /**< Value out of range */
#define UFT_E_STATE               -15       /**< Invalid state */
#define UFT_E_LOCKED              -16       /**< Resource locked */
#define UFT_E_PERMISSION          -17       /**< Permission denied */
#define UFT_E_VERSION             -18       /**< Version mismatch */
#define UFT_E_INTERNAL            -19       /**< Internal error */
#define UFT_E_SECURITY            -20       /**< Security violation (path traversal, etc.) */

/*===========================================================================
 * I/O Errors (-100 to -199)
 *===========================================================================*/

#define UFT_E_IO                  -100      /**< Generic I/O error */
#define UFT_E_FILE_NOT_FOUND      -101      /**< File not found */
#define UFT_E_FILE_EXISTS         -102      /**< File already exists */
#define UFT_E_FILE_OPEN           -103      /**< Cannot open file */
#define UFT_E_FILE_READ           -104      /**< Read error */
#define UFT_E_FILE_WRITE          -105      /**< Write error */
#define UFT_E_FILE_SEEK           -106      /**< Seek error */
#define UFT_E_FILE_CLOSE          -107      /**< Close error */
#define UFT_E_FILE_CREATE         -108      /**< Cannot create file */
#define UFT_E_FILE_DELETE         -109      /**< Cannot delete file */
#define UFT_E_FILE_TOO_LARGE      -110      /**< File too large */
#define UFT_E_FILE_TOO_SMALL      -111      /**< File too small */
#define UFT_E_FILE_TRUNCATED      -112      /**< File truncated */
#define UFT_E_PATH_INVALID        -113      /**< Invalid path */
#define UFT_E_PATH_TOO_LONG       -114      /**< Path too long */
#define UFT_E_DIR_NOT_FOUND       -115      /**< Directory not found */
#define UFT_E_DIR_NOT_EMPTY       -116      /**< Directory not empty */
#define UFT_E_DISK_FULL           -117      /**< Disk full */
#define UFT_E_READ_ONLY           -118      /**< Read-only file/device */
#define UFT_E_EOF                 -119      /**< End of file */

/*===========================================================================
 * Format/Parse Errors (-200 to -299)
 *===========================================================================*/

#define UFT_E_FORMAT              -200      /**< Generic format error */
#define UFT_E_FORMAT_UNKNOWN      -201      /**< Unknown format */
#define UFT_E_FORMAT_INVALID      -202      /**< Invalid format */
#define UFT_E_FORMAT_UNSUPPORTED  -203      /**< Unsupported format */
#define UFT_E_FORMAT_VERSION      -204      /**< Unsupported format version */
#define UFT_E_MAGIC               -205      /**< Invalid magic number */
#define UFT_E_HEADER              -206      /**< Invalid header */
#define UFT_E_CHECKSUM            -207      /**< Checksum/CRC error */
#define UFT_E_CORRUPT             -208      /**< Corrupt data */
#define UFT_E_TRUNCATED           -209      /**< Data truncated */
#define UFT_E_INCOMPLETE          -210      /**< Incomplete data */
#define UFT_E_PARSE               -211      /**< Parse error */
#define UFT_E_SYNTAX              -212      /**< Syntax error */
#define UFT_E_ENCODING            -213      /**< Encoding error */
#define UFT_E_COMPRESSION         -214      /**< Compression error */
#define UFT_E_DECOMPRESSION       -215      /**< Decompression error */

/*===========================================================================
 * Decode Errors (-300 to -399)
 *===========================================================================*/

#define UFT_E_DECODE              -300      /**< Generic decode error */
#define UFT_E_DECODE_SYNC         -301      /**< Sync not found */
#define UFT_E_DECODE_IDAM         -302      /**< IDAM not found */
#define UFT_E_DECODE_DAM          -303      /**< DAM not found */
#define UFT_E_DECODE_CRC          -304      /**< CRC error in decode */
#define UFT_E_DECODE_BITSLIP      -305      /**< Bit slip detected */
#define UFT_E_DECODE_PLL          -306      /**< PLL lock failure */
#define UFT_E_DECODE_WEAK         -307      /**< Weak bits detected */
#define UFT_E_DECODE_SPLICE       -308      /**< Write splice error */
#define UFT_E_DECODE_DENSITY      -309      /**< Density mismatch */
#define UFT_E_DECODE_TIMING       -310      /**< Timing error */
#define UFT_E_ENCODE              -320      /**< Generic encode error */
#define UFT_E_ENCODE_OVERFLOW     -321      /**< Track overflow */
#define UFT_E_NOT_DETECTED        -330      /**< Encoding not detected */
#define UFT_E_NOT_REGISTERED      -331      /**< Decoder not registered */

/*===========================================================================
 * Hardware Errors (-400 to -499)
 *===========================================================================*/

#define UFT_E_HARDWARE            -400      /**< Generic hardware error */
#define UFT_E_HW_NOT_FOUND        -401      /**< Hardware not found */
#define UFT_E_HW_OPEN             -402      /**< Cannot open device */
#define UFT_E_HW_CLOSE            -403      /**< Cannot close device */
#define UFT_E_HW_TIMEOUT          -404      /**< Hardware timeout */
#define UFT_E_HW_BUSY             -405      /**< Hardware busy */
#define UFT_E_HW_RESET            -406      /**< Hardware reset failed */
#define UFT_E_HW_COMM             -407      /**< Communication error */
#define UFT_E_HW_PROTOCOL         -408      /**< Protocol error */
#define UFT_E_HW_FIRMWARE         -409      /**< Firmware error/mismatch */
#define UFT_E_DRIVE_NOT_READY     -410      /**< Drive not ready */
#define UFT_E_NO_DISK             -411      /**< No disk in drive */
#define UFT_E_WRITE_PROTECT       -412      /**< Disk write protected */
#define UFT_E_SEEK                -413      /**< Seek error */
#define UFT_E_INDEX               -414      /**< No index pulse */
#define UFT_E_MOTOR               -415      /**< Motor error */

/*===========================================================================
 * Memory Errors (-500 to -599)
 *===========================================================================*/

#define UFT_E_MEMORY              -500      /**< Generic memory error */
#define UFT_E_NOMEM               -501      /**< Out of memory */
#define UFT_E_ALLOC               -502      /**< Allocation failed */
#define UFT_E_REALLOC             -503      /**< Reallocation failed */
#define UFT_E_FREE                -504      /**< Free error (double free) */
#define UFT_E_LEAK                -505      /**< Memory leak detected */
#define UFT_E_ALIGNMENT           -506      /**< Alignment error */
#define UFT_E_STACK               -507      /**< Stack overflow */

/*===========================================================================
 * Copy Protection Errors (-600 to -699)
 *===========================================================================*/

#define UFT_E_PROTECTION          -600      /**< Generic protection error */
#define UFT_E_PROT_DETECTED       -601      /**< Protection detected */
#define UFT_E_PROT_UNSUPPORTED    -602      /**< Unsupported protection */
#define UFT_E_PROT_INCOMPLETE     -603      /**< Incomplete protection dump */
#define UFT_E_PROT_VERIFY         -604      /**< Protection verify failed */

/*===========================================================================
 * Validation Errors (-700 to -799)
 *===========================================================================*/

#define UFT_E_VALIDATION          -700      /**< Generic validation error */
#define UFT_E_VERIFY              -701      /**< Verification failed */
#define UFT_E_MISMATCH            -702      /**< Data mismatch */
#define UFT_E_HASH                -703      /**< Hash mismatch */
#define UFT_E_SIGNATURE           -704      /**< Signature invalid */
#define UFT_E_BOUNDS              -705      /**< Bounds check failed */
#define UFT_E_CONSTRAINT          -706      /**< Constraint violation */

/*===========================================================================
 * Error Result Type
 *===========================================================================*/

/**
 * @brief Error result type
 * Note: May already be defined by uft_error.h as alias for uft_rc_t
 */
#ifndef UFT_ERROR_T_DEFINED
#define UFT_ERROR_T_DEFINED
typedef int uft_error_t;
#endif

/**
 * @brief Check if result is success
 */
static inline bool uft_succeeded(uft_error_t err) {
    return err >= 0;
}

/**
 * @brief Check if result is failure
 */
static inline bool uft_failed(uft_error_t err) {
    return err < 0;
}

/*===========================================================================
 * Error Information Functions
 *===========================================================================*/

/**
 * @brief Get error name
 */
const char* uft_error_name(uft_error_t err);

/**
 * @brief Get error description
 */
const char* uft_error_desc(uft_error_t err);

/**
 * @brief Get error category
 */
const char* uft_error_category(uft_error_t err);

/**
 * @brief Format error message
 */
int uft_error_format(uft_error_t err, char *buffer, size_t size);

/**
 * @brief Get actionable suggestion for an error
 * @param err Error code
 * @return Suggestion string or NULL if no suggestion available
 */
const char* uft_error_suggestion(uft_error_t err);

/**
 * @brief Format error with description and suggestion
 * @param err Error code
 * @param buffer Output buffer
 * @param size Buffer size
 * @return Number of characters written (excluding null terminator)
 */
int uft_error_format_full(uft_error_t err, char *buffer, size_t size);

/*===========================================================================
 * Error Context (Thread-Local)
 *===========================================================================*/

/**
 * @brief Set last error (thread-local)
 */
void uft_set_error(uft_error_t err);

/**
 * @brief Set last error with message
 */
void uft_set_error_msg(uft_error_t err, const char *msg);

/**
 * @brief Get last error (thread-local)
 */
uft_error_t uft_get_error(void);

/**
 * @brief Get last error message
 */
const char* uft_get_error_msg(void);

/**
 * @brief Clear last error
 */
void uft_clear_error(void);

/*===========================================================================
 * Legacy Compatibility Macros
 *===========================================================================*/

/* Map legacy error codes to unified codes */
#define UFT_ERR_OK              UFT_OK
#define UFT_ERR_INVALID_ARG     UFT_E_INVALID_ARG
#define UFT_ERR_BUFFER_TOO_SMALL UFT_E_BUFFER_TOO_SMALL
#define UFT_ERR_INVALID_PATH    UFT_E_PATH_INVALID
#define UFT_ERR_IO              UFT_E_IO
#define UFT_ERR_FILE_NOT_FOUND  UFT_E_FILE_NOT_FOUND
#define UFT_ERR_UNSUPPORTED     UFT_E_NOT_SUPPORTED
#define UFT_ERR_NOT_FOUND       UFT_E_NOT_FOUND

/* IR format compatibility */
#define UFT_IR_OK               UFT_OK
#define UFT_IR_ERR_NOMEM        UFT_E_NOMEM
#define UFT_IR_ERR_INVALID      UFT_E_INVALID_ARG
#define UFT_IR_ERR_OVERFLOW     UFT_E_OVERFLOW
#define UFT_IR_ERR_IO           UFT_E_IO
#define UFT_IR_ERR_FORMAT       UFT_E_FORMAT
#define UFT_IR_ERR_VERSION      UFT_E_FORMAT_VERSION
#define UFT_IR_ERR_CHECKSUM     UFT_E_CHECKSUM
#define UFT_IR_ERR_COMPRESSION  UFT_E_COMPRESSION
#define UFT_IR_ERR_NOT_FOUND    UFT_E_NOT_FOUND
#define UFT_IR_ERR_DUPLICATE    UFT_E_ALREADY_EXISTS
#define UFT_IR_ERR_CORRUPT      UFT_E_CORRUPT

/* Decoder compatibility */
#define UFT_DEC_OK              UFT_OK
#define UFT_DEC_ERR_INVALID_ARG UFT_E_INVALID_ARG
#define UFT_DEC_ERR_NO_MEMORY   UFT_E_NOMEM
#define UFT_DEC_ERR_NOT_DETECTED UFT_E_NOT_DETECTED
#define UFT_DEC_ERR_DECODE_FAILED UFT_E_DECODE
#define UFT_DEC_ERR_ENCODE_FAILED UFT_E_ENCODE
#define UFT_DEC_ERR_CRC_ERROR   UFT_E_DECODE_CRC
#define UFT_DEC_ERR_NO_SYNC     UFT_E_DECODE_SYNC
#define UFT_DEC_ERR_TRUNCATED   UFT_E_TRUNCATED
#define UFT_DEC_ERR_NOT_REGISTERED UFT_E_NOT_REGISTERED
#define UFT_DEC_ERR_UNSUPPORTED UFT_E_NOT_SUPPORTED

#ifdef __cplusplus
}
#endif

#endif /* UFT_ERROR_CODES_H */
