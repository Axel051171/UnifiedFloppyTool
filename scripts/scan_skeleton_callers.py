#!/usr/bin/env python3
"""
Phase-1 stub-inventory scanner (docs/STUB_ELIMINATION_PLAN.md).

For every unimplemented `uft_*` declaration in include/uft/**, count how
many reference sites exist in src/ and tests/. Because these functions
have NO definition anywhere (verified against the same implementation
census as audit_skeleton_headers.py), every `.c`/`.cpp` occurrence of
`name(` is a reference — a call from code that itself cannot link, or a
stray extern redeclaration. Both mean the same thing for triage:

  callers == 0  ->  DELETE candidate (nothing would notice)
  callers  > 0  ->  cascade analysis needed (the referencing code is
                    itself dead/unbuilt, or tests expect the API)

Additionally inventories:
  C1  remaining Pattern-A `*_parser_v3.c` files + their reference counts
  C5  functions defined in src/core/uft_core_stubs.c + whether a second
      definition exists elsewhere (A07-class duplicate-symbol risk)
  C8  src files carrying stub markers, classified honest vs unmarked

Usage:
  python3 scripts/scan_skeleton_callers.py                 # summary
  python3 scripts/scan_skeleton_callers.py --csv out.csv   # per-decl CSV
  python3 scripts/scan_skeleton_callers.py --yaml docs/stub_inventory.yaml

Exit code 0 (report-only). Phase-2 gates consume the YAML.
"""

from __future__ import annotations

import argparse
import re
import sys
from collections import Counter, defaultdict
from datetime import date
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from audit_skeleton_headers import collect_implementations  # noqa: E402

VENDORED_PREFIXES = (
    "src/switch/", "src/mbedtls/", "src/samdisk/", "src/formats/uff/",
)


def _read(p: Path) -> str:
    try:
        return p.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return ""


def _strip_comments(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
    return re.sub(r"//[^\n]*", "", text)


def collect_declarations(root: Path) -> dict[str, list[str]]:
    """header(rel-posix) -> sorted list of declared uft_* names."""
    decls: dict[str, list[str]] = {}
    for h in (root / "include" / "uft").rglob("*.h"):
        text = _strip_comments(_read(h))
        names: set[str] = set()
        for m in re.finditer(r"\b([\w\*\s]+?)\s+(uft_\w+)\s*\([^;{]*\)\s*;", text):
            names.add(m.group(2))
        if names:
            decls[h.relative_to(root).as_posix()] = sorted(names)
    return decls


def count_references(root: Path) -> tuple[Counter, Counter]:
    """One pass over src/ and tests/: Counter of `uft_xxx(`-shaped tokens."""
    src_refs: Counter = Counter()
    test_refs: Counter = Counter()
    ident = re.compile(r"\b(uft_\w+)\s*\(")
    for base, counter in ((root / "src", src_refs), (root / "tests", test_refs)):
        for ext in ("*.c", "*.cpp"):
            for f in base.rglob(ext):
                rel = f.relative_to(root).as_posix()
                if any(rel.startswith(p) for p in VENDORED_PREFIXES):
                    continue
                for m in ident.finditer(_strip_comments(_read(f))):
                    counter[m.group(1)] += 1
    return src_refs, test_refs


def scan_c1_v3_parsers(root: Path, src_refs: Counter, test_refs: Counter):
    out = []
    for f in sorted((root / "src" / "formats").rglob("*_parser_v3.c")):
        rel = f.relative_to(root).as_posix()
        text = _strip_comments(_read(f))
        fns = sorted(set(
            m.group(1)
            for m in re.finditer(r"\b(\w+_parse\w*|\w+_v3_\w+)\s*\([^;]*?\)\s*\{", text)
        ))
        ext_refs = 0
        whole = None
        for fn in fns:
            # references outside the file itself
            n = 0
            pat = re.compile(r"\b" + re.escape(fn) + r"\s*\(")
            for base in (root / "src", root / "tests"):
                for other in list(base.rglob("*.c")) + list(base.rglob("*.cpp")):
                    if other == f:
                        continue
                    n += len(pat.findall(_strip_comments(_read(other))))
            ext_refs += n
        proposal = "DELETE" if ext_refs == 0 else "REVIEW (has external refs)"
        out.append({
            "file": rel,
            "loc": len(_read(f).splitlines()),
            "functions": len(fns),
            "external_refs": ext_refs,
            "proposal": proposal,
        })
    return out


def _count_header_callsites(root: Path, fn: str) -> int:
    """
    Blind-spot fix (MF-294): a function with zero .c/.cpp references can
    still be load-bearing via static-inline bodies or macros in HEADERS
    (found the hard way: uft_cpu_has_feature is called from a
    static-inline in uft_simd.h, uft_track_get_sector[_count] from the
    foreach macro in uft_core.h). Count header occurrences that are NOT
    prototypes: a line matching `... fn(...);` with no assignment/call
    context is a decl and does not count.
    """
    n = 0
    proto = re.compile(
        r"^\s*[\w\*\s]+?\b" + re.escape(fn) + r"\s*\([^)]*\)\s*;\s*$")
    callish = re.compile(r"\b" + re.escape(fn) + r"\s*\(")
    for base in (root / "include", root / "src"):
        for h in base.rglob("*.h"):
            rel = h.relative_to(root).as_posix()
            if any(rel.startswith(p) for p in VENDORED_PREFIXES):
                continue
            for line in _strip_comments(_read(h)).splitlines():
                if callish.search(line) and not proto.match(line):
                    n += 1
    return n


def scan_c5_core_stubs(root: Path, src_refs: Counter, test_refs: Counter):
    stub_file = root / "src" / "core" / "uft_core_stubs.c"
    text = _read(stub_file)
    fns = sorted(set(
        m.group(1)
        for m in re.finditer(r"^[\w\*\s]+?\b(uft_\w+)\s*\([^;]*?\)\s*\{",
                             _strip_comments(text), re.MULTILINE)
    ))
    out = []
    for fn in fns:
        dup = 0
        pat = re.compile(r"\b" + re.escape(fn) + r"\s*\([^;]*?\)\s*\{", re.DOTALL)
        for c in (root / "src").rglob("*.c"):
            if c == stub_file:
                continue
            rel = c.relative_to(root).as_posix()
            if any(rel.startswith(p) for p in VENDORED_PREFIXES):
                continue
            dup += len(pat.findall(_strip_comments(_read(c))))
        refs = src_refs.get(fn, 0) + test_refs.get(fn, 0)
        hdr = _count_header_callsites(root, fn)
        if dup > 0:
            proposal = "A07-CLASS: duplicate definition elsewhere — RELOCATE/DELETE"
        elif refs <= 1 and hdr == 0:  # 1 == the definition itself
            proposal = "DELETE candidate (no callers, incl. header call-sites)"
        else:
            proposal = "DOCUMENT (real implementation with callers) or IMPLEMENT"
        out.append({"function": fn, "callers": max(0, refs - 1),
                    "header_callsites": hdr,
                    "duplicate_defs": dup, "proposal": proposal})
    return out


def scan_c8_stub_markers(root: Path):
    out = []
    marker = re.compile(r"\bSTUB\b|\bstub\b")
    honest = re.compile(
        r"NOT_IMPLEMENTED|NOT_SUPPORTED|honest|KNOWN_ISSUES|MASTER_PLAN|M3\.\d")
    for c in (root / "src").rglob("*.c"):
        rel = c.relative_to(root).as_posix()
        if any(rel.startswith(p) for p in VENDORED_PREFIXES):
            continue
        text = _read(c)
        if not marker.search(text):
            continue
        klass = "honest (tracked)" if honest.search(text) else "UNMARKED — triage"
        out.append({"file": rel, "classification": klass})
    return sorted(out, key=lambda r: (r["classification"], r["file"]))


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--csv", type=Path, help="write per-declaration CSV")
    ap.add_argument("--yaml", type=Path, help="write docs/stub_inventory.yaml")
    ap.add_argument("--root", type=Path,
                    default=Path(__file__).resolve().parent.parent)
    args = ap.parse_args()
    root = args.root.resolve()

    implemented = collect_implementations(root)
    decls = collect_declarations(root)
    src_refs, test_refs = count_references(root)

    rows = []  # (header, fn, callers_src, callers_tests)
    per_header: dict[str, dict[str, int]] = defaultdict(lambda: {
        "missing": 0, "zero_caller": 0, "with_caller": 0})
    for header, names in decls.items():
        for fn in names:
            if fn in implemented:
                continue
            cs, ct = src_refs.get(fn, 0), test_refs.get(fn, 0)
            rows.append((header, fn, cs, ct))
            ph = per_header[header]
            ph["missing"] += 1
            if cs + ct == 0:
                ph["zero_caller"] += 1
            else:
                ph["with_caller"] += 1

    total_missing = len(rows)
    zero = sum(1 for _, _, cs, ct in rows if cs + ct == 0)

    c1 = scan_c1_v3_parsers(root, src_refs, test_refs)
    c5 = scan_c5_core_stubs(root, src_refs, test_refs)
    c8 = scan_c8_stub_markers(root)

    print(f"Unimplemented uft_* declarations : {total_missing}")
    print(f"  with ZERO references (DELETE)  : {zero}")
    print(f"  with references (cascade)      : {total_missing - zero}")
    print(f"C1 pattern-A v3 parser files     : {len(c1)} "
          f"({sum(1 for e in c1 if e['proposal'] == 'DELETE')} DELETE-ready)")
    print(f"C5 core-stub functions           : {len(c5)}")
    print(f"C8 stub-marker files             : {len(c8)} "
          f"({sum(1 for e in c8 if 'UNMARKED' in e['classification'])} unmarked)")

    if args.csv:
        with args.csv.open("w", encoding="utf-8", newline="") as f:
            f.write("header,function,callers_src,callers_tests\n")
            for header, fn, cs, ct in sorted(rows):
                f.write(f"{header},{fn},{cs},{ct}\n")
        print(f"CSV written: {args.csv}")

    if args.yaml:
        lines = [
            "# Stub inventory — Phase-1 gate artifact",
            "# Generated by scripts/scan_skeleton_callers.py — do not hand-edit;",
            "# regenerate and diff instead. Consumed by Phase 2 of",
            "# docs/STUB_ELIMINATION_PLAN.md.",
            f"generated: {date.today().isoformat()}",
            "census:",
            f"  unimplemented_decls: {total_missing}",
            f"  zero_caller_decls: {zero}",
            f"  with_caller_decls: {total_missing - zero}",
            "  note: >-",
            "    A caller-count > 0 for an UNIMPLEMENTED function means the",
            "    referencing code itself cannot link — it is dead/unbuilt, or",
            "    a test expecting a planned API. Cascade analysis in Phase 2.",
            "c2_headers:",
        ]
        for header in sorted(per_header,
                             key=lambda h: -per_header[h]["missing"]):
            ph = per_header[header]
            if ph["with_caller"] == 0:
                prop = "DELETE-candidate (all decls unreferenced)"
            elif ph["zero_caller"] == 0:
                prop = "DOCUMENT/IMPLEMENT (all decls referenced)"
            else:
                prop = "SPLIT (delete unreferenced decls, keep referenced)"
            lines += [
                f"  - header: {header}",
                f"    missing: {ph['missing']}",
                f"    zero_caller: {ph['zero_caller']}",
                f"    with_caller: {ph['with_caller']}",
                f"    proposal: {prop}",
            ]
        lines.append("c1_pattern_a_parsers:")
        for e in c1:
            lines += [
                f"  - file: {e['file']}",
                f"    loc: {e['loc']}",
                f"    external_refs: {e['external_refs']}",
                f"    proposal: {e['proposal']}",
            ]
        lines.append("c5_core_stub_functions:")
        for e in c5:
            lines += [
                f"  - function: {e['function']}",
                f"    callers: {e['callers']}",
                f"    header_callsites: {e['header_callsites']}",
                f"    duplicate_defs: {e['duplicate_defs']}",
                f"    proposal: {e['proposal']}",
            ]
        lines.append("c8_stub_marker_files:")
        for e in c8:
            lines += [
                f"  - file: {e['file']}",
                f"    classification: {e['classification']}",
            ]
        lines += [
            "c4_hal_honest_stubs:",
            "  - file: src/hal/uft_xum1541.c",
            "    status: honest-stub, M3.2 wiring blocked by KNOWN_ISSUES M.4",
            "  - file: src/hal/uft_applesauce.c",
            "    status: honest-stub, M3.3 serial wiring pending",
            "c6_adf_write_side:",
            "  - see: docs/KNOWN_ISSUES.md §7.4 (v4.2 target)",
            "c7_salvage_scaffolds:",
            "  - see: src/recovery/uft_salvage_fs.c (by-design, "
            "XCOPY_INTEGRATION_TODO)",
        ]
        args.yaml.write_text("\n".join(lines) + "\n", encoding="utf-8")
        print(f"YAML written: {args.yaml}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
