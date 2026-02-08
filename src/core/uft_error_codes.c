/**
 * @file uft_error_codes.c
 * @brief Unified Error Codes Implementation (P2-ARCH-006)
 * 
 * @version 1.0.0
 * @date 2026-01-05
 */

#include "uft/core/uft_error_codes.h"
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#define THREAD_LOCAL __declspec(thread)
#else
#define THREAD_LOCAL __thread
#endif

/*===========================================================================
 * Error Database
 *===========================================================================*/

typedef struct {
    uft_error_t code;
    const char *name;
    const char *desc;
    const char *category;
} error_info_t;

static const error_info_t g_error_db[] = {
    /* Success */
    { UFT_OK, "UFT_OK", "Success", "Success" },
    
    /* Generic */
    { UFT_E_GENERIC, "UFT_E_GENERIC", "Generic error", "Generic" },
    { UFT_E_INVALID_ARG, "UFT_E_INVALID_ARG", "Invalid argument", "Generic" },
    { UFT_E_NULL_PTR, "UFT_E_NULL_PTR", "NULL pointer", "Generic" },
    { UFT_E_BUFFER_TOO_SMALL, "UFT_E_BUFFER_TOO_SMALL", "Buffer too small", "Generic" },
    { UFT_E_NOT_IMPLEMENTED, "UFT_E_NOT_IMPLEMENTED", "Not implemented", "Generic" },
    { UFT_E_NOT_SUPPORTED, "UFT_E_NOT_SUPPORTED", "Not supported", "Generic" },
    { UFT_E_NOT_FOUND, "UFT_E_NOT_FOUND", "Not found", "Generic" },
    { UFT_E_ALREADY_EXISTS, "UFT_E_ALREADY_EXISTS", "Already exists", "Generic" },
    { UFT_E_BUSY, "UFT_E_BUSY", "Resource busy", "Generic" },
    { UFT_E_TIMEOUT, "UFT_E_TIMEOUT", "Timeout", "Generic" },
    { UFT_E_CANCELLED, "UFT_E_CANCELLED", "Cancelled", "Generic" },
    { UFT_E_OVERFLOW, "UFT_E_OVERFLOW", "Overflow", "Generic" },
    { UFT_E_RANGE, "UFT_E_RANGE", "Out of range", "Generic" },
    { UFT_E_STATE, "UFT_E_STATE", "Invalid state", "Generic" },
    { UFT_E_VERSION, "UFT_E_VERSION", "Version mismatch", "Generic" },
    { UFT_E_INTERNAL, "UFT_E_INTERNAL", "Internal error", "Generic" },
    
    /* I/O */
    { UFT_E_IO, "UFT_E_IO", "I/O error", "I/O" },
    { UFT_E_FILE_NOT_FOUND, "UFT_E_FILE_NOT_FOUND", "File not found", "I/O" },
    { UFT_E_FILE_EXISTS, "UFT_E_FILE_EXISTS", "File exists", "I/O" },
    { UFT_E_FILE_OPEN, "UFT_E_FILE_OPEN", "Cannot open file", "I/O" },
    { UFT_E_FILE_READ, "UFT_E_FILE_READ", "Read error", "I/O" },
    { UFT_E_FILE_WRITE, "UFT_E_FILE_WRITE", "Write error", "I/O" },
    { UFT_E_FILE_SEEK, "UFT_E_FILE_SEEK", "Seek error", "I/O" },
    { UFT_E_FILE_TOO_LARGE, "UFT_E_FILE_TOO_LARGE", "File too large", "I/O" },
    { UFT_E_FILE_TRUNCATED, "UFT_E_FILE_TRUNCATED", "File truncated", "I/O" },
    { UFT_E_PATH_INVALID, "UFT_E_PATH_INVALID", "Invalid path", "I/O" },
    { UFT_E_DIR_NOT_FOUND, "UFT_E_DIR_NOT_FOUND", "Directory not found", "I/O" },
    { UFT_E_DISK_FULL, "UFT_E_DISK_FULL", "Disk full", "I/O" },
    { UFT_E_READ_ONLY, "UFT_E_READ_ONLY", "Read only", "I/O" },
    { UFT_E_EOF, "UFT_E_EOF", "End of file", "I/O" },
    
    /* Format */
    { UFT_E_FORMAT, "UFT_E_FORMAT", "Format error", "Format" },
    { UFT_E_FORMAT_UNKNOWN, "UFT_E_FORMAT_UNKNOWN", "Unknown format", "Format" },
    { UFT_E_FORMAT_INVALID, "UFT_E_FORMAT_INVALID", "Invalid format", "Format" },
    { UFT_E_FORMAT_UNSUPPORTED, "UFT_E_FORMAT_UNSUPPORTED", "Unsupported format", "Format" },
    { UFT_E_FORMAT_VERSION, "UFT_E_FORMAT_VERSION", "Unsupported version", "Format" },
    { UFT_E_MAGIC, "UFT_E_MAGIC", "Invalid magic", "Format" },
    { UFT_E_HEADER, "UFT_E_HEADER", "Invalid header", "Format" },
    { UFT_E_CHECKSUM, "UFT_E_CHECKSUM", "Checksum error", "Format" },
    { UFT_E_CORRUPT, "UFT_E_CORRUPT", "Corrupt data", "Format" },
    { UFT_E_TRUNCATED, "UFT_E_TRUNCATED", "Data truncated", "Format" },
    { UFT_E_PARSE, "UFT_E_PARSE", "Parse error", "Format" },
    { UFT_E_ENCODING, "UFT_E_ENCODING", "Encoding error", "Format" },
    { UFT_E_COMPRESSION, "UFT_E_COMPRESSION", "Compression error", "Format" },
    { UFT_E_DECOMPRESSION, "UFT_E_DECOMPRESSION", "Decompression error", "Format" },
    
    /* Decode */
    { UFT_E_DECODE, "UFT_E_DECODE", "Decode error", "Decode" },
    { UFT_E_DECODE_SYNC, "UFT_E_DECODE_SYNC", "Sync not found", "Decode" },
    { UFT_E_DECODE_IDAM, "UFT_E_DECODE_IDAM", "IDAM not found", "Decode" },
    { UFT_E_DECODE_DAM, "UFT_E_DECODE_DAM", "DAM not found", "Decode" },
    { UFT_E_DECODE_CRC, "UFT_E_DECODE_CRC", "CRC error", "Decode" },
    { UFT_E_DECODE_BITSLIP, "UFT_E_DECODE_BITSLIP", "Bit slip", "Decode" },
    { UFT_E_DECODE_PLL, "UFT_E_DECODE_PLL", "PLL lock failure", "Decode" },
    { UFT_E_DECODE_WEAK, "UFT_E_DECODE_WEAK", "Weak bits", "Decode" },
    { UFT_E_DECODE_TIMING, "UFT_E_DECODE_TIMING", "Timing error", "Decode" },
    { UFT_E_ENCODE, "UFT_E_ENCODE", "Encode error", "Decode" },
    { UFT_E_NOT_DETECTED, "UFT_E_NOT_DETECTED", "Not detected", "Decode" },
    { UFT_E_NOT_REGISTERED, "UFT_E_NOT_REGISTERED", "Not registered", "Decode" },
    
    /* Hardware */
    { UFT_E_HARDWARE, "UFT_E_HARDWARE", "Hardware error", "Hardware" },
    { UFT_E_HW_NOT_FOUND, "UFT_E_HW_NOT_FOUND", "Hardware not found", "Hardware" },
    { UFT_E_HW_OPEN, "UFT_E_HW_OPEN", "Cannot open device", "Hardware" },
    { UFT_E_HW_TIMEOUT, "UFT_E_HW_TIMEOUT", "Hardware timeout", "Hardware" },
    { UFT_E_HW_BUSY, "UFT_E_HW_BUSY", "Hardware busy", "Hardware" },
    { UFT_E_HW_COMM, "UFT_E_HW_COMM", "Communication error", "Hardware" },
    { UFT_E_HW_FIRMWARE, "UFT_E_HW_FIRMWARE", "Firmware error", "Hardware" },
    { UFT_E_DRIVE_NOT_READY, "UFT_E_DRIVE_NOT_READY", "Drive not ready", "Hardware" },
    { UFT_E_NO_DISK, "UFT_E_NO_DISK", "No disk", "Hardware" },
    { UFT_E_WRITE_PROTECT, "UFT_E_WRITE_PROTECT", "Write protected", "Hardware" },
    { UFT_E_SEEK, "UFT_E_SEEK", "Seek error", "Hardware" },
    { UFT_E_INDEX, "UFT_E_INDEX", "No index pulse", "Hardware" },
    
    /* Memory */
    { UFT_E_MEMORY, "UFT_E_MEMORY", "Memory error", "Memory" },
    { UFT_E_NOMEM, "UFT_E_NOMEM", "Out of memory", "Memory" },
    { UFT_E_ALLOC, "UFT_E_ALLOC", "Allocation failed", "Memory" },
    { UFT_E_ALIGNMENT, "UFT_E_ALIGNMENT", "Alignment error", "Memory" },
    
    /* Protection */
    { UFT_E_PROTECTION, "UFT_E_PROTECTION", "Protection error", "Protection" },
    { UFT_E_PROT_DETECTED, "UFT_E_PROT_DETECTED", "Protection detected", "Protection" },
    { UFT_E_PROT_UNSUPPORTED, "UFT_E_PROT_UNSUPPORTED", "Unsupported protection", "Protection" },
    
    /* Validation */
    { UFT_E_VALIDATION, "UFT_E_VALIDATION", "Validation error", "Validation" },
    { UFT_E_VERIFY, "UFT_E_VERIFY", "Verify failed", "Validation" },
    { UFT_E_MISMATCH, "UFT_E_MISMATCH", "Data mismatch", "Validation" },
    { UFT_E_HASH, "UFT_E_HASH", "Hash mismatch", "Validation" },
    { UFT_E_BOUNDS, "UFT_E_BOUNDS", "Bounds error", "Validation" },
};

#define ERROR_DB_SIZE (sizeof(g_error_db) / sizeof(g_error_db[0]))

/*===========================================================================
 * Thread-Local Error Context
 *===========================================================================*/

static THREAD_LOCAL uft_error_t g_last_error = UFT_OK;
static THREAD_LOCAL char g_error_msg[256] = {0};

/*===========================================================================
 * Lookup Functions
 *===========================================================================*/

static const error_info_t* find_error(uft_error_t err)
{
    for (size_t i = 0; i < ERROR_DB_SIZE; i++) {
        if (g_error_db[i].code == err) {
            return &g_error_db[i];
        }
    }
    return NULL;
}

const char* uft_error_name(uft_error_t err)
{
    const error_info_t *info = find_error(err);
    if (info) return info->name;
    
    /* Generate name from range */
    if (err >= -99 && err < 0) return "UFT_E_GENERIC";
    if (err >= -199 && err < -100) return "UFT_E_IO";
    if (err >= -299 && err < -200) return "UFT_E_FORMAT";
    if (err >= -399 && err < -300) return "UFT_E_DECODE";
    if (err >= -499 && err < -400) return "UFT_E_HARDWARE";
    if (err >= -599 && err < -500) return "UFT_E_MEMORY";
    if (err >= -699 && err < -600) return "UFT_E_PROTECTION";
    if (err >= -799 && err < -700) return "UFT_E_VALIDATION";
    
    return "UFT_E_UNKNOWN";
}

const char* uft_error_desc(uft_error_t err)
{
    const error_info_t *info = find_error(err);
    return info ? info->desc : "Unknown error";
}

const char* uft_error_category(uft_error_t err)
{
    const error_info_t *info = find_error(err);
    if (info) return info->category;
    
    /* Determine from range */
    if (err == 0) return "Success";
    if (err >= -99 && err < 0) return "Generic";
    if (err >= -199 && err < -100) return "I/O";
    if (err >= -299 && err < -200) return "Format";
    if (err >= -399 && err < -300) return "Decode";
    if (err >= -499 && err < -400) return "Hardware";
    if (err >= -599 && err < -500) return "Memory";
    if (err >= -699 && err < -600) return "Protection";
    if (err >= -799 && err < -700) return "Validation";
    
    return "Unknown";
}

int uft_error_format(uft_error_t err, char *buffer, size_t size)
{
    if (!buffer || size == 0) return -1;
    
    const char *name = uft_error_name(err);
    const char *desc = uft_error_desc(err);
    
    return snprintf(buffer, size, "%s (%d): %s", name, err, desc);
}

/*===========================================================================
 * Thread-Local Error Context
 *===========================================================================*/

void uft_set_error(uft_error_t err)
{
    g_last_error = err;
    g_error_msg[0] = '\0';
}

void uft_set_error_msg(uft_error_t err, const char *msg)
{
    g_last_error = err;
    if (msg) {
        snprintf(g_error_msg, sizeof(g_error_msg), "%s", msg);
    } else {
        g_error_msg[0] = '\0';
    }
}

uft_error_t uft_get_error(void)
{
    return g_last_error;
}

const char* uft_get_error_msg(void)
{
    if (g_error_msg[0]) {
        return g_error_msg;
    }
    return uft_error_desc(g_last_error);
}

void uft_clear_error(void)
{
    g_last_error = UFT_OK;
    g_error_msg[0] = '\0';
}

/*===========================================================================
 * Actionable Error Suggestions (P3-9)
 *===========================================================================*/

const char* uft_error_suggestion(uft_error_t err)
{
    switch (err) {
        /* I/O Errors */
        case UFT_E_FILE_NOT_FOUND:
            return "Check if the file path is correct and the file exists.";
        case UFT_E_FILE_OPEN:
            return "Ensure the file is not locked by another application.";
        case UFT_E_FILE_READ:
            return "Check if the file is readable and not corrupted.";
        case UFT_E_FILE_WRITE:
            return "Check disk space and write permissions.";
        case UFT_E_DISK_FULL:
            return "Free up disk space or choose a different location.";
        case UFT_E_READ_ONLY:
            return "Remove write protection or save to a different location.";
        
        /* Format Errors */
        case UFT_E_FORMAT_UNKNOWN:
            return "Try specifying the format manually with --format option.";
        case UFT_E_FORMAT_INVALID:
            return "The file may be corrupted. Try a different source.";
        case UFT_E_FORMAT_UNSUPPORTED:
            return "This format variant is not yet supported. Check documentation.";
        case UFT_E_MAGIC:
            return "File header is invalid. May be wrong format or corrupted.";
        case UFT_E_CHECKSUM:
            return "Data integrity check failed. Enable error correction.";
        case UFT_E_CORRUPT:
            return "Use --recover option to attempt data recovery.";
        
        /* Decode Errors */
        case UFT_E_DECODE_SYNC:
            return "Try adjusting PLL settings or use multi-revolution capture.";
        case UFT_E_DECODE_CRC:
            return "Enable --retry with higher revolution count.";
        case UFT_E_DECODE_PLL:
            return "Adjust --pll-window or try different encoding preset.";
        case UFT_E_DECODE_WEAK:
            return "Use --preserve-weak to keep weak bit information.";
        
        /* Hardware Errors */
        case UFT_E_HW_NOT_FOUND:
            return "Connect the device and check USB connection.";
        case UFT_E_HW_OPEN:
            return "Install device drivers or check permissions.";
        case UFT_E_HW_TIMEOUT:
            return "Check drive motor and disk insertion.";
        case UFT_E_HW_INDEX:
            return "The disk may not be spinning. Check drive mechanism.";
        case UFT_E_HW_TRK0:
            return "Head cannot find track 0. Check drive alignment.";
        
        /* Memory Errors */
        case UFT_E_MEMORY:
        case UFT_E_ALLOC:
            return "Close other applications to free memory.";
        
        default:
            return NULL;  /* No specific suggestion */
    }
}

int uft_error_format_full(uft_error_t err, char *buffer, size_t size)
{
    if (!buffer || size == 0) return -1;
    
    const char *name = uft_error_name(err);
    const char *desc = uft_error_desc(err);
    const char *suggest = uft_error_suggestion(err);
    
    int written;
    if (suggest) {
        written = snprintf(buffer, size, "%s (%d): %s\n  → %s", 
                          name, err, desc, suggest);
    } else {
        written = snprintf(buffer, size, "%s (%d): %s", name, err, desc);
    }
    
    return written;
}
