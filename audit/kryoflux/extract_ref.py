#!/usr/bin/env python3
"""
extract_ref.py — the official KryoFlux DTC command-line reference.

Emits the same JSON shape as extract_uft.py so diff.py can compare them
key-by-key.

PROVENANCE: grade = "needs-source" for the DTC CLI contract,
            "recalled" for the 24 MHz / ~41.67 ns stream clock.

KryoFlux is driven through DTC (Disk Tool Console), a CLOSED-SOURCE
vendor binary from the Software Preservation Society / KryoFlux.com.
There is no published vendor SDK for the DTC command line. DTC's flags
are documented only through:
  - the text DTC prints with `dtc` (no args) / `dtc -h`
  - the KryoFlux community wiki
  - empirical observation

Because the audit environment has NO DTC binary installed and NO
vendored copy of its `--help` output, the DTC CLI contract CANNOT be
established to vendored or even recalled grade with confidence. The
flag *meanings* below are recalled from community documentation but the
audit of them is needs-source: a definitive check requires either the
DTC binary or a vendored copy of its help text.

What CAN be stated with recalled confidence:
  - The KryoFlux stream sample clock is the master clock 24.027428 MHz
    (commonly rounded to 24 MHz => ~41.67 ns per tick). This is
    well-known KryoFlux-stream-format knowledge.

DTC flag meanings (recalled from community docs — audit grade
needs-source because not vendored / not runnable here):
  -c2      : command 2 = "read disk to stream files"
  -d0      : drive 0
  -s<N>    : start side / single side select (community docs:
             actually -s is "start track"; side selection differs by
             DTC version — THIS IS A NEEDS-SOURCE AMBIGUITY, see
             KF-D1-1)
  -b<N>    : begin track (start cylinder)
  -e<N>    : end track (end cylinder)
  -f<path> : file name prefix for output stream files
  -i0      : info / image type 0 (raw stream) — also used as a probe;
             again version-dependent semantics
  -i4      : (not used by UFT) — would request a different image type

Upgrading to "vendored": check a copy of `dtc -h` output (or the
KryoFlux DTC manual PDF) into the repo and parse it here.
"""

import json
import sys

PROVENANCE = {
    "grade": "needs-source (DTC CLI) + recalled (24 MHz stream clock)",
    "project": "Software Preservation Society — DTC (Disk Tool Console)",
    "baseline": "DTC is closed-source; no vendored --help output, "
                "no DTC binary in the audit environment",
    "files_recalled_from": ["KryoFlux community wiki (DTC flags)",
                            "KryoFlux stream format docs (24 MHz clock)"],
    "note": "The DTC command-line contract is needs-source — see "
            "audit/README.md. UFT's DTC argv cannot be verified arg-exact "
            "without a vendored dtc help text or a runnable DTC binary. "
            "mock_dtc.py emulates the flag SET UFT passes, not vendored "
            "DTC semantics.",
}

# DTC read-command flags UFT is EXPECTED to pass, with kind=needs-source
# because the reference cannot be established arg-exact here. The flag
# names below are what UFT *should* emit per recalled community docs.
READ_ARGV_FLAGS = {
    "-c2":   {"ref": "command 2 = read-to-stream (recalled; needs-source for exactness)",
              "kind": "needs-source"},
    "-d0":   {"ref": "drive 0 (recalled; needs-source for exactness)",
              "kind": "needs-source"},
    "-s{N}": {"ref": "side/start-track select — SEMANTICS AMBIGUOUS across DTC "
                     "versions (recalled; needs-source — see KF-D1-1)",
              "kind": "needs-source"},
    "-b{N}": {"ref": "begin track (recalled; needs-source for exactness)",
              "kind": "needs-source"},
    "-e{N}": {"ref": "end track (recalled; needs-source for exactness)",
              "kind": "needs-source"},
    "-f{N}": {"ref": "output file-name prefix (recalled; needs-source for exactness)",
              "kind": "needs-source"},
    "-i0":   {"ref": "image type 0 / raw stream (recalled; needs-source for exactness)",
              "kind": "needs-source"},
}

# DTC detect/probe: UFT uses `dtc -i0` alone. Whether `-i0` alone is a
# valid probe (vs requiring -c/-d) is itself needs-source.
DETECT_ARGV_FLAGS = {
    "-i0": {"ref": "UFT uses 'dtc -i0' as a bare probe — validity unverified "
                   "(recalled; needs-source — see KF-D1-2)",
            "kind": "needs-source"},
}

# Stream clock: recalled. 24.027428 MHz master clock => ~41.6 ns per
# sample, commonly rounded to 24 MHz / 41.67 ns.
TIMING = {
    "kf_sample_clock_mhz": 24,
    # The ns/sample EXPRESSION UFT uses (1000.0/24.0 = 41.667) — recalled-OK.
    "kf_sample_ns_expr": "1000.0/24.0",
}

# Standard floppy geometry. recalled — not a DTC constant but the
# physical limit KryoFlux drives operate within.
GEOMETRY = {
    "max_cylinder": 83,
    "max_head": 1,
}


def main():
    json.dump({
        "_provenance": PROVENANCE,
        "read_argv_flags": READ_ARGV_FLAGS,
        "detect_argv_flags": DETECT_ARGV_FLAGS,
        "timing": TIMING,
        "geometry": GEOMETRY,
    }, sys.stdout, indent=2)
    sys.stdout.write("\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
