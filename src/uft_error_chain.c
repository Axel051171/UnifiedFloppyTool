/**
 * @file uft_error_chain.c
 * @brief Structured Error Chain and Context Implementation
 * 
 * TICKET-010: Error Chain / Context
 */

#include "uft/uft_error_chain.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <errno.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#include <sys/time.h>
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Thread-Local Storage
 * ═══════════════════════════════════════════════════════════════════════════════ */

#ifdef _WIN32
static DWORD tls_key = TLS_OUT_OF_INDEXES;
static CRITICAL_SECTION init_lock;
static bool initialized = false;
#else
static pthread_key_t tls_key;
static pthread_once_t tls_once = PTHREAD_ONCE_INIT;
#endif

static uft_error_context_t *default_context = NULL;

/* Callback storage */
static uft_error_callback_t global_callback = NULL;
static void *global_callback_data = NULL;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Helper Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

static uint64_t get_timestamp_ms(void) {
#ifdef _WIN32
    return GetTickCount64();
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
#endif
}

static char *strdup_safe(const char *str) {
    return str ? strdup(str) : NULL;
}

static void free_error_entry(uft_error_entry_t *entry) {
    if (!entry) return;
    
    free(entry->message);
    free(entry->detail);
    free(entry->suggestion);
    
    /* Don't free cause - it's part of the chain */
    free(entry);
}

#ifndef _WIN32
static void tls_destructor(void *data) {
    if (data && data != default_context) {
        uft_error_context_destroy((uft_error_context_t *)data);
    }
}

static void init_tls(void) {
    pthread_key_create(&tls_key, tls_destructor);
}
#endif

static void ensure_initialized(void) {
#ifdef _WIN32
    if (!initialized) {
        InitializeCriticalSection(&init_lock);
        EnterCriticalSection(&init_lock);
        if (!initialized) {
            tls_key = TlsAlloc();
            initialized = true;
        }
        LeaveCriticalSection(&init_lock);
    }
#else
    pthread_once(&tls_once, init_tls);
#endif
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Context Management
 * ═══════════════════════════════════════════════════════════════════════════════ */

uft_error_context_t *uft_error_context_create(void) {
    uft_error_context_t *ctx = calloc(1, sizeof(uft_error_context_t));
    if (!ctx) return NULL;
    
    ctx->chain = NULL;
    ctx->last = NULL;
    ctx->count = 0;
    ctx->context_depth = 0;
    ctx->min_severity = UFT_SEVERITY_WARNING;
    ctx->max_entries = 100;
    ctx->capture_trace = false;
    
    return ctx;
}

void uft_error_context_destroy(uft_error_context_t *ctx) {
    if (!ctx) return;
    
    /* Free error chain */
    uft_error_entry_t *entry = ctx->chain;
    while (entry) {
        uft_error_entry_t *next = entry->next;
        free_error_entry(entry);
        entry = next;
    }
    
    free(ctx);
}

uft_error_context_t *uft_error_context_get(void) {
    ensure_initialized();
    
#ifdef _WIN32
    uft_error_context_t *ctx = (uft_error_context_t *)TlsGetValue(tls_key);
#else
    uft_error_context_t *ctx = (uft_error_context_t *)pthread_getspecific(tls_key);
#endif
    
    if (!ctx) {
        if (!default_context) {
            default_context = uft_error_context_create();
        }
        ctx = default_context;
    }
    
    return ctx;
}

void uft_error_context_set(uft_error_context_t *ctx) {
    ensure_initialized();
    
#ifdef _WIN32
    TlsSetValue(tls_key, ctx);
#else
    pthread_setspecific(tls_key, ctx);
#endif
}

void uft_error_clear(uft_error_context_t *ctx) {
    if (!ctx) ctx = uft_error_context_get();
    if (!ctx) return;
    
    uft_error_entry_t *entry = ctx->chain;
    while (entry) {
        uft_error_entry_t *next = entry->next;
        free_error_entry(entry);
        entry = next;
    }
    
    ctx->chain = NULL;
    ctx->last = NULL;
    ctx->count = 0;
}

void uft_error_configure(uft_error_context_t *ctx, uft_severity_t min_severity,
                          int max_entries, bool capture_trace) {
    if (!ctx) ctx = uft_error_context_get();
    if (!ctx) return;
    
    ctx->min_severity = min_severity;
    ctx->max_entries = max_entries;
    ctx->capture_trace = capture_trace;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Context Stack Operations
 * ═══════════════════════════════════════════════════════════════════════════════ */

void uft_error_push_context(uft_error_context_t *ctx, const char *operation) {
    if (!ctx) ctx = uft_error_context_get();
    if (!ctx || !operation) return;
    
    if (ctx->context_depth < 16) {
        ctx->context_stack[ctx->context_depth++] = operation;
    }
}

void uft_error_pop_context(uft_error_context_t *ctx) {
    if (!ctx) ctx = uft_error_context_get();
    if (!ctx) return;
    
    if (ctx->context_depth > 0) {
        ctx->context_depth--;
    }
}

const char *uft_error_current_context(uft_error_context_t *ctx) {
    if (!ctx) ctx = uft_error_context_get();
    if (!ctx || ctx->context_depth == 0) return NULL;
    
    return ctx->context_stack[ctx->context_depth - 1];
}

char *uft_error_context_path(uft_error_context_t *ctx, const char *separator) {
    if (!ctx) ctx = uft_error_context_get();
    if (!ctx || ctx->context_depth == 0) return strdup("");
    
    if (!separator) separator = " > ";
    
    size_t len = 0;
    for (int i = 0; i < ctx->context_depth; i++) {
        len += strlen(ctx->context_stack[i]) + strlen(separator);
    }
    
    char *path = malloc(len + 1);
    if (!path) return NULL;
    
    path[0] = '\0';
    for (int i = 0; i < ctx->context_depth; i++) {
        if (i > 0) strncat(path, separator, path_size - strlen(path) - 1);
        strncat(path, ctx->context_stack[i], path_size - strlen(path) - 1);
    }
    
    return path;
}

void uft_error_pop_context_cleanup(const char **dummy) {
    (void)dummy;
    uft_error_pop_context(NULL);
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Error Reporting
 * ═══════════════════════════════════════════════════════════════════════════════ */

static void add_error_entry(uft_error_context_t *ctx, uft_error_entry_t *entry) {
    /* Trim if over limit */
    while (ctx->count >= ctx->max_entries && ctx->chain) {
        uft_error_entry_t *oldest = ctx->chain;
        ctx->chain = oldest->next;
        free_error_entry(oldest);
        ctx->count--;
    }
    
    /* Add to end */
    if (!ctx->chain) {
        ctx->chain = entry;
    } else {
        ctx->last->next = entry;
    }
    ctx->last = entry;
    ctx->count++;
    
    /* Invoke callback */
    if (global_callback) {
        global_callback(entry, global_callback_data);
    }
}

uft_error_t uft_error_report(uft_error_context_t *ctx, uft_error_t code,
                              uft_severity_t severity, const char *message) {
    return uft_error_report_loc(ctx, code, severity, NULL, NULL, 0, message);
}

uft_error_t uft_error_report_loc(uft_error_context_t *ctx, uft_error_t code,
                                  uft_severity_t severity,
                                  const char *file, const char *func, int line,
                                  const char *message) {
    if (!ctx) ctx = uft_error_context_get();
    if (!ctx) return code;
    
    /* Check severity threshold */
    if (severity < ctx->min_severity) return code;
    
    uft_error_entry_t *entry = calloc(1, sizeof(uft_error_entry_t));
    if (!entry) return code;
    
    entry->code = code;
    entry->severity = severity;
    entry->category = uft_error_classify(code);
    entry->message = strdup_safe(message);
    entry->timestamp = get_timestamp_ms();
    
    entry->location.file = file;
    entry->location.function = func;
    entry->location.line = line;
    
    add_error_entry(ctx, entry);
    
    return code;
}

uft_error_t uft_error_reportf(uft_error_context_t *ctx, uft_error_t code,
                               uft_severity_t severity, const char *fmt, ...) {
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    
    return uft_error_report(ctx, code, severity, buf);
}

uft_error_t uft_error_reportf_loc(uft_error_context_t *ctx, uft_error_t code,
                                   uft_severity_t severity,
                                   const char *file, const char *func, int line,
                                   const char *fmt, ...) {
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    
    return uft_error_report_loc(ctx, code, severity, file, func, line, buf);
}

uft_error_t uft_error_report_full(uft_error_context_t *ctx, uft_error_t code,
                                   uft_severity_t severity, uft_error_category_t category,
                                   const char *message, const char *detail,
                                   const char *suggestion, uft_error_entry_t *cause) {
    if (!ctx) ctx = uft_error_context_get();
    if (!ctx) return code;
    
    if (severity < ctx->min_severity) return code;
    
    uft_error_entry_t *entry = calloc(1, sizeof(uft_error_entry_t));
    if (!entry) return code;
    
    entry->code = code;
    entry->severity = severity;
    entry->category = category;
    entry->message = strdup_safe(message);
    entry->detail = strdup_safe(detail);
    entry->suggestion = strdup_safe(suggestion);
    entry->cause = cause;
    entry->timestamp = get_timestamp_ms();
    
    add_error_entry(ctx, entry);
    
    return code;
}

uft_error_t uft_error_wrap(uft_error_context_t *ctx, uft_error_t code,
                            const char *message) {
    if (!ctx) ctx = uft_error_context_get();
    if (!ctx) return code;
    
    uft_error_entry_t *entry = calloc(1, sizeof(uft_error_entry_t));
    if (!entry) return code;
    
    entry->code = code;
    entry->severity = UFT_SEVERITY_ERROR;
    entry->category = uft_error_classify(code);
    entry->message = strdup_safe(message);
    entry->cause = (uft_error_entry_t *)ctx->last;  /* Link to previous */
    entry->timestamp = get_timestamp_ms();
    
    add_error_entry(ctx, entry);
    
    return code;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Error Query
 * ═══════════════════════════════════════════════════════════════════════════════ */

bool uft_error_has_errors(uft_error_context_t *ctx) {
    if (!ctx) ctx = uft_error_context_get();
    if (!ctx) return false;
    
    for (uft_error_entry_t *e = ctx->chain; e; e = e->next) {
        if (e->severity >= UFT_SEVERITY_ERROR) return true;
    }
    return false;
}

int uft_error_count(uft_error_context_t *ctx) {
    if (!ctx) ctx = uft_error_context_get();
    return ctx ? ctx->count : 0;
}

const uft_error_entry_t *uft_error_last(uft_error_context_t *ctx) {
    if (!ctx) ctx = uft_error_context_get();
    return ctx ? ctx->last : NULL;
}

const uft_error_entry_t *uft_error_chain(uft_error_context_t *ctx) {
    if (!ctx) ctx = uft_error_context_get();
    return ctx ? ctx->chain : NULL;
}

uft_error_t uft_error_code(uft_error_context_t *ctx) {
    const uft_error_entry_t *last = uft_error_last(ctx);
    return last ? last->code : UFT_OK;
}

const char *uft_error_message(uft_error_context_t *ctx) {
    const uft_error_entry_t *last = uft_error_last(ctx);
    return last && last->message ? last->message : "";
}

const uft_error_entry_t *uft_error_find_category(uft_error_context_t *ctx,
                                                   uft_error_category_t category) {
    if (!ctx) ctx = uft_error_context_get();
    if (!ctx) return NULL;
    
    for (uft_error_entry_t *e = ctx->chain; e; e = e->next) {
        if (e->category == category) return e;
    }
    return NULL;
}

const uft_error_entry_t *uft_error_find_severity(uft_error_context_t *ctx,
                                                   uft_severity_t min_severity) {
    if (!ctx) ctx = uft_error_context_get();
    if (!ctx) return NULL;
    
    for (uft_error_entry_t *e = ctx->chain; e; e = e->next) {
        if (e->severity >= min_severity) return e;
    }
    return NULL;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Callbacks
 * ═══════════════════════════════════════════════════════════════════════════════ */

void uft_error_set_callback(uft_error_context_t *ctx, uft_error_callback_t callback,
                             void *user_data) {
    (void)ctx;  /* Currently global only */
    global_callback = callback;
    global_callback_data = user_data;
}

void uft_error_remove_callback(uft_error_context_t *ctx) {
    (void)ctx;
    global_callback = NULL;
    global_callback_data = NULL;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Error Output
 * ═══════════════════════════════════════════════════════════════════════════════ */

void uft_error_print(uft_error_context_t *ctx) {
    if (!ctx) ctx = uft_error_context_get();
    if (!ctx || !ctx->chain) return;
    
    printf("Errors (%d):\n", ctx->count);
    
    for (uft_error_entry_t *e = ctx->chain; e; e = e->next) {
        printf("  [%s] %s: %s\n",
               uft_severity_name(e->severity),
               uft_error_code_name(e->code),
               e->message ? e->message : "(no message)");
    }
}

void uft_error_print_full(uft_error_context_t *ctx) {
    if (!ctx) ctx = uft_error_context_get();
    if (!ctx || !ctx->chain) return;
    
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("                        ERROR REPORT\n");
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("Total errors: %d\n\n", ctx->count);
    
    int i = 1;
    for (uft_error_entry_t *e = ctx->chain; e; e = e->next, i++) {
        printf("─── Error %d ───────────────────────────────────────────────────\n", i);
        printf("  Severity:  %s\n", uft_severity_name(e->severity));
        printf("  Category:  %s\n", uft_error_category_name(e->category));
        printf("  Code:      %s (%d)\n", uft_error_code_name(e->code), e->code);
        printf("  Message:   %s\n", e->message ? e->message : "(none)");
        
        if (e->detail) {
            printf("  Detail:    %s\n", e->detail);
        }
        if (e->suggestion) {
            printf("  Suggest:   %s\n", e->suggestion);
        }
        
        if (e->location.file) {
            printf("  Location:  %s:%d in %s()\n",
                   e->location.file, e->location.line,
                   e->location.function ? e->location.function : "?");
        }
        
        if (e->cause) {
            printf("  Caused by: %s\n",
                   e->cause->message ? e->cause->message : "(previous error)");
        }
        
        printf("\n");
    }
    
    printf("═══════════════════════════════════════════════════════════════\n");
}

char *uft_error_format(const uft_error_entry_t *entry) {
    if (!entry) return strdup("(no error)");
    
    char *str = malloc(512);
    if (!str) return NULL;
    
    snprintf(str, 512, "[%s] %s: %s",
             uft_severity_name(entry->severity),
             uft_error_code_name(entry->code),
             entry->message ? entry->message : "(no message)");
    
    return str;
}

char *uft_error_format_chain(uft_error_context_t *ctx) {
    if (!ctx) ctx = uft_error_context_get();
    if (!ctx || !ctx->chain) return strdup("(no errors)");
    
    size_t size = ctx->count * 256 + 256;
    char *str = malloc(size);
    if (!str) return NULL;
    
    int pos = 0;
    pos += snprintf(str + pos, size - pos, "Errors (%d):\n", ctx->count);
    
    for (uft_error_entry_t *e = ctx->chain; e && pos < (int)size - 128; e = e->next) {
        pos += snprintf(str + pos, size - pos, "  [%s] %s: %s\n",
                       uft_severity_name(e->severity),
                       uft_error_code_name(e->code),
                       e->message ? e->message : "(no message)");
    }
    
    return str;
}

char *uft_error_to_json(uft_error_context_t *ctx, bool pretty) {
    if (!ctx) ctx = uft_error_context_get();
    
    size_t size = 4096 + (ctx ? ctx->count * 512 : 0);
    char *json = malloc(size);
    if (!json) return NULL;
    
    const char *nl = pretty ? "\n" : "";
    const char *sp = pretty ? "  " : "";
    
    int pos = 0;
    pos += snprintf(json + pos, size - pos, "{%s", nl);
    pos += snprintf(json + pos, size - pos, "%s\"error_count\": %d,%s",
                   sp, ctx ? ctx->count : 0, nl);
    pos += snprintf(json + pos, size - pos, "%s\"errors\": [%s", sp, nl);
    
    if (ctx) {
        bool first = true;
        for (uft_error_entry_t *e = ctx->chain; e && pos < (int)size - 256; e = e->next) {
            if (!first) pos += snprintf(json + pos, size - pos, ",%s", nl);
            first = false;
            
            pos += snprintf(json + pos, size - pos,
                "%s%s{%s"
                "%s%s%s\"code\": %d,%s"
                "%s%s%s\"code_name\": \"%s\",%s"
                "%s%s%s\"severity\": \"%s\",%s"
                "%s%s%s\"category\": \"%s\",%s"
                "%s%s%s\"message\": \"%s\",%s"
                "%s%s%s\"timestamp\": %lu%s"
                "%s%s}",
                sp, sp, nl,
                sp, sp, sp, e->code, nl,
                sp, sp, sp, uft_error_code_name(e->code), nl,
                sp, sp, sp, uft_severity_name(e->severity), nl,
                sp, sp, sp, uft_error_category_name(e->category), nl,
                sp, sp, sp, e->message ? e->message : "", nl,
                sp, sp, sp, (unsigned long)e->timestamp, nl,
                sp, sp);
        }
    }
    
    pos += snprintf(json + pos, size - pos, "%s%s]%s}%s", nl, sp, nl, nl);
    
    return json;
}

uft_error_t uft_error_save_log(uft_error_context_t *ctx, const char *path) {
    if (!path) return UFT_ERR_INVALID_PARAM;
    
    char *json = uft_error_to_json(ctx, true);
    if (!json) return UFT_ERR_MEMORY;
    
    FILE *f = fopen(path, "w");
    if (!f) {
        return UFT_ERROR_IO;
    }
    if (!f) {
        free(json);
        return UFT_ERR_IO;
    }
    
    fputs(json, f);
    fclose(f);
    free(json);
    
    return UFT_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Utility Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

const char *uft_severity_name(uft_severity_t severity) {
    switch (severity) {
        case UFT_SEVERITY_DEBUG:   return "DEBUG";
        case UFT_SEVERITY_INFO:    return "INFO";
        case UFT_SEVERITY_WARNING: return "WARNING";
        case UFT_SEVERITY_ERROR:   return "ERROR";
        case UFT_SEVERITY_FATAL:   return "FATAL";
        default:                   return "UNKNOWN";
    }
}

const char *uft_error_category_name(uft_error_category_t category) {
    switch (category) {
        case UFT_ECAT_NONE:     return "NONE";
        case UFT_ECAT_IO:       return "IO";
        case UFT_ECAT_MEMORY:   return "MEMORY";
        case UFT_ECAT_FORMAT:   return "FORMAT";
        case UFT_ECAT_HARDWARE: return "HARDWARE";
        case UFT_ECAT_PARAM:    return "PARAM";
        case UFT_ECAT_STATE:    return "STATE";
        case UFT_ECAT_TIMEOUT:  return "TIMEOUT";
        case UFT_ECAT_PROTOCOL: return "PROTOCOL";
        case UFT_ECAT_CRC:      return "CRC";
        case UFT_ECAT_ENCODING: return "ENCODING";
        case UFT_ECAT_SYSTEM:   return "SYSTEM";
        case UFT_ECAT_USER:     return "USER";
        case UFT_ECAT_INTERNAL: return "INTERNAL";
        default:                return "UNKNOWN";
    }
}

const char *uft_error_code_name(uft_error_t code) {
    switch (code) {
        case UFT_OK:                return "OK";
        case UFT_ERR_MEMORY:        return "ERR_MEMORY";
        case UFT_ERR_IO:            return "ERR_IO";
        case UFT_ERR_INVALID_PARAM: return "ERR_INVALID_PARAM";
        case UFT_ERR_NOT_FOUND:     return "ERR_NOT_FOUND";
        case UFT_ERR_FORMAT:        return "ERR_FORMAT";
        case UFT_ERR_CRC:           return "ERR_CRC";
        case UFT_ERR_TIMEOUT:       return "ERR_TIMEOUT";
        case UFT_ERR_HARDWARE:      return "ERR_HARDWARE";
        case UFT_ERR_STATE:         return "ERR_STATE";
        case UFT_ERR_ABORTED:       return "ERR_ABORTED";
        case UFT_ERR_LIMIT:         return "ERR_LIMIT";
        case UFT_ERR_VERIFY:        return "ERR_VERIFY";
        case UFT_ERR_VALIDATION:    return "ERR_VALIDATION";
        case UFT_ERR_NO_BACKUP:     return "ERR_NO_BACKUP";
        case UFT_ERR_NO_DATA:       return "ERR_NO_DATA";
        case UFT_ERR_NOT_IMPLEMENTED: return "ERR_NOT_IMPLEMENTED";
        case UFT_ERR_SYSTEM:        return "ERR_SYSTEM";
        default:                    return "ERR_UNKNOWN";
    }
}

const char *uft_error_description(uft_error_t code) {
    switch (code) {
        case UFT_OK:                return "Operation completed successfully";
        case UFT_ERR_MEMORY:        return "Memory allocation failed";
        case UFT_ERR_IO:            return "Input/output error";
        case UFT_ERR_INVALID_PARAM: return "Invalid parameter";
        case UFT_ERR_NOT_FOUND:     return "Resource not found";
        case UFT_ERR_FORMAT:        return "Invalid format or corrupted data";
        case UFT_ERR_CRC:           return "CRC/checksum mismatch";
        case UFT_ERR_TIMEOUT:       return "Operation timed out";
        case UFT_ERR_HARDWARE:      return "Hardware communication error";
        case UFT_ERR_STATE:         return "Invalid state for operation";
        case UFT_ERR_ABORTED:       return "Operation aborted by user";
        case UFT_ERR_LIMIT:         return "Limit exceeded";
        case UFT_ERR_VERIFY:        return "Verification failed";
        case UFT_ERR_VALIDATION:    return "Validation failed";
        case UFT_ERR_NO_BACKUP:     return "No backup available";
        case UFT_ERR_NO_DATA:       return "No data available";
        case UFT_ERR_NOT_IMPLEMENTED: return "Feature not implemented";
        case UFT_ERR_SYSTEM:        return "System error";
        default:                    return "Unknown error";
    }
}

uft_error_category_t uft_error_classify(uft_error_t code) {
    switch (code) {
        case UFT_OK:                return UFT_ECAT_NONE;
        case UFT_ERR_MEMORY:        return UFT_ECAT_MEMORY;
        case UFT_ERR_IO:            return UFT_ECAT_IO;
        case UFT_ERR_INVALID_PARAM: return UFT_ECAT_PARAM;
        case UFT_ERR_NOT_FOUND:     return UFT_ECAT_IO;
        case UFT_ERR_FORMAT:        return UFT_ECAT_FORMAT;
        case UFT_ERR_CRC:           return UFT_ECAT_CRC;
        case UFT_ERR_TIMEOUT:       return UFT_ECAT_TIMEOUT;
        case UFT_ERR_HARDWARE:      return UFT_ECAT_HARDWARE;
        case UFT_ERR_STATE:         return UFT_ECAT_STATE;
        case UFT_ERR_ABORTED:       return UFT_ECAT_USER;
        case UFT_ERR_SYSTEM:        return UFT_ECAT_SYSTEM;
        default:                    return UFT_ECAT_INTERNAL;
    }
}

uft_error_t uft_error_from_errno(int errno_val) {
    switch (errno_val) {
        case 0:      return UFT_OK;
        case ENOMEM: return UFT_ERR_MEMORY;
        case ENOENT: return UFT_ERR_NOT_FOUND;
        case EACCES:
        case EPERM:  return UFT_ERR_IO;
        case EINVAL: return UFT_ERR_INVALID_PARAM;
        case EBUSY:  return UFT_ERR_STATE;
        case ETIMEDOUT: return UFT_ERR_TIMEOUT;
        default:     return UFT_ERR_SYSTEM;
    }
}

uft_error_t uft_error_from_errno_msg(int errno_val, char *msg_out, size_t msg_size) {
    if (msg_out && msg_size > 0) {
#ifdef _WIN32
        strerror_s(msg_out, msg_size, errno_val);
#else
        strerror_r(errno_val, msg_out, msg_size);
#endif
    }
    return uft_error_from_errno(errno_val);
}
