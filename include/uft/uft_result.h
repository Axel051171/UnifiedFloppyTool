/**
 * @file uft_result.h
 * @brief Unified Result Object for all parsers and operations
 * 
 * P2-001: Einheitliches Result-Object statt verschiedener Return-Types
 */

#ifndef UFT_RESULT_H
#define UFT_RESULT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Result Codes
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef enum {
    /* Success */
    UFT_RESULT_OK = 0,
    UFT_RESULT_PARTIAL = 1,         /* Partial success */
    UFT_RESULT_RECOVERED = 2,       /* Success with recovery */
    
    /* Errors */
    UFT_RESULT_ERROR = -1,          /* Generic error */
    UFT_RESULT_NOT_FOUND = -2,      /* Resource not found */
    UFT_RESULT_INVALID = -3,        /* Invalid parameter */
    UFT_RESULT_FORMAT = -4,         /* Format error */
    UFT_RESULT_IO = -5,             /* I/O error */
    UFT_RESULT_MEMORY = -6,         /* Memory allocation failed */
    UFT_RESULT_TIMEOUT = -7,        /* Operation timeout */
    UFT_RESULT_ABORT = -8,          /* User abort */
    UFT_RESULT_UNSUPPORTED = -9,    /* Unsupported operation */
    UFT_RESULT_CRC = -10,           /* CRC error */
    UFT_RESULT_SYNC = -11,          /* Sync pattern error */
    UFT_RESULT_PROTECTION = -12,    /* Copy protection detected */
} uft_result_code_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Unified Result Structure
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    /* Status */
    uft_result_code_t   code;
    bool                success;        /* Quick check: code >= 0 */
    
    /* Details */
    char                message[256];
    char                source[64];     /* Which module generated this */
    int                 line;           /* Source line (debug) */
    
    /* Statistics */
    int                 items_total;    /* Total items attempted */
    int                 items_ok;       /* Items succeeded */
    int                 items_failed;   /* Items failed */
    int                 items_skipped;  /* Items skipped */
    
    /* Quality Metrics */
    double              confidence;     /* 0.0 - 1.0 */
    double              quality_score;  /* 0 - 100 */
    int                 error_count;    /* Number of errors encountered */
    int                 warning_count;  /* Number of warnings */
    
    /* Timing */
    double              elapsed_ms;
    
    /* Optional payload */
    void                *data;          /* Caller-owned data */
    size_t              data_size;
    
    /* Linked errors */
    struct uft_result   *inner;         /* Chain for nested errors */
} uft_result_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Result Macros
 * ═══════════════════════════════════════════════════════════════════════════════ */

#define UFT_RESULT_INIT { \
    .code = UFT_RESULT_OK, \
    .success = true, \
    .message = {0}, \
    .source = {0}, \
    .line = 0, \
    .items_total = 0, \
    .items_ok = 0, \
    .items_failed = 0, \
    .items_skipped = 0, \
    .confidence = 1.0, \
    .quality_score = 100.0, \
    .error_count = 0, \
    .warning_count = 0, \
    .elapsed_ms = 0, \
    .data = NULL, \
    .data_size = 0, \
    .inner = NULL \
}

#define UFT_RESULT_SUCCESS(r) ((r)->code >= 0)
#define UFT_RESULT_FAILED(r)  ((r)->code < 0)

/* Create result with source info */
#define UFT_RESULT_OK_MSG(r, msg) do { \
    (r)->code = UFT_RESULT_OK; \
    (r)->success = true; \
    snprintf((r)->message, sizeof((r)->message), "%s", msg); \
    snprintf((r)->source, sizeof((r)->source), "%s", __func__); \
    (r)->line = __LINE__; \
} while(0)

#define UFT_RESULT_ERROR_MSG(r, c, msg) do { \
    (r)->code = (c); \
    (r)->success = false; \
    snprintf((r)->message, sizeof((r)->message), "%s", msg); \
    snprintf((r)->source, sizeof((r)->source), "%s", __func__); \
    (r)->line = __LINE__; \
} while(0)

/* ═══════════════════════════════════════════════════════════════════════════════
 * API Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Initialize result to default (success)
 */
static inline void uft_result_init(uft_result_t *r) {
    if (r) {
        *r = (uft_result_t)UFT_RESULT_INIT;
    }
}

/**
 * @brief Set result to success
 */
static inline void uft_result_ok(uft_result_t *r, const char *msg) {
    if (r) {
        r->code = UFT_RESULT_OK;
        r->success = true;
        if (msg) {
            snprintf(r->message, sizeof(r->message), "%s", msg);
        }
    }
}

/**
 * @brief Set result to error
 */
static inline void uft_result_error(uft_result_t *r, uft_result_code_t code, 
                                    const char *msg) {
    if (r) {
        r->code = code;
        r->success = false;
        if (msg) {
            snprintf(r->message, sizeof(r->message), "%s", msg);
        }
    }
}

/**
 * @brief Get code name
 */
static inline const char* uft_result_code_name(uft_result_code_t code) {
    switch (code) {
        case UFT_RESULT_OK: return "OK";
        case UFT_RESULT_PARTIAL: return "Partial";
        case UFT_RESULT_RECOVERED: return "Recovered";
        case UFT_RESULT_ERROR: return "Error";
        case UFT_RESULT_NOT_FOUND: return "Not Found";
        case UFT_RESULT_INVALID: return "Invalid";
        case UFT_RESULT_FORMAT: return "Format Error";
        case UFT_RESULT_IO: return "I/O Error";
        case UFT_RESULT_MEMORY: return "Memory Error";
        case UFT_RESULT_TIMEOUT: return "Timeout";
        case UFT_RESULT_ABORT: return "Aborted";
        case UFT_RESULT_UNSUPPORTED: return "Unsupported";
        case UFT_RESULT_CRC: return "CRC Error";
        case UFT_RESULT_SYNC: return "Sync Error";
        case UFT_RESULT_PROTECTION: return "Protection";
        default: return "Unknown";
    }
}

/**
 * @brief Print result summary
 */
static inline void uft_result_print(const uft_result_t *r) {
    if (!r) return;
    printf("Result: %s (%d)\n", uft_result_code_name(r->code), r->code);
    if (r->message[0]) printf("  Message: %s\n", r->message);
    if (r->source[0]) printf("  Source: %s:%d\n", r->source, r->line);
    if (r->items_total > 0) {
        printf("  Items: %d/%d OK, %d failed, %d skipped\n",
               r->items_ok, r->items_total, r->items_failed, r->items_skipped);
    }
    printf("  Quality: %.1f%%, Confidence: %.2f\n", r->quality_score, r->confidence);
}

/**
 * @brief Convert result to JSON string (caller must free)
 */
char* uft_result_to_json(const uft_result_t *r);

#ifdef __cplusplus
}
#endif

#endif /* UFT_RESULT_H */
