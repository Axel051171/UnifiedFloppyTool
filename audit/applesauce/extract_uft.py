#!/usr/bin/env python3
"""
extract_uft.py — pull Applesauce serial-protocol constants from UFT.

Reads, never writes. Emits a JSON dict on stdout: every protocol
constant UFT uses to talk to an Applesauce device over its USB-CDC
serial text protocol, plus where it was found (file:line). diff.py
consumes this against extract_ref.py.

Sources:
  src/hardware_providers/applesauce_provider_v2.cpp — response status
      chars (RESP_OK/ERROR/ON), 8 MHz sample clock, buffer sizes,
      and the command-string vocabulary the doc-comments enumerate.
  include/uft/hal/uft_applesauce.h                  — UFT_AS_SAMPLE_CLOCK
      #define, format enum.

The Applesauce protocol is an ASCII line protocol (command + '\\n',
single-char status response) plus a binary-bulk mode for flux transfer.
Unlike a register/opcode device there is no numeric opcode table — the
"wire constants" are the response status characters, the serial line
parameters, the sample-clock rate, and the device buffer sizes. Command
*strings* are extracted from the provider's own doc-comments (the only
place they appear, since the actual dialogue is inside an injected
IApplesauceTransport with no production construction site).
"""

import json
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
PROV_C = REPO / "src" / "hardware_providers" / "applesauce_provider_v2.cpp"
PROV_H = REPO / "src" / "hardware_providers" / "applesauce_provider_v2.h"
HAL_H = REPO / "include" / "uft" / "hal" / "uft_applesauce.h"


def main():
    if not PROV_C.exists():
        print(f"extract_uft.py: missing {PROV_C}", file=sys.stderr)
        return 2
    ctext = PROV_C.read_text(encoding="utf-8", errors="replace")
    htext = HAL_H.read_text(encoding="utf-8", errors="replace") \
        if HAL_H.exists() else ""

    result = {
        "_source": {
            "provider_c": str(PROV_C.relative_to(REPO)),
            "provider_h": str(PROV_H.relative_to(REPO)),
            "hal_h": str(HAL_H.relative_to(REPO)),
        },
        "response_chars": {},
        "timing": {},
        "buffers": {},
        "serial_params": {},
        "command_vocab": {},
    }

    # Response status characters: static constexpr char RESP_* = 'x';
    for n, line in enumerate(ctext.splitlines(), 1):
        m = re.search(r"constexpr\s+char\s+(RESP_\w+)\s*=\s*'(.)'", line)
        if m:
            result["response_chars"][m.group(1)] = [ord(m.group(2)), n,
                                                    m.group(2)]

    # Sample clock: static constexpr double AS_SAMPLE_CLOCK_HZ = 8000000.0;
    for n, line in enumerate(ctext.splitlines(), 1):
        m = re.search(r"constexpr\s+double\s+(AS_SAMPLE_CLOCK_HZ|AS_SAMPLE_NS)"
                      r"\s*=\s*([0-9.eE/* ]+);", line)
        if m:
            # AS_SAMPLE_NS is "1.0e9 / AS_SAMPLE_CLOCK_HZ" — record the clock,
            # compute ns separately.
            if m.group(1) == "AS_SAMPLE_CLOCK_HZ":
                result["timing"]["AS_SAMPLE_CLOCK_HZ"] = [
                    int(float(m.group(2).strip())), n]
        m2 = re.search(r"constexpr\s+uint32_t\s+(AS_BUFFER_\w+)\s*=\s*"
                       r"(\d+)u?", line)
        if m2:
            result["buffers"][m2.group(1)] = [int(m2.group(2)), n]

    # AS_SAMPLE_NS is derived; record the computed value for the diff.
    if "AS_SAMPLE_CLOCK_HZ" in result["timing"]:
        clk = result["timing"]["AS_SAMPLE_CLOCK_HZ"][0]
        result["timing"]["AS_SAMPLE_NS_derived"] = [
            round(1.0e9 / clk, 3),
            result["timing"]["AS_SAMPLE_CLOCK_HZ"][1]]

    # C-HAL #define UFT_AS_SAMPLE_CLOCK 8000000
    for n, line in enumerate(htext.splitlines(), 1):
        m = re.match(r"\s*#define\s+(UFT_AS_SAMPLE_CLOCK)\s+(\d+)", line)
        if m:
            result["timing"]["UFT_AS_SAMPLE_CLOCK_hal"] = [
                int(m.group(2)), n]

    # Serial line parameters — Applesauce is 115200 8N1 per the provider
    # doc-comment. Extract the baud rate literal where it is stated.
    for n, line in enumerate(ctext.splitlines(), 1):
        m = re.search(r"\b(115200)\b", line)
        if m:
            result["serial_params"]["baud"] = [115200, n]
            break
    # 8N1 is stated as text; record presence.
    for n, line in enumerate(ctext.splitlines(), 1):
        if "8N1" in line:
            result["serial_params"]["framing_8N1_documented"] = [1, n]
            break

    # Command vocabulary: the ASCII command strings appear in doc-comments
    # and (for a few) in code. Scan for the documented command tokens.
    cmd_tokens = [
        "sync:on", "sync:off", "disk:read", "disk:readx", "data:?size",
        "disk:?write", "data:clear", "disk:write", "motor:on", "motor:off",
        "head:track", "head:side", "head:zero", "sync:?speed", "?kind",
        "data:?max", "psu:?5v", "psu:?12v",
    ]
    for tok in cmd_tokens:
        for n, line in enumerate(ctext.splitlines(), 1):
            if tok in line:
                result["command_vocab"][tok] = [1, n]
                break

    json.dump(result, sys.stdout, indent=2)
    sys.stdout.write("\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
