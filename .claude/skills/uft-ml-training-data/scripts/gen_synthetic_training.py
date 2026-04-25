#!/usr/bin/env python3
"""
gen_synthetic_training.py — Generate labeled synthetic training samples.

Produces (flux, label) pairs deterministically. Output:
    tests/training/synthetic/<class>_<index>.scp
    tests/training/synthetic/<class>_<index>.label.json

Idempotent: same SEED → byte-identical output.
"""

import argparse
import hashlib
import json
import random
import struct
import sys
from pathlib import Path

SEED = 0x4E554654
OUT_DIR = (Path(__file__).resolve().parents[5] /
           "tests" / "training" / "synthetic")


def lcg(state):
    """Deterministic linear congruential generator."""
    return (state * 1103515245 + 12345) & 0xFFFFFFFF


def build_synthetic_track(track_num, n_sectors=18, sector_size=512,
                           seed_offset=0):
    """Build a synthetic track with known-good sectors."""
    state = (SEED + track_num * 1000 + seed_offset) & 0xFFFFFFFF
    sectors = []
    for s in range(n_sectors):
        data = bytearray(sector_size)
        for i in range(sector_size):
            state = lcg(state)
            data[i] = (state >> 16) & 0xFF
        sectors.append(bytes(data))
    return sectors


def encode_as_scp(tracks, n_revs=2):
    """Encode tracks as a minimal SCP file (just enough for parser)."""
    out = bytearray()
    out.extend(b"SCP")
    out.append(0x24)  # version
    out.extend(b"\x00" * 4)  # flags + start/end
    out[7] = len(tracks) - 1
    out.extend(b"\x00" * 8)  # rest of header

    track_offsets = bytearray(168 * 4)
    track_data = []
    next_offset = 16 + 168 * 4

    for i, sectors in enumerate(tracks):
        block = bytearray(b"TRK")
        block.append(i)
        rev_data = b""
        for rev in range(n_revs):
            for s in sectors:
                # Encode 16-bit timing words from sector bytes
                for byte in s[:64]:  # truncate per rev for size control
                    rev_data += struct.pack(">H", 80 + byte // 4)
        # Per-rev descriptors
        for r in range(n_revs):
            block.extend(struct.pack("<III",
                                       2_000_000 // 25,
                                       len(rev_data) // 2 // n_revs,
                                       4 + n_revs * 12 + r * len(rev_data) // n_revs))
        block.extend(rev_data)
        struct.pack_into("<I", track_offsets, i * 4, next_offset)
        track_data.append(bytes(block))
        next_offset += len(block)

    out.extend(track_offsets)
    for blk in track_data:
        out.extend(blk)
    return bytes(out)


def make_label(scheme, tracks, augmentation=None):
    """Build a label JSON for the given track set."""
    label = {
        "format": "scp",
        "ground_truth": {
            "protection_scheme": scheme,
            "track_count": len(tracks),
            "verified_decoded_sectors": [
                {
                    "track": t_idx,
                    "side": 0,
                    "sector": s_idx,
                    "data_md5": hashlib.md5(sector).hexdigest(),
                    "data_size": len(sector),
                }
                for t_idx, sectors in enumerate(tracks)
                for s_idx, sector in enumerate(sectors)
            ],
        },
        "provenance": {
            "source": "augmented" if augmentation else "synthetic",
            "synthetic_seed": hex(SEED),
            "augmentation_chain": augmentation or [],
        },
    }
    return label


def write_pair(name: str, scp: bytes, label: dict):
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    scp_path = OUT_DIR / f"{name}.scp"
    label_path = OUT_DIR / f"{name}.label.json"
    scp_path.write_bytes(scp)
    label_path.write_text(json.dumps(label, indent=2, sort_keys=True))
    print(f"  [gen] {scp_path.name} ({len(scp)} bytes) + label")


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--n-clean", type=int, default=4,
                   help="number of clean (no-protection) samples")
    p.add_argument("--n-protected", type=int, default=4,
                   help="number of protected samples per scheme")
    args = p.parse_args()

    random.seed(SEED)

    # Clean samples
    for i in range(args.n_clean):
        tracks = [build_synthetic_track(t, seed_offset=i * 100)
                  for t in range(80)]
        scp = encode_as_scp(tracks)
        label = make_label("CLEAN", tracks)
        write_pair(f"clean_{i:03d}", scp, label)

    # Protected samples — different schemes
    for scheme_idx, scheme in enumerate(["VMAX", "RAPIDLOK", "COPYLOCK"]):
        for i in range(args.n_protected):
            tracks = [build_synthetic_track(t,
                                              seed_offset=scheme_idx * 10000 + i * 100)
                      for t in range(80)]
            scp = encode_as_scp(tracks)
            label = make_label(scheme, tracks)
            write_pair(f"{scheme.lower()}_{i:03d}", scp, label)

    # Augmented samples (jitter)
    for i in range(2):
        tracks = [build_synthetic_track(t, seed_offset=999000 + i * 100)
                  for t in range(80)]
        scp = encode_as_scp(tracks)
        label = make_label("CLEAN", tracks,
                            augmentation=["jitter:50ns", "dropout:0.5%"])
        write_pair(f"clean_aug_{i:03d}", scp, label)

    print()
    n = len(list(OUT_DIR.glob("*.scp")))
    print(f"Generated {n} training pairs in {OUT_DIR}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
