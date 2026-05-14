#!/usr/bin/env python3
"""
diff.py — compare UFT's XUM1541 constants against the official reference.

Runs extract_uft.py + extract_ref.py, compares key-by-key, writes
evidence.json, prints a human summary, exits non-zero on any FAIL.

Verdict per key:
  PASS        UFT value == reference value
  FAIL        UFT value != reference value
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
    rows = []
    keys = sorted(set(uft_sec) | set(ref_sec))
    for k in keys:
        if k.startswith("_") or k == "hwtab_hint_vid_pid":
            continue
        u = uft_sec.get(k)
        r = ref_sec.get(k)
        if u is None:
            rows.append({"key": k, "uft": None, "ref": r, "verdict": "MISSING",
                         "detail": "absent in UFT source"})
            continue
        uval = u[0]
        if r is None:
            rows.append({"key": k, "uft": uval, "ref": None,
                         "verdict": "MISSING", "detail": "no reference entry",
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
    for name in ("usb_ids", "endpoints", "fw_commands", "iec_commands",
                 "cbm_status", "geometry"):
        sections[name] = _cmp_section(uft.get(name, {}), ref.get(name, {}))

    all_rows = [r for rows in sections.values() for r in rows]
    counts = {}
    for r in all_rows:
        counts[r["verdict"]] = counts.get(r["verdict"], 0) + 1

    # Cross-check: GUI-side VID/PID hint vs the xum1541_usb.h table.
    hint = uft.get("usb_ids", {}).get("hwtab_hint_vid_pid")
    hint_note = None
    if hint:
        (vid, pid), lineno = hint
        zf_vid = ref["usb_ids"]["USB_VID_ZOOMFLOPPY"]
        zf_pid = ref["usb_ids"]["USB_PID_ZOOMFLOPPY"]
        xu_pid = ref["usb_ids"]["USB_PID_XUM1541"]
        if (vid, pid) == (zf_vid, zf_pid):
            hint_note = f"hardwaretab.cpp:{lineno} hint matches ZoomFloppy"
        elif (vid, pid) == (zf_vid, xu_pid):
            hint_note = (f"hardwaretab.cpp:{lineno} labels VID:PID "
                         f"{vid:#06x}:{pid:#06x} as 'ZoomFloppy/XUM1541' but "
                         f"that PID is the *XUM1541* PID in xum1541_usb.h "
                         f"(ZoomFloppy is {zf_pid:#06x}). Mislabel — see "
                         f"REPORT.md XUM-D4-1.")
        else:
            hint_note = (f"hardwaretab.cpp:{lineno} hint {vid:#06x}:{pid:#06x} "
                         f"matches no entry in xum1541_usb.h")

    evidence = {
        "provider": "xum1541",
        "reference_provenance": ref.get("_provenance"),
        "uft_source": uft.get("_source"),
        "counts": counts,
        "gui_hint_crosscheck": hint_note,
        "sections": sections,
    }
    EVIDENCE.write_text(json.dumps(evidence, indent=2) + "\n", encoding="utf-8")

    print(f"diff.py: xum1541 — wrote {EVIDENCE.name}")
    for name, rows in sections.items():
        line = "  ".join(f"{r['key']}:{r['verdict'][0]}" for r in rows)
        print(f"  [{name}] {line}")
    print(f"  counts: {counts}")
    if hint_note:
        print(f"  gui-hint: {hint_note}")

    fails = [r for r in all_rows if r["verdict"] == "FAIL"]
    if fails:
        print("FAIL:")
        for r in fails:
            print(f"  {r['key']}: {r['detail']}")
        return 1
    print("OK — no FAIL")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
