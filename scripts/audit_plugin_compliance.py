#!/usr/bin/env python3
"""
Prinzip 7 / §M.1 — Plugin-Compliance-Audit.

Scant jede `const uft_format_plugin_t uft_format_plugin_<name> = { ... };`
Definition unter src/formats/ und prüft die Prinzip-7-Pflicht-Metadaten:

  spec_status     — NICHT UFT_SPEC_UNKNOWN
  features        — Array gesetzt (oder is_stub=true als dokumentierte Ausnahme)
  compat_entries  — empfohlen (informational)
  is_stub         — wenn true, zählt das Plugin als "ehrlich unvollständig"
                    und entbindet von den anderen Checks

Exit-Code:
  0  wenn die Zahl der compliance-passenden Plugins >= --min-pass.
  1  wenn weniger — Regression gegenüber der Baseline.

Per Default min-pass = 15 (Stand nach Prinzip-7.1-Populierung).
Bei weiterem Populieren: --min-pass erhöhen.

Beispiel:
  python scripts/audit_plugin_compliance.py            # Status-Ausgabe
  python scripts/audit_plugin_compliance.py --verbose  # pro Plugin
  python scripts/audit_plugin_compliance.py --min-pass 20  # Regression-Check
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path


PLUGIN_RE = re.compile(
    r"const\s+uft_format_plugin_t\s+uft_format_plugin_(\w+)\s*=\s*\{",
)

# Match the balanced struct body by scanning braces (simple approach — the
# codebase uses only designated initializers so no nested `{` that would confuse
# us except for array initializers which do appear in {name,status,note,...}
# feature/compat tables, but those are static-local, not inside the plugin
# literal itself).
def extract_plugin_body(source: str, start: int) -> tuple[int, str] | None:
    """Given an index `start` pointing at the opening `{` of the plugin
    initialiser, return (end_index, body_text)."""
    depth = 0
    i = start
    n = len(source)
    while i < n:
        c = source[i]
        if c == "{":
            depth += 1
        elif c == "}":
            depth -= 1
            if depth == 0:
                return i + 1, source[start : i + 1]
        i += 1
    return None


FIELD_SPEC_STATUS = re.compile(r"\.spec_status\s*=\s*(UFT_SPEC_\w+)")
FIELD_FEATURES = re.compile(r"\.features\s*=\s*(\w+)")
FIELD_COMPAT = re.compile(r"\.compat_entries\s*=\s*(\w+)")
FIELD_IS_STUB = re.compile(r"\.is_stub\s*=\s*(true|false|1|0)\b")


class PluginRecord:
    __slots__ = (
        "name",
        "path",
        "spec_status",
        "features",
        "compat",
        "is_stub",
    )

    def __init__(self, name: str, path: Path):
        self.name = name
        self.path = path
        self.spec_status: str = "UFT_SPEC_UNKNOWN"
        self.features: str | None = None
        self.compat: str | None = None
        self.is_stub: bool = False

    def compliant(self) -> bool:
        """Prinzip 7 minimal compliance.

        Either (a) the plugin declares spec_status AND a feature matrix, or
        (b) it explicitly marks itself is_stub=true so the catalog does not
        silently imply a real parser.
        """
        if self.is_stub:
            return True
        return (
            self.spec_status != "UFT_SPEC_UNKNOWN"
            and self.features is not None
            and self.features != "NULL"
        )

    def has_spec(self) -> bool:
        return self.spec_status != "UFT_SPEC_UNKNOWN"

    def has_features(self) -> bool:
        return self.features is not None and self.features != "NULL"

    def has_compat(self) -> bool:
        return self.compat is not None and self.compat != "NULL"

    def summary(self) -> str:
        bits = []
        bits.append("X" if self.has_spec() else "-")
        bits.append("X" if self.has_features() else "-")
        bits.append("X" if self.has_compat() else "-")
        bits.append("S" if self.is_stub else " ")
        return "".join(bits)


def scan_file(path: Path) -> list[PluginRecord]:
    text = path.read_text(encoding="utf-8", errors="replace")
    records: list[PluginRecord] = []
    for m in PLUGIN_RE.finditer(text):
        name = m.group(1)
        brace_idx = text.index("{", m.end() - 1)
        ext = extract_plugin_body(text, brace_idx)
        if ext is None:
            continue
        _, body = ext
        rec = PluginRecord(name, path)

        sm = FIELD_SPEC_STATUS.search(body)
        if sm:
            rec.spec_status = sm.group(1)
        fm = FIELD_FEATURES.search(body)
        if fm:
            rec.features = fm.group(1)
        cm = FIELD_COMPAT.search(body)
        if cm:
            rec.compat = cm.group(1)
        sb = FIELD_IS_STUB.search(body)
        if sb:
            rec.is_stub = sb.group(1) in ("true", "1")

        records.append(rec)
    return records


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument(
        "--root",
        type=Path,
        default=Path(__file__).resolve().parent.parent,
        help="Project root (default: parent of scripts/)",
    )
    ap.add_argument(
        "--verbose",
        "-v",
        action="store_true",
        help="List every plugin with its field status",
    )
    ap.add_argument(
        "--min-pass",
        type=int,
        default=5,
        help="Minimum number of fully-compliant plugins (spec_status AND "
        "features OR is_stub=true). Exits 1 if fewer found. Current "
        "baseline: 5 (ADF, HFE, IPF, STX, WOZ as of 2026-04-18).",
    )
    ap.add_argument(
        "--min-spec-status",
        type=int,
        default=15,
        help="Minimum number of plugins with spec_status declared. Weaker "
        "check than --min-pass; used to catch 7.1 regressions separately "
        "from 7.2. Baseline: 15.",
    )
    ap.add_argument(
        "--fail-on-regression",
        action="store_true",
        help="Synonym for --min-pass — just make intent explicit in CI",
    )
    args = ap.parse_args()

    formats_root = args.root / "src" / "formats"
    if not formats_root.is_dir():
        print(f"error: {formats_root} does not exist", file=sys.stderr)
        return 2

    records: list[PluginRecord] = []
    for c_file in sorted(formats_root.rglob("*.c")):
        records.extend(scan_file(c_file))

    if not records:
        print("error: no uft_format_plugin_t definitions found", file=sys.stderr)
        return 2

    compliant = sum(1 for r in records if r.compliant())
    with_spec = sum(1 for r in records if r.has_spec())
    with_feat = sum(1 for r in records if r.has_features())
    with_cmp = sum(1 for r in records if r.has_compat())
    stubs = sum(1 for r in records if r.is_stub)

    total = len(records)
    print("Plugin-Compliance-Audit (Prinzip 7 / §M.1)")
    print("=" * 48)
    print(f"  plugins scanned          : {total}")
    print(f"  with spec_status set     : {with_spec:3d} ({100*with_spec//total}%)")
    print(f"  with feature matrix      : {with_feat:3d} ({100*with_feat//total}%)")
    print(f"  with compat matrix       : {with_cmp:3d} ({100*with_cmp//total}%)")
    print(f"  is_stub = true           : {stubs:3d}")
    print(f"  principle-7 compliant    : {compliant:3d} ({100*compliant//total}%)")
    print(f"  baseline --min-pass      : {args.min_pass}")

    if args.verbose:
        print()
        print("legend: [spec/feat/compat/stub]  plugin (spec_status)")
        print("-" * 48)
        for r in sorted(records, key=lambda r: (not r.compliant(), r.name)):
            mark = r.summary()
            status = r.spec_status.replace("UFT_SPEC_", "")
            print(f"  [{mark}]  {r.name:25s}  {status}")

    regressions: list[str] = []
    if compliant < args.min_pass:
        regressions.append(
            f"compliant count {compliant} below baseline {args.min_pass} "
            "(feature matrix and/or spec_status missing on a previously-"
            "populated plugin)"
        )
    if with_spec < args.min_spec_status:
        regressions.append(
            f"plugins with spec_status {with_spec} below baseline "
            f"{args.min_spec_status} (§7.1 regression)"
        )

    if regressions:
        print()
        for msg in regressions:
            print(f"FAIL: {msg}", file=sys.stderr)
        print(
            "      See docs/KNOWN_ISSUES.md §7.1 / §7.2 / §M.1.",
            file=sys.stderr,
        )
        return 1

    print()
    print(
        f"OK: {compliant} fully-compliant plugins (>= {args.min_pass}), "
        f"{with_spec} with spec_status (>= {args.min_spec_status}). "
        "Remaining plugins are Prinzip-7-Lücken — track in KNOWN_ISSUES §7.1."
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
