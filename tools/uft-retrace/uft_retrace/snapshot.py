"""Compare emulator memory snapshots without guessing code semantics."""

from __future__ import annotations

from pathlib import Path

from .util import sha256_file


def changed_ranges(before: bytes, after: bytes, base: int = 0,
                   minimum_run: int = 16, merge_gap: int = 8) -> list[dict]:
    limit = max(len(before), len(after))
    raw: list[tuple[int, int]] = []
    start = None
    for index in range(limit):
        left = before[index] if index < len(before) else None
        right = after[index] if index < len(after) else None
        if left != right and start is None:
            start = index
        elif left == right and start is not None:
            raw.append((start, index))
            start = None
    if start is not None:
        raw.append((start, limit))

    merged: list[list[int]] = []
    for begin, end in raw:
        if merged and begin - merged[-1][1] <= merge_gap:
            merged[-1][1] = end
        else:
            merged.append([begin, end])

    return [
        {"file_offset": begin, "address": base + begin, "length": end - begin}
        for begin, end in merged if end - begin >= minimum_run
    ]


def compare_files(before_path: Path, after_path: Path, base: int,
                  minimum_run: int, merge_gap: int) -> dict:
    before = before_path.read_bytes()
    after = after_path.read_bytes()
    return {
        "schema": "uft-retrace-snapshot-diff-1",
        "before": {"name": before_path.name, "size": len(before),
                   "sha256": sha256_file(before_path)},
        "after": {"name": after_path.name, "size": len(after),
                  "sha256": sha256_file(after_path)},
        "base_address": base,
        "minimum_run": minimum_run,
        "merge_gap": merge_gap,
        "ranges": changed_ranges(before, after, base, minimum_run, merge_gap),
    }
