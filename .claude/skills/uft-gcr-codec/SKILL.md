---
name: uft-gcr-codec
description: Work on GCR encoding and decoding - Commodore 4-to-5, Apple 6-and-2 and 5-and-3, Victor 9000 zones - including zone tables, speed zones, half-tracks, and which copy of a table is canonical. Use when decoding or encoding GCR, when sector counts per track are wrong, when a zone boundary is suspect, or when the user mentions "GCR", "Zonentafel", "Speed-Zone", "Halbspur", "4-to-5", "6-and-2", "1541", "Victor".
---

# GCR codecs and zone tables

GCR is the one codec family in this tree without a skill, and the one where a
wrong table is hardest to see: the disk still adds up, the file size still
fits, and only the sector *positions* are wrong.

Three unrelated GCR families live here. Do not carry a rule from one to
another:

| Family | Coding | Zoning |
|---|---|---|
| Commodore 1541/1571 | 4-to-5 | 4 speed zones by track |
| Apple II / Macintosh | 6-and-2 (5-and-3 on 13-sector) | 3.5" has a 5-zone table, 5.25" has none |
| Victor 9000 | its own | **nine** zones, and a **different table per head** |

## When to use this skill

- Writing or fixing a GCR decoder or encoder
- Sector counts per track do not match the reference
- A zone boundary is off, or a track reads from the neighbouring track
- Half-track handling (G64, `.nib`, flux)
- Deciding which of several in-tree tables is the one to use

**Not** for: MFM/FM (see `uft-format-plugin`, and MF-864/869 for the FM path),
the probe (`uft-sonde-konfidenz`), protection signals (`uft-schutzsignal`).

## Before you write a table: find out which one already exists

Check this first, because the tree has duplicates and they do not agree:

- The four CBM zone lengths lie in the tree **23 times, in nine different
  counting styles** (P3-150 - and that entry itself corrects an earlier claim
  of "eleven times in four styles"; even the count of the duplicates had
  drifted).
- The Apple GCR table exists **sevenfold**, and the copy that is tested is the
  one only tests call.
- Three Victor geometries were found; the third
  (`src/formats/victor/victor9k.c`, orphaned) claims 1285 sectors - a number
  no Victor disk has (MF-1026).

**Rule:** measure which copy the production path actually uses before trusting
any of them:

```bash
grep -rn "<a distinctive value from the table>" src/ include/ | sort
graft callers <table symbol>
```

If the production path uses its own private copy, that *is* the finding -
report it; do not silently add a twenty-fourth.

## The three failure classes

### 1. A sum that adds up says nothing about the distribution inside

`victor9k` (MF-1026): two zone boundaries were off by one on head 0 - track 48
had 15 sectors instead of 14, track 70 had 12 instead of 13. Because **+1 and
-1 cancel**, the total stayed 1224, the file size check could never notice,
and **22 tracks (49..70) were read 512 bytes too high**. Track 48 emitted a
15th sector belonging to track 49; track 70 never delivered its 13th.

**Rule:** never validate a zone table by its total. Validate per track, and
build the fixture so every sector **names its own physical position**
(cylinder, head, sector number). Then a wrong boundary, a swapped table and a
different layout all fail. At T1b `victor9k` asserts: 160 tracks each
reporting the sector count MAME's table gives for *that* head and *that*
track, and 2391 of 2391 sectors at their own position mark - 1224 on head 0,
1167 on head 1, the sum of **two** tables, not 1224 x 2.

### 2. One table per head is not one table

Reading Victor head 1 with head 0's table gave **57 of 80 tracks the wrong
sector count and 79 of 80 the wrong offset** - off by 28672 bytes at track 79
- and no double-sided image could be opened at all, because the expected file
size came out as 1253376 instead of the real 1224192 (MF-1026).

### 3. A zone table does not survive a fixed-length carrier

When routing evidence through an intermediate format, the zoning can be
silently dropped:

- `2img` (Apple 3.5", zones 12/11/10/9/8): via `mfi` all 1600 blocks survive;
  via `hfe` **192 are lost** - exactly one zone, 16 tracks x 12 sectors,
  because the outermost zone does not fit HFE's fixed track length; via `mfm`
  **472** are lost (MF-1085, class MF-539).
- `victor9k`: via `mfi` all 2391 marks return, via `mfm` only **2128** - the
  HxC MFM format does not carry nine speed zones.

**Rule:** choose the intermediate by measurement, not convenience. A channel is
a channel only once you measured the right path.

## Family notes, as measured here

### Commodore

- The `+1` convention does not apply: `uft_format_add_sector()` takes a
  **0-based** index and adds 1. Its own header names Apple, Amiga and
  Commodore as the cases where that is wrong (MF-1016).
- The 1541 lays sectors on the track **ascending, not interleaved** - measured
  and settled in P3-111. P3-110 records the matching defect: a D64 producer
  wrote sectors twice on 18 of 35 tracks and omitted others.
- G64 speed offsets were masked away, and **both** tables were off by one
  half-track (P3-36). G64 has three readers here, and the one carrying the
  checks is the one nobody calls (P3-147).
- `d64_write_track_gcr()` had no caller and was not unfinished - it was the
  **richer** of the two paths (P3-116, wired in MF-862).

### Apple

- 3.5" uses **512-byte** sectors and the zone table `ns = 12 - track/16`,
  giving 12/11/10/9/8. The table proves itself:
  `16 x (12+11+10+9+8) x 512 = 409600`, and twice that is exactly the two
  3.5" lengths in MAME's own `s_formats[]` (MF-1031).
- Assuming 5.25" geometry for every 2MG file left **327 680 of 819 200 bytes**
  reachable and returned a block from the other side of the disk (MF-1031).
- 13-sector (`d13`) uses 5-and-3, physical order `(i * 10) % 13` (MF-1085).

### Victor 9000

Nine zones, two tables, 1224 sectors on head 0 and 1167 on head 1 - a
double-sided image is 1 224 192 bytes.

## Workflow

### Step 1 - Name the reference and locate the existing copies

Find the authoritative table (spec, or oracle source read as *Spec* per
MF-695) and quote it in the header. Then grep for existing copies as above and
record how many you found.

### Step 2 - Red proof first

Measure current behaviour on the production path: per track, the sector count
and the offset of sector 0. Those two columns are where zone errors show. Keep
the numbers for the commit.

### Step 3 - Build a position-naming fixture

Every sector carries its own cylinder, head and number. A running index will
not catch failure class 1.

### Step 4 - Restate the table inside the test

The test must not consult the same source as the subject under test (MF-1000).
Write the zone table a second time in the test file.

### Step 5 - Mutation matrix

Rotate the zone table by one position; move a single boundary by one; swap the
per-head tables; make the numbering 0-based. Each must fail its own assertion
- see `uft-mutationsmatrix`.

## Verification

```bash
cmake --build build-tests-ci
ctest --test-dir build-tests-ci -R "gcr|g64|d64|victor|2img|d13"
python scripts/check_consistency.py       # 0/0/0/0
```

## Done means

- [ ] The table's source is named and stands in the header
- [ ] Existing in-tree copies counted, production path's copy identified
- [ ] Per-track validation, never validation by total
- [ ] Fixture sectors name their physical position
- [ ] Per-head tables kept separate where the family has them
- [ ] The intermediate carrier was chosen by measurement
- [ ] The table is restated inside the test
- [ ] Mutation matrix N of N, including a one-position table rotation
