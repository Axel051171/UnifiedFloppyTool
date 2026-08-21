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
GOVERNED_DOCS = ["README.md", "CLAUDE.md", ".claude/CLAUDE.md",
                 "docs/MASTER_PLAN.md"]

# ── Derived claims (MF-410) ────────────────────────────────────────────────
#
# MASTER_PLAN.md drifted for months because nothing checked it: it carried a
# pinned struct size of 216 (real: 224 since MF-404) and "38 remaining test
# exclusions" (real: 5), and listed three finished P0/P1 items as open.
#
# docs/KNOWN_ISSUES.md is deliberately NOT in GOVERNED_DOCS: it records
# history ("pre-tag pass rate 151/151"), and a stale-pattern sweep would flag
# those correct historical entries as errors.
#
# The checks below therefore do not scan for numbers in general. Each one
# targets ONE sentence whose value has a machine-readable source of truth, in
# the same shape as PLUGIN_COUNT_CLAIMS above. A historical line elsewhere in
# the file is untouched.


def _pinned_plugin_struct_size(repo: Path):
    """Authoritative: the _Static_assert in the plugin header."""
    f = repo / "include/uft/uft_format_plugin.h"
    if not f.exists():
        return None
    m = re.search(r"sizeof\(uft_format_plugin_t\)\s*==\s*(\d+)",
                  f.read_text(encoding="utf-8", errors="replace"))
    return int(m.group(1)) if m else None


def _excluded_test_count(repo: Path):
    """Authoritative: active entries in tests/CMakeLists.txt EXCLUDED_TESTS.

    Commented-out entries (re-enabled tests are kept as documentation) do not
    count; that is exactly the distinction the stale "38" missed."""
    f = repo / "tests/CMakeLists.txt"
    if not f.exists():
        return None
    text = f.read_text(encoding="utf-8", errors="replace")
    m = re.search(r"set\(EXCLUDED_TESTS(.*?)^\)", text, re.S | re.M)
    if not m:
        return None
    count = 0
    for line in m.group(1).splitlines():
        stripped = line.strip()
        if stripped.startswith("#"):
            continue
        code = stripped.split("#", 1)[0]
        count += len(re.findall(r'"test_[A-Za-z0-9_]+"', code))
    return count


# (file, regex with one numeric group, source callable, description)
DERIVED_CLAIMS = [
    ("docs/MASTER_PLAN.md", r"Static_assert pinnt `sizeof == (\d+)`",
     _pinned_plugin_struct_size,
     "MASTER_PLAN UFT-004: pinned sizeof(uft_format_plugin_t)"),
    ("docs/MASTER_PLAN.md", r"noch (\d+) Exclusions",
     _excluded_test_count,
     "MASTER_PLAN UFT-T04: active EXCLUDED_TESTS entries"),
]


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

    # Derived claims: one sentence each, checked against its source (MF-410).
    for fname, pattern, source, desc in DERIVED_CLAIMS:
        f = repo / fname
        if not f.exists():
            continue
        truth = source(repo)
        if truth is None:
            errors.append(f"{fname}: {desc} — source of truth unreadable")
            continue
        text = f.read_text(encoding="utf-8", errors="replace")
        found = re.findall(pattern, text)
        if not found:
            errors.append(
                f"{fname}: {desc} — the checked sentence is gone; either "
                f"restore it or drop the entry from DERIVED_CLAIMS")
            continue
        for claimed in found:
            if int(claimed) != truth:
                errors.append(
                    f"{fname}: {desc} claims {claimed}, source says {truth}")

    # README tier summary must match the computed tiers (drift gate).
    readme = repo / "README.md"
    if readme.exists():
        m = re.search(r"T1=(\d+), T1b=(\d+), T2=(\d+), T3=(\d+)",
                      readme.read_text(encoding="utf-8", errors="replace"))
        if m:
            try:
                from gen_verification_tiers import compute_tiers
                counts: dict[str, int] = {}
                for r in compute_tiers(repo):
                    counts[r["tier"]] = counts.get(r["tier"], 0) + 1
                claimed = tuple(int(g) for g in m.groups())
                actual = tuple(counts.get(t, 0) for t in ("T1", "T1b", "T2", "T3"))
                if claimed != actual:
                    errors.append(
                        f"README.md: tier summary claims T1={claimed[0]}, "
                        f"T1b={claimed[1]}, T2={claimed[2]}, T3={claimed[3]} "
                        f"but computed tiers are T1={actual[0]}, "
                        f"T1b={actual[1]}, T2={actual[2]}, T3={actual[3]}")
            except ImportError:
                pass
    return errors


# Same-named headers that are ALLOWED to keep a shared include guard, with the
# reason. Everything else is a defect: the second header is silently skipped,
# and which one a translation unit gets depends on include order.
# MF-455: leer.
#
# Der einzige Eintrag war uft_platform.h — include/uft/uft_platform.h und
# include/uft/compat/uft_platform.h trugen beide UFT_PLATFORM_H, weshalb der
# grosse Header die compat-Schicht nie bekam und die zwei .c-Dateien, die
# compat direkt einbinden, den grossen nie sahen. Die Ausnahme stand hier, weil
# ein blosses Umbenennen des Guards ~9 doppelte Makros freigelegt und den Build
# gebrochen haette (MF-416: 187 von 197 Tests).
#
# Aufgeloest durch Trennen statt Umbenennen: uft/compat/uft_platform_base.h
# traegt die gefahrlose Haelfte und ist baumweit sichtbar,
# uft/compat/uft_platform.h behaelt die namensumdefinierenden POSIX-Shims und
# einen eigenen Guard UFT_COMPAT_PLATFORM_H. Siehe KNOWN_ISSUES ARCH-1.
GUARD_COLLISION_ALLOWED = {}


def check_include_guards(repo: Path) -> list[str]:
    """Same-named headers must not share an include guard (MF-411).

    Three tests sat excluded for months because they included phantom twins of
    real headers; the twin pair also shared a guard, so whichever came first
    silently won. This makes a new occurrence fail at commit time."""
    import collections
    inc = repo / "include"
    if not inc.is_dir():
        return []
    by_name: dict[str, list[Path]] = collections.defaultdict(list)
    for p in inc.rglob("*.h"):
        by_name[p.name].append(p)

    errors: list[str] = []
    for name, paths in sorted(by_name.items()):
        if len(paths) < 2:
            continue
        guards: dict[str, list[Path]] = collections.defaultdict(list)
        for p in paths:
            m = re.search(r"#ifndef\s+(\w+)",
                          p.read_text(encoding="utf-8", errors="replace"))
            if m:
                guards[m.group(1)].append(p)
        for guard, ps in guards.items():
            if len(ps) < 2:
                continue
            if name in GUARD_COLLISION_ALLOWED:
                continue
            rel = ", ".join(str(x.relative_to(repo)).replace("\\", "/")
                            for x in ps)
            errors.append(
                f"include guard {guard} is shared by same-named headers "
                f"({rel}) — the second is silently skipped; give the nested "
                f"one a path-derived guard")
    return errors


def check_compat_claims(repo: Path) -> list[str]:
    """A compatibility verdict must carry its evidence (MF-414).

    Principle 6 lets a plugin declare, per target consumer, whether its export
    is accepted. The one populated matrix in the tree turned out to be the
    illustrative example from docs/DESIGN_PRINCIPLES.md copied verbatim into
    shipping code — six confident verdicts, including "85% of test disks
    round-trip cleanly" for hardware the project does not own.

    Presence alone therefore proves nothing, and that is all
    audit_plugin_compliance.py could check. This rule is what distinguishes a
    measurement from a sentence: any verdict other than UNTESTED must name
    either a CI job (ci_tested = true) or a date when someone ran it
    (test_date). UNTESTED needs no evidence — it IS the absence of evidence."""
    errors: list[str] = []
    # One brace-initialised entry:
    #   { "consumer", UFT_EMU_*, note-or-NULL, date-or-NULL, ci_tested }
    # The note may be split over several adjacent string literals.
    STR = r'"[^"]*"(?:\s*"[^"]*")*'
    entry = re.compile(
        r'\{\s*(?P<consumer>' + STR + r')\s*,\s*'
        r'(?P<status>UFT_EMU_[A-Z]+)\s*,\s*'
        r'(?P<note>NULL|' + STR + r')\s*,\s*'
        r'(?P<date>NULL|' + STR + r')\s*,\s*'
        r'(?P<ci>true|false)\s*\}', re.S)

    for src in sorted((repo / "src").rglob("*.c")):
        text = src.read_text(encoding="utf-8", errors="replace")
        if "uft_plugin_compat_entry_t" not in text:
            continue
        rel = str(src.relative_to(repo)).replace("\\", "/")
        for m in entry.finditer(text):
            status = m.group("status")
            if status == "UFT_EMU_UNTESTED":
                continue
            if status == "UFT_EMU_COMPATIBLE" and m.group("note") == "NULL":
                pass  # a note is optional for a plain COMPATIBLE
            if status in ("UFT_EMU_PARTIAL", "UFT_EMU_INCOMPATIBLE") and                     m.group("note") == "NULL":
                errors.append(
                    f"{rel}: {m.group('consumer')} is {status} without a note "
                    f"explaining the limitation")
            if m.group("ci") != "true" and m.group("date") == "NULL":
                errors.append(
                    f"{rel}: {m.group('consumer')} claims {status} with no "
                    f"evidence — set ci_tested=true or record a test_date, "
                    f"or say UFT_EMU_UNTESTED")
    return errors


MACRO_BASELINE = "scripts/macro_conflict_baseline.json"

_MACRO_DEF = re.compile(
    r"^[ \t]*#[ \t]*define[ \t]+([A-Za-z_][A-Za-z0-9_]{2,})[ \t]+(\S.*?)[ \t]*$", re.M)

# Function-like macros: `#define NAME(a, b) body`, no space before the paren.
# _MACRO_DEF cannot match these (it demands whitespace after the name), and
# _conflicting_macros used to skip them outright. UFT_PREFETCH sat in that
# blind spot with two different bodies while gcc warned about it on every
# single build (MF-431).
_MACRO_FN_DEF = re.compile(
    r"^[ \t]*#[ \t]*define[ \t]+([A-Za-z_][A-Za-z0-9_]{2,})\(([^)\n]*)\)[ \t]*"
    r"(.*?)[ \t]*$", re.M)


def _macro_value(raw: str):
    """Canonical form of a macro body, or None if it is not a plain literal.

    880 and 880u are the same number; 0x1A and 0x1a the same byte; 16 and 0x10
    the same value. Those differences are spelling, not disagreement."""
    v = re.split(r"/\*|//", raw)[0].strip()
    if not v:
        return None
    if v.startswith('"') and v.endswith('"'):
        body = re.sub(r"\\x([0-9A-Fa-f]{2})",
                      lambda m: "\\x" + m.group(1).lower(), v[1:-1])
        return ("str", body)
    n = re.sub(r"[uUlL]+$", "", v.strip("()").strip())
    try:
        if n.lower().startswith("0x"):
            return ("num", int(n, 16))
        if re.fullmatch(r"-?\d+", n):
            return ("num", int(n, 10))
    except ValueError:
        pass
    return ("expr", v)


def _macro_fn_value(params: str, body: str):
    """Canonical form of a function-like macro body.

    Parameter NAMES are not part of the meaning: `#define F(x) ((x)+1)` and
    `#define F(a) ((a)+1)` are the same macro. They are rewritten to positional
    markers so only a real difference in the body counts. Arity is part of the
    signature and is kept."""
    names = [p.strip() for p in params.split(",") if p.strip()]
    b = re.split(r"/\*|//", body)[0].strip()
    if not b:
        return None
    for i, nm in enumerate(names):
        if re.fullmatch(r"[A-Za-z_]\w*", nm):
            b = re.sub(r"\b" + re.escape(nm) + r"\b", f"${i}", b)
    b = re.sub(r"\s+", " ", b)
    return ("fn", len(names), b)


def _conflicting_macros(repo: Path) -> dict[str, list[str]]:
    """Macro names whose value differs BETWEEN headers (KNOWN_ISSUES ARCH-2).

    Deliberately narrow, so the result stays worth reading:
      - definitions inside one file are collapsed (they are #if/#else branches
        of the same decision, only one is ever live)
      - a name is only reported if the set of values differs across files
      - names where every definition but one sits behind #ifndef are skipped:
        first include wins and the outcome is stable

    Function-like macros ARE included since MF-431. They used to be skipped as
    presumed-equivalent noise, and UFT_PREFETCH lived in that gap with two
    different bodies while the compiler warned about it on every build. Bodies
    are compared with parameter names normalised away, so a rename is not a
    finding but a different body is.
    """
    import collections
    per_file: dict[str, dict[str, set]] = collections.defaultdict(dict)
    guarded: dict[str, list[bool]] = collections.defaultdict(list)

    def record(name: str, val, text: str, start: int, lines, rel: str) -> None:
        if val is None:
            return
        per_file[name].setdefault(rel, set()).add(val)
        ln = text[:start].count("\n") + 1
        g = any(re.match(r"\s*#\s*ifndef\s+" + re.escape(name) + r"\b",
                         lines[k]) for k in range(max(0, ln - 4), ln - 1))
        guarded[name].append(g)

    for p in sorted((repo / "include").rglob("*.h")):
        text = p.read_text(encoding="utf-8", errors="replace")
        lines = text.splitlines()
        rel = str(p.relative_to(repo)).replace("\\", "/")
        for m in _MACRO_DEF.finditer(text):
            name, raw = m.group(1), m.group(2)
            if raw.lstrip().startswith("(") and re.match(
                    r"^\([A-Za-z_][A-Za-z0-9_]*\s*[,)]", raw.lstrip()):
                continue          # `#define N (x)` — handled by _MACRO_FN_DEF
            record(name, _macro_value(raw), text, m.start(), lines, rel)
        for m in _MACRO_FN_DEF.finditer(text):
            record(m.group(1), _macro_fn_value(m.group(2), m.group(3)),
                   text, m.start(), lines, rel)

    out: dict[str, list[str]] = {}
    for name, files in per_file.items():
        if len(files) < 2:
            continue
        value_sets = {frozenset(v) for v in files.values()}
        if len(value_sets) < 2:
            continue                          # every file agrees
        if guarded[name].count(False) <= 1:
            continue                          # #ifndef chain, first wins
        out[name] = sorted(files)
    return out


def check_macro_conflicts(repo: Path) -> list[str]:
    """Fail on a NEW macro that two headers define differently (MF-419).

    93 raw findings were triaged once (KNOWN_ISSUES ARCH-2); the survivors are
    recorded in the baseline so this gate reports only what is new. A category
    that fires 93 times on day one gets switched off instead of worked off.
    """
    current = _conflicting_macros(repo)
    bl_path = repo / MACRO_BASELINE
    if not bl_path.exists():
        return [f"{MACRO_BASELINE} missing — run "
                f"update_inventory.py --write-macro-baseline"]
    known = set(json.loads(bl_path.read_text(encoding="utf-8")).get("accepted", []))

    errors = []
    for name in sorted(set(current) - known):
        where = ", ".join(current[name])
        errors.append(
            f"macro {name} is defined with different values in {where} — "
            f"reconcile them, or add the name to {MACRO_BASELINE} with a reason")
    for name in sorted(known - set(current)):
        errors.append(
            f"macro {name} no longer conflicts — remove it from "
            f"{MACRO_BASELINE} so the baseline keeps meaning something")
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


def check_corpus_manifest(repo: Path) -> list[str]:
    """Integrity of the reference corpus: every manifest entry's file must
    match its recorded sha256. Tracked files (tests/corpus_free/) must exist;
    gitignored local-corpus files (tests/corpus/) are skipped when absent."""
    import hashlib
    errors: list[str] = []
    mf = repo / "tests" / "corpus_manifest" / "manifest.json"
    if not mf.exists():
        return []
    try:
        manifest = json.loads(mf.read_text(encoding="utf-8"))
    except json.JSONDecodeError as e:
        return [f"corpus manifest: invalid JSON: {e}"]
    for entry in manifest.get("images", []):
        rel = entry.get("file", "")
        f = repo / rel
        if not f.exists():
            if rel.startswith("tests/corpus_free/"):
                errors.append(f"corpus: tracked image missing: {rel}")
            continue                      # local-only corpus, absent is fine
        digest = hashlib.sha256(f.read_bytes()).hexdigest()
        if digest != entry.get("sha256"):
            errors.append(
                f"corpus: sha256 mismatch for {rel} — file was modified or "
                f"manifest is stale (expected {entry.get('sha256','')[:16]}…, "
                f"got {digest[:16]}…)")
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
                      ("corpus integrity", check_corpus_manifest),
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
