#!/usr/bin/env python3
"""
extract_uft.py — pull the DTC CLI invocations UFT builds from the source.

Reads, never writes. Emits a JSON dict on stdout: every DTC command-line
argument vector the KryoFluxProviderV2 constructs, plus the flux-clock
constant it assumes, plus where each was found (file:line).

KryoFlux is a CLI-WRAPPER backend: the provider has no C-HAL backbone.
It shells out to DTC (Disk Tool Console) via an injected runner. The
"wire protocol" UFT must get right is therefore the DTC *command line*,
not a byte protocol — DTC owns the USB layer.

Sources:
  src/hardware_providers/kryoflux_provider_v2.cpp  — build_read_argv(),
      do_detect_drive() argv, sample-clock constant, geometry guards
"""

import json
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
CPP = REPO / "src" / "hardware_providers" / "kryoflux_provider_v2.cpp"


def main():
    if not CPP.exists():
        print(f"extract_uft.py: missing {CPP}", file=sys.stderr)
        return 2
    text = CPP.read_text(encoding="utf-8", errors="replace")
    lines = text.splitlines()

    result = {
        "_source": {"cpp": str(CPP.relative_to(REPO))},
        # The DTC read argv is built token-by-token in build_read_argv().
        # We reconstruct the *flag set* it emits (values are runtime ints).
        "read_argv_flags": {},
        "detect_argv_flags": {},
        "timing": {},
        "geometry": {},
    }

    # build_read_argv: collect each args.push_back("...") literal flag.
    in_build_read = False
    for n, line in enumerate(lines, 1):
        if "build_read_argv(" in line and "::" in line:
            in_build_read = True
        if in_build_read:
            # std::string-concat flags like  "-s" + std::to_string(head)
            # -> emit the "-s{N}" form ONLY (the flag takes a runtime value).
            m2 = re.search(r'args\.push_back\(\s*"(-[a-zA-Z])"\s*\+', line)
            if m2:
                key = m2.group(1) + "{N}"
                result["read_argv_flags"][key] = [key, n]
            else:
                # plain literal flag (e.g. "-c2", "-d0", "-i0")
                m = re.search(r'args\.push_back\(\s*"([^"]+)"\s*\)', line)
                if m:
                    result["read_argv_flags"][m.group(1)] = [m.group(1), n]
            if line.strip() == "}" and result["read_argv_flags"]:
                in_build_read = False

    # do_detect_drive argv: { m_dtc_binary, "-i0" }
    for n, line in enumerate(lines, 1):
        m = re.search(r'argv\s*=\s*\{\s*m_dtc_binary\s*,\s*"([^"]+)"\s*\}', line)
        if m:
            result["detect_argv_flags"][m.group(1)] = [m.group(1), n]

    # Sample clock: captured.sample_ns = 1000.0 / 24.0  (24 MHz)
    for n, line in enumerate(lines, 1):
        if "sample_ns" in line and "24" in line:
            result["timing"]["kf_sample_clock_mhz"] = [24, n]
            result["timing"]["kf_sample_ns_expr"] = ["1000.0/24.0", n]
            break

    # Geometry guards: cylinder 0..83, head 0..1
    for n, line in enumerate(lines, 1):
        if re.search(r"cylinder\s*>\s*83", line):
            result["geometry"]["max_cylinder"] = [83, n]
        if re.search(r"head\s*>\s*1", line):
            result["geometry"]["max_head"] = [1, n]

    json.dump(result, sys.stdout, indent=2)
    sys.stdout.write("\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
