#!/usr/bin/env python3
"""
gen_d64_fixture.py — Deterministic Commodore 1541 D64 fixtures.

D64 = flat dump of 35 tracks, variable sectors per track.
35-track: 174848 bytes
35-track + error block: 175531 bytes (174848 + 683 error bytes)
40-track: 196608 bytes

Track 18 contains the BAM and directory.
"""

import random
import struct
from pathlib import Path

SEED = 0x4E554654
OUT_DIR = Path(__file__).resolve().parents[5] / "tests" / "vectors" / "d64"
OUT_DIR.mkdir(parents=True, exist_ok=True)

# Sectors per track (1-indexed)
D64_SPT = [0,
            21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
            19, 19, 19, 19, 19, 19, 19,
            18, 18, 18, 18, 18, 18,
            17, 17, 17, 17, 17, 17, 17, 17, 17, 17]


def track_offset(track: int) -> int:
    """Byte offset of the start of track T (1-indexed)."""
    off = 0
    for t in range(1, track):
        off += D64_SPT[t] * 256
    return off


def build_d64(n_tracks: int = 35, with_error_block: bool = False,
               bad_sectors: bool = False,
               truncate_at: int = -1) -> bytes:
    total_sectors = sum(D64_SPT[1:n_tracks + 1])
    out = bytearray(total_sectors * 256)
    rng = random.Random(SEED)

    # Fill with deterministic pattern
    for i in range(len(out)):
        out[i] = rng.randint(0, 255)

    # Track 18 Sector 0 = BAM (hardcoded valid structure)
    bam_offset = track_offset(18)
    out[bam_offset + 0] = 18     # first directory track
    out[bam_offset + 1] = 1      # first directory sector
    out[bam_offset + 2] = ord("A")  # DOS version 'A' (CBM DOS 2.6)
    out[bam_offset + 3] = 0x00
    # Disk name at offset +0x90 (PETSCII, padded with 0xA0)
    name = b"UFT-TEST\xA0\xA0\xA0\xA0\xA0\xA0\xA0\xA0"
    out[bam_offset + 0x90:bam_offset + 0x90 + 16] = name

    # Add error block if requested
    if with_error_block:
        error_block = bytearray(683)
        for i in range(683):
            error_block[i] = 0x01  # 0x01 = no error
        if bad_sectors:
            # Mark sector 0 of track 1 as uncorrectable
            error_block[0] = 0x0B  # 0x0B = data block not found
        out = bytes(out) + bytes(error_block)

    if truncate_at > 0:
        out = bytes(out)[:truncate_at]

    return bytes(out)


def write(path: Path, data: bytes):
    path.write_bytes(data)
    print(f"  [gen]  {path.relative_to(path.parents[3])} ({len(data)} bytes)")


def main():
    random.seed(SEED)
    write(OUT_DIR / "minimal_35tracks.d64", build_d64(35))
    write(OUT_DIR / "with_error_block.d64", build_d64(35, with_error_block=True))
    write(OUT_DIR / "bad_sectors.d64",
          build_d64(35, with_error_block=True, bad_sectors=True))
    write(OUT_DIR / "truncated.d64", build_d64(35, truncate_at=4096))
    print("Done. All 4 D64 fixtures generated.")


if __name__ == "__main__":
    main()
