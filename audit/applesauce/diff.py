#!/usr/bin/env python3
"""
diff.py — compare UFT's Applesauce constants against the official reference.

Runs extract_uft.py + extract_ref.py, compares key-by-key, writes
evidence.json, prints a human summary, exits non-zero on any FAIL.

Verdict per key:
  PASS        UFT value == reference value
  FAIL        UFT value != reference value
  UNVERIFIED  reference is needs-source (command-vocab grammar) — the
              token exists in UFT but cannot be byte-diffed against an
              official grammar
  MISSING     key present on one side only
"""

import json
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
EVIDENCE = HERE / "evidence.json"


def _run(script):
    out = subprocess.run([sys.executable, str(HERE / script)],
                         capture_output=True, text=True, check=False)
    if out.returncode != 0:
        print(f"diff.py: {script} failed:\n{out.stderr}", file=sys.stderr)
        sys.exit(2)
    return json.loads(out.stdout)


def _uft_scalar(u):
    """uft values are [int, lineno] or [int, lineno, char]."""
    return u[0] if u is not None else None


def _cmp_section(uft_sec, ref_sec):
    rows = []
    keys = sorted(set(uft_sec) | set(ref_sec))
    for k in keys:
        if k.startswith("_"):
            continue
        u = uft_sec.get(k)
        r = ref_sec.get(k)
        if u is None:
            rows.append({"key": k, "uft": None, "ref": r,
                         "verdict": "MISSING", "detail": "absent in UFT source"})
            continue
        uval = _uft_scalar(u)
        if r is None:
            rows.append({"key": k, "uft": uval, "ref": None,
                         "verdict": "MISSING", "detail": "no reference entry",
                         "uft_line": u[1]})
            continue
        if isinstance(r, dict):
            kind = r.get("kind", "")
            if kind == "needs-source":
                rows.append({"key": k, "uft": uval, "ref": r.get("ref"),
                             "verdict": "UNVERIFIED",
                             "detail": "command-string grammar is "
                                       "needs-source — token present in "
                                       "UFT, not byte-diffable",
                             "uft_line": u[1]})
                continue
            r = r.get("ref")
        verdict = "PASS" if uval == r else "FAIL"
        rows.append({"key": k, "uft": uval, "ref": r, "verdict": verdict,
                     "detail": "" if verdict == "PASS"
                               else f"UFT {uval!r} != ref {r!r}",
                     "uft_line": u[1]})
    return rows


def main():
    uft = _run("extract_uft.py")
    ref = _run("extract_ref.py")

    sections = {}
    for name in ("response_chars", "timing", "buffers", "serial_params",
                 "command_vocab"):
        sections[name] = _cmp_section(uft.get(name, {}), ref.get(name, {}))

    all_rows = [r for rows in sections.values() for r in rows]
    counts = {}
    for r in all_rows:
        counts[r["verdict"]] = counts.get(r["verdict"], 0) + 1

    evidence = {
        "provider": "applesauce",
        "reference_provenance": ref.get("_provenance"),
        "uft_source": uft.get("_source"),
        "counts": counts,
        "sections": sections,
        "note": "command_vocab rows are UNVERIFIED — the Applesauce "
                "line-protocol command grammar is needs-source "
                "(wiki.applesaucefdc.com not vendored). The tokens UFT "
                "uses are internally consistent but not byte-diffed "
                "against an official grammar. See REPORT.md D1.",
    }
    EVIDENCE.write_text(json.dumps(evidence, indent=2) + "\n", encoding="utf-8")

    print(f"diff.py: applesauce — wrote {EVIDENCE.name}")
    for name, rows in sections.items():
        line = "  ".join(f"{r['key']}:{r['verdict'][0]}" for r in rows)
        print(f"  [{name}] {line}")
    print(f"  counts: {counts}")

    fails = [r for r in all_rows if r["verdict"] == "FAIL"]
    if fails:
        print("FAIL:")
        for r in fails:
            print(f"  {r['key']}: {r['detail']}")
        return 1
    print("OK — no FAIL (UNVERIFIED rows are needs-source command grammar)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
