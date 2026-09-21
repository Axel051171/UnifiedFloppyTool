---
name: uft-oracle-eichung
description: Register and calibrate an external reference tool as an oracle - decide its channel under the licence rules, prove it runs, pin its version, and record when it may be overruled. Use when a new reference tool appears, when a tier raise needs a foreign hand, when an oracle disagrees with UFT, or when the user mentions "Oracle", "Orakel", "Referenzwerkzeug", "fremde Hand", "eichen", "welches Werkzeug kann das lesen".
---

# Registering and calibrating an oracle

An oracle is a foreign tool **executed** as a reference, not ported. It is the
channel that turns a T3 format into a verified one, and `docs/ORACLES.md`
carries 74 rows of them.

An oracle is a **reference, not a proof**. Two have been overruled here with
reason, and both times the oracle was wrong.

## When to use this skill

- A new reference tool has turned up and needs a channel decision
- A tier raise needs a foreign hand (`uft-tier-hebung` Step 3)
- An oracle and UFT disagree and you must decide which wins
- An oracle's build or invocation is undocumented, and the next session will
  rediscover it the hard way

**Not** for: clean-room reimplementation of a licence-blocked template (that is
the `uft-nachbau` agent), scouting for new tools (`uft-scout`,
`uft-github-scout`), the tier procedure itself (`uft-tier-hebung`).

## Step 1 - Pick the channel before you touch the code

MF-695, strongest legal channel first. "Licence before capability" does not
mean "discard the find" - it means *by which route*:

| Channel | When | Example in tree |
|---|---|---|
| **Port** | licence compatible, attribution set | - |
| **Nachbau** | behaviour documented, code blocked | Ordinal/Dewarp from flux-analyze (GPL-3) |
| **Helper process** | not linkable, executable | PFS3lib (BSD-4) |
| **Oracle** | execution free, redistribution not | `dtc` (proprietary), SHA-pinned |
| **Spec** | only the documentation is readable | HxC format descriptions |
| **Data/Fixture** | image free, code not | corpus contributions |
| **Fundus** | no channel today | `ipf-flux`, until the capsimg question is settled |

Reading a foreign source for *behaviour* and writing your own implementation
is the **Spec** channel. Say so in the header in those words - "Verhalten nach
der Dokumentation von X, eigenstaendige Implementierung" - because an
attribution is a **legal statement**, not a courtesy (MF-636). If you write
"Based on X" or "Port of X", you must name the source's licence.

## Step 2 - Prove it runs, and write down how

The build is often not the documented one, and the next session pays for the
omission. libdsk is the worked example (MF-1028): `make` is `mingw32-make.exe`
in the Qt toolchain, `./configure` runs, `make` fails on an unquoted
`C:/Program Files/...`, and the build succeeds by invoking gcc directly on 70
library files, without `tools/dskutil.c`, with `-lz`.

Record in `docs/ORACLES.md`: exact build commands, the invocation, the version
string, and a SHA-256 pin where redistribution is not allowed.

Version strings are themselves a claim to measure: `ORACLES.md` said libdsk
had no version query - `dsktrans -version` answers "libdsk version 1.5.12"
(MF-1028).

## Step 3 - Calibrate against the object, not against yourself

An oracle that only agrees with UFT proves nothing - that is the closed circle
of `apridisk` (MF-1009) and `qrst` (MF-1028), where writer and reader were
mirrors of the same invention.

Calibrate on something the **file itself** carries:

- `pri` - the spec names the CRC of the empty `"END "` chunk as `0x3D64AF78`,
  and UFT's freshly written `uft_pri_crc()` hits it (MF-1036).
- `qrst` - the checksum (sum of `byte * (1 + offset)` over the disk) stands in
  the file and is recomputed over the **read** sectors (MF-1028).
- `sap` - the checksum parameters were derived from **behaviour**, not copied:
  libsap uses its own table, and copying a table would be a GPL-2 take-over.
  The parameter set (reflected CCITT 0x8408, init 0xFFFF, span = 4 header
  bytes + plaintext) was searched across six sectors and found uniquely; 16 of
  16 track-0 checksums verify (MF-1022).

## Step 4 - Decide what the oracle may and may not settle

### It reads, or it writes - that is the tier boundary

A tool that only *reads* your file leaves the format at T2. T1b needs a
foreign **producer**. Check the tool's own module list: hxcfe marks some
formats `RW` and others `R` only (`PRI;R` kept `pri` at T2 until fluxfox
turned out to ship a produced file, MF-1071).

### "The tool cannot do it" is usually a statement about the invocation

Three corrections, same shape: `kfx` (MF-1024); `qrst`/`myz80`/`nanowasp`
(MF-1033 - the missing channel was libdsk's `-format <name>`, available all
along); `v9t9` (MF-1060 - the missing channel was a carrier format that brings
its own geometry). Before recording "no oracle available", list the
invocations you tried.

### Name equality is not identity

floptool's `cpm` is "Poly CP/M disk image" and has nothing to do with UFT's
libdsk-derived `cpm` with its 55 diskdefs. **The same name is not a channel**
(MF-1085).

### A tool whose own round trip corrupts a disk is not a producer

floptool's `esq16` was rejected by measurement: its `load()` assigns sector
numbers 0..9 while its `save()` collects 1..10, so of 1600 sectors **1440 come
back shifted by one place and 160 are zeroed** - one per track, 83 360 bytes
differing (P3-364).

### When the oracle may be overruled - twice, both with reasons

| Case | UFT wins because |
|---|---|
| `udi` (MF-1015) | `src/samdisk/udi.cpp` computes with `int32_t`, so its `crc >> 1` is an arithmetic shift; against the originator's reference code it falls |
| `dim` (MF-1037) | MAME says 9x1024 for media byte 0x03, hxcfe says 18x512 - both give 1 474 560 bytes, so file size cannot decide; the **executed** loader decided it, with 2880 sectors |

Both are written into the test with the reason. Overruling an oracle without a
measurement is just a preference.

## Step 5 - Record the divergences you keep

Where UFT deliberately differs, say so and pin it:

- `scl` - neither SAMdisk (source filename) nor HxC (`"HxCFE"`) has a real
  disk name to write, so both invent one. UFT leaves the field empty, and the
  test nails that down ("Keine erfundenen Daten", MF-1014).
- `myz80` - libdsk treats a missing sector as `0xE5` and calls that no error.
  UFT returns the same bytes but **marks them missing**: "the format says
  0xE5" and "0xE5 was read here" are two different statements (MF-980).

## Verification

```bash
# the oracle itself
<oracle> -version                     # record the string
sha256sum <oracle binary>             # pin where redistribution is barred

# the calibration
cmake --build build-tests-ci
ctest --test-dir build-tests-ci -R "<format>"
python scripts/check_consistency.py   # 0/0/0/0
```

Then regenerate the derived docs (`gen_stand.py` LAST), and write nothing to
the tree while a commit hook holds `.git/uft-commit.lock`.

## Done means

- [ ] Channel chosen per MF-695 and written in the header
- [ ] Licence of the source named wherever an attribution is claimed
- [ ] Build and invocation recorded in `docs/ORACLES.md`, including deviations
      from the documented build
- [ ] Version string measured, not assumed; SHA pinned if redistribution is barred
- [ ] Calibrated against something the file itself carries, not against UFT
- [ ] Read-only vs. producing recorded - it decides T2 vs. T1b
- [ ] Any overruling of the oracle carries a measurement and sits in the test
- [ ] Deliberate divergences written down
