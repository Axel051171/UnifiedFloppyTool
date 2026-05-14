#!/usr/bin/env python3
"""
diff.py — compare UFT's fluxengine CLI invocations against the official reference.

Runs extract_uft.py + extract_ref.py, compares key-by-key, writes
evidence.json, prints a human summary, and exits non-zero on any FAIL.

FluxEngine is a CLI-WRAPPER backend. The "diff" is over the fluxengine
command line, not a byte protocol. The reference is the CORRECTED
post-2022 FE CLI form, cross-checked against FE's own source via the
in-repo audit at tests/external_audits/fluxengine/.

Verdict per key:
  PASS        UFT emits a flag/token the reference expects
  FAIL        UFT emits a flag the reference says is wrong, OR omits a
              required one (asymmetry on an expected/recalled row)
  UNVERIFIED  reference is needs-source (e.g. the .flux sample clock)
  MISSING     token present on one side only and the missing side is the
              reference (FAIL-worthy) or UFT (informational)

This is the V2 re-check of the broken-V1 finding from
tests/external_audits/fluxengine/REPORT.md F1+F2. A clean PASS here
confirms the MF-178 CLI correction landed.
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


def _cmp_argv_section(uft_sec, ref_sec):
    """Presence-based: ref values are plain strings ('expected ...') or
       dict {'ref','kind'} for needs-source. uft values are [token, lineno]."""
    rows = []
    keys = sorted(set(uft_sec) | set(ref_sec))
    for k in keys:
        if k.startswith("_"):
            continue
        u = uft_sec.get(k)
        r = ref_sec.get(k)
        if u is not None and r is not None:
            if isinstance(r, dict):
                rows.append({"key": k, "uft": u[0], "ref": r.get("ref"),
                             "verdict": "UNVERIFIED",
                             "detail": r.get("kind"), "uft_line": u[1]})
            else:
                rows.append({"key": k, "uft": u[0], "ref": r,
                             "verdict": "PASS", "detail": "",
                             "uft_line": u[1]})
        elif u is not None and r is None:
            # UFT emits a token the reference does not expect — FAIL:
            # this is exactly the class of bug the V1 audit found (e.g.
            # a stray positional, an unknown flag, the old --revs).
            rows.append({"key": k, "uft": u[0], "ref": None,
                         "verdict": "FAIL",
                         "detail": "UFT emits a token the corrected FE CLI "
                                   "does not expect",
                         "uft_line": u[1]})
        else:  # u is None, r present
            rows.append({"key": k, "uft": None, "ref": r,
                         "verdict": "FAIL",
                         "detail": "reference expects this flag/token but "
                                   "UFT does not emit it"})
    return rows


def _cmp_timing(uft_sec, ref_sec):
    rows = []
    for k in sorted(set(uft_sec) | set(ref_sec)):
        u = uft_sec.get(k)
        r = ref_sec.get(k)
        if u is None:
            rows.append({"key": k, "uft": None, "ref": r, "verdict": "MISSING",
                         "detail": "absent in UFT source"})
            continue
        if r is None:
            rows.append({"key": k, "uft": u[0], "ref": None, "verdict": "MISSING",
                         "detail": "no reference entry"})
            continue
        if isinstance(r, dict):
            rows.append({"key": k, "uft": u[0], "ref": r.get("ref"),
                         "verdict": "UNVERIFIED", "detail": r.get("kind"),
                         "uft_line": u[1]})
            continue
        verdict = "PASS" if u[0] == r else "FAIL"
        rows.append({"key": k, "uft": u[0], "ref": r, "verdict": verdict,
                     "detail": "" if verdict == "PASS"
                               else f"UFT {u[0]!r} != ref {r!r}",
                     "uft_line": u[1]})
    return rows


def main():
    uft = _run("extract_uft.py")
    ref = _run("extract_ref.py")

    sections = {}
    for name in ("read_argv", "write_argv", "rpm_argv",
                 "version_argv", "detect_argv"):
        sections[name] = _cmp_argv_section(uft.get(name, {}), ref.get(name, {}))
    sections["timing"] = _cmp_timing(uft.get("timing", {}), ref.get("timing", {}))

    all_rows = [r for rows in sections.values() for r in rows]
    counts = {}
    for r in all_rows:
        counts[r["verdict"]] = counts.get(r["verdict"], 0) + 1

    evidence = {
        "provider": "fluxengine",
        "integration": "CLI wrapper (fluxengine subprocess via injected runner)",
        "reference_provenance": ref.get("_provenance"),
        "uft_source": uft.get("_source"),
        "counts": counts,
        "sections": sections,
    }
    EVIDENCE.write_text(json.dumps(evidence, indent=2) + "\n", encoding="utf-8")

    print(f"diff.py: fluxengine — wrote {EVIDENCE.name}")
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
    print("OK — no FAIL. The MF-178 CLI correction matches the corrected "
          "FluxEngine CLI form (cross-checked vs tests/external_audits/"
          "fluxengine/). Run mock_fluxengine.py for the FE-flag-semantics "
          "check (a copy lives at tests/external_audits/fluxengine/).")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
