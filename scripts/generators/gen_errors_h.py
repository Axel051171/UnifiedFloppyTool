#!/usr/bin/env python3
"""Generate include/uft/uft_error.h from data/errors.tsv.

Deterministic: same input bytes produce same output bytes (no timestamps,
no dict iteration ordering, rows emitted in file order).

Usage:
    python3 scripts/generators/gen_errors_h.py data/errors.tsv > uft_error.h

PHASE 4a STEP 3 decision — "option B" (separate hand-written tail header):
--------------------------------------------------------------------------
The pre-SSOT uft_error.h carries three kinds of content:

  (a) the uft_rc_t enum     — derived from data/errors.tsv (generated).
  (b) legacy alias #defines — emitted into uft_error_compat_gen.h
                               (generated from errors_legacy_aliases.tsv).
  (c) runtime surface       — uft_error_ctx_t struct, uft_strerror proto,
                               UFT_CHECK_NULL / UFT_PROPAGATE / UFT_SET_ERROR
                               macros, static inline uft_success / uft_failed
                               helpers, thread-local context setters.

(c) is orthogonal to the TSV and evolves independently (new macros, new
struct fields would normally land in uft_error.h directly). Two strategies
were considered:

  A) Carry (c) as a literal "template tail" string appended by this
     generator — simple, but conflates generated and hand-written concerns
     in one file; the tail would have to be edited inside a Python string
     literal, harming reviewability.

  B) Move (c) into a separate hand-written header
     include/uft/core/uft_error_ext.h and have the generated uft_error.h
     `#include` it. Clean separation: (a) is 100% generator output, (c)
     is 100% human-edited; no mixed-origin file. Extra include layer but
     only one — every consumer that pulls <uft/uft_error.h> still gets
     the full surface transitively.

Chosen: OPTION B. Rationale above. The generated uft_error.h is now a
thin shell that emits the enum and forwards to uft_error_ext.h. Any
change to struct uft_error_ctx / helper macros / function prototypes
lands in uft_error_ext.h directly (no regeneration needed).
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
 * The runtime surface (struct uft_error_ctx, uft_strerror, helper
 * macros) lives in the hand-maintained sibling header
 * include/uft/core/uft_error_ext.h which this file `#include`s at
 * the bottom. Legacy alias spellings (UFT_ERROR_*, UFT_E_*, etc.)
 * are provided by the generated header
 * include/uft/core/uft_error_compat_gen.h.
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

#ifdef __cplusplus
}
#endif

/* Runtime surface (struct uft_error_ctx, uft_strerror prototype, helper
 * macros, inline predicates) lives in the hand-maintained sibling header.
 * Pulled in last so every consumer of <uft/uft_error.h> transitively gets
 * the full API without a second include. */
#include "uft/core/uft_error_ext.h"

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

    # Proposed rows → stderr note so every build log reminds the
    # maintainer that unreviewed decisions are live.
    proposed = [r for r in rows if r.proposed]
    if proposed:
        sys.stderr.write(
            f"gen_errors_h: {len(proposed)} PROPOSED rows in errors.tsv "
            f"(visible in generated enum via /* PROPOSED */ marker).\n"
        )
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
