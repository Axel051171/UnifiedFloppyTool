/**
 * @file uft_error_ext.h
 * @brief Error-handling runtime surface (non-enum, hand-maintained).
 *
 * CAUTION — this header is paired with the GENERATED uft_error.h:
 *
 *   include/uft/uft_error.h            (GENERATED from data/errors.tsv)
 *   include/uft/core/uft_error_ext.h   (this file, hand-maintained)
 *   include/uft/core/uft_error_compat_gen.h  (GENERATED legacy aliases)
 *
 * The generated uft_error.h `#include`s this file so call sites that only
 * pull in <uft/uft_error.h> still get the full runtime surface. Do NOT
 * merge this content back into uft_error.h — the split is load-bearing:
 * it is why the SSOT generator can recreate uft_error.h byte-for-byte
 * from errors.tsv alone.
 *
 * Contains:
 *   - typedef struct uft_error_ctx  (extended error context)
 *   - typedef struct uft_error_info (lookup-table row shape)
 *   - const char* uft_strerror(uft_rc_t);
 *   - uft_error_set_context / _get_context / _clear_context prototypes
 *   - UFT_CHECK_NULL / UFT_CHECK_NULLS / UFT_PROPAGATE / UFT_SET_ERROR
 *   - static inline uft_success / uft_failed helpers
 *   - UFT_ERROR_NULL_POINTER back-compat alias
 *
 * If you need to change any of these, edit THIS file. Regenerate the
 * other two via `make generate` (or scripts/verify_errors_ssot.sh).
 */

#ifndef UFT_ERROR_EXT_H
#define UFT_ERROR_EXT_H

#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

/* The generated uft_error.h already provides the enum + UFT_SUCCESS /
 * UFT_OK sentinels. Including it here is a no-op guard against misuse:
 * if a caller pulls uft_error_ext.h directly without the sibling, they
 * still get the enum typedef. */
#include "uft/uft_error.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------------ */
/* Error context (runtime, not generated from TSV)                          */
/* ------------------------------------------------------------------------ */
#ifndef UFT_ERROR_CTX_DEFINED
#define UFT_ERROR_CTX_DEFINED
typedef struct uft_error_ctx {
    uft_rc_t    code;          /**< Primary error code */
    int         sys_errno;     /**< System errno if applicable (0 if not) */
    const char* file;          /**< File where error occurred */
    int         line;          /**< Line where error occurred */
    char        message[256];  /**< Human-readable error message */
    const char* function;      /**< Function name where error occurred */
    const char* extra;         /**< Extra context (optional) */
} uft_error_ctx_t;

typedef uft_error_ctx_t uft_error_context_t;
#endif /* UFT_ERROR_CTX_DEFINED */

#ifndef UFT_ERROR_INFO_DEFINED
#define UFT_ERROR_INFO_DEFINED
typedef struct uft_error_info {
    uft_rc_t    code;       /**< Error code */
    const char* name;       /**< Error name string */
    const char* message;    /**< Error description */
    const char* category;   /**< Error category */
} uft_error_info_t;
#endif /* UFT_ERROR_INFO_DEFINED */

/* ------------------------------------------------------------------------ */
/* Lookup                                                                   */
/* ------------------------------------------------------------------------ */
const char* uft_strerror(uft_rc_t rc);

#ifndef uft_error_string
#define uft_error_string(rc) uft_strerror(rc)
#endif

/* ------------------------------------------------------------------------ */
/* Predicates                                                               */
/* ------------------------------------------------------------------------ */
#ifndef UFT_FAILED
#define UFT_FAILED(rc)    ((rc) < 0)
#endif
#ifndef UFT_SUCCEEDED
#define UFT_SUCCEEDED(rc) ((rc) >= 0)
#endif

static inline bool uft_success(uft_rc_t rc) { return rc == UFT_SUCCESS; }
static inline bool uft_failed(uft_rc_t rc)  { return rc != UFT_SUCCESS; }

/* ------------------------------------------------------------------------ */
/* Thread-local context setters                                             */
/* ------------------------------------------------------------------------ */
void uft_error_set_context(const char* file, int line,
                           const char* function, const char* message);
const uft_error_context_t* uft_error_get_context(void);
void uft_error_clear_context(void);

/* ------------------------------------------------------------------------ */
/* Helper macros                                                            */
/* ------------------------------------------------------------------------ */
#ifndef UFT_CHECK_NULL
#define UFT_CHECK_NULL(ptr) \
    do { if (!(ptr)) return UFT_ERR_INVALID_ARG; } while (0)
#endif

#ifndef UFT_CHECK_NULLS
#define UFT_CHECK_NULLS(...) \
    do { \
        void* ptrs[] = { __VA_ARGS__ }; \
        for (size_t i = 0; i < sizeof(ptrs)/sizeof(ptrs[0]); i++) { \
            if (!ptrs[i]) return UFT_ERR_INVALID_ARG; \
        } \
    } while (0)
#endif

#ifndef UFT_PROPAGATE
#define UFT_PROPAGATE(expr) \
    do { \
        uft_rc_t _rc = (expr); \
        if (uft_failed(_rc)) return _rc; \
    } while (0)
#endif

#ifndef UFT_SET_ERROR
#define UFT_SET_ERROR(err_ctx, err_code, msg, ...) \
    do { \
        (err_ctx).code = (err_code); \
        (err_ctx).file = __FILE__; \
        (err_ctx).line = __LINE__; \
        snprintf((err_ctx).message, sizeof((err_ctx).message), \
                 (msg), ##__VA_ARGS__); \
    } while (0)
#endif

/* Legacy NULL-pointer alias retained for 2 doc-comment call sites. */
#ifndef UFT_ERROR_NULL_POINTER
#define UFT_ERROR_NULL_POINTER UFT_ERR_INVALID_ARG
#endif

#ifdef __cplusplus
}
#endif

/* Pull in the full generated legacy-alias surface so every consumer of
 * <uft/uft_error.h> (which transitively #includes this file) gets the
 * UFT_ERROR_*, UFT_E_*, UFT_IR_ERR_*, UFT_DEC_ERR_*, UFT_IO_ERR_*
 * compatibility spellings without a second include. Must come AFTER
 * the hand-maintained section above so compat_gen's `#ifndef` guards
 * see any human-added redefinitions first. */
#include "uft/core/uft_error_compat_gen.h"

#endif /* UFT_ERROR_EXT_H */
