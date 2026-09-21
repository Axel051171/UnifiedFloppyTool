---
name: uft-tier-hebung
description: Raise a disk format from T3 to T2, T1b or T1 - find a named reference, write the red proof first, obtain evidence from a foreign hand, and guard it with a mutation matrix. Use when a format needs verification, when a tier is disputed, when looking for an oracle or fixture for a format, or when the user mentions "Stufe heben", "T3", "T1b", "verifizieren", "fremde Hand", "Beleg fuer Format X", "warum steht X auf T3".
---

# Raising a format's verification tier

This is the most-repeated procedure in this tree: roughly eighty MF entries
are one execution of it. The tier says **where the evidence comes from** - not
that the reader is finished.

Current state (`docs/VERIFICATION_TIERS.md`, generated):
T1 = 8 · T1b = 64 · T2 = 12 · T3 = 2 (`dms`, `syn`) · n/a = 2 · total 88.

## When to use this skill

- A format sits on T3 and needs evidence
- A T2 format needs a foreign *producer* to reach T1b
- A tier claim is disputed, or a tier row looks unearned
- Searching for an oracle, a fixture, or a spec for a specific format
- Auditing whether an existing tier still holds

**Not** for: writing the plugin itself (`uft-format-plugin`), the probe
(`uft-sonde-konfidenz`), calibrating an oracle (`uft-oracle-eichung`),
building the matrix (`uft-mutationsmatrix`).

## The ladder

| Tier | The evidence is | Test |
|---|---|---|
| **T3** | none - a synthetic test, or no test | the state the five fabricated parsers (FMT-2/3/10/11/12) were green in |
| **T2** | a foreign implementation **read** the format, or its source was compared field by field | source-to-source, or a foreign reader agrees |
| **T1b** | a foreign hand **produced** the image | the fixture was written by someone else's encoder |
| **T1** | a **real** capture or a real-world image | the object existed in the world |

The T2/T1b boundary is argued most often. T1b needs a foreign **producer**; a
foreign tool that merely *reads* your file leaves you at T2. Worked example:
`pri` stayed T2 while hxcfe could only read it (`PRI;R` in its module list),
and reached T1b when fluxfox turned out to ship one in its own test directory
(MF-1071).

## What "verified" means - all three, or it is unverified

From `docs/VERIFICATION_PLAN.md` §Einfrier-Regel:

1. The behaviour comes from a **named** reference (oracle, spec, real
   capture), or from a measurement on the proven path that stands **before**
   the code - red proof first.
2. **Every** number in the commit, the header and `KNOWN_ISSUES.md` is
   measured. Anything unmeasured is written as "nicht belegt".
3. The reference stands **in the header**.

Not sufficient: "I tested it" without a named reference; a red proof that
does not fire; a measurement rig that rebuilds the production path instead of
using it.

## The six traps this tree has paid for

### 1. The empty fixture that reported success (MF-1021)

hxcfe produced a `v9t9` fixture: correct size, success reported, and the file
was **100% `0xF6`** - the format fill byte, because its RAW loader does not
know the TI geometry. Had the input not carried a self-describing pattern,
`v9t9` would have been raised on a blank disk. Its first test ever, and
worthless.

**Rule:** a generated fixture is evidence only when its **content** is proven.
Two checks, both required:

- **Content**: the payload is not one repeated byte (report the most frequent
  byte and its share), and the expected pattern hits N of N.
- **Discrimination**: the wrong layout must *fail*. `v9t9` at T1b: 720 of 720
  hits with MAME's rule, and only **18** with a linear layout - exactly the
  two tracks where both formulas coincide (MF-1060).

Packed containers (`mfi`, `ipf`) carry no tier from a conversion alone
(P3-326/P3-327).

### 2. "The tool cannot do it" was a statement about the invocation

Three times the reader was right and the claim about the tool was wrong:

- `kfx` (MF-1024) - hxcfe's `KRYOFLUXSTREAM` module writes one file per track
- `qrst`/`myz80`/`nanowasp` (MF-1033) - libdsk's missing channel was
  `-format <name>`, available all along; P3-333 had claimed it could not
  produce fixtures
- `v9t9` (MF-1060) - the missing channel was a *carrier that brings its own
  geometry*: convert via ImageDisk, which names cylinder, head and sector
  explicitly, so the loader needs no table

**Rule:** before writing "no foreign hand available", list the invocations you
tried. Ask specifically: is there a carrier format that transports the
geometry? Does the tool take an explicit `-format` / `-type` argument?

### 3. A green round trip against your own writer proves nothing

`apridisk` (MF-1009) and `qrst` (MF-1028) both had green round-trip tests
while reader and writer were mirrors of the same invented format. `opd`
(MF-1084) showed the general shape: a run through the same format can simply
pass the bytes through.

**Rule:** route the evidence through a **foreign intermediate**. `opus`
reached T1b via `floptool flopconvert opd hfe` and back - across a
2 008 020-byte MFM cell stream, so geometry, sector order and numbering had
to be modelled for real. `d13` went through MAME's `.mfi` flux format.

But choose the intermediate by measurement: for `2img`, `mfi` carried all
1600 blocks while `hfe` lost **192** (one zone: 16 tracks x 12 sectors) and
`mfm` **472**, because Apple GCR has a zone table the fixed HFE track length
cannot hold. **A channel is a channel only once you measured the right path**
(MF-1085).

### 4. The counter-check was green for the wrong reason (four times)

MF-1014, MF-1026, MF-1028, MF-1031 - and MF-1065 twice inside one test: an
assertion read `sec->id.crc_ok` while the code writes `sec->id_crc_ok`, and
`uft_format_add_sector_with_id()` sets the nested field **unconditionally
true**. The assertion was green no matter what the reader did.

**Rule:** for every counter-check, name which line makes it fail. If you
cannot, the check is decoration. Note that `uft_format_add_sector*()` always
sets `UFT_SECTOR_OK` - a short `fread` must be marked missing explicitly with
`uft_format_mark_last_missing()`.

### 5. A sum that adds up says nothing about the distribution inside

`victor9k` (MF-1026): two zone boundaries were off by one on head 0 (track
48: 15 instead of 14, track 70: 12 instead of 13). Because **+1 and -1
cancel**, the total stayed 1224 and no size check could ever notice - while
22 tracks were read 512 bytes too high.

**Rule:** the fixture must carry the **physical position** of each sector
(cylinder, head, sector number), not a running index. Then a wrong zone
boundary, a swapped table and a different layout all fail.

### 6. A bitstream format carries no sectors

HFE, PRI, MFI, WOZ deliver a cell stream; sectors appear only through a
decoder. Asserting sector counts there is a category error (P3-326). Assert
the stream instead - track count, cells per track, bit rate - and read the
expected numbers **from the file's own track table**, not from a typed
constant. MF-1125 found a 20 000-byte-wide gate guarding an 83-byte error.

## Workflow

### Step 1 - Ask what evidence is missing

Read the format's row in `docs/VERIFICATION_TIERS.md`, including its
"T2 und nicht T1b, weil ..." clause. That clause names exactly what is
missing. Check `docs/ORACLES.md` (74 rows) and the corpus manifest before
concluding nothing exists - MF-1065, MF-1068 and MF-1075 each found the image
already lying in the tree, once inside an archive that `find` does not open.

### Step 2 - Red proof before the fix

Measure the *current* behaviour with a throwaway program against the
**production path** (`uft_disk_open()`, the registered plugin), never a
rebuilt copy. Record the numbers: sectors delivered, offsets, what the first
bytes of track 0 actually are. Those numbers go into the commit.

### Step 3 - Obtain the foreign evidence

Channels, strongest first (MF-695): Port · Nachbau · helper process ·
**oracle (execute, do not port)** · spec · data/fixture · Fundus. For tiers,
oracle and fixture are the usual ones; register a new oracle per
`uft-oracle-eichung`. Prove the fixture per trap 1 before using it.

### Step 4 - Nail every number down

The test asserts each measured number. Where the oracle is overruled, say so
with a reason in the test - it has happened twice with justification
(MF-1015 SAMdisk's `int32_t` arithmetic shift; MF-1037 MAME's 0x03 value
against hxcfe's, decided by the executed loader).

Restate foreign tables **inside the test**, so it does not ask the same
source as the subject under test (MF-1000).

### Step 5 - Mutation matrix

One mutation per promise, all falling. See `uft-mutationsmatrix`.

### Step 6 - Regenerate, never hand-edit

`docs/VERIFICATION_TIERS.md` says "**NICHT von Hand editieren**". Run the
generators in this order, and write nothing to the tree while a commit hook
holds `.git/uft-commit.lock`:

```bash
python scripts/gen_verification_tiers.py --write
python scripts/gen_fs_tiers.py
python scripts/gen_erzeuger_zensus.py
python scripts/update_inventory.py
python scripts/gen_stand.py          # LAST
```

## Verification

```bash
cmake --build build-tests-ci
ctest --test-dir build-tests-ci -R "<format>"
python scripts/check_consistency.py          # 0/0/0/0
python scripts/verify_build_sources.py       # no new regressions
```

## Done means

- [ ] The reference is named and stands in the header
- [ ] The red proof was seen failing
- [ ] Fixture content proven **and** discriminating
- [ ] Every number in commit and header is measured
- [ ] Mutation matrix N of N, each mutation failing its own promise
- [ ] Tier table regenerated, not edited
- [ ] The "T2 und nicht T1b, weil ..." clause written, or removed with reason

## A tier is a statement about evidence

`ipf` sits on T1 while its sector layer is still not evaluated and the file
delivers 0 sectors (P3-360/P3-361). That is not a contradiction: the tier says
where the proof comes from, not that the reader is complete. Write the open
part down rather than letting the tier imply it away.

And a tier may never move because the number would look better: a figure
changes only when a new measurement exists (MF-1077).
