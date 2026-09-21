---
name: uft-sonde-konfidenz
description: Write or audit a UFT format probe - the probe() that decides which plugin claims a file, and the confidence number it reports. Use when adding or reviewing a probe, when a file opens as the wrong format, when a probe never rejects anything, when a tier raise is blocked by detection, or when the user mentions "Sonde", "probe", "Konfidenz", "Erkennung", "wird als falsches Format erkannt", "confidence band", "erkennt jede Datei".
---

# Format probe and confidence

The probe decides which of ~137 plugins claims a file. It is the most
defect-prone function in the format layer, and its failures are **silent**:
the wrong plugin wins, `open()` reads with the wrong geometry, and the user
gets bytes that look like data.

This skill covers writing a new probe, auditing an existing one, and the
five failure classes this tree has measured repeatedly.

## When to use this skill

- Adding `probe()` to a format plugin
- A file is detected as the wrong format, or as nothing
- A probe accepts every buffer, or rejects a valid file
- `uft-tier-hebung` is blocked because detection is unreliable
- Reviewing a confidence number that looks chosen rather than derived

**Not** for: reading sectors once the format is known (`uft-format-plugin`),
filesystem detection (`uft-filesystem`), protection signals
(`uft-schutzsignal`).

## The contract

```c
/* include/uft/uft_format_plugin.h:426 */
bool (*probe)(const uint8_t* data,     /* first bytes, >= 512    */
              size_t        size,      /* size of THIS BUFFER    */
              size_t        file_size, /* size of the WHOLE FILE */
              int*          confidence); /* [out] 0-100          */
```

`size` and `file_size` are **different numbers**. The probe buffer is
typically 4096 bytes. Confusing them is failure class 2 below, measured six
times in this tree.

### The four bands (MF-729, `uft_format_plugin.h:1078`)

| Band | Range | Means |
|---|---|---|
| `UFT_PROBE_BAND_NONE` | 0-29 | no claim |
| `UFT_PROBE_BAND_SIZE` | 30-49 | only the file size fits |
| `UFT_PROBE_BAND_STRUCT` | 50-79 | structure was read and is plausible |
| `UFT_PROBE_BAND_MAGIC` | 80-100 | an identifying mark was hit |

The bands carry **meaning, not rank**. Before MF-729 each probe chose its own
number, and the same finding ("the size fits") was reported as anything from
35 to 85 - so a PC-160K image lost to `TRD` (82) and a Macintosh-800K to
`D81` (80), purely because those numbers were picked larger.

Two gates enforce the meaning mechanically, across **all** plugins:

- `tests/test_probe_confidence_on_zeros.c` - an all-zero buffer carries no
  signature, so nothing may report >= 50.
- `tests/test_probe_confidence_on_random.c` - anything claiming 50-79 must
  reject >= 95% of random buffers.

The second gate has a named origin: `trd_probe()` raised 70 to 82 when
`data[0x8E4] <= 128` - true for **half of all byte values**, and always true
on zeros.

## The five measured failure classes

Check every one before calling a probe done. Each has cost this tree a
correction, several of them more than once.

### 1. The magic bytes do not exist (five occurrences)

A probe searched for a signature that appears in **no real file** of that
format, so every genuine file was rejected while the feature table said
"Read: SUPPORTED".

| Format | Probe looked for | Reality |
|---|---|---|
| `86f` | `"86BX"` | no such magic (MF-961) |
| `sap` | `"SAP"` at offset 0 | offset 0 is the format byte (MF-1022) |
| `myz80` | `"MYZ80 "` | first 256 bytes are all `0xE5` (MF-1029) |
| `nanowasp` | 24-byte magic + 80-byte header | no header at all (MF-1030) |
| `logical` | `"LGD\0"` + 32-byte header | no header at all (MF-1032) |

**Rule:** the magic must come from a named reference (spec, oracle, real
capture) quoted in the file header - never from a neighbouring plugin, never
from the tree's own writer. A round-trip test against your own writer proves
nothing: writer and reader are mirrors of the same invention (`apridisk`
MF-1009, `qrst` MF-1028).

**Correcting the magic alone is not the fix.** MF-707/708 deliberately did
*not* one-line-fix `86f`: with the right magic it would have accepted the
files and then parsed them wrongly. Fix detection and layout together.

### 2. `file_size` is discarded (six occurrences)

```c
/* the trap, verbatim shape */
static bool xxx_probe_plugin(const uint8_t* data, size_t size,
                             size_t file_size, int* conf)
{
    (void)file_size;                 /* <-- here */
    return xxx_detect(data, size);   /* compares against 4096, not the file */
}
```

Measured consequences: `myz80`'s size fallback was **dead code** that could
never match; `cpm`'s whole detector was unreachable, so a valid 256256-byte
file probed 0 with the sonde buffer and 1 with the full buffer; `2img` could
not reproduce MAME's `offset + length == file_size` check at all.

**Rule:** for a headerless format `file_size` is usually the *only* checkable
property. Pass it through as its own argument, and write a counter-check that
hands in a large buffer with a small `file_size` - that is the check which
caught the dead fallback.

### 3. Ambiguity is guessed instead of declined

Several sizes match more than one geometry. `cpm`: 184320 bytes matches
`amstrad-pcw` (first sector 1), `amstrad-cpc` (**193**) and `spectrum-p3`.
The old selection took the first table row - a CPC data disk would have got
**all 360 sector numbers wrong** (MF-1039).

**Rule:** on ambiguity, decline. `logical` and `cpm` both abstain now, and the
confidence drops (`cpm`: 60 -> 40) because what is actually recognised is
smaller than it looked.

### 4. The probe agrees and `open()` cannot read

The probe wins the detection race with confidence 95, then `open()` returns
`-25`. The user is told this *is* the format and that it cannot be read.

Measured: `86f` (MF-961), `sap` (MF-1022), `pri` (MF-1036, probe 95 /
open -25), `2img` (MF-1031).

**Rule:** probe and `open()` must agree on the same field layout. Audit them
as one unit; a probe change without an `open()` read-back is half a change.

### 5. Confidence above its band without cover

50-79 means "I read the structure". 80+ means "I hit an identifying mark".
256 identical bytes are a **convention, not a signature** - `myz80` is capped
at 70 for exactly that reason; 409600 bytes is also an Apple 800K size, so
`nanowasp` sits at 40 ("only the size").

**Rule:** name, in the code comment, which bytes justify the band. If you
cannot name them, lower the number.

## Workflow

### Step 1 - Establish the reference before writing code

Find the named reference (spec, oracle binary, real capture) and quote it in
the file header. `docs/ORACLES.md` lists 74 registered oracles; see
`uft-oracle-eichung` to calibrate a new one. Reading a foreign implementation
for *behaviour* is the `Spec` channel (MF-695) - read, do not copy.

### Step 2 - Red proof first

Write the failing assertion **before** the fix, and watch it fail:

```c
/* the file the probe must accept */
assert(plugin->probe(buf, sizeof buf, real_file_size, &conf) == true);
/* the band it may not exceed */
assert(uft_probe_band(conf) <= UFT_PROBE_BAND_STRUCT);
/* the buffers it must reject */
assert(plugin->probe(zeros, sizeof zeros, sizeof zeros, &conf) == false
       || conf < UFT_PROBE_CONF_STRUCT_MIN);
```

A red proof that does not fire is not a red proof
(`docs/VERIFICATION_PLAN.md` §Einfrier-Regel).

### Step 3 - Write the probe

Order the checks cheapest-first and let each one raise the band explicitly:

```c
static bool xxx_probe_plugin(const uint8_t* data, size_t size,
                             size_t file_size, int* confidence)
{
    if (!data || size < 512 || !confidence) return false;
    *confidence = 0;

    /* 1. size band - the whole file, not the buffer */
    if (file_size < XXX_MIN_SIZE || (file_size % XXX_TRACK_LEN) != 0)
        return false;
    *confidence = 40;                 /* BAND_SIZE: only the size fits */

    /* 2. mark band - justified by these bytes, named here */
    if (memcmp(data, XXX_MAGIC, sizeof XXX_MAGIC) != 0) return true;
    *confidence = 85;                 /* BAND_MAGIC: 4-byte signature */
    return true;
}
```

If two format tables match `file_size`, return `false` rather than pick one.

### Step 4 - Register it

The registry probe function must be reachable. MF-447 measured that the
registry was empty at runtime and `uft_disk_open()` returned NULL for every
file - a probe nobody calls is worth nothing (`uft-format-plugin` Step 5).

### Step 5 - Mutation matrix

One mutation per promise; see `uft-mutationsmatrix`. A probe typically needs
mutations for: the magic, each size rule, the band ceiling, the `file_size`
pass-through, and the ambiguity abstention.

## Verification

```bash
cmake --build build-tests-ci
ctest --test-dir build-tests-ci -R "probe_confidence"     # both gates
ctest --test-dir build-tests-ci -R "<format>"
python scripts/check_consistency.py                        # 0/0/0/0
```

Then re-derive the docs in this order (`gen_stand.py` LAST, and nothing
writes to the tree while a commit hook holds `.git/uft-commit.lock`):

```bash
python scripts/gen_verification_tiers.py --write
python scripts/gen_fs_tiers.py
python scripts/gen_erzeuger_zensus.py
python scripts/update_inventory.py
python scripts/gen_stand.py
```

## Done means

- [ ] Magic and layout come from a **named** reference, quoted in the header
- [ ] The red proof was seen failing before the fix
- [ ] `file_size` is used as its own argument, with a counter-check
- [ ] The confidence band is justified by named bytes in a comment
- [ ] Both confidence gates pass
- [ ] Ambiguity declines instead of guessing
- [ ] `probe()` and `open()` agree on the same layout
- [ ] Mutation matrix N of N
