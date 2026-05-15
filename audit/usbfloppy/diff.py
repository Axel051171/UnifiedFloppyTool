#!/usr/bin/env python3
"""
diff.py — compare UFT's USB-Floppy (UFI) constants against the official reference.

Runs extract_uft.py + extract_ref.py, compares key-by-key, writes
evidence.json, prints a human summary, exits non-zero if any FAIL.

Verdict per key:
  PASS        UFT value == reference value
  FAIL        UFT value != reference value
  UNVERIFIED  reference is recalled-grade for a value that cannot be
              independently cross-checked here (none in this provider —
              UFI opcodes are standard SCSI opcodes)
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


def _norm(v):
    """UFT side stores [value, lineno]; unwrap to value."""
    if isinstance(v, list) and len(v) == 2 and isinstance(v[1], int):
        return v[0]
    return v


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
        uval = _norm(u)
        uline = u[1] if (isinstance(u, list) and len(u) == 2) else None
        if r is None:
            rows.append({"key": k, "uft": uval, "ref": None,
                         "verdict": "MISSING", "detail": "no reference entry",
                         "uft_line": uline})
            continue
        verdict = "PASS" if uval == r else "FAIL"
        rows.append({"key": k, "uft": uval, "ref": r, "verdict": verdict,
                     "detail": "" if verdict == "PASS"
                               else f"UFT {uval!r} != ref {r!r}",
                     "uft_line": uline})
    return rows


def main():
    uft = _run("extract_uft.py")
    ref = _run("extract_ref.py")

    sections = {}
    for name in ("opcodes", "cdb_lengths", "geometry"):
        sections[name] = _cmp_section(uft.get(name, {}), ref.get(name, {}))

    all_rows = [r for rows in sections.values() for r in rows]
    counts = {}
    for r in all_rows:
        counts[r["verdict"]] = counts.get(r["verdict"], 0) + 1

    evidence = {
        "provider": "usbfloppy",
        "reference_provenance": ref.get("_provenance"),
        "uft_source": uft.get("_source"),
        "uft_note": uft.get("_note"),
        "counts": counts,
        "sections": sections,
    }
    EVIDENCE.write_text(json.dumps(evidence, indent=2) + "\n", encoding="utf-8")

    print(f"diff.py: usbfloppy — wrote {EVIDENCE.name}")
    for name, rows in sections.items():
        line = "  ".join(f"{r['key'].replace('UFT_UFI_', '')}:{r['verdict'][0]}"
                          for r in rows)
        print(f"  [{name}] {line}")
    print(f"  counts: {counts}")

    fails = [r for r in all_rows if r["verdict"] == "FAIL"]
    if fails:
        print("FAIL:")
        for r in fails:
            print(f"  {r['key']}: {r['detail']}")
        return 1
    print("OK — no FAIL (UFI opcodes match the recalled USB-IF spec; D1 is "
          "wire-protocol-clean. The FAIL is at a different layer: the "
          "platform backend is absent — see REPORT.md D4/D5)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
