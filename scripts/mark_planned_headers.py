#!/usr/bin/env python3
"""
DOCUMENT-Welle (M1/MF-011): mark DOCUMENT-bucket skeleton headers as
/* PLANNED FEATURE */ so consumers are aware these APIs are promised but
not yet implemented.

Reads docs/skeleton_triage.csv, processes every row with bucket=DOCUMENT:
  1. Inserts a visible banner after the file-level doxygen block
     (idempotent: skips files that already contain the marker).
  2. Records a KNOWN_ISSUES.md appendix entry grouped by subsystem.

Exit 0 on success.
"""

from __future__ import annotations

import csv
import re
import sys
from pathlib import Path
from collections import defaultdict

ROOT = Path(__file__).resolve().parent.parent
TRIAGE = ROOT / "docs" / "skeleton_triage.csv"

MARKER = "UFT_SKELETON_PLANNED"
BANNER = f"""
/* ══════════════════════════════════════════════════════════════════════════ *
 * {MARKER}
 * PLANNED FEATURE — {{scope}}
 *
 * This header declares {{decls}} public functions, of which {{missing}} have no
 * implementation in the source tree. Callers exist but will link-fail or
 * silently no-op until the feature is implemented.
 *
 * Status: tracked in docs/KNOWN_ISSUES.md under "Planned APIs".
 * Scope: see docs/MASTER_PLAN.md (M1/MF-011 DOCUMENT-Welle).
 * Do NOT add new call sites to functions from this header without first
 * implementing them or removing the prototype.
 * ══════════════════════════════════════════════════════════════════════════ */
"""


SCOPE_BY_PREFIX = [
    ("include/uft/batch/",        "Batch processing"),
    ("include/uft/core/",         "Core infrastructure"),
    ("include/uft/crc/",          "CRC engines"),
    ("include/uft/decoder/",      "Decoder pipeline"),
    ("include/uft/encoding/",     "Encoding detection"),
    ("include/uft/flux/",         "Flux analysis"),
    ("include/uft/formats/",      "Format plugins"),
    ("include/uft/fs/",           "Filesystem layer"),
    ("include/uft/hal/internal/", "HAL internals"),
    ("include/uft/hal/",          "Hardware abstraction"),
    ("include/uft/ml/",           "Machine-learning decode"),
    ("include/uft/ocr/",          "Label OCR"),
    ("include/uft/params/",       "Parameter registry"),
    ("include/uft/parsers/",      "Parser helpers"),
    ("include/uft/protection/",   "Copy-protection detection"),
    ("include/uft/recovery/",     "Recovery pipeline"),
    ("include/uft/ride/",         "RIDE integration"),
]


def classify_scope(path: str) -> str:
    for prefix, label in SCOPE_BY_PREFIX:
        if path.startswith(prefix):
            return label
    return "Root-level API"


def insert_banner(header_path: Path, scope: str, decls: int, missing: int) -> bool:
    text = header_path.read_text(encoding="utf-8", errors="replace")
    if MARKER in text:
        return False  # already marked

    banner = BANNER.format(scope=scope, decls=decls, missing=missing)

    # Insertion point: end of first /** ... */ block if within first 30 lines,
    # otherwise right after the include guard opener (#define UFT_FOO_H),
    # otherwise at the very top.
    m_doxy = re.search(r"^/\*\*.*?\*/", text, re.DOTALL | re.MULTILINE)
    if m_doxy and m_doxy.start() < 1500:
        # Insert banner right after the doxy block
        i = m_doxy.end()
        # Keep any blank line after doxy
        new_text = text[:i] + "\n" + banner + text[i:]
    else:
        m_guard = re.search(r"^#\s*define\s+UFT_[A-Z0-9_]+_H\s*$", text, re.MULTILINE)
        if m_guard:
            i = m_guard.end()
            new_text = text[:i] + "\n" + banner + text[i:]
        else:
            new_text = banner + "\n" + text

    header_path.write_text(new_text, encoding="utf-8")
    return True


def main() -> int:
    rows: list[tuple[int, int, str]] = []
    with TRIAGE.open("r", encoding="utf-8") as fh:
        reader = csv.DictReader(fh)
        for row in reader:
            if row["bucket"] != "DOCUMENT":
                continue
            rows.append((int(row["decls"]), int(row["missing"]), row["path"]))

    if not rows:
        print("No DOCUMENT-bucket rows found.", file=sys.stderr)
        return 1

    modified: int = 0
    skipped_already_marked: int = 0
    missing_on_disk: int = 0
    by_scope: dict[str, list[tuple[int, int, str]]] = defaultdict(list)

    for decls, missing, rel in rows:
        p = ROOT / rel
        if not p.exists():
            print(f"MISSING: {rel}", file=sys.stderr)
            missing_on_disk += 1
            continue
        scope = classify_scope(rel)
        by_scope[scope].append((decls, missing, rel))
        if insert_banner(p, scope, decls, missing):
            modified += 1
        else:
            skipped_already_marked += 1

    print(f"DOCUMENT headers processed   : {len(rows)}")
    print(f"  modified (banner inserted) : {modified}")
    print(f"  already marked (skipped)   : {skipped_already_marked}")
    print(f"  missing on disk            : {missing_on_disk}")
    print()
    print("=== Grouped by scope ===")
    for scope in sorted(by_scope):
        entries = by_scope[scope]
        total_missing = sum(m for _, m, _ in entries)
        print(f"  {scope:<30} {len(entries):>3} headers · {total_missing:>4} missing decls")

    # Emit an appendix snippet for KNOWN_ISSUES.md
    appendix_path = ROOT / "docs" / "PLANNED_APIS.md"
    with appendix_path.open("w", encoding="utf-8") as fh:
        fh.write("# Planned APIs (MF-011 DOCUMENT-Welle)\n\n")
        fh.write(f"**Stand:** auto-generated, {len(rows)} headers · "
                 f"{sum(m for _, m, _ in rows)} unimplemented declarations\n\n")
        fh.write("Every header listed below declares public `uft_*` functions "
                 "that are promised by the API surface but have no implementation "
                 "in `src/`. Each file now carries a `/* PLANNED FEATURE */` "
                 "banner so consumers are aware before adding new call sites. "
                 "Implementation belongs to M2/M3 depending on subsystem.\n\n")
        for scope in sorted(by_scope):
            entries = sorted(by_scope[scope], key=lambda r: -r[1])
            fh.write(f"## {scope}\n\n")
            fh.write("| decls | missing | consumers | header |\n")
            fh.write("|------:|--------:|----------:|--------|\n")
            # Re-read consumers from CSV
            cons_map: dict[str, int] = {}
            with TRIAGE.open("r", encoding="utf-8") as cfh:
                reader = csv.DictReader(cfh)
                for row in reader:
                    cons_map[row["path"]] = int(row["consumers"])
            for d, m, path in entries:
                fh.write(f"| {d} | {m} | {cons_map.get(path, '?')} | "
                         f"`{path}` |\n")
            fh.write("\n")
    print(f"\nWrote {appendix_path.relative_to(ROOT)}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
