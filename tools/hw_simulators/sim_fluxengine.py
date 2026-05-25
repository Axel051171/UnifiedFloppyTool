#!/usr/bin/env python3
"""
sim_fluxengine.py — fluxengine CLI simulator (V415-PLAN MF-267).

Pretends to be the `fluxengine` binary. Accepts the argv subset that
FluxEngineProviderV2 emits and writes the .flux output that the
provider's downstream SCP-pivot parsing expects, sourcing data from
the existing 130 MB synthetic-flux corpus.

Argv contract handled (per src/hardware_providers/fluxengine_provider_v2.cpp):
  Read:    read -c <profile> --tracks=cNhM --drive.revolutions=R -o <out.flux>
  Write:   write -c <profile> --tracks=cNhM -i <in.flux>
  Version: --version | version
  Rpm:     rpm

Anything else → unsupported, exit 2.

The .flux format produced is the SuperCardPro-compatible variant
(fluxengine read writes a tiny `.flux` SQLite file in production —
this simulator writes the matching SCP bytes the provider's pivot
expects). See ../README.md for the limitations list.
"""
from __future__ import annotations
import sys
import re
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
CORPUS_DIR = REPO_ROOT / "tests" / "differential" / "corpus" / "flux"


# Map fluxengine profile names to corpus fixtures.
PROFILE_TO_FIXTURE = {
    "ibm.720":              CORPUS_DIR / "ibm_dd.scp",
    "ibm.1440":             CORPUS_DIR / "ibm_hd.scp",
    "atarist.720":          CORPUS_DIR / "atarist_dd.scp",
    "commodore.1541":       CORPUS_DIR / "c64_gcr.scp",
    "apple2.appledos.140":  CORPUS_DIR / "apple2_525.scp",
    "amiga.amigados":       CORPUS_DIR / "amiga_dd.scp",
    # Unknown profile → fall back to IBM DD (most permissive decoder).
    "_default":             CORPUS_DIR / "ibm_dd.scp",
}

VERSION_BANNER = "FluxEngine 0.99.99-sim (V415-PLAN MF-267 simulator)"


def fixture_for_profile(profile: str) -> Path:
    return PROFILE_TO_FIXTURE.get(profile, PROFILE_TO_FIXTURE["_default"])


def parse_tracks_spec(spec: str):
    """Parse `c0-79h0` or `c5h1` form → (cyl_lo, cyl_hi, head)."""
    m = re.match(r"c(\d+)(?:-(\d+))?h(\d+)", spec)
    if not m:
        return None
    lo = int(m.group(1))
    hi = int(m.group(2)) if m.group(2) else lo
    head = int(m.group(3))
    return lo, hi, head


def slice_scp_track(fixture: Path, cyl: int, head: int) -> bytes:
    """
    Return a small slice of the fixture for the requested (cyl, head).

    Real fluxengine output is per-revolution flux for one track. For
    simulator purposes we return the first 64 KB of the fixture — this
    is enough for the V2 provider's SCP-pivot parser to see a valid
    SCP header and either succeed-decode or fall through to
    FluxMarginal. The track itself is shared, since we don't have
    per-track decomposition of the corpus here.
    """
    if not fixture.exists():
        return b""
    return fixture.read_bytes()[: 64 * 1024]


def cmd_read(argv):
    profile = None
    tracks = None
    out_path = None
    i = 0
    while i < len(argv):
        a = argv[i]
        if a == "-c" and i + 1 < len(argv):
            profile = argv[i + 1]
            i += 2
        elif a.startswith("--tracks="):
            tracks = a.split("=", 1)[1]
            i += 1
        elif a == "-o" and i + 1 < len(argv):
            out_path = Path(argv[i + 1])
            i += 2
        elif a.startswith("--drive.revolutions=") or \
             a.startswith("--retries="):
            i += 1
        else:
            i += 1

    if not out_path:
        sys.stderr.write("sim_fluxengine: read needs -o <out.flux>\n")
        return 2

    fixture = fixture_for_profile(profile or "_default")
    tspec = parse_tracks_spec(tracks) if tracks else (0, 0, 0)
    cyl_lo, cyl_hi, head = tspec or (0, 0, 0)

    data = slice_scp_track(fixture, cyl_lo, head)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_bytes(data)

    sys.stdout.write(
        f"sim_fluxengine: read profile={profile} tracks={tracks} "
        f"→ {out_path} ({len(data)} bytes from {fixture.name})\n"
    )
    return 0


def cmd_write(argv):
    in_path = None
    for i, a in enumerate(argv):
        if a == "-i" and i + 1 < len(argv):
            in_path = Path(argv[i + 1])
    if in_path is None or not in_path.exists():
        sys.stderr.write("sim_fluxengine: write needs valid -i <input.flux>\n")
        return 2
    sys.stdout.write(f"sim_fluxengine: write OK (consumed {in_path}, "
                     f"{in_path.stat().st_size} bytes)\n")
    return 0


def cmd_rpm():
    # FluxEngine rpm command output → exactly the line the provider parses.
    sys.stdout.write("Rotational period: 200.000 ms (300.000 RPM)\n")
    return 0


def cmd_version():
    sys.stdout.write(VERSION_BANNER + "\n")
    return 0


def main():
    argv = sys.argv[1:]
    if not argv:
        sys.stderr.write("sim_fluxengine: no command given\n")
        return 2

    if argv[0] in ("--version", "version"):
        return cmd_version()
    if argv[0] == "rpm":
        return cmd_rpm()
    if argv[0] == "read":
        return cmd_read(argv[1:])
    if argv[0] == "write":
        return cmd_write(argv[1:])

    sys.stderr.write(f"sim_fluxengine: unsupported subcommand: {argv}\n")
    return 2


if __name__ == "__main__":
    sys.exit(main())
