#!/usr/bin/env python3
"""
extract_uft.py — pull the fluxengine CLI invocations UFT builds from source.

Reads, never writes. Emits a JSON dict on stdout: every fluxengine
command-line argument vector the FluxEngineProviderV2 constructs, plus
the flux-clock constant it assumes, plus where each was found
(file:line).

FluxEngine is a CLI-WRAPPER backend: the provider has no C-HAL backbone.
It shells out to the `fluxengine` binary via an injected runner. The
"wire protocol" UFT must get right is the fluxengine *command line* —
fluxengine owns the USB layer.

This audit checks the V2 provider AFTER the MF-178 CLI-syntax correction
(see fluxengine_provider_v2.cpp:134-163) and the FE-F2/FE-F6 refinements
(task #113): the profile is no longer hard-coded "ibm" but the m_profile
ctor parameter (FE-F2 -> emitted as the {profile} placeholder token),
and a query_version() function emits a `fluxengine version` invocation
(FE-F6 -> the version_argv section).

Sources:
  src/hardware_providers/fluxengine_provider_v2.cpp — build_read_argv(),
      build_write_argv(), do_measure_rpm()/query_version()/
      do_detect_drive() argv, sample-clock constant
"""

import json
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
CPP = REPO / "src" / "hardware_providers" / "fluxengine_provider_v2.cpp"


def _collect_argv(lines, start_fn_marker):
    """Collect args.push_back(...) tokens inside a build_*_argv body.

    Handles three forms:
      - string literal              args.push_back("read")        -> read
      - concat flag                 args.push_back("--tracks=c"+) -> --tracks=cNhM
      - member variable (FE-F2)     args.push_back(m_profile)     -> {profile}
        The profile was hard-coded "ibm" before FE-F2; it is now the
        m_profile ctor parameter, so the extractor emits a {profile}
        placeholder token instead of a fixed string.
    """
    flags = {}
    inside = False
    for n, line in enumerate(lines, 1):
        if start_fn_marker in line and "::" in line:
            inside = True
        if inside:
            # concat flag: "--tracks=c" + ...   -> emit "--tracks=cNhM"
            mc = re.search(r'args\.push_back\(\s*"(--tracks=c)"', line)
            # any other concat flag: "FLAG" + std::to_string(...)
            mconcat = re.search(r'args\.push_back\(\s*"([^"]+)"\s*\+', line)
            # member-variable token: args.push_back(m_profile);  (FE-F2)
            mvar = re.search(r'args\.push_back\(\s*(m_\w+)\s*\)', line)
            # plain literal: args.push_back("...");
            mplain = re.search(r'args\.push_back\(\s*"([^"]+)"\s*\)', line)
            if mc:
                flags["--tracks=cNhM"] = n
            elif mconcat:
                val = mconcat.group(1)
                flags[(val + "{N}") if val.endswith("=") else val] = n
            elif mvar:
                var = mvar.group(1)
                # m_fe_binary is argv[0] — the program path, not part of the
                # CLI contract being audited. The pre-FE-F2 extractor (which
                # only matched string literals) never emitted it; keep that.
                if var != "m_fe_binary":
                    token = ("{profile}" if var == "m_profile"
                             else "{" + var.removeprefix("m_") + "}")
                    flags[token] = n
            elif mplain:
                flags[mplain.group(1)] = n
            if line.strip() == "}" and flags:
                inside = False
    return flags


def main():
    if not CPP.exists():
        print(f"extract_uft.py: missing {CPP}", file=sys.stderr)
        return 2
    text = CPP.read_text(encoding="utf-8", errors="replace")
    lines = text.splitlines()

    result = {
        "_source": {"cpp": str(CPP.relative_to(REPO))},
        "read_argv": {},
        "write_argv": {},
        "rpm_argv": {},
        "version_argv": {},
        "detect_argv": {},
        "timing": {},
    }

    # read / write argv from build_*_argv()
    for k, v in _collect_argv(lines, "build_read_argv(").items():
        result["read_argv"][k] = [k, v]
    for k, v in _collect_argv(lines, "build_write_argv(").items():
        result["write_argv"][k] = [k, v]

    # { m_fe_binary, "<subcommand>" } appears in three functions:
    #   do_measure_rpm   -> "rpm"
    #   query_version    -> "version"   (added by FE-F6)
    #   do_detect_drive  -> "rpm"
    # Assign each to its section by the enclosing function rather than by
    # order, so adding/removing a function does not mis-bucket the others.
    cur_fn = None
    for n, line in enumerate(lines, 1):
        if "FluxEngineProviderV2::do_measure_rpm" in line:
            cur_fn = "rpm_argv"
        elif "FluxEngineProviderV2::query_version" in line:
            cur_fn = "version_argv"
        elif "FluxEngineProviderV2::do_detect_drive" in line:
            cur_fn = "detect_argv"
        m = re.search(r'argv\s*=\s*\{\s*m_fe_binary\s*,\s*"([^"]+)"\s*\}', line)
        if m and cur_fn:
            result[cur_fn][m.group(1)] = [m.group(1), n]

    # Sample clock: captured.sample_ns = 125.0  (8 MHz)
    for n, line in enumerate(lines, 1):
        if "sample_ns" in line and "125.0" in line:
            result["timing"]["fe_sample_clock_mhz"] = [8, n]
            result["timing"]["fe_sample_ns"] = [125, n]
            break

    json.dump(result, sys.stdout, indent=2)
    sys.stdout.write("\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
