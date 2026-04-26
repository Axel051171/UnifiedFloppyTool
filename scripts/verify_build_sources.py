#!/usr/bin/env python3
"""
Build-System Parity Check (MF-006).

qmake's `UnifiedFloppyTool.pro` lists every source file explicitly.
`CMakeLists.txt` uses `file(GLOB_RECURSE ...)` with a handful of
exclusion regexes. Whenever a new `.c` / `.cpp` lands under `src/`,
CMake picks it up automatically but qmake does NOT — producing a
silent functionality gap in the primary release build (see CI
break from v4.1.3 after the SSOT cutover, commit 6184f42).

This script catches the gap. It returns a non-zero exit code
ONLY when NEW divergence is introduced — i.e. a file added to the
source tree between this run and the committed baseline. A static
historical divergence (samdisk vendor code, `_parser_v3.c` stub
proliferation, mbedtls example mains) is recorded in
data/build_system_baseline.tsv and ignored.

What we check:
  (A) files CMake would glob but are missing from .pro       = silent miss
  (B) files listed in .pro but not present on disk           = stale entry

A and B are only release-blocking when they differ from the committed
baseline. The baseline itself is whittled down by separate cleanup
work (MF-004 stub elimination etc.).

Run:
  python3 scripts/verify_build_sources.py                  # against baseline
  python3 scripts/verify_build_sources.py --verbose
  python3 scripts/verify_build_sources.py --rebuild-baseline   # refresh after cleanup
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path


# Exclusion regexes mirrored from CMakeLists.txt `list(FILTER ... EXCLUDE REGEX)`.
CMAKE_EXCLUSIONS: list[re.Pattern[str]] = [
    re.compile(r".*/tests?/.*"),
    re.compile(r".*/test_.*"),
    re.compile(r".*_example\.cpp$"),
    re.compile(r".*/formats_v2/.*"),
    re.compile(r"^src/main\.cpp$"),
]

SOURCE_EXTS = {".c", ".cpp"}


def collect_cmake_globbed_sources(repo_root: Path) -> set[str]:
    """Replicate what CMake's GLOB_RECURSE picks up for UFT_SOURCES."""
    out: set[str] = set()
    src = repo_root / "src"
    if not src.is_dir():
        return out
    for path in src.rglob("*"):
        if path.suffix not in SOURCE_EXTS:
            continue
        if not path.is_file():
            continue
        rel = path.relative_to(repo_root).as_posix()
        if any(p.search(rel) for p in CMAKE_EXCLUSIONS):
            continue
        out.add(rel)
    return out


def parse_pro_sources(pro_path: Path) -> set[str]:
    """Return the SOURCES listed in the .pro file."""
    return _parse_pro_lists(pro_path, key="SOURCES", exts=(".c", ".cpp"))


def parse_pro_headers(pro_path: Path) -> set[str]:
    """Return the HEADERS listed in the .pro file."""
    return _parse_pro_lists(pro_path, key="HEADERS", exts=(".h", ".hpp"))


def _parse_pro_lists(pro_path: Path, key: str, exts: tuple[str, ...]) -> set[str]:
    """Common parser for SOURCES/HEADERS — handles `+=` and line continuations."""
    if not pro_path.is_file():
        sys.stderr.write(f"verify_build_sources: {pro_path} not found\n")
        sys.exit(2)

    text = pro_path.read_text(encoding="utf-8", errors="replace")
    text = re.sub(r"\\\s*\n\s*", " ", text)  # join line continuations

    pat = re.compile(rf"^\s*{key}\s*\+?=\s*(.*?)\s*$")
    out: set[str] = set()
    for line in text.splitlines():
        m = pat.match(line)
        if not m:
            continue
        rest = re.sub(r"#.*$", "", m.group(1))
        for tok in rest.split():
            if tok.startswith("$$") or tok == "+=":
                continue
            if tok.endswith(exts):
                out.add(tok)
    return out


def load_baseline(path: Path) -> tuple[set[str], set[str]]:
    """Return (in_cmake_not_in_pro, in_pro_not_on_disk) sets from TSV."""
    a: set[str] = set()
    b: set[str] = set()
    if not path.is_file():
        return a, b
    for raw in path.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        kind, _, file_ = line.partition("\t")
        if kind == "A":
            a.add(file_)
        elif kind == "B":
            b.add(file_)
    return a, b


def write_baseline(path: Path, miss_a: set[str], miss_b: set[str]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as f:
        f.write(
            "# Build-system divergence baseline (MF-006).\n"
            "# Accepted historical gaps between .pro and CMake GLOB.\n"
            "# Format:  <kind>\\t<path>\n"
            "#   kind A = file on disk, absent from .pro (CMake compiles it, qmake skips)\n"
            "#   kind B = file listed in .pro but not on disk (stale entry)\n"
            "#\n"
            "# New divergence (not in this file) makes verify_build_sources.py fail.\n"
            "# This baseline shrinks as cleanup work (MF-004 stubs, samdisk split,\n"
            "# etc.) lands. Regenerate with:\n"
            "#   python3 scripts/verify_build_sources.py --rebuild-baseline\n\n"
        )
        for p in sorted(miss_a):
            f.write(f"A\t{p}\n")
        for p in sorted(miss_b):
            f.write(f"B\t{p}\n")


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--verbose", action="store_true")
    ap.add_argument("--rebuild-baseline", action="store_true",
                    help="Overwrite the baseline with current state. Use after "
                    "cleanup commits that legitimately resolve entries.")
    ap.add_argument("--root", type=Path,
                    default=Path(__file__).resolve().parent.parent)
    ap.add_argument("--emit-cmake-sources", action="store_true",
                    help="Print .pro SOURCES as semicolon-list for CMake "
                    "execute_process(). Filters to files that exist on disk.")
    ap.add_argument("--emit-cmake-headers", action="store_true",
                    help="Print .pro HEADERS as semicolon-list for CMake.")
    args = ap.parse_args()

    repo = args.root.resolve()
    pro = repo / "UnifiedFloppyTool.pro"
    baseline_path = repo / "data" / "build_system_baseline.tsv"

    if args.emit_cmake_sources or args.emit_cmake_headers:
        items = (parse_pro_sources(pro) if args.emit_cmake_sources
                 else parse_pro_headers(pro))
        # Drop stale entries (file not on disk) so CMake never tries to
        # compile a missing path.
        existing = sorted(p for p in items if (repo / p).is_file())
        sys.stdout.write(";".join(existing))
        return 0

    cmake_sources = collect_cmake_globbed_sources(repo)
    pro_sources = parse_pro_sources(pro)

    miss_a = cmake_sources - pro_sources   # on disk, not in .pro
    miss_b = {p for p in pro_sources if not (repo / p).is_file()}

    if args.rebuild_baseline:
        write_baseline(baseline_path, miss_a, miss_b)
        print(f"rebuilt baseline {baseline_path}:")
        print(f"  A: {len(miss_a)} files on disk, absent from .pro")
        print(f"  B: {len(miss_b)} stale .pro entries")
        return 0

    base_a, base_b = load_baseline(baseline_path)
    new_a = miss_a - base_a   # NEW silent misses (regression)
    new_b = miss_b - base_b   # NEW stale entries
    fixed_a = base_a - miss_a  # baseline entries that are now resolved
    fixed_b = base_b - miss_b

    print(f"Build-system parity check (MF-006)")
    print(f"  CMake GLOB source files    : {len(cmake_sources):4d}")
    print(f"  .pro SOURCES entries       : {len(pro_sources):4d}")
    print(f"  Baseline accepted gaps A/B : {len(base_a):4d} / {len(base_b):4d}")
    print(f"  Current gaps A/B           : {len(miss_a):4d} / {len(miss_b):4d}")
    print(f"  NEW regressions A/B        : {len(new_a):4d} / {len(new_b):4d}")
    print(f"  Baseline entries resolved  : {len(fixed_a):4d} / {len(fixed_b):4d}")

    if args.verbose or new_a or new_b:
        if new_a:
            print("\n  === NEW (A) on disk, missing from .pro ===")
            for p in sorted(new_a):
                print(f"    + {p}")
        if new_b:
            print("\n  === NEW (B) stale .pro entry ===")
            for p in sorted(new_b):
                print(f"    - {p}")

    if fixed_a or fixed_b:
        print("\n  NOTE: baseline entries resolved above — consider running")
        print("        `verify_build_sources.py --rebuild-baseline` to shrink")
        print("        the accepted-gap set.")

    if new_a or new_b:
        print("\nFAIL: new build-system divergence since baseline (MF-006).")
        print("      Add the file to UnifiedFloppyTool.pro (usual case) or")
        print("      extend CMakeLists.txt exclusions (if intentionally skipped).")
        return 1

    print("\nOK: no new divergence beyond the accepted baseline.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
