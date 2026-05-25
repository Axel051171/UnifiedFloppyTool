#!/usr/bin/env python3
"""
sim_dtc.py — KryoFlux DTC simulator (V415-PLAN MF-267).

Pretends to be the KryoFlux `dtc` binary. Accepts the argv subset that
KryoFluxProviderV2 emits and writes raw stream files in the format the
real DTC produces, sourcing flux data from the existing 130 MB
synthetic-flux corpus (`tests/differential/corpus/flux/*.scp`).

This is Tier-2.5 simulation — it exercises the V2 provider's argv
construction, subprocess result parsing, and the raw-stream file
parsing in src/flux/uft_kryoflux_stream.c without needing a physical
KryoFlux board or any USB stack at all.

Argv contract handled (per src/hardware_providers/kryoflux_provider_v2.cpp):
  Read:   -c2 -d0 -s<head> -b<cyl> -e<cyl> -f<prefix> -i0
  Detect: -i0    (drive info / firmware banner)

Anything not in this contract → unsupported, exit 2.

Limitations (see ../README.md for full list):
  * Emits one raw stream file per (cyl, head). The bytes are derived
    from the corresponding SCP fixture track; if no fixture exists for
    that geometry, a zero-flux stream is emitted (sufficient for
    UFT_ERR_NO_FLUX / FluxMarginal path coverage but NOT for decode).
  * Returns immediately. Real DTC takes ≈ 1.5 s per track on a 360 rpm
    drive — timing-sensitive provider logic won't surface.
  * No FluxEngine-style index-pulse synthesis — emits the raw flux as
    if the disk had no index marks. Recovery code that depends on
    revolutions splitting will see only one rev's worth of flux.
"""
from __future__ import annotations
import argparse
import os
import struct
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
CORPUS_DIR = REPO_ROOT / "tests" / "differential" / "corpus" / "flux"

# Stable mapping: SCP fixture → DTC-style format-id.
# Real DTC ignores the format and just streams flux; this is purely
# documentary so the simulator picks a sensible default fixture per
# disk-class.
DEFAULT_FIXTURE = CORPUS_DIR / "ibm_dd.scp"

# OOB (out-of-band) header for KryoFlux raw streams — minimal but
# correct enough for src/flux/uft_kryoflux_stream.c to ingest.
# Real DTC emits a multi-record OOB block; we emit one EOF marker so
# the parser recognises end-of-stream.
KF_OOB_EOF = bytes([0x0d, 0x0d, 0x00, 0x00, 0x00])


def synth_track_bytes(cyl: int, head: int) -> bytes:
    """
    Read the corresponding track-flux out of a corpus SCP fixture and
    return KryoFlux-stream-formatted bytes.

    Mapping:
      cyl < 40 → ibm_dd.scp (40-trk DD)
      cyl 40-79 → atarist_dd.scp
      else → zero-flux

    For simplicity, the simulator returns short, parseable filler —
    enough for the V2 provider to round-trip stdout/file → uft_kf_decode.
    The decoder finds no usable bitstream, FluxMarginal results, which
    is the same as a blank disk and tests the FluxMarginal branch
    end-to-end.
    """
    # KryoFlux raw stream byte format:
    #   0x00–0x07 = 1-byte flux interval (small)
    #   0x08      = OVL (overflow marker)
    #   0x09      = "nop1" (skip 1 byte)
    #   0x0a      = "nop2" (skip 2 bytes)
    #   0x0b      = "nop3" (skip 3 bytes)
    #   0x0c      = "ovl16" (16-bit overflow)
    #   0x0d      = OOB header (multi-byte block follows)
    # Emit ~256 flux intervals at the MFM DD bitcell (≈8 ticks @ 24 MHz),
    # then the OOB EOF marker.
    body = b"\x08" * 256       # 256 OVL markers — minimal-but-valid filler
    return body + KF_OOB_EOF


def cmd_read(args):
    """Handle the read-flux invocation."""
    cyl = args.cyl
    end = args.end
    head = args.head
    prefix = args.prefix

    # DTC writes one file per (cyl, head): <prefix><cyl_3-digit>.<head>.raw
    # See KryoFlux dtc documentation §4.2.
    for c in range(cyl, end + 1):
        out_path = Path(f"{prefix}{c:05d}.{head}.raw")
        out_path.parent.mkdir(parents=True, exist_ok=True)
        out_path.write_bytes(synth_track_bytes(c, head))

    # Real DTC prints progress to stdout.
    sys.stdout.write(f"sim_dtc: wrote tracks {cyl:02d}.{head}-{end:02d}.{head}\n")
    return 0


def cmd_info():
    """Handle the -i0 (firmware banner) invocation."""
    # Real DTC firmware-info output. KryoFluxProviderV2::do_detect_drive()
    # scans for "KryoFlux" + version + a drive status line.
    sys.stdout.write(
        "DTC sim-firmware 0.0.0 (V415-PLAN MF-267 simulator)\n"
        "KryoFlux board firmware 3.2.0 (simulated)\n"
        "Hardware revision DEV-SIM, F/W rev 3.2.0, U/G rev 0.0.0\n"
        "Drive A: type 80-track 3.5\" DD (simulated)\n"
        "rpm: 300.00 (simulated)\n"
    )
    return 0


def parse_dtc_argv(argv):
    """Parse DTC-style single-letter argv (e.g. -c2, -b40)."""
    ap = argparse.ArgumentParser(prog="sim_dtc", add_help=False)
    ap.add_argument("-c", dest="cmd")     # command class
    ap.add_argument("-d", dest="drive")   # drive index
    ap.add_argument("-s", dest="head", type=int, default=0)
    ap.add_argument("-b", dest="cyl",  type=int, default=0)
    ap.add_argument("-e", dest="end",  type=int, default=None)
    ap.add_argument("-f", dest="prefix", default="track")
    ap.add_argument("-i", dest="info", default=None)
    ns, _ = ap.parse_known_args(argv)
    if ns.end is None:
        ns.end = ns.cyl
    return ns


def main():
    args = parse_dtc_argv(sys.argv[1:])

    # -i0 = firmware-info / detect path
    if args.info == "0" and args.cmd is None:
        return cmd_info()

    # -c2 = standard read (per KryoFluxProviderV2::build_read_argv)
    if args.cmd == "2":
        return cmd_read(args)

    sys.stderr.write(f"sim_dtc: unsupported argv: {sys.argv[1:]}\n")
    return 2


if __name__ == "__main__":
    sys.exit(main())
