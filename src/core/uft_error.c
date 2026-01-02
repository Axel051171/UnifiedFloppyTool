/**
 * @file uft_error.c
 * @brief UnifiedFloppyTool - Error Handling Implementation
 * 
 * Implementiert Error-Strings, Namen und Thread-lokalen Kontext.
 * 
 * @author UFT Team
 * @date 2025
 */

#include "uft/uft_error.h"
#include <string.h>
#include <stdio.h>

// ============================================================================
// Thread-Local Storage
// ============================================================================

#if defined(_WIN32)
    #define UFT_THREAD_LOCAL __declspec(thread)
#elif defined(__GNUC__) || defined(__clang__)
    #define UFT_THREAD_LOCAL __thread
#else
    #define UFT_THREAD_LOCAL  // Fallback: kein TLS
#endif

/**
 * @brief Thread-lokaler Fehlerkontext
 */
static UFT_THREAD_LOCAL uft_error_context_t tls_error_context = {
    .code = UFT_OK,
    .file = NULL,
    .line = 0,
    .function = NULL,
    .extra = NULL
};

/**
 * @brief Thread-lokaler Extra-Buffer
 */
static UFT_THREAD_LOCAL char tls_extra_buffer[256] = {0};

// ============================================================================
// Error Info Table
// ============================================================================

/**
 * @brief Statische Tabelle aller Fehler-Informationen
 * 
 * Sortiert nach Error-Code für binäre Suche.
 */
static const uft_error_info_t error_table[] = {
    // Erfolg
    { UFT_OK,                      "UFT_OK",                      "Success",                                "Success" },
    
    // Allgemeine Fehler (-1 bis -99)
    { UFT_ERROR,                   "UFT_ERROR",                   "Generic error",                          "General" },
    { UFT_ERROR_INVALID_ARG,       "UFT_ERROR_INVALID_ARG",       "Invalid argument",                       "General" },
    { UFT_ERROR_NULL_POINTER,      "UFT_ERROR_NULL_POINTER",      "NULL pointer passed",                    "General" },
    { UFT_ERROR_NOT_IMPLEMENTED,   "UFT_ERROR_NOT_IMPLEMENTED",   "Feature not implemented",                "General" },
    { UFT_ERROR_NOT_SUPPORTED,     "UFT_ERROR_NOT_SUPPORTED",     "Operation not supported",                "General" },
    { UFT_ERROR_BUFFER_TOO_SMALL,  "UFT_ERROR_BUFFER_TOO_SMALL",  "Buffer too small",                       "General" },
    { UFT_ERROR_OUT_OF_RANGE,      "UFT_ERROR_OUT_OF_RANGE",      "Index out of range",                     "General" },
    { UFT_ERROR_TIMEOUT,           "UFT_ERROR_TIMEOUT",           "Operation timed out",                    "General" },
    { UFT_ERROR_CANCELLED,         "UFT_ERROR_CANCELLED",         "Operation cancelled by user",            "General" },
    
    // Speicher-Fehler (-100 bis -199)
    { UFT_ERROR_NO_MEMORY,         "UFT_ERROR_NO_MEMORY",         "Out of memory",                          "Memory" },
    { UFT_ERROR_ALLOC_FAILED,      "UFT_ERROR_ALLOC_FAILED",      "Memory allocation failed",               "Memory" },
    
    // Datei-Fehler (-200 bis -299)
    { UFT_ERROR_FILE_NOT_FOUND,    "UFT_ERROR_FILE_NOT_FOUND",    "File not found",                         "File" },
    { UFT_ERROR_FILE_EXISTS,       "UFT_ERROR_FILE_EXISTS",       "File already exists",                    "File" },
    { UFT_ERROR_FILE_OPEN,         "UFT_ERROR_FILE_OPEN",         "Cannot open file",                       "File" },
    { UFT_ERROR_FILE_READ,         "UFT_ERROR_FILE_READ",         "File read error",                        "File" },
    { UFT_ERROR_FILE_WRITE,        "UFT_ERROR_FILE_WRITE",        "File write error",                       "File" },
    { UFT_ERROR_FILE_SEEK,         "UFT_ERROR_FILE_SEEK",         "File seek error",                        "File" },
    { UFT_ERROR_FILE_CORRUPT,      "UFT_ERROR_FILE_CORRUPT",      "File is corrupted",                      "File" },
    { UFT_ERROR_FILE_TOO_LARGE,    "UFT_ERROR_FILE_TOO_LARGE",    "File too large",                         "File" },
    { UFT_ERROR_FILE_PERMISSION,   "UFT_ERROR_FILE_PERMISSION",   "Permission denied",                      "File" },
    
    // Format-Fehler (-300 bis -399)
    { UFT_ERROR_FORMAT_UNKNOWN,    "UFT_ERROR_FORMAT_UNKNOWN",    "Unknown format",                         "Format" },
    { UFT_ERROR_FORMAT_INVALID,    "UFT_ERROR_FORMAT_INVALID",    "Invalid format",                         "Format" },
    { UFT_ERROR_FORMAT_MISMATCH,   "UFT_ERROR_FORMAT_MISMATCH",   "Format mismatch",                        "Format" },
    { UFT_ERROR_FORMAT_VERSION,    "UFT_ERROR_FORMAT_VERSION",    "Unsupported format version",             "Format" },
    { UFT_ERROR_FORMAT_CONVERT,    "UFT_ERROR_FORMAT_CONVERT",    "Format conversion failed",               "Format" },
    { UFT_ERROR_BAD_MAGIC,         "UFT_ERROR_BAD_MAGIC",         "Invalid magic bytes",                    "Format" },
    { UFT_ERROR_BAD_CHECKSUM,      "UFT_ERROR_BAD_CHECKSUM",      "Checksum mismatch",                      "Format" },
    { UFT_ERROR_BAD_HEADER,        "UFT_ERROR_BAD_HEADER",        "Invalid header",                         "Format" },
    
    // Disk-Fehler (-400 bis -499)
    { UFT_ERROR_DISK_NOT_READY,    "UFT_ERROR_DISK_NOT_READY",    "Disk not ready",                         "Disk" },
    { UFT_ERROR_DISK_CHANGED,      "UFT_ERROR_DISK_CHANGED",      "Disk was changed",                       "Disk" },
    { UFT_ERROR_DISK_REMOVED,      "UFT_ERROR_DISK_REMOVED",      "Disk was removed",                       "Disk" },
    { UFT_ERROR_DISK_PROTECTED,    "UFT_ERROR_DISK_PROTECTED",    "Disk is write-protected",                "Disk" },
    { UFT_ERROR_NO_DISK,           "UFT_ERROR_NO_DISK",           "No disk inserted",                       "Disk" },
    { UFT_ERROR_DISK_FULL,         "UFT_ERROR_DISK_FULL",         "Disk is full",                           "Disk" },
    
    // Track/Sektor-Fehler (-500 bis -599)
    { UFT_ERROR_TRACK_NOT_FOUND,   "UFT_ERROR_TRACK_NOT_FOUND",   "Track not found",                        "Sector" },
    { UFT_ERROR_SECTOR_NOT_FOUND,  "UFT_ERROR_SECTOR_NOT_FOUND",  "Sector not found",                       "Sector" },
    { UFT_ERROR_CRC_ERROR,         "UFT_ERROR_CRC_ERROR",         "CRC error",                              "Sector" },
    { UFT_ERROR_ID_CRC_ERROR,      "UFT_ERROR_ID_CRC_ERROR",      "ID field CRC error",                     "Sector" },
    { UFT_ERROR_DATA_CRC_ERROR,    "UFT_ERROR_DATA_CRC_ERROR",    "Data field CRC error",                   "Sector" },
    { UFT_ERROR_NO_SYNC,           "UFT_ERROR_NO_SYNC",           "No sync pattern found",                  "Sector" },
    { UFT_ERROR_NO_INDEX,          "UFT_ERROR_NO_INDEX",          "No index pulse detected",                "Sector" },
    { UFT_ERROR_WEAK_BITS,         "UFT_ERROR_WEAK_BITS",         "Weak bits detected",                     "Sector" },
    { UFT_ERROR_FUZZY_BITS,        "UFT_ERROR_FUZZY_BITS",        "Fuzzy bits detected",                    "Sector" },
    { UFT_ERROR_READ_PROTECTED,    "UFT_ERROR_READ_PROTECTED",    "Copy protection detected",               "Sector" },
    { UFT_ERROR_UNFORMATTED,       "UFT_ERROR_UNFORMATTED",       "Track is unformatted",                   "Sector" },
    { UFT_ERROR_VERIFY_FAILED,     "UFT_ERROR_VERIFY_FAILED",     "Verify failed after write",              "Sector" },
    
    // Hardware-Fehler (-600 bis -699)
    { UFT_ERROR_DEVICE_NOT_FOUND,  "UFT_ERROR_DEVICE_NOT_FOUND",  "Device not found",                       "Hardware" },
    { UFT_ERROR_DEVICE_BUSY,       "UFT_ERROR_DEVICE_BUSY",       "Device is busy",                         "Hardware" },
    { UFT_ERROR_DEVICE_ERROR,      "UFT_ERROR_DEVICE_ERROR",      "Device error",                           "Hardware" },
    { UFT_ERROR_DEVICE_OFFLINE,    "UFT_ERROR_DEVICE_OFFLINE",    "Device is offline",                      "Hardware" },
    { UFT_ERROR_USB_ERROR,         "UFT_ERROR_USB_ERROR",         "USB communication error",                "Hardware" },
    { UFT_ERROR_MOTOR_ERROR,       "UFT_ERROR_MOTOR_ERROR",       "Motor control error",                    "Hardware" },
    { UFT_ERROR_SEEK_ERROR,        "UFT_ERROR_SEEK_ERROR",        "Head seek error",                        "Hardware" },
    { UFT_ERROR_DRIVE_NOT_FOUND,   "UFT_ERROR_DRIVE_NOT_FOUND",   "Drive not found",                        "Hardware" },
    { UFT_ERROR_DRIVE_BUSY,        "UFT_ERROR_DRIVE_BUSY",        "Drive is busy",                          "Hardware" },
    
    // Plugin-Fehler (-700 bis -799)
    { UFT_ERROR_PLUGIN_NOT_FOUND,  "UFT_ERROR_PLUGIN_NOT_FOUND",  "Plugin not found",                       "Plugin" },
    { UFT_ERROR_PLUGIN_LOAD,       "UFT_ERROR_PLUGIN_LOAD",       "Failed to load plugin",                  "Plugin" },
    { UFT_ERROR_PLUGIN_VERSION,    "UFT_ERROR_PLUGIN_VERSION",    "Incompatible plugin version",            "Plugin" },
    { UFT_ERROR_PLUGIN_INIT,       "UFT_ERROR_PLUGIN_INIT",       "Plugin initialization failed",           "Plugin" },
    
    // Decoder-Fehler (-800 bis -899)
    { UFT_ERROR_DECODE_FAILED,     "UFT_ERROR_DECODE_FAILED",     "Decoding failed",                        "Decoder" },
    { UFT_ERROR_ENCODE_FAILED,     "UFT_ERROR_ENCODE_FAILED",     "Encoding failed",                        "Decoder" },
    { UFT_ERROR_UNKNOWN_ENCODING,  "UFT_ERROR_UNKNOWN_ENCODING",  "Unknown encoding",                       "Decoder" },
    { UFT_ERROR_PLL_FAILED,        "UFT_ERROR_PLL_FAILED",        "PLL failed to lock",                     "Decoder" },
};

#define ERROR_TABLE_SIZE (sizeof(error_table) / sizeof(error_table[0]))

// ============================================================================
// Error Lookup
// ============================================================================

/**
 * @brief Sucht Fehler-Info in der Tabelle
 * 
 * Verwendet lineare Suche (Tabelle ist nicht sortiert nach Code).
 */
static const uft_error_info_t* find_error_info(uft_error_t err) {
    for (size_t i = 0; i < ERROR_TABLE_SIZE; i++) {
        if (error_table[i].code == err) {
            return &error_table[i];
        }
    }
    return NULL;
}

const char* uft_error_string(uft_error_t err) {
    const uft_error_info_t* info = find_error_info(err);
    if (info) {
        return info->message;
    }
    
    // Unbekannter Fehler
    static UFT_THREAD_LOCAL char unknown_buffer[64];
    snprintf(unknown_buffer, sizeof(unknown_buffer), "Unknown error (%d)", err);
    return unknown_buffer;
}

const char* uft_error_name(uft_error_t err) {
    const uft_error_info_t* info = find_error_info(err);
    if (info) {
        return info->name;
    }
    return "UFT_ERROR_UNKNOWN";
}

const uft_error_info_t* uft_error_get_info(uft_error_t err) {
    const uft_error_info_t* info = find_error_info(err);
    if (info) {
        return info;
    }
    
    // Fallback auf generischen Fehler
    static const uft_error_info_t unknown_info = {
        .code = -1,
        .name = "UFT_ERROR_UNKNOWN",
        .message = "Unknown error code",
        .category = "Unknown"
    };
    return &unknown_info;
}

// ============================================================================
// Error Context
// ============================================================================

void uft_error_set_context(const char* file, int line, const char* func,
                           const char* extra_info) {
    tls_error_context.file = file;
    tls_error_context.line = line;
    tls_error_context.function = func;
    
    if (extra_info) {
        strncpy(tls_extra_buffer, extra_info, sizeof(tls_extra_buffer) - 1);
        tls_extra_buffer[sizeof(tls_extra_buffer) - 1] = '\0';
        tls_error_context.extra = tls_extra_buffer;
    } else {
        tls_extra_buffer[0] = '\0';
        tls_error_context.extra = NULL;
    }
}

const uft_error_context_t* uft_error_get_context(void) {
    return &tls_error_context;
}

void uft_error_clear_context(void) {
    tls_error_context.code = UFT_OK;
    tls_error_context.file = NULL;
    tls_error_context.line = 0;
    tls_error_context.function = NULL;
    tls_error_context.extra = NULL;
    tls_extra_buffer[0] = '\0';
}

/**
 * @brief Setzt Fehlercode und Kontext in einem Aufruf
 */
uft_error_t uft_error_set(uft_error_t code, const char* file, int line, 
                          const char* func, const char* extra) {
    tls_error_context.code = code;
    uft_error_set_context(file, line, func, extra);
    return code;
}

// ============================================================================
// Error Formatting
// ============================================================================

/**
 * @brief Formatiert vollständige Fehlermeldung
 * 
 * @param err Fehlercode
 * @param buffer Zielpuffer
 * @param size Puffergröße
 * @return Anzahl geschriebener Zeichen
 */
int uft_error_format(uft_error_t err, char* buffer, size_t size) {
    const uft_error_info_t* info = uft_error_get_info(err);
    const uft_error_context_t* ctx = uft_error_get_context();
    
    int written;
    
    if (ctx->file && ctx->line > 0) {
        // Mit Kontext
        if (ctx->extra) {
            written = snprintf(buffer, size, 
                "[%s] %s: %s (%s:%d in %s)",
                info->category, info->name, info->message,
                ctx->file, ctx->line, ctx->function ? ctx->function : "?");
        } else {
            written = snprintf(buffer, size,
                "[%s] %s: %s (%s:%d)",
                info->category, info->name, info->message,
                ctx->file, ctx->line);
        }
    } else {
        // Ohne Kontext
        written = snprintf(buffer, size, "[%s] %s: %s",
            info->category, info->name, info->message);
    }
    
    return written;
}

/**
 * @brief Schreibt Fehler auf stderr
 */
void uft_error_print(uft_error_t err) {
    char buffer[512];
    uft_error_format(err, buffer, sizeof(buffer));
    fprintf(stderr, "UFT Error: %s\n", buffer);
}

// ============================================================================
// Error Stack (optional, für Debugging)
// ============================================================================

#define UFT_ERROR_STACK_SIZE 16

typedef struct {
    uft_error_t code;
    const char* file;
    int line;
    const char* function;
} uft_error_stack_entry_t;

static UFT_THREAD_LOCAL uft_error_stack_entry_t error_stack[UFT_ERROR_STACK_SIZE];
static UFT_THREAD_LOCAL int error_stack_top = 0;

/**
 * @brief Pusht Fehler auf den Stack
 */
void uft_error_push(uft_error_t code, const char* file, int line, const char* func) {
    if (error_stack_top < UFT_ERROR_STACK_SIZE) {
        error_stack[error_stack_top].code = code;
        error_stack[error_stack_top].file = file;
        error_stack[error_stack_top].line = line;
        error_stack[error_stack_top].function = func;
        error_stack_top++;
    }
}

/**
 * @brief Löscht den Error-Stack
 */
void uft_error_stack_clear(void) {
    error_stack_top = 0;
}

/**
 * @brief Gibt Error-Stack auf stderr aus
 */
void uft_error_stack_print(void) {
    if (error_stack_top == 0) {
        fprintf(stderr, "UFT Error Stack: (empty)\n");
        return;
    }
    
    fprintf(stderr, "UFT Error Stack:\n");
    for (int i = error_stack_top - 1; i >= 0; i--) {
        const uft_error_stack_entry_t* e = &error_stack[i];
        fprintf(stderr, "  #%d: %s at %s:%d in %s()\n",
            error_stack_top - i,
            uft_error_name(e->code),
            e->file ? e->file : "?",
            e->line,
            e->function ? e->function : "?");
    }
}

// ============================================================================
// Convenience Macros (für Header)
// ============================================================================

/*
 * Diese sollten in uft_error.h definiert werden:
 * 
 * #define UFT_RETURN_ERROR(code) \
 *     return uft_error_set(code, __FILE__, __LINE__, __func__, NULL)
 * 
 * #define UFT_RETURN_ERROR_MSG(code, msg) \
 *     return uft_error_set(code, __FILE__, __LINE__, __func__, msg)
 */
