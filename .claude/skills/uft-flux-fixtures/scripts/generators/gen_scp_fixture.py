#!/usr/bin/env python3
"""
gen_scp_fixture.py — Generate deterministic SCP flux fixtures for UFT tests.

Produces 4 variants:
    tests/vectors/scp/minimal_mfm_80tracks.scp       — clean MFM, 3 revs
    tests/vectors/scp/weak_sector_at_track_18.scp    — induced jitter track
    tests/vectors/scp/corrupted_crc.scp              — CRC-wrong MFM track
    tests/vectors/scp/truncated.scp                  — truncated mid-track

Idempotent: re-running produces byte-identical output.

SCP format reference: http://www.cbmstuff.com/downloads/scp/scp_image_specs.txt
- All multi-byte fields little-endian
- 4-byte ASCII magic "SCP" + version byte
- Per-track block offsets in 4-byte LE (0x10..0x27F for tracks 0..167)
- Per-track data: header + rev blocks of 16-bit BE flux time deltas at 25ns
"""

import random
import struct
from pathlib import Path

SEED = 0x4E554654  # 'NUFT'
# Path: .claude/skills/uft-flux-fixtures/scripts/generators/gen_scp_fixture.py
#       parents[5]=uft-root, parents[4]=.claude, etc.
OUT_DIR = Path(__file__).resolve().parents[5] / "tests" / "vectors" / "scp"
OUT_DIR.mkdir(parents=True, exist_ok=True)

# ---- SCP constants ------------------------------------------------------

SCP_MAGIC = b"SCP"
SCP_VERSION = 0x24     # v2.4 (as written by Supercard Pro device)
TICKS_PER_NS = 25      # SCP timing resolution: 25 ns per tick
REVS_PER_TRACK = 2
AVG_BIT_CELL_NS = 2000  # DD MFM: 2us
# For test fixtures we use 2ms "tracks" instead of real 200ms — keeps files
# small (~100 KB instead of 36 MB). Real SCPs decode unchanged; fixtures
# are for testing parser paths, not decoding real disks.
REV_TIME_NS = 2_000_000


def scp_header(n_tracks: int, revolutions: int, flags: int = 0) -> bytearray:
    """Build the 16-byte SCP header."""
    hdr = bytearray(16)
    hdr[0:3] = SCP_MAGIC
    hdr[3] = SCP_VERSION
    hdr[4] = 0x00  # disk type: generic
    hdr[5] = revolutions
    hdr[6] = 0      # start track
    hdr[7] = n_tracks - 1  # end track
    hdr[8] = flags
    hdr[9] = 0      # bit cell encoding (0 = 16-bit)
    hdr[10] = 0     # heads (0 = both)
    hdr[11] = 0     # resolution (0 = 25ns)
    # bytes 12..15 = checksum of all data after offset 16 (LE32)
    return hdr


def synthesize_mfm_track(track_num: int, jitter_ns: int = 0,
                          corrupt_crc: bool = False) -> bytes:
    """
    Emit raw flux transitions for one track as 16-bit BE timing deltas.
    Deltas are in 25ns ticks.
    """
    # MFM cell time: ~2us, so a single-flux cell is ~2000ns = 80 ticks
    # '101' pattern (minimum) = 2 us delta
    # '1001' pattern (medium) = 3 us delta
    # '10001' pattern (longest) = 4 us delta
    # We synthesize a realistic distribution.
    rng = random.Random(SEED + track_num)

    deltas = []
    time_ns = 0
    while time_ns < REV_TIME_NS:
        # Biased distribution: 50% short, 35% medium, 15% long
        roll = rng.random()
        if roll < 0.50:
            delta_ns = 2000
        elif roll < 0.85:
            delta_ns = 3000
        else:
            delta_ns = 4000

        # Add jitter if requested
        if jitter_ns > 0:
            delta_ns += rng.randint(-jitter_ns, jitter_ns)

        ticks = max(1, delta_ns // TICKS_PER_NS)
        while ticks > 0xFFFF:
            deltas.append(0x0000)  # overflow marker
            ticks -= 0xFFFF
        deltas.append(ticks)
        time_ns += delta_ns

    # Corrupt CRC: flip some bits in the middle
    if corrupt_crc and len(deltas) > 100:
        for i in range(50, 60):
            deltas[i] ^= 0x00FF

    # Pack as big-endian 16-bit
    return b"".join(struct.pack(">H", d & 0xFFFF) for d in deltas)


def build_track_block(track_num: int, revolutions: int,
                       jitter_ns: int = 0,
                       corrupt_crc: bool = False) -> bytes:
    """Build a complete track block: TRK header + rev descriptors + data."""
    # TRK header: "TRK" + track number
    buf = bytearray()
    buf.extend(b"TRK")
    buf.append(track_num)

    # Per-rev descriptors: 12 bytes each (index time, flux count, flux offset)
    rev_data_chunks = []
    rev_offsets = []
    current_offset = 4 + revolutions * 12  # after TRK + rev descriptors

    for rev in range(revolutions):
        flux_data = synthesize_mfm_track(track_num,
                                           jitter_ns=jitter_ns,
                                           corrupt_crc=corrupt_crc and rev == 0)
        # Flux count = number of 16-bit words
        flux_count = len(flux_data) // 2
        index_time_ticks = REV_TIME_NS // TICKS_PER_NS

        rev_offsets.append((index_time_ticks, flux_count, current_offset))
        rev_data_chunks.append(flux_data)
        current_offset += len(flux_data)

    # Write rev descriptors
    for idx_ticks, flux_count, offset in rev_offsets:
        buf.extend(struct.pack("<III", idx_ticks, flux_count, offset))

    # Write flux data
    for chunk in rev_data_chunks:
        buf.extend(chunk)

    return bytes(buf)


def build_scp_file(n_tracks: int,
                    revolutions: int = REVS_PER_TRACK,
                    jitter_track: int = -1,
                    corrupt_track: int = -1,
                    truncate_at: int = -1) -> bytes:
    """Build a complete SCP file."""
    header = scp_header(n_tracks, revolutions)
    track_offset_table = bytearray(168 * 4)  # 168 slots, 4 bytes each

    track_data_blocks = []
    current_offset = 16 + 168 * 4  # after header + offset table

    for track in range(n_tracks):
        jitter = 200 if track == jitter_track else 0
        corrupt = track == corrupt_track

        block = build_track_block(track, revolutions,
                                    jitter_ns=jitter,
                                    corrupt_crc=corrupt)
        struct.pack_into("<I", track_offset_table, track * 4, current_offset)
        track_data_blocks.append(block)
        current_offset += len(block)

    full_data = header + bytes(track_offset_table)
    for block in track_data_blocks:
        full_data += block

    # Compute and write checksum (over bytes after offset 16)
    checksum = sum(full_data[16:]) & 0xFFFFFFFF
    full_data = (full_data[:12] +
                 struct.pack("<I", checksum) +
                 full_data[16:])

    if truncate_at > 0:
        full_data = full_data[:truncate_at]

    return full_data


def write_fixture(path: Path, data: bytes):
    path.write_bytes(data)
    print(f"  [gen]  {path.relative_to(path.parents[3])} ({len(data)} bytes)")


def main():
    random.seed(SEED)

    # 1. Minimal clean MFM, 80 tracks, 3 revs
    data = build_scp_file(80)
    write_fixture(OUT_DIR / "minimal_mfm_80tracks.scp", data)

    # 2. Weak sector at track 18 (jitter)
    data = build_scp_file(80, jitter_track=18)
    write_fixture(OUT_DIR / "weak_sector_at_track_18.scp", data)

    # 3. Corrupted CRC at track 5
    data = build_scp_file(80, corrupt_track=5)
    write_fixture(OUT_DIR / "corrupted_crc.scp", data)

    # 4. Truncated mid-track
    data = build_scp_file(80, truncate_at=4096)
    write_fixture(OUT_DIR / "truncated.scp", data)

    print("Done. All 4 SCP fixtures generated deterministically.")


if __name__ == "__main__":
    main()
