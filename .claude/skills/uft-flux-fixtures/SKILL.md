---
name: uft-flux-fixtures
description: |
  Use when generating synthetic flux/bitstream test data (SCP, HFE,
  KryoFlux RAW, A2R, MOOF) for unit tests, PoCs, or bug reproduction.
  Trigger phrases: "erzeuge testdaten", "synthetic flux file", "test
  fixture für SCP", "PoC input data", "generate fake disk image",
  "minimal valid SCP/HFE/KF file". Uses scripts/generators/*.py pattern
  to produce deterministic fixtures checked into tests/vectors/.
---

# UFT Flux Fixture Generator

Use this skill when a test needs a real-looking SCP/HFE/KF/A2R/MOOF
file. UFT's rule: fixtures must be deterministic, small (<1MB typical),
and live under `tests/vectors/` with a Python generator that
reproduces them byte-for-byte.

## Step 1: Pick the target format

| Format | Use when testing | Generator complexity |
|---|---|---|
| **SCP**       | PLL, flux decode, multi-rev fusion | S: header + transitions |
| **HFE v1/v2** | bitstream decode, sector extraction | M: track-data LUT |
| **HFE v3**    | v3-specific chunk types          | L: chunk encoder |
| **KryoFlux**  | raw stream parser                | M: stream encoding |
| **A2R**       | Apple flux (GCR)                 | M: chunk + timing |
| **MOOF**      | Applesauce Mac flux              | M: track-block LUT |
| **ADF**       | Amiga filesystem tests           | S: size + bootblock |
| **D64 image** | Commodore filesystem tests       | S: 174848 bytes + BAM |

Start with the smallest generator that exercises the code under test.

## Step 2: Generator location + naming

Python 3 generator: `scripts/generators/gen_<format>_fixture.py`.
Output: `tests/vectors/<format>/<description>.<ext>`.
Both are committed to git — re-running the generator should produce
identical bytes (deterministic).

Example:
```
scripts/generators/gen_scp_fixture.py
tests/vectors/scp/minimal_80tracks_5rev_mfm.scp
tests/vectors/scp/weak_sector_at_track_18.scp
```

## Step 3: SCP generator skeleton

```python
#!/usr/bin/env python3
"""Generate minimal valid SCP flux files for UFT unit tests.

Output:
  minimal_80tracks_5rev_mfm.scp — clean MFM, no weak bits, 5 revs
  weak_sector_at_track_18.scp   — synthetic weak sector for recovery tests
"""
import struct
import pathlib

OUT_DIR = pathlib.Path(__file__).parent.parent.parent / "tests" / "vectors" / "scp"
OUT_DIR.mkdir(parents=True, exist_ok=True)

SCP_MAGIC = b"SCP"

def build_scp_header(version, n_tracks, revolutions, bitrate_khz=500):
    hdr = bytearray(16)
    hdr[0:3] = SCP_MAGIC
    hdr[3] = version                              # e.g. 0x24
    struct.pack_into("<H", hdr, 4, revolutions)   # LE16 revs
    struct.pack_into("<H", hdr, 6, n_tracks - 1)  # end track (0-indexed)
    hdr[8] = 0  # flags
    hdr[9] = 0  # bit-cell width
    hdr[10] = 0 # heads (0 = both)
    # bytes 12..15 = file checksum (computed at end, zero for now)
    return hdr

def synthesise_mfm_track(n_revolutions, avg_cell_ns=2000):
    """Return raw transition-interval bytes for one full track."""
    # SCP format: 16-bit transition intervals at 40 ns resolution
    # TODO: emit real MFM pattern; this is a placeholder
    ...
```

Keep generators small and readable — they ARE the documentation of
what a valid fixture looks like.

## Step 4: Binary determinism

Rules:
- **No `random`** unless seeded with a constant (`random.seed(0x4E554654)`).
- **No timestamps** in the output.
- **No locale-dependent formatting** (LC_ALL=C equivalent).
- Re-running the generator twice must produce `diff -q` clean output.
- Test this in CI: `scripts/verify_fixtures.sh` rebuilds and diffs.

## Step 5: Commit the output files

Fixtures are committed binary:
```bash
git add tests/vectors/scp/minimal_80tracks_5rev_mfm.scp
git add scripts/generators/gen_scp_fixture.py
```

For large fixtures (>100KB), document size in the commit message.
Prefer multiple small fixtures over one large kitchen-sink file.

## Step 6: Use in tests

```c
/* tests/test_scp_parser.c */
static const char *FIXTURE = "tests/vectors/scp/minimal_80tracks_5rev_mfm.scp";

TEST(parse_fixture_returns_80_tracks) {
    uft_scp_file_t scp;
    ASSERT(uft_scp_open(FIXTURE, &scp) == UFT_OK);
    ASSERT(scp.track_count == 80);
    uft_scp_close(&scp);
}
```

For CMake discovery: fixtures are found relative to `${CMAKE_SOURCE_DIR}`.
Set the test working directory explicitly:

```cmake
set_tests_properties(test_scp_parser PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
```

## Common fixture patterns

| Pattern | Purpose |
|---|---|
| `minimal_*`       | Smallest valid file for the format |
| `corrupted_*`     | Byte-flipped to test error paths |
| `weak_sector_*`   | Induced jitter regions for recovery tests |
| `truncated_*`     | File cut mid-chunk for robustness tests |
| `wrong_magic_*`   | Header byte-swapped to test probe rejection |
| `large_*`         | Stress test (1MB+) — use sparingly |

## Anti-patterns

- **Don't check in real captured disks** — copyright risk. Fixtures
  must be synthetic or derived from public reference material.
- **Don't use test files outside `tests/vectors/`** — scattered
  fixtures become unfindable.
- **Don't generate at test runtime** — makes tests non-reproducible.
  The only exception is a Python generator invoked by CMake at
  configure time.

## Related

- `tests/vectors/` — existing fixtures (mirror this structure)
- `scripts/generators/` — existing generators (if empty, this skill
  starts the pattern)
- `docs/DESIGN_PRINCIPLES.md` Principle 1 (no silent data loss —
  fixtures must faithfully represent real pathologies)
