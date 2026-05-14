#!/usr/bin/env python3
"""
mock_fluxengine.py — mock fluxengine binary for the V2-provider audit.

This is the V2-provider-focused sibling of the existing
tests/external_audits/fluxengine/mock_fluxengine.py. That one was
written to demonstrate that the *broken V1* CLI (positional `ibm`,
`-c N` for cylinder, `-h H`, `--revs`) is rejected by the real
fluxengine. THIS one accepts ONLY the *corrected* post-MF-178 V2 CLI
form and rejects both garbage AND the old V1 syntax — so it doubles as
a regression gate: if the V2 provider ever drifts back to the pre-MF-178
syntax, this mock fails.

Provenance honesty (see extract_ref.py): the FluxEngine CLI flag
semantics encoded here are recalled, cross-checked against FE's own
source via tests/external_audits/fluxengine/. They are NOT vendored and
NOT end-to-end-tested against a real fluxengine binary (the in-repo
audit's Stufe 5 / the MF-178 comment both flag this). A green run means
"the V2 provider's argv is well-formed against the corrected FE CLI
model" — not "byte-exact against real FE".

Corrected V2 CLI form this mock accepts:
  read:   fluxengine read  -c <profile> -s drive:0 --tracks=cNhM \
                           --drive.revolutions=R -o <out>
  write:  fluxengine write -c <profile> -d drive:0 --tracks=cNhM -i <in>
  rpm:    fluxengine rpm

Usage (CI shim):
  ln -s mock_fluxengine.py fluxengine ; PATH=$PWD:$PATH ; run UFT tests
Exit code: 0 if argv is accepted, 1 if any error.
"""

import sys

KNOWN_SUBCOMMANDS = {"rpm", "read", "write", "rawwrite", "convert", "seek",
                     "format", "inspect", "analyse"}

KNOWN_FLAGS = {  # flag -> takes a separate value token?
    "-s": True,  "--source": True,
    "-o": True,  "--output": True,
    "-d": True,  "--dest": True,
    "-i": True,  "--input": True,
    "-c": True,  "--config": True,    # config/PROFILE by name — NOT a cylinder
    "-t": True,  "--cylinder": True,  # cylinder flag — only valid for `seek`
    "--tracks": True,                 # e.g. --tracks=c40h0  (value can be inline)
    "--drive.revolutions": True,      # e.g. --drive.revolutions=2
    "--version": False, "--help": False, "--doc": False,
}

# Pre-MF-178 V1 flags that MUST now be rejected (regression gate).
DEAD_V1_FLAGS = {"-h", "--revs"}

KNOWN_PROFILES = {"acornadfs", "acorndfs", "amiga", "apple2", "atarist",
                  "brother", "commodore", "ibm", "mac", "micropolis",
                  "n88basic", "northstar", "rx50", "ti99", "victor9k",
                  "zilogmcz", "f85", "hplif", "ampro", "bk", "eco1"}

errors = []
warnings = []
args = sys.argv[1:]
print(f"[mock-fluxengine] argv = {args!r}", file=sys.stderr)

if not args:
    errors.append("no subcommand")
elif args[0].startswith("--"):
    if args[0] not in KNOWN_FLAGS:
        errors.append(f"unknown top-level flag: {args[0]}")
elif args[0] not in KNOWN_SUBCOMMANDS:
    errors.append(f"unknown subcommand: {args[0]}")
else:
    sub = args[0]
    i = 1
    while i < len(args):
        tok = args[i]
        if tok.startswith("-"):
            flag = tok.split("=", 1)[0]
            if flag in DEAD_V1_FLAGS:
                errors.append(
                    f"flag {flag!r} is dead pre-MF-178 V1 syntax — the V2 "
                    f"provider must not emit it (regression!)")
                i += 1
                continue
            if flag not in KNOWN_FLAGS:
                errors.append(f"unknown flag for {sub!r}: {flag}")
                i += 1
                continue
            has_inline = "=" in tok
            if not has_inline and KNOWN_FLAGS[flag] and i + 1 < len(args):
                val = args[i + 1]
                if flag in ("-c", "--config"):
                    if val.isdigit():
                        errors.append(
                            f"-c {val}: '-c' loads a profile BY NAME; got a "
                            f"numeric value — this is the pre-MF-178 "
                            f"cylinder-vs-config bug (regression!)")
                    elif val not in KNOWN_PROFILES and not val.endswith(".textpb"):
                        warnings.append(f"-c {val}: not a known builtin profile")
                i += 2
            else:
                i += 1
        else:
            # bare positional — FE takes no positional after read/write.
            errors.append(
                f"unexpected positional {tok!r} after {sub!r}: profile must "
                f"be passed via -c {tok} (pre-MF-178 V1 bug)")
            i += 1

# Simulate rpm output so UFT's regex parser sees something.
if not errors and args and args[0] == "rpm":
    print("Rotational period is 200.0 ms (300.0 rpm)")

print(f"\n[mock-fluxengine] {len(errors)} error(s), {len(warnings)} warning(s)",
      file=sys.stderr)
for e in errors:
    print(f"  ERROR: {e}", file=sys.stderr)
for w in warnings:
    print(f"  WARN:  {w}", file=sys.stderr)
sys.exit(1 if errors else 0)
