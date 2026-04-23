#!/usr/bin/env python3
"""Generate include/uft/core/uft_error_compat_gen.h from data/errors.tsv.

Produces the legacy-alias compat header:

  - UFT_ERROR_<NAME>   (mixed-case; canonical rows → UFT_ERR_<NAME>)
  - UFT_E_<NAME>       (short-form; canonical rows)
  - Every legacy alias catalogued in data/errors_legacy_aliases.tsv
    (80–120 symbols from the pre-SSOT compat sources).

Each alias is `#define`d only if not already present, so including this
header alongside the three legacy compat headers during the transition
does not produce redefinition warnings.

All aliases get a `UFT_DEPRECATED_ALIAS` marker (#pragma GCC warning or
__attribute__((deprecated)) wrapper) so new usages surface in build logs.
The warning is opt-in via -DUFT_WARN_LEGACY_ERROR_ALIASES to avoid a
flood during the transition; quick-fix turns it on repo-wide when
mechanical cleanup starts.

Inputs:
    argv[1] — path to data/errors.tsv (canonical enum)
    argv[2] — path to data/errors_legacy_aliases.tsv (legacy aliases);
              optional for backward-compat — if omitted, only the
              hard-coded minimum alias set is emitted.
"""
from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from gen_errors_common import HEADER_BANNER, parse  # noqa: E402


def parse_legacy_aliases(path: Path) -> list[tuple[str, str, str]]:
    """Parse errors_legacy_aliases.tsv.

    Returns list of (symbol_name, source_file, proposed_canonical) tuples
    in file order. Comment lines (starting with '#') and blank lines are
    skipped. Malformed rows raise SystemExit(2).
    """
    if not path.is_file():
        sys.stderr.write(f"gen_errors_compat: legacy TSV not found: {path}\n")
        sys.exit(2)

    out: list[tuple[str, str, str]] = []
    seen: set[str] = set()
    with path.open("r", encoding="utf-8", newline="") as f:
        for lineno, raw in enumerate(f, 1):
            line = raw.rstrip("\r\n")
            stripped = line.strip()
            if not stripped or stripped.startswith("#"):
                continue
            parts = line.split("\t")
            if len(parts) != 4:
                sys.stderr.write(
                    f"gen_errors_compat: {path}:{lineno}: "
                    f"expected 4 tab-separated fields, got {len(parts)}\n"
                )
                sys.exit(2)
            symbol, _current, source, canonical = (p.strip() for p in parts)
            if symbol in seen:
                sys.stderr.write(
                    f"gen_errors_compat: {path}:{lineno}: "
                    f"duplicate legacy symbol {symbol}\n"
                )
                sys.exit(2)
            seen.add(symbol)
            out.append((symbol, source, canonical))
    return out


HEAD = """#ifndef UFT_ERROR_COMPAT_GEN_H
#define UFT_ERROR_COMPAT_GEN_H

/**
 * @file uft_error_compat_gen.h
 * @brief Legacy error-alias compatibility (GENERATED).
 *
 * Generated from data/errors.tsv + data/errors_legacy_aliases.tsv by
 * scripts/generators/gen_errors_compat.py. Do not edit by hand.
 *
 * Provides every alias spelling catalogued in the four pre-SSOT
 * compat sources:
 *   - UFT_ERROR_<NAME>   (mixed-case, ~1127 uses in 107 files)
 *   - UFT_E_<NAME>       (short-form P2-ARCH-006 spelling)
 *   - UFT_ERR_NOMEM / UFT_ERR_UNSUPPORTED / ... (legacy short names)
 *   - UFT_E<short>       (posix-ish EINVAL, EIO, ...)
 *   - UFT_IR_ERR_* / UFT_DEC_ERR_* / UFT_IO_ERR_* (subsystem families)
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
/* 1. UFT_ERROR_<NAME> — mixed-case canonical alias (auto-emitted from TSV) */
/* ------------------------------------------------------------------------ */
"""

MID_1 = """
/* ------------------------------------------------------------------------ */
/* 2. UFT_E_<NAME> — short-form (auto-emitted from canonical TSV)           */
/* ------------------------------------------------------------------------ */
"""

MID_2 = """
/* ------------------------------------------------------------------------ */
/* 3. Legacy aliases catalogued in data/errors_legacy_aliases.tsv           */
/*    (UFT_ERR_*, UFT_ERROR_*, UFT_E_*, UFT_IR_*, UFT_DEC_*, UFT_IO_*,      */
/*     UFT_E<short>). Grouped by prefix for readability.                    */
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
    if len(argv) not in (2, 3):
        sys.stderr.write(
            "usage: gen_errors_compat.py "
            "<errors.tsv> [errors_legacy_aliases.tsv]\n"
        )
        return 2

    rows = parse(Path(argv[1]))
    canonical_names = {r.name for r in rows}

    legacy_aliases: list[tuple[str, str, str]] = []
    if len(argv) == 3:
        legacy_aliases = parse_legacy_aliases(Path(argv[2]))

    # Track every symbol we've emitted so the output has zero duplicates
    # across the four emission loops.
    emitted: set[str] = set()

    def emit_define(sym: str, target_expr: str) -> None:
        """Emit `#ifndef sym / #define sym target_expr / #endif` once."""
        if sym in emitted:
            return
        emitted.add(sym)
        sys.stdout.write(f"#ifndef {sym}\n")
        sys.stdout.write(f"#define {sym} {target_expr}\n")
        sys.stdout.write("#endif\n")

    out = sys.stdout
    out.write(HEADER_BANNER)
    out.write(HEAD)

    # 1. UFT_ERROR_<NAME> — auto from canonical TSV
    for r in rows:
        if r.name == "SUCCESS":
            emit_define("UFT_ERROR_SUCCESS", "UFT_SUCCESS")
            continue
        emit_define(f"UFT_ERROR_{r.name}", f"UFT_ERR_{r.name}")

    # 2. UFT_E_<NAME> — auto from canonical TSV
    out.write(MID_1)
    for r in rows:
        if r.name == "SUCCESS":
            continue
        emit_define(f"UFT_E_{r.name}", f"UFT_ERR_{r.name}")

    # 3. Every legacy alias from errors_legacy_aliases.tsv
    out.write(MID_2)
    last_group: str | None = None
    for symbol, _source, canonical in legacy_aliases:
        if canonical not in canonical_names:
            sys.stderr.write(
                f"gen_errors_compat: alias {symbol} targets unknown "
                f"canonical {canonical}\n"
            )
            return 2
        # Group header comment (prefix-based) when prefix changes.
        # UFT_ERR_*, UFT_ERROR_*, UFT_E_*, UFT_IR_*, UFT_DEC_*, UFT_IO_*,
        # UFT_E<uppercase-no-underscore> = short posix.
        if symbol.startswith("UFT_ERR_"):
            group = "UFT_ERR_*"
        elif symbol.startswith("UFT_ERROR_"):
            group = "UFT_ERROR_*"
        elif symbol.startswith("UFT_E_"):
            group = "UFT_E_*"
        elif symbol.startswith("UFT_IR_"):
            group = "UFT_IR_*"
        elif symbol.startswith("UFT_DEC_"):
            group = "UFT_DEC_*"
        elif symbol.startswith("UFT_IO_"):
            group = "UFT_IO_*"
        else:
            group = "UFT_E<short>"
        if group != last_group:
            out.write(f"\n/* --- {group} --- */\n")
            last_group = group

        # SUCCESS / OK special-case: reference the sentinel UFT_SUCCESS
        # rather than the non-existent UFT_ERR_SUCCESS identifier.
        target = (
            "UFT_SUCCESS" if canonical == "SUCCESS"
            else f"UFT_ERR_{canonical}"
        )
        emit_define(symbol, target)

    out.write(TAIL)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
