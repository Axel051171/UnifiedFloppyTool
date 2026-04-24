#!/usr/bin/env python3
"""
Skeleton-Header Audit for UnifiedFloppyTool.

Identifies .h files in include/uft/ that declare many uft_* functions but
have few (or zero) implementations elsewhere in the tree. Such files are
"phantom APIs" — they promise functions that don't exist, violating
Principle 1 ("no silent data loss" -> by extension: no phantom features).

Usage:
  python3 scripts/audit_skeleton_headers.py
  python3 scripts/audit_skeleton_headers.py --min-decls 10 --min-ratio 0.8
  python3 scripts/audit_skeleton_headers.py --markdown > audit.md

Methodology (see docs/SKELETON_HEADERS_AUDIT.md for rationale):
  - Collect all `uft_*(...) {` function DEFINITIONS from src/**/*.c, .cpp,
    plus static-inline definitions in any header, plus function-style
    #define macros. These count as "implemented".
  - For each include/uft/**/*.h count `<ret> uft_*(...);` PROTOTYPES
    whose identifier is NOT in the implemented set.
  - Report headers where unimplemented/total >= threshold.

Exit code 0 (report-only). Intended as informational CI tool, not a gate.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path


def collect_implementations(root: Path) -> set[str]:
    defined: set[str] = set()

    for ext in ("*.c", "*.cpp"):
        for c in (root / "src").rglob(ext):
            try:
                text = c.read_text(encoding="utf-8", errors="replace")
            except OSError:
                continue
            # Match: <ret-type-tokens> uft_foo(...args...) {
            for m in re.finditer(r"\b(uft_\w+)\s*\([^;]*?\)\s*\{", text, re.DOTALL):
                defined.add(m.group(1))

    # static inline defs + function-style macros in ANY header
    for h in (root / "include").rglob("*.h"):
        try:
            text = h.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        # strip comments
        text = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
        text = re.sub(r"//[^\n]*", "", text)
        for m in re.finditer(
            r"static\s+inline[\w\s\*]*\s+(uft_\w+)\s*\([^;]*?\)\s*\{",
            text,
            re.DOTALL,
        ):
            defined.add(m.group(1))
        for m in re.finditer(r"^#\s*define\s+(uft_\w+)\s*\(", text, re.MULTILINE):
            defined.add(m.group(1))

    return defined


def scan_skeletons(
    root: Path, implemented: set[str], min_decls: int, min_ratio: float
) -> list[tuple[int, int, float, str]]:
    skeletons: list[tuple[int, int, float, str]] = []
    for h in (root / "include" / "uft").rglob("*.h"):
        try:
            text = h.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        text = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
        text = re.sub(r"//[^\n]*", "", text)

        decls: set[str] = set()
        # Match: <ret> uft_foo(...);    (ends in semicolon, no brace)
        for m in re.finditer(
            r"\b([\w\*\s]+?)\s+(uft_\w+)\s*\([^;{]*\)\s*;", text
        ):
            decls.add(m.group(2))

        if not decls:
            continue

        missing = decls - implemented
        ratio = len(missing) / len(decls)
        if len(decls) >= min_decls and ratio >= min_ratio:
            skeletons.append(
                (len(decls), len(missing), ratio, h.relative_to(root).as_posix())
            )

    skeletons.sort(key=lambda r: (-r[1], -r[2]))
    return skeletons


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--min-decls", type=int, default=10)
    ap.add_argument("--min-ratio", type=float, default=0.8)
    ap.add_argument(
        "--markdown",
        action="store_true",
        help="Emit a Markdown table suitable for docs/SKELETON_HEADERS_AUDIT.md",
    )
    ap.add_argument(
        "--root",
        type=Path,
        default=Path(__file__).resolve().parent.parent,
    )
    args = ap.parse_args()

    root = args.root.resolve()
    impl = collect_implementations(root)
    skel = scan_skeletons(root, impl, args.min_decls, args.min_ratio)

    if args.markdown:
        print("| decls | missing | pct | path |")
        print("|------:|--------:|----:|------|")
        for d, m, r, f in skel:
            print(f"| {d} | {m} | {int(r * 100)}% | `{f}` |")
        return 0

    total_missing = sum(m for _, m, _, _ in skel)
    print(
        f"Skeleton headers (>= {args.min_decls} decls, "
        f">= {int(args.min_ratio * 100)}% missing): {len(skel)}"
    )
    print(f"Total unimplemented declarations: {total_missing}")
    print()
    print(f"{'decls':>5} {'miss':>5} {'pct':>5}  path")
    for d, m, r, f in skel[:40]:
        print(f"{d:5d} {m:5d} {int(r * 100):4d}%  {f}")
    if len(skel) > 40:
        print(f"... ({len(skel) - 40} more)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
