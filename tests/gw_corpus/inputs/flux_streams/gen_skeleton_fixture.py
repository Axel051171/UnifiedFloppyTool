#!/usr/bin/env python3
"""
gen_skeleton_fixture.py — generate the ONE synthetic SCP fixture that
bootstraps the gw_corpus skeleton (P3.1, Tester-Strategie Welle 1).

This is NOT a real disk capture. It is a deterministic, byte-stable
synthetic MFM flux stream whose only job is to give the differential
test harness a non-empty input to chew on while the real corpus is
still being recorded.

Forensic-honesty note (DESIGN_PRINCIPLES "Keine erfundenen Daten"):
the output file is named `synthetic_mfm_dd_10trk.scp` — it deliberately
does NOT carry a name like `ibm_360k_dos_boot.scp`, because it contains
no DOS boot sector and no real disk content. Real, honestly-named
captures replace it in Welle 2; see ../../README.md.

Idempotent: re-running produces a byte-identical file.

SCP format reference: http://www.cbmstuff.com/downloads/scp/scp_image_specs.txt
"""

import random
import struct
from pathlib import Path

SEED = 0x4E554654  # 'NUFT'
OUT_DIR = Path(__file__).resolve().parent
FIXTURE = OUT_DIR / "synthetic_mfm_dd_10trk.scp"

SCP_MAGIC = b"SCP"
SCP_VERSION = 0x24       # v2.4 (as written by a SuperCard Pro device)
TICKS_PER_NS = 25        # SCP timing resolution: 25 ns per tick
REVS_PER_TRACK = 2
N_TRACKS = 10            # tiny on purpose: skeleton, not coverage
# 2 ms synthetic "revolution" instead of the real ~200 ms — keeps the
# fixture a few KB instead of multiple MB. Parser paths exercise the
# same code; this fixture is for harness wiring, not disk decoding.
REV_TIME_NS = 2_000_000


def scp_header(n_tracks: int, revolutions: int) -> bytearray:
    hdr = bytearray(16)
    hdr[0:3] = SCP_MAGIC
    hdr[3] = SCP_VERSION
    hdr[4] = 0x00              # disk type: generic
    hdr[5] = revolutions
    hdr[6] = 0                 # start track
    hdr[7] = n_tracks - 1      # end track
    hdr[8] = 0                 # flags
    hdr[9] = 0                 # bit cell encoding (0 = 16-bit)
    hdr[10] = 0                # heads (0 = both)
    hdr[11] = 0                # resolution (0 = 25 ns)
    # bytes 12..15 = LE32 checksum of all data after offset 16
    return hdr


def synthesize_mfm_track(track_num: int) -> bytes:
    """Raw flux transitions for one track as 16-bit BE timing deltas."""
    rng = random.Random(SEED + track_num)
    deltas = []
    time_ns = 0
    while time_ns < REV_TIME_NS:
        roll = rng.random()
        if roll < 0.50:
            delta_ns = 2000      # '101'   minimum MFM cell
        elif roll < 0.85:
            delta_ns = 3000      # '1001'  medium
        else:
            delta_ns = 4000      # '10001' longest
        ticks = max(1, delta_ns // TICKS_PER_NS)
        deltas.append(ticks)
        time_ns += delta_ns
    return b"".join(struct.pack(">H", d & 0xFFFF) for d in deltas)


def build_track_block(track_num: int, revolutions: int) -> bytes:
    buf = bytearray()
    buf.extend(b"TRK")
    buf.append(track_num)

    rev_data_chunks = []
    rev_offsets = []
    current_offset = 4 + revolutions * 12   # after TRK + rev descriptors

    for _rev in range(revolutions):
        flux_data = synthesize_mfm_track(track_num)
        flux_count = len(flux_data) // 2
        index_time_ticks = REV_TIME_NS // TICKS_PER_NS
        rev_offsets.append((index_time_ticks, flux_count, current_offset))
        rev_data_chunks.append(flux_data)
        current_offset += len(flux_data)

    for idx_ticks, flux_count, offset in rev_offsets:
        buf.extend(struct.pack("<III", idx_ticks, flux_count, offset))
    for chunk in rev_data_chunks:
        buf.extend(chunk)
    return bytes(buf)


def build_scp_file(n_tracks: int, revolutions: int) -> bytes:
    header = scp_header(n_tracks, revolutions)
    track_offset_table = bytearray(168 * 4)   # 168 slots, 4 bytes each

    track_data_blocks = []
    current_offset = 16 + 168 * 4
    for track in range(n_tracks):
        block = build_track_block(track, revolutions)
        struct.pack_into("<I", track_offset_table, track * 4, current_offset)
        track_data_blocks.append(block)
        current_offset += len(block)

    full_data = header + bytes(track_offset_table)
    for block in track_data_blocks:
        full_data += block

    checksum = sum(full_data[16:]) & 0xFFFFFFFF
    full_data = full_data[:12] + struct.pack("<I", checksum) + full_data[16:]
    return full_data


def main() -> None:
    random.seed(SEED)
    data = build_scp_file(N_TRACKS, REVS_PER_TRACK)
    FIXTURE.write_bytes(data)
    print(f"[gen] {FIXTURE.name} ({len(data)} bytes)")


if __name__ == "__main__":
    main()
