#!/usr/bin/env python3
"""
diff.py — compare UFT's FC5025 constants against the official reference.

Runs extract_uft.py + extract_ref.py, compares key-by-key, writes
evidence.json, prints a human summary, exits non-zero on any FAIL.

Verdict per key:
  PASS        UFT value == reference value
  FAIL        UFT value != reference value
  UNVERIFIED  reference is needs-source / uft-internal — not a fixed
              external wire value the audit can byte-diff
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
    """uft values are [int, lineno] or [[vid,pid], lineno]."""
    return u[0] if u is not None else None


def _cmp_section(uft_sec, ref_sec):
    rows = []
    keys = sorted(set(uft_sec) | set(ref_sec))
    for k in keys:
        if k.startswith("_") or k in ("cpp_cited_vid_pid",):
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
            # reference is a value + kind: compare value but mark per kind
            refval = r.get("ref")
            kind = r.get("kind", "")
            if kind in ("uft-internal-ordinal", "internal-consistency"):
                verdict = "PASS" if uval == refval else "FAIL"
                detail = r.get("note", "")
                if verdict == "FAIL":
                    detail = f"UFT {uval} != contract {refval} — {detail}"
            else:
                verdict = "UNVERIFIED"
                detail = f"{kind}: {r.get('note', '')}"
            rows.append({"key": k, "uft": uval, "ref": refval,
                         "verdict": verdict, "detail": detail,
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
    for name in ("usb_ids", "format_enum", "defaults"):
        sections[name] = _cmp_section(uft.get(name, {}), ref.get(name, {}))

    all_rows = [r for rows in sections.values() for r in rows]
    counts = {}
    for r in all_rows:
        counts[r["verdict"]] = counts.get(r["verdict"], 0) + 1

    evidence = {
        "provider": "fc5025",
        "reference_provenance": ref.get("_provenance"),
        "uft_source": uft.get("_source"),
        "counts": counts,
        "sections": sections,
        "note": "D1 protocol-opcode + CLI-argv layer is NOT in this diff: "
                "the FC5025 V2 provider exposes no CBW opcode table or "
                "fcimage argv table — the dialogue lives in an injected "
                "runner with no production construction site. That layer "
                "is UNVERIFIED/needs-source. See REPORT.md D1.",
    }
    EVIDENCE.write_text(json.dumps(evidence, indent=2) + "\n", encoding="utf-8")

    print(f"diff.py: fc5025 — wrote {EVIDENCE.name}")
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
    print("OK — no FAIL (UNVERIFIED rows are needs-source, see provenance)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
