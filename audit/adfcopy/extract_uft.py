#!/usr/bin/env python3
"""
extract_uft.py — pull ADF-Copy protocol constants from the UFT V2 source.

Reads, never writes. Emits a JSON dict on stdout: every wire-protocol
constant UFT uses to talk to an ADF-Copy / ADF-Drive device, plus where
it was found (file:line). diff.py consumes this against extract_ref.py.

NOTE on provenance: ADF-Copy has NO C-HAL backend. The V2 provider is a
Qt-side runner-injection design. The protocol command opcodes
(ADFC_CMD_*) are documented in adfcopy_provider_v2.{h,cpp} as text
comments only — the original adfcopyhardwareprovider.h that physically
held the #defines was DELETED with the V1 hierarchy (MF-169 / P1.17).
What survives are:
  - RESP_OK / RESP_ERROR / RESP_NODISK   — real `static constexpr` in the .cpp
  - ADFC_SAMPLE_CLOCK_HZ / ADFC_SAMPLE_NS — real `static constexpr` in the .cpp
  - ADFC_STD_CYLINDERS / ADFC_HEADS       — real `static constexpr` in the .cpp
  - ADFC_CMD_* opcodes                    — only in comments / static_assert text
  - VID/PID 0x16C0:0x0483                 — only in ProviderError prose strings

Sources:
  src/hardware_providers/adfcopy_provider_v2.cpp  — real constexpr constants
  src/hardware_providers/adfcopy_provider_v2.h    — opcode comments, VID/PID prose
"""

import json
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
CPP = REPO / "src" / "hardware_providers" / "adfcopy_provider_v2.cpp"
HDR = REPO / "src" / "hardware_providers" / "adfcopy_provider_v2.h"


def _scan_constexpr(text, names):
    """static constexpr <type> NAME = <rhs>;  ->  {NAME: [value, lineno]}

    If the RHS is a pure literal, value is the parsed number.
    If the RHS is a computed expression (e.g. `1.0e9 / ADFC_SAMPLE_CLOCK_HZ`),
    value is the *evaluated* result when all symbols are known literals,
    otherwise the string {"derived": "<rhs>"} so diff.py can flag it.
    """
    out = {}
    lits = {}  # name -> numeric literal, for resolving derived expressions
    for n, line in enumerate(text.splitlines(), 1):
        for name in names:
            m = re.search(rf"constexpr\s+\w+\s+{re.escape(name)}\s*=\s*"
                          r"([^;]+);", line)
            if not m:
                continue
            rhs = m.group(1).strip()
            # pure char literal
            cm = re.fullmatch(r"'(.)'", rhs)
            if cm:
                out[name] = [ord(cm.group(1)), n]
                lits[name] = ord(cm.group(1))
                continue
            # pure numeric literal
            nm = re.fullmatch(r"[-+]?[0-9]*\.?[0-9]+(?:[eE][-+]?[0-9]+)?", rhs)
            if nm:
                val = float(rhs) if re.search(r"[.eE]", rhs) else int(rhs, 0)
                out[name] = [val, n]
                lits[name] = val
                continue
            # derived expression: try to evaluate with known literals
            expr = rhs
            for k, v in lits.items():
                expr = re.sub(rf"\b{re.escape(k)}\b", repr(v), expr)
            try:
                # only allow arithmetic on numbers
                if re.fullmatch(r"[-+*/(). 0-9eE]+", expr):
                    out[name] = [float(eval(expr, {"__builtins__": {}})), n]
                    continue
            except Exception:
                pass
            out[name] = [{"derived": rhs}, n]
    return out


def _scan_opcode_comments(text):
    """ADFC_CMD_NAME (0xNN) appearing anywhere — opcode is comment-only."""
    out = {}
    for n, line in enumerate(text.splitlines(), 1):
        for m in re.finditer(r"(ADFC_CMD_\w+)\s*\(?\s*(0x[0-9A-Fa-f]+)\s*\)?", line):
            name = m.group(1)
            val = int(m.group(2), 0)
            # first sighting wins (keeps line of first definition-like mention)
            if name not in out:
                out[name] = [val, n]
    return out


def main():
    if not CPP.exists() or not HDR.exists():
        print(f"extract_uft.py: missing {CPP} or {HDR}", file=sys.stderr)
        return 2
    cpp = CPP.read_text(encoding="utf-8", errors="replace")
    hdr = HDR.read_text(encoding="utf-8", errors="replace")

    result = {
        "_source": {
            "cpp": str(CPP.relative_to(REPO)),
            "hdr": str(HDR.relative_to(REPO)),
        },
        "_note": "ADF-Copy has NO C-HAL; opcodes are comment/static_assert text "
                 "only (adfcopyhardwareprovider.h deleted by MF-169). RESP_*, "
                 "sample-clock and geometry are real constexpr in the .cpp.",
        "responses": {},
        "timing": {},
        "geometry": {},
        "cmd_opcodes": {},
        "usb_ids": {},
    }

    # Real constexpr — these are byte-exact verifiable.
    resp = _scan_constexpr(cpp, ["RESP_OK", "RESP_ERROR", "RESP_NODISK"])
    result["responses"] = resp

    timing = _scan_constexpr(cpp, ["ADFC_SAMPLE_CLOCK_HZ", "ADFC_SAMPLE_NS"])
    result["timing"] = timing

    geom = _scan_constexpr(cpp, ["ADFC_STD_CYLINDERS", "ADFC_HEADS"])
    result["geometry"] = geom

    # Comment-only opcodes — scanned from both files, no #define exists.
    ops = _scan_opcode_comments(cpp)
    ops_h = _scan_opcode_comments(hdr)
    for k, v in ops_h.items():
        ops.setdefault(k, v)
    result["cmd_opcodes"] = ops

    # VID/PID — only in ProviderError prose. Scan for the pattern.
    for n, line in enumerate(cpp.splitlines(), 1):
        m = re.search(r"VID:PID\s*(0x[0-9A-Fa-f]+):(0x[0-9A-Fa-f]+)", line)
        if m and "ADFC_VID_PID" not in result["usb_ids"]:
            result["usb_ids"]["ADFC_VID_PID"] = [
                [int(m.group(1), 0), int(m.group(2), 0)], n
            ]

    json.dump(result, sys.stdout, indent=2)
    sys.stdout.write("\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
