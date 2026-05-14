#!/usr/bin/env python3
"""
diff.py — compare UFT's DTC CLI invocations against the official reference.

Runs extract_uft.py + extract_ref.py, compares key-by-key, writes
evidence.json, prints a human summary, and exits non-zero on any FAIL.

KryoFlux is a CLI-WRAPPER backend. The "diff" is over the DTC command
line, not a byte protocol. Verdict per key:
  PASS        UFT emits the flag the reference expects, with matching value
  FAIL        UFT emits a flag the reference says is wrong / nonexistent
  UNVERIFIED  reference is needs-source — DTC CLI not vendored / not runnable
  MISSING     flag present on one side only

NOTE: nearly every DTC-CLI row is UNVERIFIED by design — DTC is
closed-source and there is no vendored --help output. See extract_ref.py
PROVENANCE. mock_dtc.py provides a complementary check: it emulates the
flag SET UFT passes and confirms UFT does not pass an obviously-malformed
argv (the KryoFlux analogue of FluxEngine's mock_fluxengine.py).
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


def _cmp_section(uft_sec, ref_sec):
    """uft values are [value, lineno]; ref values are plain OR
       {'ref','kind'} for needs-source."""
    rows = []
    keys = sorted(set(uft_sec) | set(ref_sec))
    for k in keys:
        if k.startswith("_"):
            continue
        u = uft_sec.get(k)
        r = ref_sec.get(k)
        if u is None:
            rows.append({"key": k, "uft": None, "ref": r, "verdict": "MISSING",
                         "detail": "flag expected by reference but not "
                                   "emitted by UFT"})
            continue
        if r is None:
            rows.append({"key": k, "uft": u[0], "ref": None, "verdict": "MISSING",
                         "detail": "UFT emits a flag with no reference entry"})
            continue
        uval = u[0]
        if isinstance(r, dict):  # needs-source
            rows.append({"key": k, "uft": uval, "ref": r.get("ref"),
                         "verdict": "UNVERIFIED",
                         "detail": f"{r.get('kind')} — DTC CLI contract not "
                                   f"vendored / not runnable in this environment",
                         "uft_line": u[1]})
            continue
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
    for name in ("read_argv_flags", "detect_argv_flags", "timing", "geometry"):
        sections[name] = _cmp_section(uft.get(name, {}), ref.get(name, {}))

    all_rows = [r for rows in sections.values() for r in rows]
    counts = {}
    for r in all_rows:
        counts[r["verdict"]] = counts.get(r["verdict"], 0) + 1

    evidence = {
        "provider": "kryoflux",
        "integration": "CLI wrapper (DTC subprocess via injected runner)",
        "reference_provenance": ref.get("_provenance"),
        "uft_source": uft.get("_source"),
        "counts": counts,
        "sections": sections,
    }
    EVIDENCE.write_text(json.dumps(evidence, indent=2) + "\n", encoding="utf-8")

    print(f"diff.py: kryoflux — wrote {EVIDENCE.name}")
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
    print("OK — no FAIL. NOTE: UNVERIFIED rows dominate (DTC is closed-source; "
          "no vendored CLI contract — see extract_ref.py PROVENANCE). "
          "Run mock_dtc.py for the complementary argv-shape check.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
