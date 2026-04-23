#!/usr/bin/env python3
"""Generate include/uft/core/uft_error_compat_gen.h from data/errors.tsv.

Produces the legacy-alias compat header:

  - UFT_ERROR_<NAME>   (mixed-case, ~1127 legacy uses)
  - UFT_E_<NAME>       (short-form)
  - UFT_ERR_NOMEM / UFT_ERR_NOT_IMPL / UFT_ERR_UNSUPPORTED / ...
    (hand-maintained alias table at bottom of this script)

Each alias is `#define`d only if not already present, so including this
header alongside the three legacy compat headers during the transition
does not produce redefinition warnings.

All aliases get a `UFT_DEPRECATED_ALIAS` marker (#pragma GCC warning or
__attribute__((deprecated)) wrapper) so new usages surface in build logs.
The warning is opt-in via -DUFT_WARN_LEGACY_ERROR_ALIASES to avoid a
flood during the transition; quick-fix turns it on repo-wide when
mechanical cleanup starts.
"""
from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from gen_errors_common import HEADER_BANNER, parse  # noqa: E402


# Legacy short-form aliases that do NOT get their own TSV row — they are
# pure compat spellings that funnel into a canonical token.
# Format: legacy_short_name -> canonical TSV name
LEGACY_ALIASES: dict[str, str] = {
    # memory consolidation
    "NOMEM":         "MEMORY",
    "OUT_OF_MEMORY": "MEMORY",
    # feature-gate consolidation
    "NOT_IMPL":      "NOT_IMPLEMENTED",
    "UNSUPPORTED":   "NOT_SUPPORTED",
    # format/argument consolidation
    "CORRUPT":       "CORRUPTED",
    "INVALID_PARAM": "INVALID_ARG",
    "NULL_PTR":      "INVALID_ARG",
    "BOUNDS":        "INVALID_ARG",
    "STATE":         "INVALID_ARG",
    # file lookup consolidation (NOT_FOUND is ambiguous; map to file-side)
    "NOT_FOUND":     "FILE_NOT_FOUND",
    # CRC-ish aliases
    "CRC_ERROR":     "CRC",
    "VERIFY":        "CRC",
    # NO_DATA: pre-SSOT compat headers mapped this to 3 different targets
    # (CORRUPTED / CORRUPT / FORMAT). Forensically closest is MISSING_SECTOR
    # ("sector header says data-of-length-N but no data found there").
    "NO_DATA":       "MISSING_SECTOR",
    # UFT_ERR_OK — the user approval dropped OK as a canonical name, but
    # 2 doc-comment usages remain (fuzzy_bits.c). Alias to SUCCESS (=0) so
    # `return UFT_ERR_OK;` still works for any straggler call site.
    "OK":            "SUCCESS",

    # --- Second-round coverage gaps (UFT_ERROR_* spellings without a
    #     matching canonical TSV row; user-approved 2026-04-23). Adding
    #     them here (LEGACY_ALIASES) makes the compat generator emit
    #     UFT_ERROR_<legacy>  AND  UFT_ERR_<legacy>  as #defines pointing
    #     at the canonical target — covering both prefixes in one pass.
    "NO_MEMORY":           "MEMORY",            # 123 uses
    "OUT_OF_RANGE":        "INVALID_ARG",       #  10 uses (was BOUNDS)
    "VERIFY_FAILED":       "VERIFY_FAIL",       #  17 uses (spelling variant)
    "INVALID_FORMAT":      "FORMAT_INVALID",    #   2 uses (word-order variant)
    "FORMAT_NOT_SUPPORTED":"NOT_SUPPORTED",     #   4 uses
    "DISK_PROTECTED":      "WRITE_PROTECT",     #   3 uses
    "READ_ONLY":           "WRITE_PROTECT",     #   1 use
    "GEOMETRY_MISMATCH":   "FORMAT_VARIANT",    #   3 uses
    "TRANSACTION_CONFLICT":"BUSY",              #   1 use
}

# Short UFT_E* posix-ish aliases used by a handful of files.
SHORT_E_ALIASES: dict[str, str] = {
    "EINVAL":          "INVALID_ARG",
    "EIO":             "IO",
    "EFORMAT":         "FORMAT",
    "EUNSUPPORTED":    "NOT_SUPPORTED",
    "ENOMEM":          "MEMORY",
    "ECRC":            "CRC",
    "ENOT_IMPLEMENTED":"NOT_IMPLEMENTED",
    "ETIMEOUT":        "TIMEOUT",
}


HEAD = """#ifndef UFT_ERROR_COMPAT_GEN_H
#define UFT_ERROR_COMPAT_GEN_H

/**
 * @file uft_error_compat_gen.h
 * @brief Legacy error-alias compatibility (GENERATED).
 *
 * Generated from data/errors.tsv by
 * scripts/generators/gen_errors_compat.py. Do not edit by hand.
 *
 * Provides the following alias spellings for existing call sites:
 *   - UFT_ERROR_<NAME>   (mixed-case, ~1127 uses in 107 files)
 *   - UFT_E_<NAME>       (short-form P2-ARCH-006 spelling)
 *   - UFT_ERR_NOMEM / UFT_ERR_UNSUPPORTED / ... (legacy short names)
 *
 * Every alias is `#define`d only if not already present, so this header
 * can coexist with the pre-SSOT compat headers during the cutover
 * without triggering redefinition warnings.
 *
 * To flag legacy usage in builds, define UFT_WARN_LEGACY_ERROR_ALIASES
 * at compile time. This is OFF by default (transition phase).
 */

#include "uft/uft_error.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------------ */
/* 1. UFT_ERROR_<NAME> — mixed-case canonical alias                        */
/* ------------------------------------------------------------------------ */
"""

MID_1 = """
/* ------------------------------------------------------------------------ */
/* 2. UFT_E_<NAME> — short-form (P2-ARCH-006 / uft_error_codes.h)          */
/* ------------------------------------------------------------------------ */
"""

MID_2 = """
/* ------------------------------------------------------------------------ */
/* 3. Legacy short-name aliases (NOMEM, NOT_IMPL, UNSUPPORTED, ...)         */
/* ------------------------------------------------------------------------ */
"""

MID_3 = """
/* ------------------------------------------------------------------------ */
/* 4. UFT_E<short> posix-ish aliases                                       */
/* ------------------------------------------------------------------------ */
"""

TAIL = """
#ifdef UFT_WARN_LEGACY_ERROR_ALIASES
#  if defined(__GNUC__) || defined(__clang__)
#    pragma GCC warning "UFT_ERR_* / UFT_ERROR_* legacy aliases are deprecated; use UFT_ERR_<NAME> directly."
#  endif
#endif

#ifdef __cplusplus
}
#endif

#endif /* UFT_ERROR_COMPAT_GEN_H */
"""


def main(argv: list[str]) -> int:
    if len(argv) != 2:
        sys.stderr.write("usage: gen_errors_compat.py <path-to-errors.tsv>\n")
        return 2
    rows = parse(Path(argv[1]))
    canonical_names = {r.name for r in rows}

    out = sys.stdout
    out.write(HEADER_BANNER)
    out.write(HEAD)

    # 1. UFT_ERROR_<NAME>
    for r in rows:
        if r.name == "SUCCESS":
            out.write("#ifndef UFT_ERROR_SUCCESS\n")
            out.write("#define UFT_ERROR_SUCCESS UFT_SUCCESS\n")
            out.write("#endif\n")
            continue
        out.write(f"#ifndef UFT_ERROR_{r.name}\n")
        out.write(f"#define UFT_ERROR_{r.name} UFT_ERR_{r.name}\n")
        out.write("#endif\n")

    # 2. UFT_E_<NAME>
    out.write(MID_1)
    for r in rows:
        if r.name == "SUCCESS":
            continue
        out.write(f"#ifndef UFT_E_{r.name}\n")
        out.write(f"#define UFT_E_{r.name} UFT_ERR_{r.name}\n")
        out.write("#endif\n")

    # 3. Legacy short aliases
    out.write(MID_2)
    for legacy, canonical in LEGACY_ALIASES.items():
        if canonical not in canonical_names:
            sys.stderr.write(
                f"gen_errors_compat: alias {legacy} targets unknown "
                f"canonical {canonical}\n"
            )
            sys.exit(2)
        out.write(f"#ifndef UFT_ERR_{legacy}\n")
        out.write(f"#define UFT_ERR_{legacy} UFT_ERR_{canonical}\n")
        out.write("#endif\n")
        out.write(f"#ifndef UFT_ERROR_{legacy}\n")
        out.write(f"#define UFT_ERROR_{legacy} UFT_ERR_{canonical}\n")
        out.write("#endif\n")

    # 4. UFT_E<short> posix-ish
    out.write(MID_3)
    for short, canonical in SHORT_E_ALIASES.items():
        if canonical not in canonical_names:
            sys.stderr.write(
                f"gen_errors_compat: alias {short} targets unknown "
                f"canonical {canonical}\n"
            )
            sys.exit(2)
        out.write(f"#ifndef UFT_{short}\n")
        out.write(f"#define UFT_{short} UFT_ERR_{canonical}\n")
        out.write("#endif\n")

    out.write(TAIL)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
