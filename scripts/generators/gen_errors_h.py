#!/usr/bin/env python3
"""Generate include/uft/uft_error.h from data/errors.tsv.

Deterministic: same input bytes produce same output bytes (no timestamps,
no dict iteration ordering, rows emitted in file order).

Usage:
    python3 scripts/generators/gen_errors_h.py data/errors.tsv > uft_error.h

The output is the FULL canonical header including the enum, helper
macros, the uft_error_ctx_t struct, and the uft_strerror() prototype.
The rest of the runtime surface (context setters, inline helpers, etc.)
is carried over unchanged from the pre-SSOT header so existing callers
compile without modification.
"""
from __future__ import annotations

import sys
from pathlib import Path

# Import helper from sibling module when invoked directly.
sys.path.insert(0, str(Path(__file__).resolve().parent))
from gen_errors_common import HEADER_BANNER, parse  # noqa: E402


TEMPLATE_HEAD = """#ifndef UFT_ERROR_H
#define UFT_ERROR_H

/**
 * @file uft_error.h
 * @brief Unified error handling for UnifiedFloppyTool (GENERATED).
 *
 * This header is generated from data/errors.tsv by
 * scripts/generators/gen_errors_h.py. Edit the TSV, not this file.
 *
 * The legacy UFT_ERROR_* mixed-case spelling and short-form aliases
 * (UFT_ERR_NOMEM, UFT_ERR_NOT_IMPL, ...) are provided by the sister
 * header include/uft/core/uft_error_compat_gen.h (also generated).
 */

#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef UFT_ERROR_ENUM_DEFINED
#define UFT_ERROR_ENUM_DEFINED
typedef enum {
"""

TEMPLATE_TAIL_ENUM = """} uft_rc_t;

#ifndef UFT_ERROR_T_DEFINED
#define UFT_ERROR_T_DEFINED
typedef uft_rc_t uft_error_t;
#endif
#else
/* Enum already defined by another header — provide opaque int typedef. */
typedef int uft_rc_t;
#ifndef UFT_ERROR_T_DEFINED
#define UFT_ERROR_T_DEFINED
typedef int uft_error_t;
#endif
#ifndef UFT_SUCCESS
#define UFT_SUCCESS 0
#endif
#endif /* UFT_ERROR_ENUM_DEFINED */

#ifndef UFT_OK
#define UFT_OK UFT_SUCCESS
#endif

/* Error context (runtime, not generated from TSV) ------------------------ */
typedef struct uft_error_ctx {
    uft_rc_t    code;
    int         sys_errno;
    const char* file;
    int         line;
    char        message[256];
    const char* function;
    const char* extra;
} uft_error_ctx_t;
typedef uft_error_ctx_t uft_error_context_t;

typedef struct uft_error_info {
    uft_rc_t    code;
    const char* name;
    const char* message;
    const char* category;
} uft_error_info_t;

/* Lookup ------------------------------------------------------------------ */
const char* uft_strerror(uft_rc_t rc);
#define uft_error_string(rc) uft_strerror(rc)

#define UFT_FAILED(rc)    ((rc) < 0)
#define UFT_SUCCEEDED(rc) ((rc) >= 0)

static inline bool uft_success(uft_rc_t rc) { return rc == UFT_SUCCESS; }
static inline bool uft_failed(uft_rc_t rc)  { return rc != UFT_SUCCESS; }

void uft_error_set_context(const char* file, int line,
                           const char* function, const char* message);
const uft_error_context_t* uft_error_get_context(void);
void uft_error_clear_context(void);

#define UFT_CHECK_NULL(ptr) \\
    do { if (!(ptr)) return UFT_ERR_INVALID_ARG; } while (0)

#define UFT_CHECK_NULLS(...) \\
    do { \\
        void* ptrs[] = { __VA_ARGS__ }; \\
        for (size_t i = 0; i < sizeof(ptrs)/sizeof(ptrs[0]); i++) { \\
            if (!ptrs[i]) return UFT_ERR_INVALID_ARG; \\
        } \\
    } while (0)

#define UFT_PROPAGATE(expr) \\
    do { \\
        uft_rc_t _rc = (expr); \\
        if (uft_failed(_rc)) return _rc; \\
    } while (0)

#define UFT_SET_ERROR(err_ctx, err_code, msg, ...) \\
    do { \\
        (err_ctx).code = (err_code); \\
        (err_ctx).file = __FILE__; \\
        (err_ctx).line = __LINE__; \\
        snprintf((err_ctx).message, sizeof((err_ctx).message), \\
                 (msg), ##__VA_ARGS__); \\
    } while (0)

#define UFT_ERROR_NULL_POINTER UFT_ERR_INVALID_ARG

#ifdef __cplusplus
}
#endif

#endif /* UFT_ERROR_H */
"""


def main(argv: list[str]) -> int:
    if len(argv) != 2:
        sys.stderr.write("usage: gen_errors_h.py <path-to-errors.tsv>\n")
        return 2
    rows = parse(Path(argv[1]))

    out = sys.stdout
    out.write(HEADER_BANNER)
    out.write(TEMPLATE_HEAD)

    # Emit enum members in file order. Comments on proposed rows get a
    # marker so they're visible in the generated header.
    last_category: str | None = None
    for r in rows:
        if r.category != last_category:
            out.write(f"\n    /* --- {r.category} --- */\n")
            last_category = r.category
        marker = " /* PROPOSED — pending human review */" if r.proposed else ""
        # Success is named UFT_SUCCESS, not UFT_ERR_SUCCESS.
        ident = "UFT_SUCCESS" if r.name == "SUCCESS" else f"UFT_ERR_{r.name}"
        out.write(f"    {ident} = {r.code}, /**< {r.description} */{marker}\n")

    out.write(TEMPLATE_TAIL_ENUM)

    # Proposed rows → #pragma messages so every build log reminds the
    # maintainer that unreviewed decisions are live.
    proposed = [r for r in rows if r.proposed]
    if proposed:
        sys.stderr.write(
            f"gen_errors_h: {len(proposed)} PROPOSED rows in errors.tsv "
            f"(visible in build log via #pragma message in compat header).\n"
        )
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
