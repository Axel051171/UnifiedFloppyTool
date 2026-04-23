#!/usr/bin/env python3
"""Generate src/core/uft_error_strings.c from data/errors.tsv.

Emits a single C file defining uft_strerror() via a linear-search lookup
table. (Linear search is fine: ~50 entries, called only in error paths.)
"""
from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from gen_errors_common import HEADER_BANNER, parse  # noqa: E402


TEMPLATE_HEAD = """/**
 * @file uft_error_strings.c
 * @brief uft_strerror() implementation (GENERATED).
 *
 * Generated from data/errors.tsv by
 * scripts/generators/gen_errors_strings.py.  Do not edit by hand.
 */

#include "uft/uft_error.h"

#include <stddef.h>

struct uft_err_entry {
    int         code;
    const char* name;
    const char* description;
    const char* category;
};

static const struct uft_err_entry UFT_ERR_TABLE[] = {
"""

TEMPLATE_TAIL = """    { 0, NULL, NULL, NULL }  /* sentinel */
};

const char* uft_strerror(uft_rc_t rc) {
    for (const struct uft_err_entry* e = UFT_ERR_TABLE; e->name != NULL; ++e) {
        if (e->code == (int)rc) {
            return e->description;
        }
    }
    return "Unknown UFT error code";
}

/* Extended lookup helpers (name + description) for diagnostic callers. */

const char* uft_error_name(uft_error_t err) {
    for (const struct uft_err_entry* e = UFT_ERR_TABLE; e->name != NULL; ++e) {
        if (e->code == (int)err) {
            return e->name;
        }
    }
    return "UFT_ERR_UNKNOWN";
}

const char* uft_error_desc(uft_error_t err) {
    return uft_strerror((uft_rc_t)err);
}

const char* uft_error_category(uft_error_t err) {
    for (const struct uft_err_entry* e = UFT_ERR_TABLE; e->name != NULL; ++e) {
        if (e->code == (int)err) {
            return e->category;
        }
    }
    return "UNKNOWN";
}
"""


def c_escape(s: str) -> str:
    return s.replace("\\", "\\\\").replace("\"", "\\\"")


def main(argv: list[str]) -> int:
    if len(argv) != 2:
        sys.stderr.write("usage: gen_errors_strings.py <path-to-errors.tsv>\n")
        return 2
    rows = parse(Path(argv[1]))

    out = sys.stdout
    out.write(HEADER_BANNER)
    out.write(TEMPLATE_HEAD)

    for r in rows:
        ident = "UFT_SUCCESS" if r.name == "SUCCESS" else f"UFT_ERR_{r.name}"
        out.write(
            f"    {{ {r.code:>5}, \"{ident}\", "
            f"\"{c_escape(r.description)}\", "
            f"\"{r.category}\" }},\n"
        )

    out.write(TEMPLATE_TAIL)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
