/**
 * @file uft_error_handling.c
 * @brief Error handling implementation
 */

#include "uft/core/uft_error_handling.h"
#include <stdarg.h>
#include <string.h>
#include <time.h>

// Global state
uft_log_level_t g_uft_log_level = UFT_LOG_WARN;
static uft_log_callback_t g_log_callback = NULL;

void uft_set_log_callback(uft_log_callback_t callback) {
    g_log_callback = callback;
}

void uft_set_log_level(uft_log_level_t level) {
    g_uft_log_level = level;
}

static const char* level_strings[] = {
    "DEBUG", "INFO", "WARN", "ERROR", "FATAL"
};

void uft_log_internal(uft_log_level_t level, const char* file, int line,
                       const char* func, const char* fmt, ...) {
    if (level < g_uft_log_level) return;
    
    char message[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(message, sizeof(message), fmt, args);
    va_end(args);
    
    if (g_log_callback) {
        g_log_callback(level, file, line, func, message);
    } else {
        // Default: stderr
        const char* basename = strrchr(file, '/');
        if (!basename) basename = strrchr(file, '\\');
        basename = basename ? basename + 1 : file;
        
        time_t now = time(NULL);
        struct tm* tm = localtime(&now);
        
        fprintf(stderr, "[%02d:%02d:%02d] [%s] %s:%d %s(): %s\n",
                tm->tm_hour, tm->tm_min, tm->tm_sec,
                level_strings[level],
                basename, line, func, message);
    }
}

// Error string table
typedef struct {
    uft_error_t code;
    const char* name;
    const char* user_msg;
    const char* suggestion;
} error_info_t;

static const error_info_t error_table[] = {
    {UFT_OK, "OK", "Operation successful", NULL},
    {UFT_ERROR_NULL_POINTER, "NULL_POINTER", 
     "Internal error occurred", "Please report this bug"},
    {UFT_ERROR_NO_MEMORY, "NO_MEMORY",
     "Not enough memory", "Close other applications or reduce image size"},
    {UFT_ERROR_FILE_OPEN, "FILE_OPEN",
     "Cannot open file", "Check file exists and you have permission"},
    {UFT_ERROR_FILE_READ, "FILE_READ",
     "Cannot read file", "File may be corrupted or disk has errors"},
    {UFT_ERROR_FILE_WRITE, "FILE_WRITE",
     "Cannot write file", "Check disk space and write permissions"},
    {UFT_ERROR_FILE_SEEK, "FILE_SEEK",
     "Cannot access file position", "File may be corrupted"},
    {UFT_ERROR_FORMAT_INVALID, "FORMAT_INVALID",
     "File format not recognized", "Select correct format or try auto-detect"},
    {UFT_ERROR_FORMAT_UNSUPPORTED, "FORMAT_UNSUPPORTED",
     "Format not supported", "Convert to a supported format first"},
    {UFT_ERROR_OUT_OF_RANGE, "OUT_OF_RANGE",
     "Value out of valid range", "Check track/sector numbers"},
    {UFT_ERROR_OVERFLOW, "OVERFLOW",
     "Numeric overflow detected", "File may be corrupted or malicious"},
    {UFT_ERROR_INVALID_ARGUMENT, "INVALID_ARGUMENT",
     "Invalid parameter", "Check your input values"},
    {UFT_ERROR_DISK_NOT_OPEN, "DISK_NOT_OPEN",
     "Disk is not open", "Open a disk image first"},
    {UFT_ERROR_DISK_PROTECTED, "DISK_PROTECTED",
     "Disk is write-protected", "Open in read-write mode or remove protection"},
    {UFT_ERROR_CRC, "CRC_ERROR",
     "Data checksum error", "Sector is damaged, data may be corrupted"},
    {UFT_ERROR_SECTOR_NOT_FOUND, "SECTOR_NOT_FOUND",
     "Sector not found", "Track may be unformatted or damaged"},
    {UFT_ERROR_TIMEOUT, "TIMEOUT",
     "Operation timed out", "Check hardware connection"},
    {UFT_ERROR_HARDWARE, "HARDWARE",
     "Hardware error", "Check USB connection and try again"},
    {0, NULL, NULL, NULL}  // Terminator
};

const char* uft_error_string(uft_error_t err) {
    for (const error_info_t* e = error_table; e->name; e++) {
        if (e->code == err) return e->name;
    }
    return "UNKNOWN";
}

const char* uft_error_details(uft_error_t err,
                               const char** user_msg,
                               const char** suggestion) {
    for (const error_info_t* e = error_table; e->name; e++) {
        if (e->code == err) {
            if (user_msg) *user_msg = e->user_msg;
            if (suggestion) *suggestion = e->suggestion;
            return e->name;
        }
    }
    if (user_msg) *user_msg = "Unknown error";
    if (suggestion) *suggestion = "Please report this issue";
    return "UNKNOWN";
}
