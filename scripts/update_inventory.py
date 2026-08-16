#!/usr/bin/env python3
"""Inventory SSOT + format-layer freeze gate (MF-363/364).

Two structural problems this addresses (docs/VERIFICATION_PLAN.md):

1. NUMBER DRIFT — hand-maintained counts in README/CLAUDE.md diverged from
   the code (80/84/88 plugins, "151/151" tests vs. real 205). check_inventory()
   compares every numeric plugin-count claim in the governed docs against the
   code-derived registry and flags stale patterns.

2. FREEZE RULE — no new unverified code in the format/decoder layer.
   check_freeze() compares the registered plugin symbols against
   scripts/format_freeze_baseline.json: a NEW symbol not whitelisted fails.
   check_tiers_fresh() ensures docs/VERIFICATION_TIERS.md is regenerated
   whenever registry/tests/spec inputs change (lockfile semantics).

All three are wired into scripts/check_consistency.py (pre-commit + CI).

Usage:
  python scripts/update_inventory.py            # run all checks, report
  python scripts/update_inventory.py --write-baseline
        # consciously regenerate the freeze baseline (e.g. after removing a
        # plugin, or after a whitelisted addition landed with its lifts)
"""
from __future__ import annotations
import argparse
import json
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from gen_format_list import scan  # noqa: E402

BASELINE = "scripts/format_freeze_baseline.json"

# (file, regex with one numeric group, description) — every match must equal
# the code-derived plugin count.
PLUGIN_COUNT_CLAIMS = [
    ("README.md", r"(\d+)\s+registered plugin parsers", "README parser count"),
    ("CLAUDE.md", r"\((\d+) registered plugins", "CLAUDE.md section header"),
    ("CLAUDE.md", r"(\d+) Plugin-B Registrierungen", "CLAUDE.md metrics"),
]

# Patterns that must not (re)appear in governed docs — known dead claims.
STALE_PATTERNS = [
    (r"\b151/151\b", "stale test count 151/151 (real count: see ctest -N)"),
    (r"\bfully wired plugin parsers\b",
     "'fully wired' capability overclaim (removed in MF-363)"),
]
GOVERNED_DOCS = ["README.md", "CLAUDE.md", ".claude/CLAUDE.md"]


def check_inventory(repo: Path) -> list[str]:
    errors: list[str] = []
    real = len(scan(repo))
    for fname, pattern, desc in PLUGIN_COUNT_CLAIMS:
        f = repo / fname
        if not f.exists():
            continue
        text = f.read_text(encoding="utf-8", errors="replace")
        for m in re.finditer(pattern, text):
            claimed = int(m.group(1))
            if claimed != real:
                errors.append(
                    f"{fname}: {desc} claims {claimed}, code says {real} "
                    f"(fix the doc or run gen_format_list.py)")
    for fname in GOVERNED_DOCS:
        f = repo / fname
        if not f.exists():
            continue
        text = f.read_text(encoding="utf-8", errors="replace")
        for pattern, desc in STALE_PATTERNS:
            if re.search(pattern, text):
                errors.append(f"{fname}: {desc}")
    return errors


def check_freeze(repo: Path) -> list[str]:
    errors: list[str] = []
    bl_path = repo / BASELINE
    if not bl_path.exists():
        return [f"{BASELINE} missing — freeze rule cannot be enforced "
                f"(run update_inventory.py --write-baseline)"]
    bl = json.loads(bl_path.read_text(encoding="utf-8"))
    baseline = set(bl.get("symbols", []))
    whitelist = set(bl.get("whitelist", []))
    current = {p["symbol"] for p in scan(repo)}

    new = current - baseline - whitelist
    for sym in sorted(new):
        errors.append(
            f"EINFRIER-REGEL (MF-363): new format plugin '{sym}' registered. "
            f"No new unverified format/decoder code — the moratorium requires "
            f"lifting existing formats to T1/T1b first "
            f"(docs/VERIFICATION_PLAN.md). If this addition satisfies the "
            f"rule, add '{sym}' to the whitelist in {BASELINE} with lift "
            f"evidence in the same commit.")
    removed = baseline - current
    for sym in sorted(removed):
        errors.append(
            f"freeze baseline: plugin '{sym}' no longer registered — remove "
            f"it from {BASELINE} (--write-baseline) in the same commit.")
    return errors


def check_tiers_fresh(repo: Path) -> list[str]:
    try:
        from gen_verification_tiers import compute_tiers, render_md, GENERATED_DOC
    except ImportError as e:                       # pragma: no cover
        return [f"cannot import gen_verification_tiers: {e}"]
    doc = repo / GENERATED_DOC
    expected = render_md(compute_tiers(repo))
    current = doc.read_text(encoding="utf-8") if doc.exists() else ""
    if current != expected:
        return [f"{GENERATED_DOC} is stale vs. registry/tests/spec inputs — "
                f"run: python scripts/gen_verification_tiers.py --write"]
    return []


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--root", type=Path,
                    default=Path(__file__).resolve().parent.parent)
    ap.add_argument("--write-baseline", action="store_true")
    args = ap.parse_args()
    repo = args.root.resolve()

    if args.write_baseline:
        syms = sorted(p["symbol"] for p in scan(repo))
        bl_path = repo / BASELINE
        old = json.loads(bl_path.read_text(encoding="utf-8")) if bl_path.exists() else {}
        old["symbols"] = syms
        old.setdefault("whitelist", [])
        bl_path.write_text(json.dumps(old, indent=2) + "\n",
                           encoding="utf-8", newline="\n")
        print(f"baseline updated: {len(syms)} symbols")
        return 0

    errors = []
    for label, fn in (("inventory drift", check_inventory),
                      ("format-layer freeze", check_freeze),
                      ("verification tiers stale", check_tiers_fresh)):
        for e in fn(repo):
            errors.append(f"[{label}] {e}")
    if errors:
        print("\n".join(errors))
        return 1
    print("OK: inventory consistent, freeze respected, tiers fresh")
    return 0


if __name__ == "__main__":
    sys.exit(main())
