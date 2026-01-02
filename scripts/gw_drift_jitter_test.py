#!/usr/bin/env python3
import argparse
import os
import struct
import subprocess
import statistics
from pathlib import Path

def run(cmd, check=True):
    print(">>", " ".join(cmd))
    return subprocess.run(cmd, check=check)

def parse_scp_index_times(path: Path):
    """
    Minimal SCP parser for index timing:
    We only need per-revolution index-to-index length (in ticks) if present.

    SCP format basics:
    - Header 0x00.. contains 'SCP' signature and resolution.
    - Track table points to track data blocks.
    - Each track block contains revolution data, each with length and flux.

    This parser is intentionally conservative: it extracts revolution lengths
    where available. If your SCP writer stores explicit index, this works well.
    """
    data = path.read_bytes()
    if data[0:3] != b"SCP":
        raise ValueError("Not an SCP file")

    # Header:
    # 0x0A: timebase (ticks per microsecond-ish) depends on implementation.
    # Many tools store resolution at 0x0B or in extended area.
    # We'll read "resolution" at 0x0B (common: 0=25ns, 1=50ns, etc.) if present.
    # If unknown, we still compute relative jitter/drift in ticks.
    # SCP rev lengths are usually 32-bit ticks.
    track_tbl_off = 0x10
    track_offsets = []
    for i in range(168):  # 0..167 tracks table in many SCP files
        off = struct.unpack_from("<I", data, track_tbl_off + i*4)[0]
        track_offsets.append(off)

    # Find first non-zero track
    t_off = next((o for o in track_offsets if o != 0), None)
    if not t_off:
        raise ValueError("No tracks found in SCP")

    # Track header at t_off:
    # 0x00: 'TRK'
    if data[t_off:t_off+3] != b"TRK":
        raise ValueError("Track header not found at expected offset")

    # 0x0A: nr_revs (common)
    nr_revs = data[t_off+0x0A]
    # Revolution table often starts at 0x10: nr_revs * 4 bytes length
    rev_tbl = t_off + 0x10
    rev_lengths = []
    for r in range(nr_revs):
        rl = struct.unpack_from("<I", data, rev_tbl + r*4)[0]
        if rl != 0:
            rev_lengths.append(rl)

    if not rev_lengths:
        raise ValueError("No revolution lengths found (SCP variant?)")

    return rev_lengths

def stats(vals):
    if len(vals) < 2:
        return {}
    mean = statistics.mean(vals)
    stdev = statistics.pstdev(vals)
    return {
        "n": len(vals),
        "mean": mean,
        "min": min(vals),
        "max": max(vals),
        "pkpk": max(vals) - min(vals),
        "stdev": stdev,
    }

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", required=True, help="Greaseweazle serial port, e.g. /dev/ttyACM0")
    ap.add_argument("--track", type=int, default=0, help="Track to sample")
    ap.add_argument("--side", type=int, default=0, choices=[0,1], help="Side")
    ap.add_argument("--revs", type=int, default=50, help="Number of revolutions to capture")
    ap.add_argument("--out", default="gw_test.scp", help="Output SCP filename")
    ap.add_argument("--passes", type=int, default=5, help="Repeat capture passes (drift over time)")
    args = ap.parse_args()

    out = Path(args.out).resolve()
    all_pass_means = []

    for p in range(args.passes):
        if out.exists():
            out.unlink()

        # Capture a single track as SCP. We force raw flux capture with multiple revs.
        # Command line may differ slightly depending on your gw tools version.
        run([
            "gw", "--port", args.port,
            "read", "--format", "scp",
            "--revs", str(args.revs),
            f"{out}:{args.track}.{args.side}"
        ])

        rev_lengths = parse_scp_index_times(out)
        s = stats(rev_lengths)

        print(f"\nPASS {p+1}/{args.passes}")
        print(f"  revs: {s['n']}")
        print(f"  mean ticks: {s['mean']:.2f}")
        print(f"  pk-pk ticks: {s['pkpk']}")
        print(f"  stdev ticks: {s['stdev']:.2f}")

        all_pass_means.append(s["mean"])

    # Drift across passes (mean rev length moving)
    if len(all_pass_means) >= 2:
        d = stats(all_pass_means)
        print("\nDRIFT (mean rev length across passes):")
        print(f"  passes: {d['n']}")
        print(f"  mean(mean): {d['mean']:.2f} ticks")
        print(f"  pk-pk drift: {d['pkpk']:.2f} ticks")
        print(f"  stdev drift: {d['stdev']:.2f} ticks")

if __name__ == "__main__":
    main()
