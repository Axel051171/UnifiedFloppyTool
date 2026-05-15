#!/usr/bin/env python3
"""
diff.py — compare UFT's Greaseweazle constants against the official reference.

Runs extract_uft.py + extract_ref.py, compares key-by-key, writes
evidence.json (the snapshot of every compared value + verdict), prints a
human summary, and exits non-zero if any comparison is FAIL.

Verdict per key:
  PASS        UFT value == reference value
  FAIL        UFT value != reference value
  UNVERIFIED  reference is device-reported / not establishable as a constant
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


def _cmp_section(uft_sec, ref_sec):
    """uft values are [int, lineno] or [[..],lineno]; ref values are plain."""
    rows = []
    keys = sorted(set(uft_sec) | set(ref_sec))
    for k in keys:
        if k.startswith("_") or k == "hwtab_vid_pid":
            continue
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
        uval = u[0]
        if isinstance(r, dict):  # device-reported timing
            rows.append({"key": k, "uft": uval, "ref": r.get("ref"),
                         "verdict": "UNVERIFIED",
                         "detail": f"{r.get('kind')} — fallback constant, "
                                   f"not a fixed protocol value",
                         "uft_line": u[1]})
            continue
        verdict = "PASS" if uval == r else "FAIL"
        rows.append({"key": k, "uft": uval, "ref": r, "verdict": verdict,
                     "detail": "" if verdict == "PASS"
                               else f"UFT {uval:#x} != ref {r:#x}",
                     "uft_line": u[1]})
    return rows


def main():
    uft = _run("extract_uft.py")
    ref = _run("extract_ref.py")

    sections = {}
    for name in ("usb_ids", "cmd_opcodes", "ack_codes", "timing"):
        sections[name] = _cmp_section(uft.get(name, {}), ref.get(name, {}))

    all_rows = [r for rows in sections.values() for r in rows]
    counts = {}
    for r in all_rows:
        counts[r["verdict"]] = counts.get(r["verdict"], 0) + 1

    evidence = {
        "provider": "greaseweazle",
        "reference_provenance": ref.get("_provenance"),
        "uft_source": uft.get("_source"),
        "counts": counts,
        "sections": sections,
    }
    EVIDENCE.write_text(json.dumps(evidence, indent=2) + "\n", encoding="utf-8")

    print(f"diff.py: greaseweazle — wrote {EVIDENCE.name}")
    for name, rows in sections.items():
        line = "  ".join(f"{r['key'].replace('UFT_GW_', '')}:{r['verdict'][0]}"
                          for r in rows)
        print(f"  [{name}] {line}")
    print(f"  counts: {counts}")

    fails = [r for r in all_rows if r["verdict"] == "FAIL"]
    if fails:
        print("FAIL:")
        for r in fails:
            print(f"  {r['key']}: {r['detail']}")
        return 1
    print("OK — no FAIL (UNVERIFIED rows are device-reported, expected)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
