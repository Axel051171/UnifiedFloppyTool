#!/usr/bin/env python3
"""Shared TSV parser for the error-code SSOT generators.

Reads data/errors.tsv and returns a deterministic list of Row tuples.
Python 3.9+ stdlib only. No third-party deps.

Row schema:
  code (int), name (str), category (str), description (str), proposed (bool)

Rows are returned in file order, which matches the numeric order the
human author wrote them in. Deterministic: identical input bytes produce
identical output list.
"""
from __future__ import annotations

import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Iterator


VALID_CATEGORIES = {
    "ARG", "IO", "FORMAT", "RESOURCE", "FEATURE",
    "HARDWARE", "INTERNAL", "MISC", "PROTECTION",
}


@dataclass(frozen=True)
class Row:
    code: int
    name: str
    category: str
    description: str
    proposed: bool  # True if the preceding comment line was "# PROPOSED"


def parse(tsv_path: Path) -> list[Row]:
    """Parse errors.tsv; raise SystemExit(2) on any malformed row."""
    if not tsv_path.is_file():
        sys.stderr.write(f"gen_errors: TSV not found: {tsv_path}\n")
        sys.exit(2)

    rows: list[Row] = []
    seen_codes: dict[int, str] = {}
    seen_names: set[str] = set()
    pending_proposed = False

    with tsv_path.open("r", encoding="utf-8", newline="") as f:
        for lineno, raw in enumerate(f, start=1):
            line = raw.rstrip("\r\n")
            stripped = line.strip()
            if not stripped:
                pending_proposed = False
                continue
            if stripped.startswith("#"):
                # Track the single-line "# PROPOSED" marker that precedes
                # rows awaiting human decision.
                if stripped.upper() == "# PROPOSED":
                    pending_proposed = True
                # Any other comment resets the proposed flag UNLESS the
                # very next non-empty line is a data row.
                continue

            parts = line.split("\t")
            if len(parts) != 4:
                sys.stderr.write(
                    f"gen_errors: {tsv_path}:{lineno}: "
                    f"expected 4 tab-separated fields, got {len(parts)}\n"
                )
                sys.exit(2)
            code_s, name, category, description = (p.strip() for p in parts)

            try:
                code = int(code_s)
            except ValueError:
                sys.stderr.write(
                    f"gen_errors: {tsv_path}:{lineno}: "
                    f"code {code_s!r} is not an integer\n"
                )
                sys.exit(2)

            if not name or not name.replace("_", "").isalnum():
                sys.stderr.write(
                    f"gen_errors: {tsv_path}:{lineno}: "
                    f"bad name {name!r}\n"
                )
                sys.exit(2)

            if category not in VALID_CATEGORIES:
                sys.stderr.write(
                    f"gen_errors: {tsv_path}:{lineno}: "
                    f"unknown category {category!r}; "
                    f"valid: {sorted(VALID_CATEGORIES)}\n"
                )
                sys.exit(2)

            if code in seen_codes:
                sys.stderr.write(
                    f"gen_errors: {tsv_path}:{lineno}: "
                    f"duplicate code {code} "
                    f"(previously {seen_codes[code]})\n"
                )
                sys.exit(2)
            if name in seen_names:
                sys.stderr.write(
                    f"gen_errors: {tsv_path}:{lineno}: "
                    f"duplicate name {name}\n"
                )
                sys.exit(2)

            seen_codes[code] = name
            seen_names.add(name)
            rows.append(Row(code, name, category, description, pending_proposed))
            pending_proposed = False

    return rows


def iter_nonproposed(rows: list[Row]) -> Iterator[Row]:
    for r in rows:
        if not r.proposed:
            yield r


def iter_proposed(rows: list[Row]) -> Iterator[Row]:
    for r in rows:
        if r.proposed:
            yield r


HEADER_BANNER = (
    "/* =====================================================================\n"
    " * GENERATED FILE — DO NOT EDIT BY HAND.\n"
    " * Source of truth: data/errors.tsv\n"
    " * Regenerate with: make generate  (or scripts/verify_errors_ssot.sh)\n"
    " * Any manual edits will be overwritten on the next generator run.\n"
    " * ===================================================================== */\n"
)
