#!/usr/bin/env python3
"""
mock_dtc.py — mock DTC (Disk Tool Console) binary for KryoFlux.

The KryoFlux analogue of tests/external_audits/fluxengine/mock_fluxengine.py.
DTC is closed-source and not installed in the audit environment, so this
mock accepts ONLY the flag set UFT's KryoFluxProviderV2 actually passes,
and rejects anything that looks malformed. It lets a CI smoke-test
validate UFT's DTC invocations without a real DTC binary.

IMPORTANT — provenance honesty (see extract_ref.py):
  This mock encodes the flag SET UFT passes plus recalled DTC flag
  semantics. It is NOT a vendored DTC contract. A green run here means
  "UFT's argv is well-formed against the recalled DTC flag model" — NOT
  "UFT's argv is byte-exact against the real DTC". The needs-source
  ambiguities (esp. -s side-vs-start-track, KF-D1-1) are flagged as
  warnings, not hard errors, precisely because they are unverified.

UFT's actual invocations (from kryoflux_provider_v2.cpp):
  read:   dtc -c2 -d0 -s{head} -b{cyl} -e{cyl} -f{prefix} -i0
  detect: dtc -i0

Usage (CI shim):
  ln -s mock_dtc.py dtc ; PATH=$PWD:$PATH ; run UFT integration tests
Exit code: 0 if argv is accepted, 1 if a hard error is found.
"""

import sys

# Flags DTC is known (recalled) to accept. value: does it take an inline
# numeric/string suffix (e.g. -c2, -s0, -fprefix)?
KNOWN_FLAGS = {
    "-c": True,   # command selector  (-c2 = read to stream)
    "-d": True,   # drive selector    (-d0)
    "-s": True,   # side / start-track (SEMANTICS NEEDS-SOURCE — KF-D1-1)
    "-b": True,   # begin track
    "-e": True,   # end track
    "-f": True,   # output file prefix
    "-i": True,   # image / info type (-i0 = raw stream)
    "-g": True,   # (recalled) other DTC flags — accepted, not used by UFT
    "-h": False,  # help
}

# UFT only ever uses these exact flag stems — anything else is suspicious.
UFT_EXPECTED_STEMS = {"-c", "-d", "-s", "-b", "-e", "-f", "-i"}

errors = []
warnings = []
args = sys.argv[1:]
print(f"[mock-dtc] argv = {args!r}", file=sys.stderr)

if not args:
    # bare `dtc` prints help and exits 0 in the real tool
    print("DTC - Disk Tool Console (mock). Use -h for help.")
    sys.exit(0)

seen_command = None
seen_drive = None
for tok in args:
    if not tok.startswith("-"):
        errors.append(f"unexpected bare positional argument: {tok!r} "
                      f"(DTC takes only -flags)")
        continue
    stem = tok[:2]
    suffix = tok[2:]
    if stem not in KNOWN_FLAGS:
        errors.append(f"unknown DTC flag: {stem!r} (in token {tok!r})")
        continue
    if stem not in UFT_EXPECTED_STEMS and stem != "-h":
        warnings.append(f"flag {stem!r} is valid DTC but not one UFT is "
                        f"expected to emit")
    # numeric-suffix flags
    if KNOWN_FLAGS[stem] and stem in ("-c", "-d", "-s", "-b", "-e", "-i"):
        if suffix == "":
            errors.append(f"flag {stem!r} requires an inline value "
                          f"(e.g. {stem}0); got bare {tok!r}")
        elif stem in ("-c", "-d", "-b", "-e", "-i") and not suffix.isdigit():
            errors.append(f"flag {stem!r} expects a numeric value; "
                          f"got {tok!r}")
        elif stem == "-s" and not suffix.isdigit():
            errors.append(f"flag -s expects a numeric value; got {tok!r}")
    if stem == "-c":
        seen_command = suffix
    if stem == "-d":
        seen_drive = suffix
    if stem == "-s" and suffix in ("0", "1"):
        # UFT passes -s{head}. If DTC's -s is "start track" not "side",
        # this would silently capture the wrong track. NEEDS-SOURCE.
        warnings.append(f"-s{suffix}: UFT passes the HEAD number here, but "
                        f"DTC's -s semantics (side vs start-track) are "
                        f"version-dependent and UNVERIFIED — see KF-D1-1")

# A read invocation must carry -c and -d; a bare probe is `dtc -i0`.
is_probe = (args == ["-i0"])
if not is_probe:
    if seen_command is None:
        warnings.append("no -c command flag — only valid for the bare "
                        "`dtc -i0` probe form")
    if seen_drive is None and seen_command is not None:
        warnings.append("a -c read command without -d drive selector")

print(f"\n[mock-dtc] {len(errors)} error(s), {len(warnings)} warning(s)",
      file=sys.stderr)
for e in errors:
    print(f"  ERROR: {e}", file=sys.stderr)
for w in warnings:
    print(f"  WARN:  {w}", file=sys.stderr)

# Simulate a minimal DTC banner on the probe path so UFT's parser sees
# something (UFT scans for 'firmware' + 'rpm').
if not errors and is_probe:
    print("KryoFlux DiskSystem, firmware 3.00s")
    print("index: 200.00ms")

sys.exit(1 if errors else 0)
