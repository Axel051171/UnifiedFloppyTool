---
name: uft-mutationsmatrix
description: Build the mutation matrix that proves a test can actually fail - one deliberate defect per promise, each caught by its own assertion. Use when a test suite is green and you need to know whether it guards anything, before claiming a fix is verified, when a red proof does not fire, or when the user mentions "Mutationsmatrix", "Rotbeweis", "kann der Test ueberhaupt rot werden", "bewacht der Test etwas".
---

# The mutation matrix

A green test proves nothing until you have seen it go red. This tree has
measured the alternative twice at full cost: five parsers were built against
invented specs and their tests were **green** (FMT-2/3/10/11/12), and
`tests/test_libdsk_formats.c` sat in `EXCLUDED_TESTS` - a test that could not
turn red because it was never built (MF-1028).

The matrix answers one question: **for each promise this test makes, is there
a defect that would make it fail?**

## When to use this skill

- Before declaring any format, probe or decoder fix verified
- A test suite is green and you want to know what it actually guards
- A red proof does not fire and you need to find out why
- Reviewing someone else's test for decorative assertions

**Not** for: writing the feature (`uft-format-plugin`), choosing evidence
(`uft-tier-hebung`), performance work.

## The shape

One row per promise: the mutation, the promise it should break, the result.

```
| # | Mutation                                   | Faellt          | Ergebnis |
|---|--------------------------------------------|-----------------|----------|
| 1 | Magic auf "XXXX" geaendert                 | probe_accepts   | ROT  ok  |
| 2 | Spurversatz cyl<->head vertauscht          | sector_position | ROT  ok  |
| 3 | Sektor-IDs 0-basiert statt 1-basiert       | sector_ids      | ROT  ok  |
| 4 | obere Schranke in read_track entfernt      | no_overrun      | GRUEN !! |
```

Report it in the commit body as "Mutationsmatrix N von N". Anything short of
N of N is named, not hidden.

## The four rules this tree learned the hard way

### 1. A crash counts as a hit

The measuring tool carried the very defect it was built to find: the matrix
evaluated only `[ROT]` lines and reported a **crash** as "slipped through".
That was exactly the state of the `cas` mutation which re-emitted an empty
block - and behind it sat a heap corruption, `STATUS_HEAP_CORRUPTION`
(0xC0000374) (MF-1040).

Since then a crash counts as a hit, in all thirteen matrix scripts. A new
harness must classify three outcomes, not two:

```
GRUEN   -> mutation slipped through   (a real gap)
ROT     -> caught by an assertion     (hit)
CRASH   -> caught by the runtime      (hit, and worth a note)
```

### 2. A green counter-check can be green for the wrong reason

Four occurrences: MF-1014, MF-1026, MF-1028, MF-1031 - plus MF-1065 twice
inside a single test.

| Case | Why it was green |
|---|---|
| `fdi_pc98` | the "psize does not fit" check already tripped the *second* condition, never the one under test |
| `pri` | the CRC counter-check flipped a byte in the **size field**, not the payload |
| `atx` | the assertion read `sec->id.crc_ok`; the code writes `sec->id_crc_ok`, and the nested field is set **unconditionally true** |
| `atx` | the weak-bit assertion checked the `weak_mask` pointer, not the `UFT_SECTOR_WEAK` flag |

**Rule:** for every mutation, name the line that makes it fail, and isolate
it. If a mutation trips a *different* guard first, change the fixture so only
the guard under test can react. `fdi_pc98` is the pattern: isolate through the
geometry while keeping the file size correct.

### 3. A mutation that cannot be isolated is named, not dropped

`qrst` reached nine of ten, and the tenth is written down rather than
massaged: "silently accept unknown track-record types" cannot be isolated,
because an unknown record type leaves its payload unconsumed and the record
walk fails anyway (MF-1028).

Redundancy is likewise **measured, not asserted**: at `2img` the upper bound
in `read_track` and the one in `uft_2img_track_offset` turned out to be
mutually redundant - each alone holds the promise, only removing both makes it
fall (MF-1031). Write that down; do not report it as a miss.

### 4. Run round two - and expect the misses to be yours

Several formats passed only on the second run, and the escapes were defects in
the author's own counter-checks, not in the subject:

- `pri` - five slipped at first; four were gaps in my own checks (MF-1036)
- `ipf` - 7 of 14 in round one; **four of the seven escapes were errors in the
  mutation itself**, three unreachable on that corpus (measured: all 6419
  SYNC elements have bit 0 == bit 7; 0 FUZZY elements; 0 tracks with
  `track_bits % 8`) (MF-1079)

**Rule:** when a mutation slips through, first ask whether the *mutation* is
wrong or unreachable on this fixture, before concluding the test has a gap.
Prove unreachability with a number.

## Workflow

### Step 1 - List the promises

Read the test and write down, one line each, what it claims. A promise is
something a defect could break: a magic, an offset formula, a sector count, a
band ceiling, a status flag, an error path.

### Step 2 - Baseline run first

Run the unmutated build and record it green. `cas` (MF-1040) showed why: a
harness that misreads outcomes produces a matrix wrong in both directions.

### Step 3 - One mutation at a time

Mutate the **production source**, not the test. Rebuild, run, record, revert
before the next one. Keep each mutation to one line where possible - a
multi-line mutation that fails tells you less.

Useful mutation classes for this tree:

| Class | Example |
|---|---|
| magic | change one signature byte |
| geometry | swap cylinder/head in the offset formula |
| numbering | make sector IDs 0-based instead of 1-based |
| bounds | remove an upper bound in `read_track` |
| status | report a short read as `UFT_SECTOR_OK` |
| band | raise the probe confidence one band |
| table | rotate a skew or zone table by one position |
| back-mutation | restore the *old* defect the fix removed |

The back-mutation is the strongest row: it proves the test would have caught
the original bug. `scl` used it - "Daten auf LBA 0", the old defect, and five
assertions fall (MF-1014). For `nanowasp`, rotating the skew table by **one**
position was enough (MF-1030).

### Step 4 - Script it, and keep the script

Write the matrix as a file with Write, never a heredoc - a heredoc eats `\n`,
`\t`, `\r\n` and every backslash; this tree has paid for that more than twenty
times, once by destroying the helper meant to avoid the problem (MF-1019).
Put the script next to the test so the next change can re-run it.

### Step 5 - Report it

In the commit body: "Mutationsmatrix N von N", plus a line for anything
un-isolatable and why.

## Verification

```bash
cmake --build build-tests-ci                     # baseline green
ctest --test-dir build-tests-ci -R "<test>"
# then per mutation: edit source, rebuild, run, revert
```

Windows note: subprocesses need MinGW's `bin/` on PATH. Without it the gcc
driver exits rc 1 with **no output at all** - it cannot find its `cc1` - and a
matrix then reports build failures as if they were results. That is a tool
error, not a subject error; one matrix reported 11 build failures / 0 hits
this way (MF-1125).

## Done means

- [ ] Every promise has a row
- [ ] Baseline recorded green before the first mutation
- [ ] Each mutation isolated - it fails its own assertion, not a neighbour's
- [ ] Crashes counted as hits
- [ ] At least one back-mutation restoring the original defect
- [ ] Escapes investigated as possible errors in the mutation, with a number
- [ ] Un-isolatable rows named in the commit, not dropped
- [ ] The matrix script is committed, not thrown away
