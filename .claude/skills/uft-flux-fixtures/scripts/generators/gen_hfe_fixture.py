#!/usr/bin/env python3
"""
gen_hfe_fixture.py — Generate deterministic HFE v1 bitstream fixtures.

HFE v1 format (HxC Floppy Emulator):
- 512-byte header at offset 0 ('HXCPICFE' magic + geometry)
- Per-track block list at offset 0x200 (n_tracks * 4 bytes: offset+length)
- Track data: interleaved side 0 / side 1 bitstreams in 256-byte rows
"""

import random
import struct
from pathlib import Path

SEED = 0x4E554654
OUT_DIR = Path(__file__).resolve().parents[5] / "tests" / "vectors" / "hfe"
OUT_DIR.mkdir(parents=True, exist_ok=True)


def hfe_header(n_tracks=80, n_sides=2, bitrate_kbps=250, rpm=300):
    hdr = bytearray(512)
    hdr[0:8] = b"HXCPICFE"
    hdr[8] = 0          # format revision
    hdr[9] = n_tracks
    hdr[10] = n_sides
    hdr[11] = 0xFF      # track encoding: 0xFF = IBM MFM
    struct.pack_into("<H", hdr, 12, bitrate_kbps)
    struct.pack_into("<H", hdr, 14, rpm)
    hdr[16] = 0x07      # interface mode: 0x07 = SHUGART
    hdr[17] = 0x00      # reserved
    struct.pack_into("<H", hdr, 18, 1)  # track list offset = 1 * 512
    return hdr


def track_block(track_num, side_count=2, length=0x1800):
    """Generate bit data for one track (both sides interleaved in 256B rows)."""
    rng = random.Random(SEED + track_num)
    # MFM-like pseudo-pattern
    data = bytearray(length)
    for i in range(length):
        data[i] = rng.randint(0, 255)
    return bytes(data)


def build_hfe(n_tracks=80, corrupt_track=-1, truncate_at=-1):
    header = hfe_header(n_tracks)
    track_offsets = bytearray(1024)  # 256 slots max

    # Track list starts at offset 0x200 (sector 1 in HFE terms)
    tracks = []
    next_offset = 2   # sector 2 (after header + track list)

    for t in range(n_tracks):
        data = track_block(t)
        if t == corrupt_track:
            # Corrupt some bytes
            data = bytearray(data)
            for i in range(100, 150):
                data[i] ^= 0x5A
            data = bytes(data)
        n_sectors = (len(data) + 511) // 512
        struct.pack_into("<HH", track_offsets, t * 4,
                         next_offset, len(data))
        tracks.append(data)
        next_offset += n_sectors

    out = bytearray(header + bytes(track_offsets[:n_tracks * 4]))
    # Pad header+track_list to next 512 boundary
    while len(out) % 512 != 0:
        out.append(0xFF)

    for data in tracks:
        out.extend(data)
        # Pad to 512-byte boundary
        while len(out) % 512 != 0:
            out.append(0xFF)

    if truncate_at > 0:
        out = out[:truncate_at]

    return bytes(out)


def write(path: Path, data: bytes):
    path.write_bytes(data)
    print(f"  [gen]  {path.relative_to(path.parents[3])} ({len(data)} bytes)")


def main():
    random.seed(SEED)
    write(OUT_DIR / "minimal_80tracks.hfe", build_hfe(80))
    write(OUT_DIR / "weak_sector.hfe", build_hfe(80, corrupt_track=18))
    write(OUT_DIR / "corrupted_crc.hfe", build_hfe(80, corrupt_track=5))
    write(OUT_DIR / "truncated.hfe", build_hfe(80, truncate_at=2048))
    print("Done. All 4 HFE fixtures generated deterministically.")


if __name__ == "__main__":
    main()
