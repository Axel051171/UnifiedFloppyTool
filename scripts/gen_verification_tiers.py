#!/usr/bin/env python3
"""Generate the per-format verification-tier table (T1/T1b/T2/T3).

SSOT for the tier definitions: docs/VERIFICATION_PLAN.md (MF-363/364).

  T1  — test against a REAL reference image (hardware dump / original file)
  T1b — test against a CROSS-TOOL image (produced by a canonical third-party
        tool such as VICE/WinUAE/HxC/SAMdisk, read back by UFT)
  T2  — synthetic round-trip test AND the byte layout was verified against an
        authoritative external reference (docs/spec_verification.json entry)
  T3  — unverified: no test, or a synthetic test WITHOUT spec verification
        (green tests against invented specs are exactly the fabrication trap
        that produced FMT-2/3/10/11/12 — a test alone proves self-consistency,
        not real-world correctness)

Inputs (all in-repo):
  - plugin registry        : code scan via gen_format_list.scan()
  - tests/CMakeLists.txt   : test -> plugin-source mapping (target_sources)
  - tests/test_*.c[pp]     : `uft_format_plugin_<sym>` references
  - docs/spec_verification.json     : per-plugin spec-verification evidence
  - tests/corpus_manifest/manifest.json : corpus entries (T1/T1b evidence)

Usage:
  python scripts/gen_verification_tiers.py            # summary counts
  python scripts/gen_verification_tiers.py --md       # markdown table (stdout)
  python scripts/gen_verification_tiers.py --write    # (re)generate docs/VERIFICATION_TIERS.md
  python scripts/gen_verification_tiers.py --check    # exit 1 if docs/VERIFICATION_TIERS.md is stale
"""
from __future__ import annotations
import argparse
import json
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from gen_format_list import scan  # noqa: E402

GENERATED_DOC = "docs/VERIFICATION_TIERS.md"
HEADER = (
    "# Format-Verifikations-Stufen (generiert)\n\n"
    "**NICHT von Hand editieren** — erzeugt von "
    "`scripts/gen_verification_tiers.py` (MF-364). Definitionen: "
    "[`VERIFICATION_PLAN.md`](VERIFICATION_PLAN.md).\n\n"
    "Ein T3 mit Test-Eintrag bedeutet: es existiert ein synthetischer Test, "
    "aber die Byte-Struktur wurde nie gegen eine autoritative externe Quelle "
    "verifiziert — genau die Konstellation, in der die fabrizierten Parser "
    "(FMT-2/3/10/11/12) gruen waren.\n"
)


def _tests_from_cmake(repo: Path) -> dict[str, set[str]]:
    """Map plugin source path -> set of test names that target_sources it."""
    cml = repo / "tests" / "CMakeLists.txt"
    mapping: dict[str, set[str]] = {}
    if not cml.exists():
        return mapping
    cur_test = None
    for line in cml.read_text(encoding="utf-8", errors="replace").splitlines():
        line = line.split("#", 1)[0]          # strip comments
        m = re.search(r'STREQUAL\s+"(test_\w+)"', line)
        if m:
            cur_test = m.group(1)
            continue
        # Inside a test's elseif block, any src/formats source path is a
        # target_sources argument — target_sources( itself may be on a
        # previous line (multi-line blocks, e.g. the SCP tests).
        if cur_test:
            for path in re.findall(r'(src/formats/[\w/\.]+\.c)\b', line):
                mapping.setdefault(path, set()).add(cur_test)
    return mapping


def _tests_by_symbol_ref(repo: Path) -> dict[str, set[str]]:
    """Map plugin symbol -> set of test names whose source references it."""
    out: dict[str, set[str]] = {}
    tdir = repo / "tests"
    for f in list(tdir.glob("test_*.c")) + list(tdir.glob("test_*.cpp")):
        text = f.read_text(encoding="utf-8", errors="replace")
        for sym in set(re.findall(r"uft_format_plugin_(\w+)\b", text)):
            out.setdefault(sym, set()).add(f.stem)
    return out


def _excluded_tests(repo: Path) -> set[str]:
    cml = repo / "tests" / "CMakeLists.txt"
    if not cml.exists():
        return set()
    text = cml.read_text(encoding="utf-8", errors="replace")
    m = re.search(r"set\(EXCLUDED_TESTS(.*?)\)", text, re.S)
    if not m:
        return set()
    body = re.sub(r"#.*", "", m.group(1))
    return set(re.findall(r"(test_\w+)", body))


def _load_json(path: Path) -> dict:
    if not path.exists():
        return {}
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as e:
        print(f"ERROR: {path}: invalid JSON: {e}", file=sys.stderr)
        raise SystemExit(2)


def compute_tiers(repo: Path) -> list[dict]:
    plugins = scan(repo)
    cmake_map = _tests_from_cmake(repo)
    symref_map = _tests_by_symbol_ref(repo)
    excluded = _excluded_tests(repo)
    spec = _load_json(repo / "docs" / "spec_verification.json")
    known_syms = {p["symbol"] for p in plugins}
    for key, entry in spec.items():
        if key.startswith("_") or not isinstance(entry, dict):
            continue
        if key not in known_syms and not entry.get("nonplugin"):
            print(f"WARN: spec_verification.json key '{key}' matches no "
                  f"registered plugin (add \"nonplugin\": true if intended)",
                  file=sys.stderr)
    manifest = _load_json(repo / "tests" / "corpus_manifest" / "manifest.json")
    corpus_by_fmt: dict[str, list[dict]] = {}
    for entry in manifest.get("images", []):
        corpus_by_fmt.setdefault(entry.get("format", ""), []).append(entry)

    # Directory credit: a test that target_sources a MODULE file of a format
    # (e.g. apple/uft_woz.c while the plugin struct lives in uft_woz_plugin.c)
    # counts for the plugin — but ONLY when that directory contains exactly one
    # plugin, so a shared directory can never over-credit.
    dir_plugins: dict[str, list[str]] = {}
    for p in plugins:
        d = "/".join(p["file"].split("/")[:-1])
        dir_plugins.setdefault(d, []).append(p["symbol"])
    dir_tests: dict[str, set[str]] = {}
    for path, tests_ in cmake_map.items():
        d = "/".join(path.split("/")[:-1])
        if len(dir_plugins.get(d, [])) == 1:
            dir_tests.setdefault(dir_plugins[d][0], set()).update(tests_)

    rows = []
    for p in plugins:
        sym = p["symbol"]
        tests = set(symref_map.get(sym, set()))
        tests |= cmake_map.get(p["file"], set())
        tests |= dir_tests.get(sym, set())
        tests = {t for t in tests if t not in excluded}

        spec_entry = spec.get(sym)
        corpus = corpus_by_fmt.get(sym, [])
        real = [c for c in corpus
                if c.get("origin") == "real" and c.get("test") in tests]
        xtool = [c for c in corpus
                 if c.get("origin") == "cross-tool" and c.get("test") in tests]

        if real:
            tier = "T1"
        elif xtool:
            tier = "T1b"
        elif tests and spec_entry:
            tier = "T2"
        else:
            tier = "T3"

        rows.append({
            "symbol": sym,
            "name": p["name"],
            "file": p["file"],
            "tier": tier,
            "tests": sorted(tests),
            "spec": (spec_entry or {}).get("spec_source", ""),
            "evidence": (spec_entry or {}).get("evidence", ""),
            "corpus": len(real) + len(xtool),
        })
    order = {"T1": 0, "T1b": 1, "T2": 2, "T3": 3}
    rows.sort(key=lambda r: (order[r["tier"]], r["symbol"]))
    return rows


def render_md(rows: list[dict]) -> str:
    counts = {}
    for r in rows:
        counts[r["tier"]] = counts.get(r["tier"], 0) + 1
    lines = [HEADER]
    lines.append("## Zusammenfassung\n")
    lines.append("| Stufe | Formate |")
    lines.append("|---|---|")
    for t in ("T1", "T1b", "T2", "T3"):
        lines.append(f"| {t} | {counts.get(t, 0)} |")
    lines.append(f"| **gesamt** | **{len(rows)}** |")
    lines.append("")
    lines.append("## Pro Format\n")
    lines.append("| Plugin | Stufe | Tests | Spec-Quelle | Evidenz | Korpus-Images |")
    lines.append("|---|---|---|---|---|---|")
    for r in rows:
        tests = ", ".join(f"`{t}`" for t in r["tests"]) or "—"
        spec = r["spec"] or "—"
        ev = r["evidence"] or "—"
        lines.append(f"| `{r['symbol']}` | **{r['tier']}** | {tests} | "
                     f"{spec} | {ev} | {r['corpus'] or '—'} |")
    lines.append("")
    return "\n".join(lines)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--root", default=Path(__file__).resolve().parent.parent,
                    type=Path)
    ap.add_argument("--md", action="store_true")
    ap.add_argument("--write", action="store_true")
    ap.add_argument("--check", action="store_true",
                    help="exit 1 if the generated doc is stale vs. reality")
    args = ap.parse_args()
    repo = args.root.resolve()

    rows = compute_tiers(repo)
    md = render_md(rows)
    doc = repo / GENERATED_DOC

    if args.md:
        print(md)
        return 0
    if args.write:
        doc.write_text(md, encoding="utf-8", newline="\n")
        print(f"wrote {GENERATED_DOC} ({len(rows)} plugins)")
        return 0
    if args.check:
        current = doc.read_text(encoding="utf-8") if doc.exists() else ""
        if current != md:
            print(f"STALE: {GENERATED_DOC} does not match reality — "
                  f"run: python scripts/gen_verification_tiers.py --write")
            return 1
        print(f"OK: {GENERATED_DOC} up to date")
        return 0

    counts: dict[str, int] = {}
    for r in rows:
        counts[r["tier"]] = counts.get(r["tier"], 0) + 1
    print(f"Verification tiers ({len(rows)} plugins):")
    for t in ("T1", "T1b", "T2", "T3"):
        print(f"  {t:3s}: {counts.get(t, 0)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
