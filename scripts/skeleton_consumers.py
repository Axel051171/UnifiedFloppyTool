#!/usr/bin/env python3
"""
Skeleton-Header Consumer Census.

For each skeleton header identified by audit_skeleton_headers.py, count the
number of source files (outside the header itself) that #include it.

Output buckets per MASTER_PLAN.md Regel 1:
  - DELETE    : 0 consumers  → safe to remove
  - DOCUMENT  : 1+ consumers but no implementation → mark /* PLANNED */
  - IMPLEMENT : 1+ consumers AND already partial impl → finish or remove calls

This script only classifies; it does not delete anything.
"""

from __future__ import annotations

import re
import subprocess
import sys
from pathlib import Path
from collections import defaultdict


ROOT = Path(__file__).resolve().parent.parent


def list_skeletons() -> list[tuple[int, int, str]]:
    """Invoke audit_skeleton_headers.py in markdown mode and parse its table."""
    res = subprocess.run(
        [sys.executable, str(ROOT / "scripts" / "audit_skeleton_headers.py"),
         "--markdown"],
        capture_output=True, text=True, check=True,
    )
    out: list[tuple[int, int, str]] = []
    for line in res.stdout.splitlines():
        m = re.match(r"\|\s*(\d+)\s*\|\s*(\d+)\s*\|\s*\d+%\s*\|\s*`([^`]+)`\s*\|", line)
        if m:
            out.append((int(m.group(1)), int(m.group(2)), m.group(3)))
    return out


def count_consumers(header_rel: str) -> tuple[int, list[str]]:
    """Count files that #include this header, excluding the header itself."""
    header_path = Path(header_rel)
    name = header_path.name
    # Also try path-suffix matches (for subdir headers like uft/hal/foo.h)
    # but the cheap/reliable check is the basename.
    pattern = re.compile(
        r'#\s*include\s*[<"][^">]*?' + re.escape(name) + r'[">]'
    )
    consumers: list[str] = []
    for ext in ("*.c", "*.cpp", "*.h", "*.hpp"):
        for path in ROOT.rglob(ext):
            if path.resolve() == (ROOT / header_path).resolve():
                continue
            # Skip build output and third-party mirrors
            parts = path.parts
            if any(p in ("build", "build-debug", ".git", "external") for p in parts):
                continue
            try:
                text = path.read_text(encoding="utf-8", errors="replace")
            except OSError:
                continue
            if pattern.search(text):
                consumers.append(str(path.relative_to(ROOT)).replace("\\", "/"))
    return len(consumers), consumers


def has_any_impl(header_rel: str, impl_symbols: set[str]) -> bool:
    """Does the header have at least one declared function with an impl?"""
    text = (ROOT / header_rel).read_text(encoding="utf-8", errors="replace")
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
    text = re.sub(r"//[^\n]*", "", text)
    decls: set[str] = set()
    for m in re.finditer(r"\b([\w\*\s]+?)\s+(uft_\w+)\s*\([^;{]*\)\s*;", text):
        decls.add(m.group(2))
    return bool(decls & impl_symbols)


def collect_impl_symbols() -> set[str]:
    defined: set[str] = set()
    for ext in ("*.c", "*.cpp"):
        for c in (ROOT / "src").rglob(ext):
            try:
                text = c.read_text(encoding="utf-8", errors="replace")
            except OSError:
                continue
            for m in re.finditer(r"\b(uft_\w+)\s*\([^;]*?\)\s*\{", text, re.DOTALL):
                defined.add(m.group(1))
    for h in (ROOT / "include").rglob("*.h"):
        try:
            text = h.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        text = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
        text = re.sub(r"//[^\n]*", "", text)
        for m in re.finditer(
            r"static\s+inline[\w\s\*]*\s+(uft_\w+)\s*\([^;]*?\)\s*\{",
            text, re.DOTALL,
        ):
            defined.add(m.group(1))
        for m in re.finditer(r"^#\s*define\s+(uft_\w+)\s*\(", text, re.MULTILINE):
            defined.add(m.group(1))
    return defined


def main() -> int:
    skeletons = list_skeletons()
    impl_symbols = collect_impl_symbols()

    buckets: dict[str, list[tuple[int, int, int, str]]] = defaultdict(list)
    details: list[tuple[str, int, list[str]]] = []

    for decls, missing, rel in skeletons:
        n_consumers, consumers = count_consumers(rel)
        if n_consumers == 0:
            bucket = "DELETE"
        elif has_any_impl(rel, impl_symbols):
            bucket = "IMPLEMENT"
        else:
            bucket = "DOCUMENT"
        buckets[bucket].append((decls, missing, n_consumers, rel))
        details.append((rel, n_consumers, consumers))

    print(f"Skeleton headers examined: {len(skeletons)}")
    for b in ("DELETE", "DOCUMENT", "IMPLEMENT"):
        print(f"  {b:<10} {len(buckets[b]):>4}")
    print()

    for bucket in ("DELETE", "DOCUMENT", "IMPLEMENT"):
        rows = sorted(buckets[bucket], key=lambda r: r[3])
        print(f"=== {bucket} ({len(rows)}) ===")
        print(f"{'decls':>5} {'miss':>5} {'cons':>5}  path")
        for d, m, c, p in rows:
            print(f"{d:5d} {m:5d} {c:5d}  {p}")
        print()

    # Write machine-readable CSV for further tooling
    csv_path = ROOT / "docs" / "skeleton_triage.csv"
    with csv_path.open("w", encoding="utf-8") as fh:
        fh.write("bucket,decls,missing,consumers,path\n")
        for bucket, rows in buckets.items():
            for d, m, c, p in rows:
                fh.write(f"{bucket},{d},{m},{c},{p}\n")
    print(f"Wrote {csv_path.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
